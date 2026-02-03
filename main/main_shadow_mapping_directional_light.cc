//
// Created by forna on 02.02.2026.
//
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <ctime>
#include <corecrt_math_defines.h>
#include <imgui.h>
#include <SDL3/SDL.h>

#include "model_loader.h"
#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

#include "stb_image.h"

class FramebufferRenderer : public common::DrawInterface, public common::SystemInterface {
public:
    enum PostEffect {
        EFFECT_NONE = 0,
        EFFECT_INVERT,
        EFFECT_GRAYSCALE,
        EFFECT_SEPIA,
        EFFECT_EDGE_DETECTION,
        EFFECT_BLUR,
        EFFECT_SHARPEN,
        EFFECT_EMBOSS,
        EFFECT_SOBEL,
        EFFECT_GAUSSIAN_BLUR,
        EFFECT_BLOOM, // Effet avancé: bloom
        EFFECT_NIGHT_VISION,
        EFFECT_COUNT
    };

    enum RenderMode {
        RENDER_DEFERRED = 0,
        RENDER_SHADOW_MAPPING,
        RENDER_SSAO,
        RENDER_DEBUG_SHADOW,
        RENDER_DEBUG_SSAO,
        RENDER_MODE_COUNT
    };

    void Begin() override {
        std::cout << "SkyboxRenderer::Begin() - Initialisation framebuffer" << std::endl;
        std::cout << "GL_VERSION: " << glGetString(GL_VERSION) << "\n";
        std::cout << "GLSL_VERSION: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
        // Utilisation de GetWindow() qui est déjà typé SDL_Window*
        SDL_Window *window = common::GetWindow();
        SetMouseLook(true);
        // Récupérer la taille réelle de la fenêtre
        SDL_GetWindowSize(window, &screenWidth_, &screenHeight_);
        width_ = screenWidth_;
        height_ = screenHeight_;
        initGL();
        createFramebuffer();
        createScreenQuad();
        // load model
        model_ = std::make_unique<Model>("data/model/nanosuit2/nanosuit.obj");
        core::Vec3F c = model_->GetCenter();
        float offsetY = -model_->GetMinY();
        target_[0] = c.x;
        target_[1] = c.y + offsetY;
        target_[2] = c.z;

        //orbitRadius_ = model_->GetBoundingRadius() * 2.5f;
        orbitRadius_ = std::max(orbitRadius_, 5.0f);
        std::cout << "Center: " << target_[0] << "," << target_[1] << "," << target_[2]
                << "  Radius: " << orbitRadius_ << std::endl;
        // optionnel: angle de départ pour bien voir
        yaw_ = 90.0f;
        pitch_ = 10.0f;
        // Créer des framebuffers supplémentaires pour les effets
        createMultiPassFramebuffers();
        createBloomFramebuffers();
        createSkyboxRessources();

        initPostProcessingShaders();
        initBloomShaders();

        // Initialiser les matrices de convolution
        initConvolutionKernels();
        initCubeResources();
        cubeDiffuseTex_ = loadTexture2D("data/textures/brickwall.jpg", true);
        cubeNormalTex_ = loadTexture2D("data/textures/brickwall_normal.jpg", true);

        createGBuffer(width_, height_);

        initShadowMapping();
        checkShadowFBO();
        initSSAO();

        createCubeLine();
        initCubeInstancing();


        // Initialiser ImGui si disponible
#ifdef IMGUI_ENABLED
        initImGui();
#endif
    }

    void Update(float dt) override {
        time_ += dt;

        // --- clavier ---
        const bool *keys = SDL_GetKeyboardState(nullptr);

        // ---- TOGGLE TAB (anti-rebond) ----
        bool tabDown = keys[SDL_SCANCODE_TAB];
        if (tabDown && !tabWasDown_) {
            SetMouseLook(!mouseLookEnabled_);
        }
        tabWasDown_ = tabDown;

        // 1-9: changer effet de post-processing
        for (int i = SDL_SCANCODE_1; i <= SDL_SCANCODE_9; i++) {
            if (keys[i] && !keyWasDown_[i]) {
                int effect = (i - SDL_SCANCODE_1) % (EFFECT_COUNT - 1) + 1;
                currentEffect_ = static_cast<PostEffect>(effect);
                std::cout << "Effet changé: " << effectNames_[currentEffect_] << std::endl;
            }
            keyWasDown_[i] = keys[i];
        }

        // 0: pas d'effet
        if (keys[SDL_SCANCODE_0] && !keyWasDown_[SDL_SCANCODE_0]) {
            currentEffect_ = EFFECT_NONE;
            std::cout << "Effet: Aucun" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_0] = keys[SDL_SCANCODE_0];

        // ESPACE: toggle bloom
        if (keys[SDL_SCANCODE_SPACE] && !keyWasDown_[SDL_SCANCODE_SPACE]) {
            useBloom_ = !useBloom_;
            std::cout << "Bloom: " << (useBloom_ ? "ON" : "OFF") << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_SPACE] = keys[SDL_SCANCODE_SPACE];

        // F1-F4: Changer le mode de rendu
        if (keys[SDL_SCANCODE_F1] && !keyWasDown_[SDL_SCANCODE_F1]) {
            currentRenderMode_ = RENDER_DEFERRED;
            std::cout << "Mode: Deferred Shading" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_F1] = keys[SDL_SCANCODE_F1];

        if (keys[SDL_SCANCODE_F2] && !keyWasDown_[SDL_SCANCODE_F2]) {
            currentRenderMode_ = RENDER_SHADOW_MAPPING;
            std::cout << "Mode: Shadow Mapping" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_F2] = keys[SDL_SCANCODE_F2];

        if (keys[SDL_SCANCODE_F3] && !keyWasDown_[SDL_SCANCODE_F3]) {
            currentRenderMode_ = RENDER_SSAO;
            std::cout << "Mode: SSAO" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_F3] = keys[SDL_SCANCODE_F3];

        if (keys[SDL_SCANCODE_F4] && !keyWasDown_[SDL_SCANCODE_F4]) {
            currentRenderMode_ = (currentRenderMode_ == RENDER_DEBUG_SHADOW)
                                     ? RENDER_SHADOW_MAPPING
                                     : RENDER_DEBUG_SHADOW;
            std::cout << "Mode: Debug Shadow" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_F4] = keys[SDL_SCANCODE_F4];

        if (keys[SDL_SCANCODE_F5] && !keyWasDown_[SDL_SCANCODE_F5]) {
            currentRenderMode_ = (currentRenderMode_ == RENDER_DEBUG_SSAO) ? RENDER_SSAO : RENDER_DEBUG_SSAO;
            std::cout << "Mode: Debug SSAO" << std::endl;
        }
        keyWasDown_[SDL_SCANCODE_F5] = keys[SDL_SCANCODE_F5];

        // ---- si la caméra est désactivée, on ne bouge pas ----
        if (!mouseLookEnabled_) return;


        // --- souris (relative) ---
        float mdx = 0.0f, mdy = 0.0f;
        SDL_GetRelativeMouseState(&mdx, &mdy);

        mdx *= mouseSensitivity_;
        mdy *= mouseSensitivity_;

        yaw_ += mdx;
        pitch_ -= mdy;

        if (pitch_ > maxPitch_) pitch_ = maxPitch_;
        if (pitch_ < minPitch_) pitch_ = minPitch_;
    }

    void FixedUpdate() override {
    }

    void End() override {
        SetMouseLook(false);


        if (gBuffer_)
            glDeleteFramebuffers(1, &gBuffer_);
        if (gPosition_) glDeleteTextures(1, &gPosition_);
        if (gNormal_) glDeleteTextures(1, &gNormal_);
        if (gAlbedoSpec_) glDeleteTextures(1, &gAlbedoSpec_);
        if (rboDepth_)
            glDeleteRenderbuffers(1, &rboDepth_);

        // Nettoyer les ressources Shadow Mapping
        if (shadowFBO_)
            glDeleteFramebuffers(1, &shadowFBO_);
        if (shadowDepthTex_) glDeleteTextures(1, &shadowDepthTex_);
        if (shadowProgram_)
            glDeleteProgram(shadowProgram_);

        // Nettoyer les ressources SSAO
        if (ssaoFBO_)
            glDeleteFramebuffers(1, &ssaoFBO_);
        if (ssaoColorBuffer_) glDeleteTextures(1, &ssaoColorBuffer_);
        if (ssaoBlurFBO_)
            glDeleteFramebuffers(1, &ssaoBlurFBO_);
        if (ssaoBlurBuffer_) glDeleteTextures(1, &ssaoBlurBuffer_);
        if (ssaoProgram_)
            glDeleteProgram(ssaoProgram_);
        if (ssaoBlurProgram_)
            glDeleteProgram(ssaoBlurProgram_);

        cleanup();
    }

    void PreDraw() override {
    }

    void Draw() override {
        switch (currentRenderMode_) {
            case RENDER_DEFERRED:
                renderDeferred();
                break;
            case RENDER_SHADOW_MAPPING:
                renderShadowMapping();
                break;
            case RENDER_SSAO:
                renderSSAO();
                break;
            case RENDER_DEBUG_SHADOW:
                renderDebugShadow();
                break;
            case RENDER_DEBUG_SSAO:
                renderDebugSSAO();
                break;
        }

        // RENDER PASS 3: Interface utilisateur
#ifdef IMGUI_ENABLED
        renderImGui();
#endif
    }

    void PostDraw() override {
    }

private:
    float time_ = 0.0f;
    float lightPos[3] = {3.0f, 3.0f, 3.0f};
    float camPos_[3] = {0.0f, 0.0f, 3.0f};
    float camFront_[3] = {0.0f, 0.0f, 1.0f};
    float camUp_[3] = {0.0f, 1.0f, 0.0f};

    float yaw_ = -90.0f; // regarde vers -Z
    float pitch_ = 0.0f;

    // centre de l’orbite (le modèle)
    float target_[3] = {0.0f, 0.0f, 0.0f};

    float modelYawOffset_ = 0.0f;

    // rayon d’orbite
    float orbitRadius_ = 20.0f;

    // limites de pitch
    float minPitch_ = -89.0f;
    float maxPitch_ = 89.0f;

    bool firstMouse_ = true;
    float lastMouseX_ = 400.0f;
    float lastMouseY_ = 300.0f;

    bool mouseLookEnabled_ = true;
    bool tabWasDown_ = false; //anti-rebond

    float mouseSensitivity_ = 0.10f;
    float moveSpeed_ = 3.5f;

    // Variables de contrôle
    PostEffect currentEffect_ = EFFECT_NONE;
    bool useBloom_ = false;
    float bloomThreshold_ = 0.7f;
    float bloomIntensity_ = 1.0f;
    int blurIterations_ = 4;

    RenderMode currentRenderMode_ = RENDER_DEFERRED;

    // Noms des effets pour l'affichage
    std::vector<std::string> effectNames_ = {
        "Aucun",
        "Inversion",
        "Grayscale",
        "Sepia",
        "Edge Detection",
        "Flou",
        "Renforcement",
        "Emboss",
        "Sobel",
        "Gaussian Blur",
        "Bloom",
        "Vision Nocturne"
    };
    //Noms des modes de rendu
    std::vector<std::string> renderModeNames_ = {
        "Deferred Shading",
        "Shadow Mapping",
        "SSAO",
        "Debug Shadow",
        "Debug SSAO"
    };

    // État des touches
    bool keyWasDown_[512] = {false};

    // Matrices de convolution prédéfinies
    std::vector<std::vector<float> > convolutionKernels_;

    // Shaders
    GLuint modelProgram_ = 0;
    GLuint skyboxProgram_ = 0;
    GLuint postProgram_ = 0;
    GLuint bloomExtractProgram_ = 0;
    GLuint bloomBlurProgram_ = 0;
    GLuint bloomCombineProgram_ = 0;

    //Shadow Mapping
    GLuint shadowProgram_ = 0;
    GLuint shadowFBO_ = 0;
    GLuint shadowDepthTex_ = 0;
    const int SHADOW_WIDTH = 2048;
    const int SHADOW_HEIGHT = 2048;
    float lightProjection_[16] = {};
    float lightView_[16] = {};
    float lightSpaceMatrix_[16] = {};

    //SSAO
    GLuint ssaoProgram_ = 0;
    GLuint ssaoBlurProgram_ = 0;
    GLuint ssaoFBO_ = 0;
    GLuint ssaoColorBuffer_ = 0;
    GLuint ssaoBlurFBO_ = 0;
    GLuint ssaoBlurBuffer_ = 0;
    GLuint noiseTexture_ = 0;
    std::vector<core::Vec3F> ssaoKernel_;
    int ssaoKernelSize_ = 64;
    float ssaoRadius_ = 0.5f;
    float ssaoBias_ = 0.025f;
    float ssaoPower_ = 2.0f;

    // Textures et FBOs
    int screenWidth_ = 800;
    int screenHeight_ = 600;
    int width_ = 800;
    int height_ = 600;

    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint rboDepthStencil_ = 0;

    // FBOs supplémentaires pour multi-pass
    GLuint pingpongFBO_[2] = {0, 0};
    GLuint pingpongTex_[2] = {0, 0};

    // FBOs pour le bloom
    GLuint bloomFBO_[2] = {0, 0};
    GLuint bloomTex_[2] = {0, 0};

    // Géométrie
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    GLuint skyboxVAO_ = 0;
    GLuint skyboxVBO_ = 0;
    GLuint cubemapTex_ = 0;

    // CUBES
    GLuint cubeProgram_ = 0;
    GLuint cubeVAO_ = 0, cubeVBO_ = 0, cubeEBO_ = 0;
    GLsizei cubeIndexCount_ = 0;
    GLuint cubeInstanceVBO_ = 0;
    GLuint cubeDiffuseTex_ = 0;
    GLuint cubeNormalTex_ = 0;
    GLuint deferredProgram_ = 0;
    GLuint gBuffer_ = 0, gPosition_ = 0, gNormal_ = 0, gAlbedoSpec_ = 0;
    GLuint rboDepth_ = 0;
    std::vector<core::Vec3F> cubeCenters_;
    float cubeHalfSize_ = 0.5f; // AABB = center ± halfSize

    std::vector<float> cubeInstanceMatrices_; // mat4 * N
    int drawnCubes_ = 0; // debug (combien dessinés après culling)

    // Modèle
    std::unique_ptr<Model> model_;

    struct Plane {
        core::Vec3F n;
        float d{}; // distance (ax+by+cz+d=0)
        [[nodiscard]] float distance(const core::Vec3F &p) const {
            return n.x * p.x + n.y * p.y + n.z * p.z + d;
        }
    };

    struct Frustrum {
        Plane p[6]; // 0:L 1:R 2:B 3:T 4:N 5:F
    };

    struct AABB {
        core::Vec3F mn;
        core::Vec3F mx;
    };

    //Shadow Mapping
    void initShadowMapping() {
        glGenFramebuffers(1, &shadowFBO_);

        glGenTextures(1, &shadowDepthTex_);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr); // Utiliser GL_FLOAT

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);

        // IMPORTANT: Pas de buffer de couleur pour shadow map
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // Vérifier le FBO
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR: Shadow FBO not complete!" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // SHADER SHADOW CORRECT - FRAGMENT SHADER DOIT ÉCRIRE LA PROFONDEUR
        const std::string shadowVS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 uLightSpaceMatrix;
        uniform mat4 uModel;

        void main() {
            gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
        }
    )";

        const std::string shadowFS = R"(
        #version 330 core

        void main() {
            // Fragment shader vide - la profondeur est écrite automatiquement
            // gl_FragDepth est automatiquement rempli avec gl_Position.z
        }
    )";

        shadowProgram_ = createProgram(shadowVS, shadowFS);

        // DEBUG: Vérifier que le shader est créé
        if (shadowProgram_ == 0) {
            std::cerr << "ERROR: Failed to create shadow program!" << std::endl;
        } else {
            std::cout << "[INFO] Shadow Mapping initialisé ("
                    << SHADOW_WIDTH << "x" << SHADOW_HEIGHT << ")" << std::endl;
        }
    }

    void renderShadowMap() {
       // Configuration lumière - AUGMENTEZ la taille et ajustez la position
    float lightPos[3] = {15.0f, 25.0f, 15.0f}; // Plus haut, plus loin
    float lightTarget[3] = {target_[0], target_[1], target_[2]}; // Cibler le centre de la scène

    // Projection orthographique PLUS GRANDE pour couvrir toute la scène
    float size = 100.0f; // Augmentez considérablement la taille
    float near_plane = 1.0f, far_plane = 100.0f;

    // Matrice projection
    identityMatrix(lightProjection_);
    lightProjection_[0] = 2.0f / size;
    lightProjection_[5] = 2.0f / size;
    lightProjection_[10] = -2.0f / (far_plane - near_plane);
    lightProjection_[14] = -(far_plane + near_plane) / (far_plane - near_plane);

    // Matrice vue - vérifiez que la lumière regarde bien vers le bas
    lookAtMatrix(lightView_, lightPos[0], lightPos[1], lightPos[2],
                 lightTarget[0], lightTarget[1], lightTarget[2],
                 0.0f, 1.0f, 0.0f);

    // DEBUG: Afficher les matrices pour vérification
    std::cout << "Light Position: " << lightPos[0] << ", " << lightPos[1] << ", " << lightPos[2] << std::endl;
    std::cout << "Light Target: " << lightTarget[0] << ", " << lightTarget[1] << ", " << lightTarget[2] << std::endl;
    std::cout << "Ortho Size: " << size << ", Near: " << near_plane << ", Far: " << far_plane << std::endl;

    // Matrice espace lumière
    multiplyMat4(lightSpaceMatrix_, lightProjection_, lightView_);

    // Rendu shadow map
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);

    // Effacer avec 1.0 (profondeur maximale)
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // IMPORTANT: Désactiver le culling pour voir toutes les faces
    glDisable(GL_CULL_FACE);

    // Utiliser un shader shadow SIMPLIFIÉ mais qui fonctionne
    const std::string simpleShadowVS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 uLightSpaceMatrix;
        uniform mat4 uModel;

        void main() {
            gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
        }
    )";

    const std::string simpleShadowFS = R"(
        #version 330 core
        void main() {
            // Fragment shader minimal mais valide
            // gl_FragDepth sera automatiquement gl_Position.z
        }
    )";

    static GLuint simpleShadowProgram = 0;
    if (simpleShadowProgram == 0) {
        simpleShadowProgram = createProgram(simpleShadowVS, simpleShadowFS);
        std::cout << "Simple shadow program created: " << simpleShadowProgram << std::endl;
    }

    glUseProgram(simpleShadowProgram);

    // Passer la matrice
    GLuint loc = glGetUniformLocation(simpleShadowProgram, "uLightSpaceMatrix");
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, lightSpaceMatrix_);
    } else {
        std::cerr << "ERROR: uLightSpaceMatrix uniform not found!" << std::endl;
    }

    // DEBUG: Dessiner TOUS les cubes
    glBindVertexArray(cubeVAO_);

    // Cube au centre
    float modelMatrix[16];
    identityMatrix(modelMatrix);
    modelMatrix[12] = target_[0];
    modelMatrix[13] = target_[1];
    modelMatrix[14] = target_[2];

    loc = glGetUniformLocation(simpleShadowProgram, "uModel");
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, modelMatrix);
    }

    glDrawElements(GL_TRIANGLES, cubeIndexCount_, GL_UNSIGNED_INT, 0);

    // Tous les cubes de la ligne
    for (int i = 0; i < cubeCenters_.size(); ++i) {
        identityMatrix(modelMatrix);
        modelMatrix[12] = cubeCenters_[i].x;
        modelMatrix[13] = cubeCenters_[i].y;
        modelMatrix[14] = cubeCenters_[i].z;

        if (loc != -1) {
            glUniformMatrix4fv(loc, 1, GL_FALSE, modelMatrix);
        }

        glDrawElements(GL_TRIANGLES, cubeIndexCount_, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);

    // Restaurer
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "DEBUG: Shadow map rendered with " << (cubeCenters_.size() + 1) << " objects" << std::endl;
    }

    //SSAO
    void initSSAO() {
        // Générer le kernel SSAO
        std::default_random_engine generator(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);

        for (int i = 0; i < ssaoKernelSize_; ++i) {
            core::Vec3F sample(
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) // Entre 0 et 1
            );

            // Normaliser et mettre à l'échelle
            float length = std::sqrt(sample.x * sample.x + sample.y * sample.y + sample.z * sample.z);
            if (length > 0.0f) {
                sample.x /= length;
                sample.y /= length;
                sample.z /= length;
            }

            sample = sample * randomFloats(generator);

            // Échelle pour donner plus de poids aux samples proches
            float scale = float(i) / float(ssaoKernelSize_);
            scale = 0.1f + scale * scale * 0.9f;
            sample.x *= scale;
            sample.y *= scale;
            sample.z *= scale;

            ssaoKernel_.push_back(sample);
        }

        // Générer une texture de bruit
        std::vector<core::Vec3F> ssaoNoise;
        for (int i = 0; i < 16; i++) {
            core::Vec3F noise(
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                0.0f
            );
            ssaoNoise.push_back(noise);
        }

        glGenTextures(1, &noiseTexture_);
        glBindTexture(GL_TEXTURE_2D, noiseTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Créer FBO pour SSAO
        glGenFramebuffers(1, &ssaoFBO_);
        glGenFramebuffers(1, &ssaoBlurFBO_);

        // Créer buffer SSAO
        glGenTextures(1, &ssaoColorBuffer_);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer_, 0);

        // Créer buffer blur SSAO
        glGenTextures(1, &ssaoBlurBuffer_);
        glBindTexture(GL_TEXTURE_2D, ssaoBlurBuffer_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurBuffer_, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Créer les shaders SSAO - VERSION SIMPLIFIÉE
        const std::string ssaoVS = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aTexCoord;

            out vec2 TexCoord;

            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

        const std::string ssaoFS = R"(
            #version 330 core
            out float FragColor;

            in vec2 TexCoord;

            uniform sampler2D gPosition;
            uniform sampler2D gNormal;
            uniform sampler2D texNoise;

            uniform mat4 projection;

            const int kernelSize = 16;
            uniform vec3 samples[16];
            uniform float radius = 0.5;
            uniform float bias = 0.025;

            // Tile noise texture over screen
            const vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0);

            void main() {
                // Get input for SSAO algorithm
                vec3 fragPos = texture(gPosition, TexCoord).rgb;
                vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
                vec3 randomVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);

                // Create TBN matrix
                vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
                vec3 bitangent = cross(normal, tangent);
                mat3 TBN = mat3(tangent, bitangent, normal);

                // Calculate occlusion
                float occlusion = 0.0;
                for(int i = 0; i < kernelSize; ++i) {
                    // Get sample position
                    vec3 samplePos = TBN * samples[i];
                    samplePos = fragPos + samplePos * radius;

                    // Project sample position
                    vec4 offset = vec4(samplePos, 1.0);
                    offset = projection * offset;
                    offset.xyz /= offset.w;
                    offset.xyz = offset.xyz * 0.5 + 0.5;

                    // Get sample depth
                    float sampleDepth = texture(gPosition, offset.xy).z;

                    // Range check and accumulate
                    float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
                    occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
                }

                occlusion = 1.0 - (occlusion / float(kernelSize));
                FragColor = occlusion;
            }
        )";

        const std::string ssaoBlurFS = R"(
            #version 330 core
            out float FragColor;

            in vec2 TexCoord;

            uniform sampler2D ssaoInput;

            void main() {
                vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
                float result = 0.0;
                for(int x = -2; x <= 2; ++x) {
                    for(int y = -2; y <= 2; ++y) {
                        vec2 offset = vec2(float(x), float(y)) * texelSize;
                        result += texture(ssaoInput, TexCoord + offset).r;
                    }
                }
                FragColor = result / 25.0;
            }
        )";

        ssaoProgram_ = createProgram(ssaoVS, ssaoFS);
        ssaoBlurProgram_ = createProgram(ssaoVS, ssaoBlurFS);

        std::cout << "[INFO] SSAO initialisé (kernel size: " << ssaoKernelSize_ << ")" << std::endl;
    }

    void renderSSAOPass(float *proj) {
        // Pass SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(ssaoProgram_);

        // Passer les textures du G-buffer
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition_);
        glUniform1i(glGetUniformLocation(ssaoProgram_, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal_);
        glUniform1i(glGetUniformLocation(ssaoProgram_, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, noiseTexture_);
        glUniform1i(glGetUniformLocation(ssaoProgram_, "texNoise"), 2);

        // Passer le kernel SSAO (16 premiers samples)
        int samplesToUse = std::min(64, ssaoKernelSize_);
        for (int i = 0; i < samplesToUse; ++i) {
            std::string name = "samples[" + std::to_string(i) + "]";
            glUniform3f(glGetUniformLocation(ssaoProgram_, name.c_str()),
                        ssaoKernel_[i].x, ssaoKernel_[i].y, ssaoKernel_[i].z);
        }

        // Autres uniforms
        glUniformMatrix4fv(glGetUniformLocation(ssaoProgram_, "projection"),
                           1, GL_FALSE, proj);
        glUniform1f(glGetUniformLocation(ssaoProgram_, "radius"), ssaoRadius_);
        glUniform1f(glGetUniformLocation(ssaoProgram_, "bias"), ssaoBias_);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Blur SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO_);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(ssaoBlurProgram_);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
        glUniform1i(glGetUniformLocation(ssaoBlurProgram_, "ssaoInput"), 0);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Modes de Rendu
    void renderDeferred() {
        float view[16], proj[16];
        calculateCameraMatrices(view, proj);

        // Étape 1: Geometry Pass (identique)
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        renderGeometryPass(view, proj);

        // Étape 2: TEST SIMPLE - Afficher juste l'albedo
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth_, screenHeight_);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);

        // Shader simplifié pour debug
        const std::string debugVS = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            TexCoord = aTexCoord;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

        const std::string debugFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gAlbedoSpec;

        void main() {
            // Juste afficher l'albedo
            vec4 albedo = texture(gAlbedoSpec, TexCoord);

            if(albedo.a < 0.1) {
                // Fond
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
            } else {
                // Multiplier pour mieux voir
                FragColor = vec4(albedo.rgb * 2.0, 1.0);
            }
        }
    )";

        static GLuint debugProgram = 0;
        if (debugProgram == 0) {
            debugProgram = createProgram(debugVS, debugFS);
        }

        glUseProgram(debugProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
        glUniform1i(glGetUniformLocation(debugProgram, "gAlbedoSpec"), 0);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    void renderShadowMapping() {
        // 1. Créer la shadow map
        renderShadowMap();

        // 2. Rendu normal avec ombres
        float view[16], proj[16];
        calculateCameraMatrices(view, proj);

        // Geometry pass (RENDRE les géométries dans le G-buffer)
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        renderGeometryPass(view, proj); // CE N'ÉTAIT PAS APPELÉ !

        // Lighting pass avec ombres
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth_, screenHeight_);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // Shader shadow simplifié - CORRIGÉ
        const std::string shadowSimpleFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D gAlbedoSpec;
        uniform sampler2D shadowMap;
        uniform mat4 lightSpaceMatrix;
        uniform vec3 viewPos;
        uniform vec3 lightPos;

        float ShadowCalculation(vec4 fragPosLightSpace) {
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;

            if(projCoords.z > 1.0) return 0.0;

            float closestDepth = texture(shadowMap, projCoords.xy).r;
            float currentDepth = projCoords.z;

            float bias = 0.005;
            float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

            return shadow;
        }

        void main() {
            vec3 fragPos = texture(gPosition, TexCoord).rgb;
            vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
            vec3 albedo = texture(gAlbedoSpec, TexCoord).rgb;
            float specular = texture(gAlbedoSpec, TexCoord).a;

            // Vérifier si c'est un pixel vide
            if(length(fragPos) < 0.01) {
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
                return;
            }

            // Lumière directionnelle
            vec3 lightDir = normalize(lightPos - fragPos);
            vec3 lightColor = vec3(1.0, 0.95, 0.9);

            // Ambient
            vec3 ambient = albedo * 0.1;

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * albedo * lightColor;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - fragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
            vec3 specularLight = specular * spec * lightColor;

            // Calcul ombre
            vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
            float shadow = ShadowCalculation(fragPosLightSpace);

            // Résultat final avec ombres
            vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specularLight);
            FragColor = vec4(lighting, 1.0);
        }
    )";

        static GLuint shadowSimpleProgram = 0;
        if (shadowSimpleProgram == 0) {
            const std::string vs = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";
            shadowSimpleProgram = createProgram(vs, shadowSimpleFS);
            std::cout << "Shadow simple program créé: " << shadowSimpleProgram << std::endl;
        }

        glUseProgram(shadowSimpleProgram);

        // Textures du G-buffer
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition_);
        glUniform1i(glGetUniformLocation(shadowSimpleProgram, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal_);
        glUniform1i(glGetUniformLocation(shadowSimpleProgram, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
        glUniform1i(glGetUniformLocation(shadowSimpleProgram, "gAlbedoSpec"), 2);

        // Shadow map
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
        glUniform1i(glGetUniformLocation(shadowSimpleProgram, "shadowMap"), 3);

        // Uniforms
        glUniform3f(glGetUniformLocation(shadowSimpleProgram, "viewPos"),
                    camPos_[0], camPos_[1], camPos_[2]);

        glUniformMatrix4fv(glGetUniformLocation(shadowSimpleProgram, "lightSpaceMatrix"),
                           1, GL_FALSE, lightSpaceMatrix_);

        float lightPos[3] = {10.0f, 20.0f, 10.0f};
        glUniform3f(glGetUniformLocation(shadowSimpleProgram, "lightPos"),
                    lightPos[0], lightPos[1], lightPos[2]);

        // Rendu du quad plein écran
        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    void renderSSAO() {
        float view[16], proj[16];
        calculateCameraMatrices(view, proj);

        // 1. Geometry pass
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        renderGeometryPass(view, proj);

        // 2. SSAO pass - DÉCOMMENTER cette ligne !
        renderSSAOPass(proj);

        // 3. Lighting pass avec SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth_, screenHeight_);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // Shader SSAO avec occlusion réelle
        const std::string ssaoFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D gAlbedoSpec;
        uniform sampler2D ssaoBuffer;
        uniform vec3 viewPos;

        void main() {
            vec3 fragPos = texture(gPosition, TexCoord).rgb;
            vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
            vec3 albedo = texture(gAlbedoSpec, TexCoord).rgb;
            float specular = texture(gAlbedoSpec, TexCoord).a;
            float ao = texture(ssaoBuffer, TexCoord).r;

            if(length(fragPos) < 0.01) {
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
                return;
            }

            // Lumière directionnelle
            vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
            vec3 lightColor = vec3(1.0, 0.95, 0.9);

            // Ambient avec occlusion SSAO
            vec3 ambient = albedo * 0.1 * ao;

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * albedo * lightColor;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - fragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
            vec3 specularLight = specular * spec * lightColor;

            vec3 lighting = ambient + diffuse + specularLight;
            FragColor = vec4(lighting, 1.0);
        }
    )";

        static GLuint ssaoProgram = 0;
        if (ssaoProgram == 0) {
            const std::string vs = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";
            ssaoProgram = createProgram(vs, ssaoFS);
            std::cout << "SSAO program créé: " << ssaoProgram << std::endl;
        }

        glUseProgram(ssaoProgram);

        // Textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition_);
        glUniform1i(glGetUniformLocation(ssaoProgram, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal_);
        glUniform1i(glGetUniformLocation(ssaoProgram, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
        glUniform1i(glGetUniformLocation(ssaoProgram, "gAlbedoSpec"), 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, ssaoBlurBuffer_); // Utiliser le buffer SSAO blurré
        glUniform1i(glGetUniformLocation(ssaoProgram, "ssaoBuffer"), 3);

        // View position
        glUniform3f(glGetUniformLocation(ssaoProgram, "viewPos"),
                    camPos_[0], camPos_[1], camPos_[2]);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    void renderDebugShadow() {
        // 1. Vérifier d'abord le FBO
    checkShadowFBO();

    // 2. Rendre la shadow map AVEC DEBUG VISUEL
    std::cout << "=== DEBUG SHADOW MAP ===" << std::endl;
    renderShadowMap();

    // 3. Lire et analyser les données
    std::vector<float> depthData(SHADOW_WIDTH * SHADOW_HEIGHT);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depthData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Analyse plus détaillée
    int validPixels = 0;
    float minDepth = 1.0f, maxDepth = 0.0f;
    float sumDepth = 0.0f;

    for (int i = 0; i < SHADOW_WIDTH * SHADOW_HEIGHT; i++) {
        if (depthData[i] < 1.0f) {
            validPixels++;
            if (depthData[i] < minDepth) minDepth = depthData[i];
            if (depthData[i] > maxDepth) maxDepth = depthData[i];
            sumDepth += depthData[i];
        }
    }

    std::cout << "DEBUG Shadow Map Analysis:" << std::endl;
    std::cout << "  Valid pixels (depth < 1.0): " << validPixels << "/"
              << (SHADOW_WIDTH * SHADOW_HEIGHT) << std::endl;
    std::cout << "  Min depth: " << minDepth << std::endl;
    std::cout << "  Max depth: " << maxDepth << std::endl;
    std::cout << "  Avg depth (valid): " << (validPixels > 0 ? sumDepth/validPixels : 0) << std::endl;

    // 4. Afficher la texture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // Shader debug amélioré
    const std::string debugFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D depthMap;

        void main() {
            float depth = texture(depthMap, TexCoord).r;

            // Afficher en heatmap pour mieux voir
            vec3 color;

            if (depth > 0.999) {
                // Profondeur maximale = noir (pas d'objet)
                color = vec3(0.0, 0.0, 0.0);
            } else if (depth < 0.3) {
                // Très proche = bleu
                color = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), depth/0.3);
            } else if (depth < 0.6) {
                // Moyen = cyan -> jaune
                color = mix(vec3(0.0, 1.0, 1.0), vec3(1.0, 1.0, 0.0), (depth-0.3)/0.3);
            } else {
                // Loin = jaune -> rouge
                color = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (depth-0.6)/0.4);
            }

            // Augmenter le contraste
            color = pow(color, vec3(0.5));
            FragColor = vec4(color, 1.0);
        }
    )";

    static GLuint debugProgram = 0;
    if (debugProgram == 0) {
        const std::string vs = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";
        debugProgram = createProgram(vs, debugFS);
        std::cout << "Debug shadow program created: " << debugProgram << std::endl;
    }

    glUseProgram(debugProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);

    // Important: Définir les paramètres de texture pour l'affichage
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUniform1i(glGetUniformLocation(debugProgram, "depthMap"), 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    }

    void renderDebugSSAO() {
        float view[16], proj[16];
        calculateCameraMatrices(view, proj);

        // Geometry Pass
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        renderGeometryPass(view, proj);

        // SSAO Pass
        renderSSAOPass(proj);

        // Afficher le buffer SSAO
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth_, screenHeight_);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);

        const std::string debugFS = R"(
            #version 330 core
            out vec4 FragColor;
            in vec2 TexCoord;
            uniform sampler2D ssaoBuffer;

            void main() {
                float ao = texture(ssaoBuffer, TexCoord).r;
                FragColor = vec4(vec3(ao), 1.0);
            }
        )";

        static GLuint debugProgram = 0;
        if (debugProgram == 0) {
            const std::string vs = R"(
                #version 330 core
                layout(location=0) in vec2 aPos;
                layout(location=1) in vec2 aTexCoord;
                out vec2 TexCoord;
                void main() {
                    TexCoord = aTexCoord;
                    gl_Position = vec4(aPos, 0.0, 1.0);
                }
            )";
            debugProgram = createProgram(vs, debugFS);
        }

        glUseProgram(debugProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoBlurBuffer_);
        glUniform1i(glGetUniformLocation(debugProgram, "ssaoBuffer"), 0);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void calculateCameraMatrices(float *view, float *proj) {
        float camX = target_[0] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::cos(yaw_ * M_PI / 180);
        float camY = target_[1] + orbitRadius_ * std::sin(pitch_ * M_PI / 180);
        float camZ = target_[2] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::sin(yaw_ * M_PI / 180);

        camPos_[0] = camX;
        camPos_[1] = camY;
        camPos_[2] = camZ;

        lookAtMatrix(view, camX, camY, camZ, target_[0], target_[1], target_[2], 0, 1, 0);
        perspectiveMatrix(proj, 60.0f, (float) width_ / height_, 0.1f, 500.0f);
    }

    void renderGeometryPass(float *view, float *proj) {
        glUseProgram(cubeProgram_);

        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_, "uProj"), 1, GL_FALSE, proj);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeDiffuseTex_);
        glUniform1i(glGetUniformLocation(cubeProgram_, "uDiffuse"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cubeNormalTex_);
        glUniform1i(glGetUniformLocation(cubeProgram_, "uNormalMap"), 1);

        glUniform1i(glGetUniformLocation(cubeProgram_, "useNormal"), 1);

        // Mise à jour des instances
        cubeInstanceMatrices_.clear();
        for (const auto &c: cubeCenters_) {
            float m[16];
            identityMatrix(m);
            m[12] = c.x;
            m[13] = c.y;
            m[14] = c.z;
            cubeInstanceMatrices_.insert(cubeInstanceMatrices_.end(), m, m + 16);
        }

        glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        cubeInstanceMatrices_.size() * sizeof(float),
                        cubeInstanceMatrices_.data());

        glBindVertexArray(cubeVAO_);
        glDrawElementsInstanced(GL_TRIANGLES, cubeIndexCount_, GL_UNSIGNED_INT, 0,
                                (GLsizei) (cubeInstanceMatrices_.size() / 16));
        glBindVertexArray(0);
    }

    void checkShadowFBO() const {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

        switch(status) {
            case GL_FRAMEBUFFER_COMPLETE:
                std::cout << "Shadow FBO: Complete" << std::endl;
                break;
            case GL_FRAMEBUFFER_UNDEFINED:
                std::cerr << "Shadow FBO: Undefined" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                std::cerr << "Shadow FBO: Incomplete attachment" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                std::cerr << "Shadow FBO: Missing attachment" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                std::cerr << "Shadow FBO: Incomplete draw buffer" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                std::cerr << "Shadow FBO: Incomplete read buffer" << std::endl;
                break;
            case GL_FRAMEBUFFER_UNSUPPORTED:
                std::cerr << "Shadow FBO: Unsupported format" << std::endl;
                break;
            case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                std::cerr << "Shadow FBO: Incomplete multisample" << std::endl;
                break;
            default:
                std::cerr << "Shadow FBO: Unknown error (" << status << ")" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    static void normalizePlane(Plane &pl) {
        float len = std::sqrt(pl.n.x * pl.n.x + pl.n.y * pl.n.y + pl.n.z * pl.n.z);
        if (len > 0.0f) {
            pl.n.x /= len;
            pl.n.y /= len;
            pl.n.z /= len;
            pl.d /= len;
        }
    }

    static float mrc(const float *m, int row, int col) { return m[col * 4 + row]; }

    static Frustrum exractFrustrum(const float *proj, const float *view) {
        float vp[16];
        multiplyMat4(vp, proj, view);

        Frustrum fr;
        auto makePlane = [&](int signRow, int axisRow)-> Plane {
            Plane pl;
            pl.n.x = mrc(vp, 3, 0) + signRow * mrc(vp, axisRow, 0);
            pl.n.y = mrc(vp, 3, 1) + signRow * mrc(vp, axisRow, 1);
            pl.n.z = mrc(vp, 3, 2) + signRow * mrc(vp, axisRow, 2);
            pl.d = mrc(vp, 3, 3) + signRow * mrc(vp, axisRow, 3);
            normalizePlane(pl);
            return pl;
        };

        fr.p[0] = makePlane(+1, 0); // Left  = row3 + row0
        fr.p[1] = makePlane(-1, 0); // Right = row3 - row0
        fr.p[2] = makePlane(+1, 1); // Bottom
        fr.p[3] = makePlane(-1, 1); // Top
        fr.p[4] = makePlane(+1, 2); // Near
        fr.p[5] = makePlane(-1, 2); // Far
        return fr;
    }

    // Test AABB vs frustum
    static bool aabbInFrustum(const Frustrum &f, const AABB &b) {
        for (int i = 0; i < 6; ++i) {
            const Plane &p = f.p[i];

            core::Vec3F v;
            v.x = (p.n.x >= 0.0f) ? b.mx.x : b.mn.x;
            v.y = (p.n.y >= 0.0f) ? b.mx.y : b.mn.y;
            v.z = (p.n.z >= 0.0f) ? b.mx.z : b.mn.z;

            if (p.distance(v) < 0.0f) return false;
        }
        return true;
    }

    void initCubeResources() {
        const std::string vs = R"(
    #version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;

// instancing
layout(location=4) in vec4 iM0;
layout(location=5) in vec4 iM1;
layout(location=6) in vec4 iM2;
layout(location=7) in vec4 iM3;

uniform mat4 uView;
uniform mat4 uProj;

out vec2 vUV;
out vec3 vPosWS;
out mat3 vTBN;
out vec3 Normal;

void main()
{
    mat4 M = mat4(iM0, iM1, iM2, iM3);
    mat3 normalMatrices= mat3(transpose(inverse(M)));
    Normal=normalize(normalMatrices * aNormal);
    vec3 N = normalize(normalMatrices * aNormal);
    vec3 T = normalize(normalMatrices * aTangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    vTBN = mat3(T, B, N);
    vUV = aUV;
    vec4 worldPos = M * vec4(aPos, 1.0);
    vPosWS = worldPos.xyz;

    gl_Position = uProj * uView * worldPos;
}
)";

        const std::string fs = R"(
#version 330 core
layout(location=0) out vec4 gPosition;
layout(location=1) out vec4 gNormal;
layout(location=2) out vec4 gAlbedoSpec;


in vec2 vUV;
in vec3 vPosWS;
in mat3 vTBN;
in vec3 Normal;

uniform sampler2D uDiffuse;
uniform sampler2D uNormalMap;
uniform bool useNormal;

void main(){

    gPosition=vec4(vPosWS, 1.0);

    // tangent -> world
    vec3 N = normalize(Normal);

    if(useNormal){
        // normal map (tangent space)
        vec3 nTS = texture(uNormalMap, vUV).rgb;
        nTS = normalize(nTS * 2.0 - 1.0); // [0,1] -> [-1,1]
        N=normalize(vTBN * nTS);
    }
    gNormal=vec4(N*0.5+0.5, 1.0);

    vec4 diffuseTex = texture(uDiffuse, vUV);
    vec3 albedo = diffuseTex.rgb;
    float specular = 0.6; // Force spéculaire constante pour test

    if(length(albedo) < 0.01) {
        albedo = vec3(0.8, 0.6, 0.4); // Couleur de brique par défaut
    }

    gAlbedoSpec = vec4(albedo, specular);
}
)";
        const std::string drv = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aTexCoord;
        out vec2 TexCoord;
void main(){
    TexCoord= aTexCoord;
    gl_Position= vec4(aPos.x, aPos.y, 0.0, 1.0);
}

)";
        const std::string drf = R"(#version 330 core
layout(location=0) out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 viewPos;
uniform float specularPow;

struct PointLight{
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};
uniform int pointLightCount;
uniform PointLight pointLights[1];

uniform int debugMode;

void main(){
    // Mode debug: 0=normal, 1=positions, 2=normales, 3=albedo, 4=UV
    if(debugMode == 1) {
        // Afficher les positions (normalisées pour la visualisation)
        vec3 pos = texture(gPosition, TexCoord).rgb;
        FragColor = vec4(normalize(pos) * 0.5 + 0.5, 1.0);
        return;
    }
    else if(debugMode == 2) {
        // Afficher les normales (déjà en [0,1])
        FragColor = texture(gNormal, TexCoord);
        return;
    }
    else if(debugMode == 3) {
        // Afficher l'albedo (couleur de la texture)
        FragColor = texture(gAlbedoSpec, TexCoord);
        return;
    }
    else if(debugMode == 4) {
        // Afficher les coordonnées UV
        FragColor = vec4(TexCoord, 0.0, 1.0);
        return;
    }

    // Lire les données du G-buffer
    vec4 positionData = texture(gPosition, TexCoord);
    vec4 normalData = texture(gNormal, TexCoord);
    vec4 albedoSpecData = texture(gAlbedoSpec, TexCoord);

    // DEBUG: Visualiser les buffers individuellement
    //FragColor = positionData; // Visualiser les positions
    //FragColor = normalData;   // Visualiser les normales
    //FragColor = albedoSpecData; // Visualiser l'albedo
    //return;

    // Vérifier si c'est un pixel valide
    if(positionData.a < 0.1) {
        // C'est le fond
        FragColor = vec4(0.1, 0.1, 0.15, 1.0);
        return;
    }

    vec3 fragPos = positionData.rgb;
    vec3 normal = normalize(normalData.rgb * 2.0 - 1.0); // [0,1] -> [-1,1]
    vec3 albedo = albedoSpecData.rgb;
    float specularStrength = albedoSpecData.a;

    // DEBUG: Vérifier que l'albedo n'est pas noir
     if(length(albedo) < 0.01) {
         FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Rouge si problème
         return;
     }

    // Éclairage
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 lighting = albedo * 0.1; // Ambient

    for(int i = 0; i < pointLightCount; i++){
        vec3 lightDir = normalize(pointLights[i].position - fragPos);

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * albedo * pointLights[i].color;

        // Specular (Blinn-Phong pour de meilleurs résultats)
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPow);
        vec3 specular = specularStrength * spec * pointLights[i].color;

        // Attenuation
        float distance = length(pointLights[i].position - fragPos);
        float attenuation = 1.0 / (pointLights[i].constant +
                          pointLights[i].linear * distance +
                          pointLights[i].quadratic * (distance * distance));

        lighting += (diffuse + specular) * attenuation;
    }

    FragColor = vec4(lighting, 1.0);
})";

        cubeProgram_ = createProgram(vs, fs);
        deferredProgram_ = createProgram(drv, drf);
        static constexpr float vertices[] = {
            // +X
            +0.5f, -0.5f, -0.5f, 1, 0, 0, 0, 0, 0, 0, -1,
            +0.5f, -0.5f, +0.5f, 1, 0, 0, 1, 0, 0, 0, -1,
            +0.5f, +0.5f, +0.5f, 1, 0, 0, 1, 1, 0, 0, -1,
            +0.5f, +0.5f, -0.5f, 1, 0, 0, 0, 1, 0, 0, -1,

            // -X
            -0.5f, -0.5f, +0.5f, -1, 0, 0, 0, 0, 0, 0, 1,
            -0.5f, -0.5f, -0.5f, -1, 0, 0, 1, 0, 0, 0, 1,
            -0.5f, +0.5f, -0.5f, -1, 0, 0, 1, 1, 0, 0, 1,
            -0.5f, +0.5f, +0.5f, -1, 0, 0, 0, 1, 0, 0, 1,

            // +Y
            -0.5f, +0.5f, -0.5f, 0, 1, 0, 0, 0, 1, 0, 0,
            +0.5f, +0.5f, -0.5f, 0, 1, 0, 1, 0, 1, 0, 0,
            +0.5f, +0.5f, +0.5f, 0, 1, 0, 1, 1, 1, 0, 0,
            -0.5f, +0.5f, +0.5f, 0, 1, 0, 0, 1, 1, 0, 0,

            // -Y
            -0.5f, -0.5f, +0.5f, 0, -1, 0, 0, 0, 1, 0, 0,
            +0.5f, -0.5f, +0.5f, 0, -1, 0, 1, 0, 1, 0, 0,
            +0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 1, 1, 0, 0,
            -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1, 1, 0, 0,

            // +Z
            -0.5f, -0.5f, +0.5f, 0, 0, 1, 0, 0, 1, 0, 0,
            +0.5f, -0.5f, +0.5f, 0, 0, 1, 1, 0, 1, 0, 0,
            +0.5f, +0.5f, +0.5f, 0, 0, 1, 1, 1, 1, 0, 0,
            -0.5f, +0.5f, +0.5f, 0, 0, 1, 0, 1, 1, 0, 0,

            // -Z
            +0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0, -1, 0, 0,
            -0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0, -1, 0, 0,
            -0.5f, +0.5f, -0.5f, 0, 0, -1, 1, 1, -1, 0, 0,
            +0.5f, +0.5f, -0.5f, 0, 0, -1, 0, 1, -1, 0, 0,
        };

        static constexpr unsigned int indices[] = {
            // +X (flip)
            0, 2, 1, 0, 3, 2,

            // -X (flip)
            4, 6, 5, 4, 7, 6,

            // +Y (flip)
            8, 10, 9, 8, 11, 10,

            // -Y (flip)
            12, 14, 13, 12, 15, 14,

            // +Z (OK)
            16, 17, 18, 18, 19, 16,

            // -Z (OK)
            20, 21, 22, 22, 23, 20
        };
        cubeIndexCount_ = (GLsizei) (sizeof(indices) / sizeof(indices[0]));

        glGenVertexArrays(1, &cubeVAO_);
        glGenBuffers(1, &cubeVBO_);
        glGenBuffers(1, &cubeEBO_);

        glBindVertexArray(cubeVAO_);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // glEnableVertexAttribArray(0);
        // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
        //
        // glBindVertexArray(0);
        constexpr GLsizei stride = 11 * sizeof(float);

        // pos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *) 0);
        // normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *) (3 * sizeof(float)));
        // uv
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *) (6 * sizeof(float)));
        // tangent
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *) (8 * sizeof(float)));
    }

    void initCubeInstancing() {
        glBindVertexArray(cubeVAO_);

        glGenBuffers(1, &cubeInstanceVBO_);
        glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO_);
        glBufferData(GL_ARRAY_BUFFER, cubeCenters_.size() * 16 * sizeof(float), nullptr,GL_DYNAMIC_DRAW);
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE,
                                  16 * sizeof(float),
                                  (void *) (i * 4 * sizeof(float)));
            glVertexAttribDivisor(4 + i, 1);
        }
        glBindVertexArray(0);
    }

    void createCubeLine() {
        cubeCenters_.clear();

        // Positionnez les cubes autour du centre de la scène
        for (int i = -20; i <= 20; ++i) {
            cubeCenters_.push_back(core::Vec3F{
                target_[0] + i * 3.0f,  // Plus espacés
                target_[1] + 0.5f,      // Un peu au-dessus du sol
                target_[2] - 10.0f      // Devant la caméra
            });
        }

        std::cout << "Created " << cubeCenters_.size() << " cubes" << std::endl;
    }

    static GLuint loadTexture2D(const std::string &path, bool flipY) {
        stbi_set_flip_vertically_on_load(flipY);

        int w, h, comp;
        unsigned char *data = stbi_load(path.c_str(), &w, &h, &comp, 0);
        if (!data) {
            std::cerr << "[Texture] FAILED: " << path
                    << " reason: " << stbi_failure_reason() << "\n";
            return 0;
        }

        GLenum format = GL_RGB;
        if (comp == 1) format = GL_RED;
        else if (comp == 3) format = GL_RGB;
        else if (comp == 4) format = GL_RGBA;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        return tex;
    }

    static void identityMatrix(float *m) {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static void normalize(float v[3]) {
        float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (len > 0.00001f) {
            v[0] /= len;
            v[1] /= len;
            v[2] /= len;
        }
    }

    static void cross(float out[3], const float a[3], const float b[3]) {
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
    }

    void SetMouseLook(bool enabled) {
        mouseLookEnabled_ = enabled;
        SDL_Window *window = common::GetWindow();

        SDL_SetWindowRelativeMouseMode(window, enabled);
        float dx, dy;
        SDL_GetRelativeMouseState(&dx, &dy);
        firstMouse_ = true;
    }

    // Initialisation des matrices de convolution
    void initConvolutionKernels() {
        convolutionKernels_.resize(EFFECT_COUNT);

        // Aucun effet (identité)
        convolutionKernels_[EFFECT_NONE] = {
            0, 0, 0,
            0, 1, 0,
            0, 0, 0
        };
        // Edge Detection
        convolutionKernels_[EFFECT_EDGE_DETECTION] = {
            -1, -1, -1,
            -1, 8, -1,
            -1, -1, -1
        };

        // Flou
        convolutionKernels_[EFFECT_BLUR] = {
            1 / 16.0f, 2 / 16.0f, 1 / 16.0f,
            2 / 16.0f, 4 / 16.0f, 2 / 16.0f,
            1 / 16.0f, 2 / 16.0f, 1 / 16.0f
        };

        // Renforcement
        convolutionKernels_[EFFECT_SHARPEN] = {
            0, -1, 0,
            -1, 5, -1,
            0, -1, 0
        };

        // Emboss
        convolutionKernels_[EFFECT_EMBOSS] = {
            -2, -1, 0,
            -1, 1, 1,
            0, 1, 2
        };

        // Sobel (horizontal)
        convolutionKernels_[EFFECT_SOBEL] = {
            -1, 0, 1,
            -2, 0, 2,
            -1, 0, 1
        };

        // Gaussian Blur 5x5 (simplifié)
        convolutionKernels_[EFFECT_GAUSSIAN_BLUR] = {
            1 / 256.0f, 4 / 256.0f, 6 / 256.0f, 4 / 256.0f, 1 / 256.0f,
            4 / 256.0f, 16 / 256.0f, 24 / 256.0f, 16 / 256.0f, 4 / 256.0f,
            6 / 256.0f, 24 / 256.0f, 36 / 256.0f, 24 / 256.0f, 6 / 256.0f,
            4 / 256.0f, 16 / 256.0f, 24 / 256.0f, 16 / 256.0f, 4 / 256.0f,
            1 / 256.0f, 4 / 256.0f, 6 / 256.0f, 4 / 256.0f, 1 / 256.0f
        };
    }

    void createGBuffer(int width, int height) {
        glGenFramebuffers(1, &gBuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);

        glGenTextures(1, &gPosition_);
        glBindTexture(GL_TEXTURE_2D, gPosition_);
        glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition_, 0);

        // 2. Normales : Format GL_RGBA16F
        glGenTextures(1, &gNormal_);
        glBindTexture(GL_TEXTURE_2D, gNormal_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal_, 0);

        // 3. Albedo + Spéculaire
        glGenTextures(1, &gAlbedoSpec_);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec_, 0);

        // Depth buffer
        GLuint rboDepth = 0;
        glGenRenderbuffers(1, &rboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
        // ────────────────────────────────────────────────

        GLuint attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, attachments);

        // GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        // if (status != GL_FRAMEBUFFER_COMPLETE) {
        //     std::cerr << "G-Buffer incomplet ! Status = 0x" << std::hex << status << std::endl;
        //     // → ajoute un log ici pour debugger
        // }

        // Vérification
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "G-Buffer incomplet ! Status = 0x" << std::hex
                    << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
        } else {
            std::cout << "[INFO] G-Buffer created successfully ("
                    << width << "x" << height << ")" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Création du framebuffer principal
    void createFramebuffer() {
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        // Texture couleur
        glGenTextures(1, &colorTex_);
        glBindTexture(GL_TEXTURE_2D, colorTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

        // Renderbuffer pour depth-stencil
        glGenRenderbuffers(1, &rboDepthStencil_);
        glBindRenderbuffer(GL_RENDERBUFFER, rboDepthStencil_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepthStencil_);

        // Vérification
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[FBO] Framebuffer incomplet!" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Création des framebuffers pour multi-pass
    void createMultiPassFramebuffers() {
        for (int i = 0; i < 2; i++) {
            glGenFramebuffers(1, &pingpongFBO_[i]);
            glGenTextures(1, &pingpongTex_[i]);

            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO_[i]);
            glBindTexture(GL_TEXTURE_2D, pingpongTex_[i]);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongTex_[i], 0);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "[PingPong FBO " << i << "] incomplet!" << std::endl;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Création des framebuffers pour le bloom
    void createBloomFramebuffers() {
        for (int i = 0; i < 2; i++) {
            glGenFramebuffers(1, &bloomFBO_[i]);
            glGenTextures(1, &bloomTex_[i]);

            glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO_[i]);
            glBindTexture(GL_TEXTURE_2D, bloomTex_[i]);

            // Textures plus petites pour le bloom (downsampling)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_ / 2, height_ / 2, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex_[i], 0);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "[Bloom FBO " << i << "] incomplet!" << std::endl;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Création du quad écran pour post-processing
    void createScreenQuad() {
        const float quad[] = {
            // x, y, u, v
            -1, -1, 0, 0,
            1, -1, 1, 0,
            1, 1, 1, 1,

            -1, -1, 0, 0,
            1, 1, 1, 1,
            -1, 1, 0, 1
        };

        glGenVertexArrays(1, &quadVAO_);
        glGenBuffers(1, &quadVBO_);
        glBindVertexArray(quadVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), &quad, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

        glBindVertexArray(0);
    }

    // Initialisation des shaders de post-processing
    void initPostProcessingShaders() {
        const char *postVertexShader = R"(
            #version 330 core
            precision mediump float;

            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aUV;

            out vec2 vUV;

            void main() {
                vUV = aUV;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

        const char *postFragmentShader = R"(
    #version 330 core
    precision mediump float;

    in vec2 vUV;
    out vec4 FragColor;

    uniform sampler2D uScene;
    uniform int uEffect;
    uniform vec2 uTexelSize;
    uniform float uTime;
    uniform float uBloomThreshold;
    uniform float uBloomIntensity;

    // Fonction pour convertir en grayscale
    float luminance(vec3 color) {
        return dot(color, vec3(0.299, 0.587, 0.114));
    }

    // Fonction de convolution 3x3
    vec3 convolution3x3(sampler2D tex, vec2 uv, vec2 texelSize) {
        vec3 sum = vec3(0.0);

        // Kernel d'edge detection par défaut
        float kernel[9] = float[](
            -1.0, -1.0, -1.0,
            -1.0,  8.0, -1.0,
            -1.0, -1.0, -1.0
        );

        if (uEffect == 5) { // Flou
            kernel = float[](
                1.0/16.0, 2.0/16.0, 1.0/16.0,
                2.0/16.0, 4.0/16.0, 2.0/16.0,
                1.0/16.0, 2.0/16.0, 1.0/16.0
            );
        } else if (uEffect == 6) { // Renforcement
            kernel = float[](
                0.0, -1.0, 0.0,
                -1.0, 5.0, -1.0,
                0.0, -1.0, 0.0
            );
        }

        int index = 0;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 color = texture(tex, uv + offset).rgb;
                sum += color * kernel[index];
                index++;
            }
        }
        return sum;
    }

    void main() {
        vec3 color = texture(uScene, vUV).rgb;
        vec3 result = color;

        if (uEffect == 1) { // Inversion
            result = vec3(1.0) - color;
        }
        else if (uEffect == 2) { // Grayscale
            float gray = luminance(color);
            result = vec3(gray);
        }
        else if (uEffect == 3) { // Sepia
            float gray = luminance(color);
            result = vec3(gray * vec3(1.2, 1.0, 0.8));
        }
        else if (uEffect == 4) { // Edge Detection
            result = convolution3x3(uScene, vUV, uTexelSize);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 5) { // Flou
            result = convolution3x3(uScene, vUV, uTexelSize);
        }
        else if (uEffect == 6) { // Renforcement
            result = convolution3x3(uScene, vUV, uTexelSize);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 7) { // Emboss
            // Kernel emboss simplifié
            result = vec3(0.5) + (color - texture(uScene, vUV - vec2(1.0, 1.0) * uTexelSize).rgb);
        }
        else if (uEffect == 8) { // Sobel simplifié
            vec3 gx = vec3(0.0);
            vec3 gy = vec3(0.0);

            float sobelX[9] = float[](
                -1.0, 0.0, 1.0,
                -2.0, 0.0, 2.0,
                -1.0, 0.0, 1.0
            );

            float sobelY[9] = float[](
                -1.0, -2.0, -1.0,
                0.0,  0.0,  0.0,
                1.0,  2.0,  1.0
            );

            int index = 0;
            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 offset = vec2(float(x), float(y)) * uTexelSize;
                    vec3 texColor = texture(uScene, vUV + offset).rgb;
                    gx += texColor * sobelX[index];
                    gy += texColor * sobelY[index];
                    index++;
                }
            }

            result = sqrt(gx * gx + gy * gy);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 9) { // Gaussian Blur simplifié
            vec3 sum = vec3(0.0);
            float weight = 1.0 / 9.0;

            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 offset = vec2(float(x), float(y)) * uTexelSize;
                    sum += texture(uScene, vUV + offset).rgb * weight;
                }
            }
            result = sum;
        }
        else if (uEffect == 11) { // Vision Nocturne
            float gray = luminance(color);
            result = vec3(0.1, 0.8, 0.1) * gray;

            // Ajouter du bruit
            float noise = fract(sin(dot(vUV, vec2(12.9898, 78.233))) * 43758.5453);
            result += noise * 0.05;

            // Ajouter des scanlines
            if (mod(gl_FragCoord.y, 3.0) < 1.0) {
                result *= 0.9;
            }
        }

        FragColor = vec4(result, 1.0);
    }
)";

        postProgram_ = createProgram(postVertexShader, postFragmentShader);
    }

    // Initialisation des shaders pour le bloom
    void initBloomShaders() {
        // Shader d'extraction des zones lumineuses
        const char *bloomExtractFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uScene;
        uniform float uThreshold;

        float luminance(vec3 color) {
            return dot(color, vec3(0.299, 0.587, 0.114));
        }

        void main() {
            vec3 color = texture(uScene, vUV).rgb;
            float brightness = luminance(color);

            if (brightness > uThreshold) {
                FragColor = vec4(color, 1.0);
            } else {
                FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }
    )";

        const char *bloomVertexShader = R"(
        #version 330 core
        precision mediump float;

        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;

        out vec2 vUV;

        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

        // Shader de flou gaussien - CORRIGÉ
        const char *bloomBlurFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uImage;
        uniform bool uHorizontal;
        uniform vec2 uTexelSize;

        void main() {
            vec2 texelSize = uTexelSize;
            vec3 result = vec3(0.0);

            // Noyau gaussien 9x1
            float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

            result = texture(uImage, vUV).rgb * weight[0];

            if (uHorizontal) {
                for (int i = 1; i < 5; i++) {
                    result += texture(uImage, vUV + vec2(texelSize.x * float(i), 0.0)).rgb * weight[i];
                    result += texture(uImage, vUV - vec2(texelSize.x * float(i), 0.0)).rgb * weight[i];
                }
            } else {
                for (int i = 1; i < 5; i++) {
                    result += texture(uImage, vUV + vec2(0.0, texelSize.y * float(i))).rgb * weight[i];
                    result += texture(uImage, vUV - vec2(0.0, texelSize.y * float(i))).rgb * weight[i];
                }
            }

            FragColor = vec4(result, 1.0);
        }
    )";

        // Shader de combinaison bloom - SIMPLIFIÉ POUR ÉVITER LES ERREURS
        const char *bloomCombineFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uScene;
        uniform sampler2D uBloomBlur;
        uniform float uBloomIntensity;

        void main() {
            vec3 scene = texture(uScene, vUV).rgb;
            vec3 bloom = texture(uBloomBlur, vUV).rgb;

            // Combinaison additive
            vec3 result = scene + bloom * uBloomIntensity;

            FragColor = vec4(result, 1.0);
        }
    )";

        bloomExtractProgram_ = createProgram(bloomVertexShader, bloomExtractFS);
        bloomBlurProgram_ = createProgram(bloomVertexShader, bloomBlurFS);
        bloomCombineProgram_ = createProgram(bloomVertexShader, bloomCombineFS);
    }

    GLfloat lightX = 0;
    GLfloat lightY = 0;
    GLfloat lightZ = 0;
    // Rendu de la scène dans le framebuffer
    void renderSceneToFramebuffer() {
        // Calcul caméra (comme avant)
        float view[16], proj[16];
        float camX = target_[0] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::cos(yaw_ * M_PI / 180);
        float camY = target_[1] + orbitRadius_ * std::sin(pitch_ * M_PI / 180);
        float camZ = target_[2] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::sin(yaw_ * M_PI / 180);

        lookAtMatrix(view, camX, camY, camZ, target_[0], target_[1], target_[2], 0, 1, 0);
        perspectiveMatrix(proj, 60.0f, (float) width_ / height_, 0.1f, 500.0f);

        // ========== ÉTAPE 1: Remplir le G-buffer ==========
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
        glViewport(0, 0, width_, height_);

        //Nettoyer avec une couleur noire TRANSPARENTE
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activer le test de profondeur
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glUseProgram(cubeProgram_);

        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_, "uProj"), 1, GL_FALSE, proj);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeDiffuseTex_);
        glUniform1i(glGetUniformLocation(cubeProgram_, "uDiffuse"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cubeNormalTex_);
        glUniform1i(glGetUniformLocation(cubeProgram_, "uNormalMap"), 1);

        // Optionnel : activer la normal map (si uniform existe)
        glUniform1i(glGetUniformLocation(cubeProgram_, "useNormal"), 1);

        // Mise à jour des instances
        cubeInstanceMatrices_.clear();
        for (const auto &c: cubeCenters_) {
            float m[16];
            identityMatrix(m);
            m[12] = c.x;
            m[13] = c.y;
            m[14] = c.z;
            cubeInstanceMatrices_.insert(cubeInstanceMatrices_.end(), m, m + 16);
        }

        glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        cubeInstanceMatrices_.size() * sizeof(float),
                        cubeInstanceMatrices_.data());

        glBindVertexArray(cubeVAO_);
        glDrawElementsInstanced(GL_TRIANGLES, cubeIndexCount_, GL_UNSIGNED_INT, 0,
                                (GLsizei) (cubeInstanceMatrices_.size() / 16));
        glBindVertexArray(0);

        // ========== ÉTAPE 2: Lighting pass ==========
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Retour à l'écran principal
        glViewport(0, 0, screenWidth_, screenHeight_);

        // Nettoyer l'écran avec une couleur de fond
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Désactiver le test de profondeur pour le quad plein écran
        glDisable(GL_DEPTH_TEST);

        glUseProgram(deferredProgram_);

        // Activer les textures du G-buffer
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition_);
        glUniform1i(glGetUniformLocation(deferredProgram_, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal_);
        glUniform1i(glGetUniformLocation(deferredProgram_, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
        glUniform1i(glGetUniformLocation(deferredProgram_, "gAlbedoSpec"), 2);

        // Uniforms critiques – noms exacts du shader
        glUniform3f(glGetUniformLocation(deferredProgram_, "viewPos"), camX, camY, camZ);
        glUniform1f(glGetUniformLocation(deferredProgram_, "specularPow"), 32.0f);

        // Lumière
        glUniform1i(glGetUniformLocation(deferredProgram_, "debugMode"), 1);

        // Position de la lumière qui suit la caméra
        // float lightX = camX + 5.0f;
        // float lightY = camY + 5.0f;
        // float lightZ = camZ + 5.0f;
        float lightOrbitRadius = 15.0f;
        float lightAngle = time_ * 0.5f; // Animation lente
        float lightX = target_[0] + cos(lightAngle) * lightOrbitRadius;
        float lightY = target_[1] + 8.0f; // Hauteur fixe
        float lightZ = target_[2] + sin(lightAngle) * lightOrbitRadius;

        glUniform1i(glGetUniformLocation(deferredProgram_, "pointLightCount"), 1);
        glUniform3f(glGetUniformLocation(deferredProgram_, "pointLights[0].position"),
                    lightX, lightY, lightZ);

        glUniform3f(glGetUniformLocation(deferredProgram_, "pointLights[0].color"),
                    1.2f, 1.1f, 1.0f);
        glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].constant"), 1.0f);
        glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].linear"), 0.09f);
        glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].quadratic"), 0.032f);

        // Rendu du quad plein écran
        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    // Rendu avec effet de bloom
    void renderWithBloom() {
        // Étape 1: Extraire les zones lumineuses
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO_[0]);
        glViewport(0, 0, width_ / 2, height_ / 2);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(bloomExtractProgram_);
        glUniform1f(glGetUniformLocation(bloomExtractProgram_, "uThreshold"), bloomThreshold_);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex_);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Étape 2: Flou gaussien (ping-pong)
        bool horizontal = true;
        bool first_iteration = true;

        glUseProgram(bloomBlurProgram_);
        glUniform2f(glGetUniformLocation(bloomBlurProgram_, "uTexelSize"),
                    1.0f / (width_ / 2), 1.0f / (height_ / 2));

        for (int i = 0; i < blurIterations_ * 2; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO_[horizontal ? 1 : 0]);
            glUniform1i(glGetUniformLocation(bloomBlurProgram_, "uHorizontal"), horizontal);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? bloomTex_[0] : bloomTex_[horizontal ? 0 : 1]);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            horizontal = !horizontal;
            if (first_iteration) first_iteration = false;
        }

        // Étape 3: Combiner bloom avec la scène
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO_[0]);
        glViewport(0, 0, width_, height_);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(bloomCombineProgram_);
        glUniform1f(glGetUniformLocation(bloomCombineProgram_, "uBloomIntensity"), bloomIntensity_);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTex_);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTex_[1]);
        glUniform1i(glGetUniformLocation(bloomCombineProgram_, "uBloomBlur"), 1);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Étape 4: Appliquer l'effet de post-processing final
        renderPostProcessingToScreen(pingpongTex_[0]);
    }

    // Rendu simple de post-processing
    void renderPostProcessing() {
        renderPostProcessingToScreen(colorTex_);
    }

    // Applique le post-processing à une texture et affiche à l'écran
    void renderPostProcessingToScreen(GLuint sourceTexture) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth_, screenHeight_);
        glDisable(GL_DEPTH_TEST);

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(postProgram_);

        // Passer les uniformes
        glUniform1i(glGetUniformLocation(postProgram_, "uEffect"), currentEffect_);
        glUniform2f(glGetUniformLocation(postProgram_, "uTexelSize"),
                    1.0f / width_, 1.0f / height_);
        glUniform1f(glGetUniformLocation(postProgram_, "uTime"), time_);
        glUniform1f(glGetUniformLocation(postProgram_, "uBloomThreshold"), bloomThreshold_);
        glUniform1f(glGetUniformLocation(postProgram_, "uBloomIntensity"), bloomIntensity_);

        // Passer le kernel de convolution si nécessaire
        if (currentEffect_ == EFFECT_GAUSSIAN_BLUR &&
            currentEffect_ < convolutionKernels_.size()) {
            const auto &kernel = convolutionKernels_[currentEffect_];
            glUniform1fv(glGetUniformLocation(postProgram_, "uKernel"),
                         kernel.size(), kernel.data());
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sourceTexture);
        glUniform1i(glGetUniformLocation(postProgram_, "uScene"), 0);

        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
        glUseProgram(0);
    }


    static GLuint createModelShaderProgram() {
        // Vertex Shader
        const char *vertexSource = R"(
         #version 330 core
    precision mediump float;

    layout(location = 0) in vec3 aPosition;
    layout(location = 1) in vec3 aNormal;
    layout(location = 2) in vec2 aTexCoord;

    out vec3 FragPos;
    out vec3 Normal;
    out vec2 TexCoord;

    uniform mat4 uModel;
    uniform mat4 uView;
    uniform mat4 uProj;

    void main() {
        FragPos = vec3(uModel * vec4(aPosition, 1.0));

        // SIMPLIFIÉ: pas de transformation inverse/transpose
        Normal = normalize(mat3(uModel) * aNormal);

        // OU: utiliser directement les normales du modèle
        Normal = normalize(aNormal);

        TexCoord = aTexCoord;
        gl_Position = uProj * uView * vec4(FragPos, 1.0);
    }
)";

        // UTILISEZ CE SHADER POUR DEBUG
        const char *fragmentSource = R"(
        #version 330 core
        precision mediump float;

        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoord;

        out vec4 FragColor;

        uniform sampler2D texture_diffuse1;
        uniform vec3 uViewPos;
        uniform vec3 uLightPos;
        uniform vec3 uLightColor;
        uniform float uMaterialShininess;

        uniform int uDebugMode; // 0=normal, 1=normales, 2=lightDir, 3=diffuse, 4=texture

        void main() {
            // MODE DEBUG - changer avec les touches
            if (uDebugMode == 1) {
                // Afficher les normales
                FragColor = vec4(normalize(Normal) * 0.5 + 0.5, 1.0);
                return;
            }

            if (uDebugMode == 2) {
                // Afficher la direction de la lumière
                vec3 lightDir = normalize(uLightPos - FragPos);
                FragColor = vec4(lightDir * 0.5 + 0.5, 1.0);
                return;
            }

            if (uDebugMode == 3) {
                // Afficher seulement la composante diffuse
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float NdotL = dot(norm, lightDir);
                float diff = abs(NdotL); // Utiliser la valeur absolue
                FragColor = vec4(vec3(diff), 1.0);
                return;
            }

            if (uDebugMode == 4) {
                // Afficher seulement la texture
                vec4 texSample = texture(texture_diffuse1, TexCoord);
                FragColor = texSample;
                return;
            }

            // MODE NORMAL (avec fallback si problème)
            vec4 texSample = texture(texture_diffuse1, TexCoord);
            vec3 diffuseColor;

            if (length(texSample.rgb) < 0.01) {
                // Fallback - couleur basée sur la normale
                diffuseColor = normalize(Normal) * 0.5 + 0.5;
            } else {
                diffuseColor = texSample.rgb;
            }

            // Ambient FORCÉ (même si lumière éteinte)
            vec3 ambient = diffuseColor * 0.8; // 80% d'ambient minimum

            // Diffuse
            vec3 norm = normalize(Normal);
            vec3 lightDir = normalize(uLightPos - FragPos);
            float diff = max(dot(norm, lightDir), 0.0);
            vec3 diffuse = uLightColor * diff * diffuseColor;

            // Specular (optionnel)
            vec3 viewDir = normalize(uViewPos - FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);
            float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterialShininess);
            vec3 specular = uLightColor * spec * 0.8;

            // Résultat avec ambient garanti
            vec3 result = ambient + diffuse + specular;

            // S'assurer que le résultat n'est pas noir
            if (length(result) < 0.01) {
                result = diffuseColor; // Fallback à la couleur de base
            }

            FragColor = vec4(result, 1.0);
        }
    )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error(std::string("Erreur linkage shader: ") + infoLog);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        std::cout << "✓ Shader debug créé" << std::endl;
        return program;
    }

    void initGL() {
        // Créer les shaders
        //modelProgram_ = createModelShaderProgram();

        // Configuration des textures dans le shader
        glUseProgram(modelProgram_);

        // Définir les locations des textures
        GLint diffuseLoc = glGetUniformLocation(modelProgram_, "texture_diffuse1");
        GLint specularLoc = glGetUniformLocation(modelProgram_, "texture_specular1");

        if (diffuseLoc >= 0) {
            glUniform1i(diffuseLoc, 0); // Texture diffuse sur unité 0
        }
        if (specularLoc >= 0) {
            glUniform1i(specularLoc, 1); // Texture specular sur unité 1
        }

        glUseProgram(0);

        // Initialiser les shaders de post-processing
        initPostProcessingShaders();

        // Configuration OpenGL de base
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glFrontFace(GL_CCW);
        // Cacher les faces arrière
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        // Sens antihoraire = face avant

        // // Activer le blending pour la transparence
        // glEnable(GL_BLEND);
        // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // glEnable(GL_CULL_FACE);
        // glCullFace(GL_BACK);
    }

    void createSkyboxRessources() {
        // 1) géométrie cube skybox (positions uniquement)
        constexpr float skyboxVertices[] = {
            -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
            -1, -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1,
            1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1,
            -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1, 1,
            -1, 1, -1, 1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1, 1, -1, 1, -1,
            -1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, -1, 1, 1, -1, 1
        };
        glGenVertexArrays(1, &skyboxVAO_);
        glGenBuffers(1, &skyboxVBO_);
        glBindVertexArray(skyboxVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
        glBindVertexArray(0);

        // 2) shaders skybox (inline, comme ton demo)
        const std::string vs = R"(
            #version 330 core
            precision highp float;
            layout (location = 0) in vec3 aPos;
            out vec3 TexCoords;

            uniform mat4 uView;
            uniform mat4 uProj;

            void main() {
                TexCoords = aPos;

                // enlever la translation de la view (skybox centrée)
                mat4 viewNoTranslation = mat4(mat3(uView));
                vec4 pos = uProj * viewNoTranslation * vec4(aPos, 1.0);

                // profondeur à 1.0
                gl_Position = pos.xyww;
            }
        )";

        const std::string fs = R"(
            #version 330 core
            precision mediump float;
            precision mediump samplerCube;
            in vec3 TexCoords;
            out vec4 FragColor;

            uniform samplerCube uSkybox;

            void main() {

                FragColor = texture(uSkybox, TexCoords);
            }
        )";
        skyboxProgram_ = createProgram(vs, fs);
        glUseProgram(skyboxProgram_);
        glUniform1i(glGetUniformLocation(skyboxProgram_, "uSkybox"), 0);

        glUseProgram(0);

        // 3) chargement cubemap
        // IMPORTANT : ordre standard cubemap OpenGL
        // right, left, top, bottom, front, back
        cubemapTex_ = loadCubemap({
            "data/assets/right.jpg",
            "data/assets/left.jpg",
            "data/assets/top.jpg",
            "data/assets/bottom.jpg",
            "data/assets/front.jpg",
            "data/assets/back.jpg"
        });
    }

    static GLuint loadCubemap(const std::vector<std::string> &path) {
        // Vérification de sécurité
        if (path.size() < 6) {
            std::cerr << "[Skybox] Error: Path vector must contain 6 textures.\n";
            return 0;
        }

        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

        // pour cubemap : généralement on ne flip PAS
        stbi_set_flip_vertically_on_load(false);

        int w, h, comp;
        for (int i = 0; i < 6; i++) {
            unsigned char *data = stbi_load(path[i].c_str(), &w, &h, &comp, 0);
            if (!data) {
                std::cerr << "[Skybox] FAILED to load: " << path[i]
                        << " | reason: " << stbi_failure_reason() << "\n";
            } else {
                std::cout << "[Skybox] Loaded: " << path[i]
                        << " (" << w << "x" << h << ", comp=" << comp << ")\n";
            }
            //GLenum format = (comp==4)? GL_RGBA:GL_RGB;
            GLenum format = GL_RGB;
            if (comp == 1) format = GL_RED;
            else if (comp == 3) format = GL_RGB;
            else if (comp == 4) format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format,GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return texID;
    }

    static GLuint compileShader(GLenum type, const std::string &source) {
        GLuint shader = glCreateShader(type);
        const char *src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::string shaderType = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            throw std::runtime_error("Erreur compilation " + shaderType + ":\n" + std::string(infoLog));
        }
        return shader;
    }

    static GLuint createProgram(const std::string &vertexSrc, const std::string &fragmentSrc) {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error("Erreur link program:\n" + std::string(infoLog));
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    static void lookAtMatrix(float *m,
                             float eyeX, float eyeY, float eyeZ,
                             float centerX, float centerY, float centerZ,
                             float upX, float upY, float upZ) {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= fl;
        fy /= fl;
        fz /= fl;

        float ux = upX, uy = upY, uz = upZ;
        float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
        ux /= ul;
        uy /= ul;
        uz /= ul;

        float rx = fy * uz - fz * uy;
        float ry = fz * ux - fx * uz;
        float rz = fx * uy - fy * ux;

        float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
        rx /= rl;
        ry /= rl;
        rz /= rl;

        float ux2 = ry * fz - rz * fy;
        float uy2 = rz * fx - rx * fz;
        float uz2 = rx * fy - ry * fx;

        // colonne-major (OpenGL)
        m[0] = rx;
        m[4] = ux2;
        m[8] = -fx;
        m[12] = 0.0f;
        m[1] = ry;
        m[5] = uy2;
        m[9] = -fy;
        m[13] = 0.0f;
        m[2] = rz;
        m[6] = uz2;
        m[10] = -fz;
        m[14] = 0.0f;
        m[3] = 0;
        m[7] = 0;
        m[11] = 0;
        m[15] = 1.0f;

        // translation
        float t[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -eyeX, -eyeY, -eyeZ, 1
        };

        float out[16];
        multiplyMat4(out, m, t);
        for (int i = 0; i < 16; i++) m[i] = out[i];
    }

    static void perspectiveMatrix(float *m, float fov, float aspect, float near, float far) {
        float rad = fov * 3.1415926535f / 180.0f;
        float f = 1.0f / std::tan(rad * 0.5f);

        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = f / aspect;
        m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    static void multiplyMat4(float *out, const float *a, const float *b) {
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                out[c * 4 + r] =
                        a[0 * 4 + r] * b[c * 4 + 0] +
                        a[1 * 4 + r] * b[c * 4 + 1] +
                        a[2 * 4 + r] * b[c * 4 + 2] +
                        a[3 * 4 + r] * b[c * 4 + 3];
            }
        }
    }

    // Nettoyage
    void cleanup() {
        // Nettoyer les ressources OpenGL
        if (skyboxProgram_)
            glDeleteProgram(skyboxProgram_);
        if (cubemapTex_) glDeleteTextures(1, &cubemapTex_);
        if (skyboxVBO_)
            glDeleteBuffers(1, &skyboxVBO_);
        if (skyboxVAO_)
            glDeleteVertexArrays(1, &skyboxVAO_);

        if (cubeProgram_)
            glDeleteProgram(cubeProgram_);
        if (cubeVBO_)
            glDeleteBuffers(1, &cubeVBO_);
        if (cubeEBO_)
            glDeleteBuffers(1, &cubeEBO_);
        if (cubeVAO_)
            glDeleteVertexArrays(1, &cubeVAO_);

        cubeProgram_ = cubeVAO_ = cubeVBO_ = cubeEBO_ = 0;

        if (postProgram_)
            glDeleteProgram(postProgram_);
        if (bloomExtractProgram_)
            glDeleteProgram(bloomExtractProgram_);
        if (bloomBlurProgram_)
            glDeleteProgram(bloomBlurProgram_);
        if (bloomCombineProgram_)
            glDeleteProgram(bloomCombineProgram_);

        if (fbo_)
            glDeleteFramebuffers(1, &fbo_);
        if (colorTex_) glDeleteTextures(1, &colorTex_);
        if (rboDepthStencil_)
            glDeleteRenderbuffers(1, &rboDepthStencil_);

        for (int i = 0; i < 2; i++) {
            if (pingpongFBO_[i])
                glDeleteFramebuffers(1, &pingpongFBO_[i]);
            if (pingpongTex_[i]) glDeleteTextures(1, &pingpongTex_[i]);
            if (bloomFBO_[i])
                glDeleteFramebuffers(1, &bloomFBO_[i]);
            if (bloomTex_[i]) glDeleteTextures(1, &bloomTex_[i]);
        }

        if (quadVAO_)
            glDeleteVertexArrays(1, &quadVAO_);
        if (quadVBO_)
            glDeleteBuffers(1, &quadVBO_);

        skyboxProgram_ = 0;
        cubemapTex_ = 0;
        skyboxVAO_ = 0;
        skyboxVBO_ = 0;
    }
};


std::unique_ptr<FramebufferRenderer> g_framebufferDemo;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "FrameBuffer DEMO - Techniques Avancé" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Contrôles:" << std::endl;
        std::cout << "  TAB: Toggle camera control" << std::endl;
        std::cout << "  0-9: Changer effet de post-processing" << std::endl;
        std::cout << "  ESPACE: Toggle bloom effect" << std::endl;
        std::cout << "  F1: Deferred Shading" << std::endl;
        std::cout << "  F2: Shadow Mapping" << std::endl;
        std::cout << "  F3: SSAO (Screen Space Ambient Occlusion)" << std::endl;
        std::cout << "  F4: Debug Shadow Map" << std::endl;
        std::cout << "  F5: Debug SSAO Buffer" << std::endl;
        std::cout << "========================================" << std::endl;

        common::WindowConfig config;
        config.width = 1280;
        config.height = 720;
        config.title = "FrameBuffer Demo - Post-Processing Avancé";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        g_framebufferDemo = std::make_unique<FramebufferRenderer>();
        common::SystemObserverSubject::AddObserver(g_framebufferDemo.get());
        common::DrawObserverSubject::AddObserver(g_framebufferDemo.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(g_framebufferDemo.get());
        common::DrawObserverSubject::RemoveObserver(g_framebufferDemo.get());
        g_framebufferDemo.reset();

        std::cout << "\n✓ Démonstration terminée avec succès!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_framebufferDemo) {
            common::SystemObserverSubject::RemoveObserver(g_framebufferDemo.get());
            common::DrawObserverSubject::RemoveObserver(g_framebufferDemo.get());
            g_framebufferDemo.reset();
        }
        return -1;
    }
    return 0;
}
