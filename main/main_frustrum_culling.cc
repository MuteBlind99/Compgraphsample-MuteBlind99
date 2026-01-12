//
// Created by forna on 09.01.2026.
//
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
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
        EFFECT_BLOOM,  // Effet avancé: bloom
        EFFECT_NIGHT_VISION,
        EFFECT_COUNT
    };

    void Begin() override {
        std::cout << "SkyboxRenderer::Begin() - Initialisation framebuffer" << std::endl;

        // Utilisation de GetWindow() qui est déjà typé SDL_Window*
        SDL_Window *window = common::GetWindow();
        SetMouseLook(true);
        // Récupérer la taille réelle de la fenêtre
        SDL_GetWindowSize(window, &screenWidth_,&screenHeight_);
        width_=screenWidth_;
        height_=screenHeight_;
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
        createCubeLine();

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
        cleanup();
    }

    void PreDraw() override {
    }

    void Draw() override {

        // RENDER PASS 1: Scène principale dans le FBO
        renderSceneToFramebuffer();
        // Position de la lumière
        float lightX = target_[0] + 3.0f;
        float lightY = target_[1] + 5.0f;
        float lightZ = target_[2] + 3.0f;

        std::cout << "[DEBUG] Light position: " << lightX << ", " << lightY << ", " << lightZ << std::endl;
        // RENDER PASS 2: Post-processing (multi-pass si nécessaire)
        if (useBloom_ && currentEffect_ != EFFECT_NONE) {
            renderWithBloom();
        } else {
            renderPostProcessing();
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

    // État des touches
    bool keyWasDown_[512] = {false};

    // Matrices de convolution prédéfinies
    std::vector<std::vector<float>> convolutionKernels_;

    // Shaders
    GLuint modelProgram_ = 0;
    GLuint skyboxProgram_ = 0;
    GLuint postProgram_ = 0;
    GLuint bloomExtractProgram_ = 0;
    GLuint bloomBlurProgram_ = 0;
    GLuint bloomCombineProgram_ = 0;

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

    std::vector<core::Vec3F> cubeCenters_;
    float cubeHalfSize_ = 0.5f;   // AABB = center ± halfSize

    int drawnCubes_ = 0;          // debug (combien dessinés après culling)

    // Modèle
    std::unique_ptr<Model> model_;

    struct Plane {
        core::Vec3F n;
        float d{}; // distance (ax+by+cz+d=0)
        [[nodiscard]] float distance(const core::Vec3F& p)const {
            return n.x*p.x + n.y*p.y +n.z*p.z+d;
        }
    };

    struct Frustrum {
        Plane p[6]; // 0:L 1:R 2:B 3:T 4:N 5:F
    };
    struct AABB {
        core::Vec3F mn;
        core::Vec3F mx;
    };
    static void normalizePlane(Plane& pl) {
        float len =std::sqrt(pl.n.x* pl.n.x + pl.n.y* pl.n.y + pl.n.z* pl.n.z);
        if (len> 0.0f) {
            pl.n.x/=len; pl.n.y/=len; pl.n.z/=len;
            pl.d /=len;
        }
    }
    static float mrc(const float* m, int row, int col) {return m[col*4+row];}

    static Frustrum exractFrustrum(const float* proj, const float* view) {
        float vp[16];
        multiplyMat4(vp,proj, view);

        Frustrum f;
        auto makePlane=[&](int signRow, int axisRow)->Plane {
            Plane pl;
            pl.n.x = mrc(vp, 3, 0) + signRow * mrc(vp, axisRow, 0);
            pl.n.y = mrc(vp, 3, 1) + signRow * mrc(vp, axisRow, 1);
            pl.n.z = mrc(vp, 3, 2) + signRow * mrc(vp, axisRow, 2);
            pl.d   = mrc(vp, 3, 3) + signRow * mrc(vp, axisRow, 3);
            normalizePlane(pl);
            return pl;
        };

        f.p[0] = makePlane(+1, 0); // Left  = row3 + row0
        f.p[1] = makePlane(-1, 0); // Right = row3 - row0
        f.p[2] = makePlane(+1, 1); // Bottom
        f.p[3] = makePlane(-1, 1); // Top
        f.p[4] = makePlane(+1, 2); // Near
        f.p[5] = makePlane(-1, 2); // Far
        return f;
    }
    // Test AABB vs frustum
    static bool aabbInFrustum(const Frustrum& f, const AABB& b) {
        for (int i=0;i <6; ++i) {
            const Plane& p= f.p[i];

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
        #version 300 es
        precision highp float;
        layout(location=0) in vec3 aPos;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        void main(){
            gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
        }
    )";

        const std::string fs = R"(
        #version 300 es
        precision mediump float;
        out vec4 FragColor;
        uniform vec3 uColor;
        void main(){
            FragColor = vec4(uColor, 1.0);
        }
    )";

        cubeProgram_ = createProgram(vs, fs);

        // Cube géométrie (positions only) + indices
        const float h = 0.5f;
        const float vertices[] = {
            -h,-h,-h,  +h,-h,-h,  +h,+h,-h,  -h,+h,-h, // back
            -h,-h,+h,  +h,-h,+h,  +h,+h,+h,  -h,+h,+h  // front
        };

        const unsigned int indices[] = {
            0,1,2, 2,3,0, // back
            4,5,6, 6,7,4, // front
            0,4,7, 7,3,0, // left
            1,5,6, 6,2,1, // right
            3,2,6, 6,7,3, // top
            0,1,5, 5,4,0  // bottom
        };
        cubeIndexCount_ = (GLsizei)(sizeof(indices)/sizeof(indices[0]));

        glGenVertexArrays(1, &cubeVAO_);
        glGenBuffers(1, &cubeVBO_);
        glGenBuffers(1, &cubeEBO_);

        glBindVertexArray(cubeVAO_);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

        glBindVertexArray(0);
    }
    void createCubeLine() {
        cubeCenters_.clear();

        // 101 cubes alignés sur X, devant le modèle
        for (int i = -50; i <= 50; ++i) {
            cubeCenters_.push_back(core::Vec3F{
                target_[0] + i * 2.0f,
                target_[1],
                target_[2] - 15.0f
            });
        }
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
            -1,  8, -1,
            -1, -1, -1
        };

        // Flou
        convolutionKernels_[EFFECT_BLUR] = {
            1/16.0f, 2/16.0f, 1/16.0f,
            2/16.0f, 4/16.0f, 2/16.0f,
            1/16.0f, 2/16.0f, 1/16.0f
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
            1/256.0f, 4/256.0f,  6/256.0f,  4/256.0f, 1/256.0f,
            4/256.0f, 16/256.0f, 24/256.0f, 16/256.0f, 4/256.0f,
            6/256.0f, 24/256.0f, 36/256.0f, 24/256.0f, 6/256.0f,
            4/256.0f, 16/256.0f, 24/256.0f, 16/256.0f, 4/256.0f,
            1/256.0f, 4/256.0f,  6/256.0f,  4/256.0f, 1/256.0f
        };
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_/2, height_/2, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
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
             1,  1, 1, 1,

            -1, -1, 0, 0,
             1,  1, 1, 1,
            -1,  1, 0, 1
        };

        glGenVertexArrays(1, &quadVAO_);
        glGenBuffers(1, &quadVBO_);
        glBindVertexArray(quadVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), &quad, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
    }
    // Initialisation des shaders de post-processing
    void initPostProcessingShaders() {
        const char* postVertexShader = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aUV;

            out vec2 vUV;

            void main() {
                vUV = aUV;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

        const char* postFragmentShader = R"(
    #version 300 es
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
    const char* bloomExtractFS = R"(
        #version 300 es
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

    const char* bloomVertexShader = R"(
        #version 300 es
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
    const char* bloomBlurFS = R"(
        #version 300 es
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
    const char* bloomCombineFS = R"(
        #version 300 es
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

    // Rendu de la scène dans le framebuffer
    void renderSceneToFramebuffer() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calcul des matrices
        float view[16], proj[16], modelMat[16];
        Frustrum fr = exractFrustrum(proj, view);
        drawnCubes_=0;

        float yawRad = yaw_ * 3.1415926535f / 180.0f;
        float pitchRad = pitch_ * 3.1415926535f / 180.0f;

        // Position caméra
        float camX = target_[0] + orbitRadius_ * std::cos(pitchRad) * std::cos(yawRad);
        float camY = target_[1] + orbitRadius_ * std::sin(pitchRad);
        float camZ = target_[2] + orbitRadius_ * std::cos(pitchRad) * std::sin(yawRad);

        // Matrice de vue
        lookAtMatrix(view,
                    camX, camY, camZ,
                    target_[0], target_[1], target_[2],
                    0.0f, 1.0f, 0.0f);

        // Matrice de projection
        perspectiveMatrix(proj, 60.0f, (float)width_ / (float)height_, 0.1f, 500.0f);

        // Rotation du modèle
        identityMatrix(modelMat);
        float dx = camX - target_[0];
        float dz = camZ - target_[2];
        float angleY = std::atan2(dx, dz) + modelYawOffset_;

        float c = std::cos(angleY);
        float s = std::sin(angleY);

        modelMat[0] = c;
        modelMat[8] = s;
        modelMat[2] = -s;
        modelMat[10] = c;
        modelMat[13] += -model_->GetMinY();

        // Rendu du modèle
        glUseProgram(modelProgram_);

        GLint loc;
        loc = glGetUniformLocation(modelProgram_, "uLightColor");
        if (loc >= 0) glUniform3f(loc, 1.0f, 1.0f, 1.0f);

        loc = glGetUniformLocation(modelProgram_, "uMaterialShininess");
        if (loc >= 0) glUniform1f(loc, 32.0f);

        loc = glGetUniformLocation(modelProgram_, "uViewPos");
        if (loc >= 0) glUniform3f(loc, camX, camY, camZ);

        loc = glGetUniformLocation(modelProgram_, "uLightPos");
        if (loc >= 0) glUniform3f(loc, target_[0] + 3.0f, target_[1] + 5.0f, target_[2] + 3.0f);

        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uModel"), 1, GL_FALSE, modelMat);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uProj"), 1, GL_FALSE, proj);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        model_->Draw(modelProgram_);
        glEnable(GL_BLEND);

        //Cube
        glUseProgram(cubeProgram_);
        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_,"uView"),1,GL_FALSE,view);
        glUniformMatrix4fv(glGetUniformLocation(cubeProgram_,"uProj"),1,GL_FALSE,proj);
        glUniform3f(glGetUniformLocation(cubeProgram_, "uColor"),0.2f,0.8f,0.3f);

        glBindVertexArray(cubeVAO_);

        for (const auto& cpos:cubeCenters_) {
            AABB box;
            box.mn= core::Vec3F{cpos.x - cubeHalfSize_, cpos.y - cubeHalfSize_, cpos.z - cubeHalfSize_};
            box.mx= core::Vec3F{cpos.x + cubeHalfSize_, cpos.y + cubeHalfSize_, cpos.z + cubeHalfSize_};
            if (!aabbInFrustum(fr,box)) {
                continue;
            }
            float model[16];
            identityMatrix(model);
            model[12] = cpos.x;
            model[13] = cpos.y;
            model[14] = cpos.z;

            glUniformMatrix4fv(glGetUniformLocation(cubeProgram_, "uModel"), 1, GL_FALSE, model);
            glDrawElements(GL_TRIANGLES, cubeIndexCount_, GL_UNSIGNED_INT, 0);
            drawnCubes_++;
        }
        glBindVertexArray(0);

        // Rendu du skybox
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glUseProgram(skyboxProgram_);

        // Vue sans translation pour le skybox
        float viewNoTrans[16];
        memcpy(viewNoTrans, view, sizeof(viewNoTrans));
        viewNoTrans[12] = viewNoTrans[13] = viewNoTrans[14] = 0.0f;

        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uProj"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uView"), 1, GL_FALSE, viewNoTrans);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex_);
        glBindVertexArray(skyboxVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
        glUseProgram(0);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        for (const auto& cpos : cubeCenters_) {
            AABB box;
            box.mn = core::Vec3F{cpos.x - cubeHalfSize_, cpos.y - cubeHalfSize_, cpos.z - cubeHalfSize_};
            box.mx = core::Vec3F{cpos.x + cubeHalfSize_, cpos.y + cubeHalfSize_, cpos.z + cubeHalfSize_};

            if (!aabbInFrustum(fr, box))
                continue; // ✅ cube hors frustum => pas de draw call

            float cubeModel[16];
            identityMatrix(cubeModel);
            cubeModel[12] = cpos.x; // translation en column-major
            cubeModel[13] = cpos.y;
            cubeModel[14] = cpos.z;
            // glUseProgram(cubeProgram_);
            // glUniformMatrix4fv(uModel/uView/uProj, ...)
            // glBindVertexArray(cubeVAO_);
            // glDrawArrays / glDrawElements
        }

    }

    // Rendu avec effet de bloom
    void renderWithBloom() {
        // Étape 1: Extraire les zones lumineuses
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO_[0]);
        glViewport(0, 0, width_/2, height_/2);
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
                   1.0f / (width_/2), 1.0f / (height_/2));

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
            const auto& kernel = convolutionKernels_[currentEffect_];
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
         #version 300 es
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
        #version 300 es
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
        modelProgram_ = createModelShaderProgram();

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

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // Cacher les faces arrière
        glFrontFace(GL_CCW); // Sens antihoraire = face avant

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
            #version 300 es
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
            #version 300 es
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
        if (skyboxProgram_) glDeleteProgram(skyboxProgram_);
        if (cubemapTex_) glDeleteTextures(1, &cubemapTex_);
        if (skyboxVBO_) glDeleteBuffers(1, &skyboxVBO_);
        if (skyboxVAO_) glDeleteVertexArrays(1, &skyboxVAO_);

        if (cubeProgram_) glDeleteProgram(cubeProgram_);
        if (cubeVBO_) glDeleteBuffers(1, &cubeVBO_);
        if (cubeEBO_) glDeleteBuffers(1, &cubeEBO_);
        if (cubeVAO_) glDeleteVertexArrays(1, &cubeVAO_);

        cubeProgram_ = cubeVAO_ = cubeVBO_ = cubeEBO_ = 0;

        if (postProgram_) glDeleteProgram(postProgram_);
        if (bloomExtractProgram_) glDeleteProgram(bloomExtractProgram_);
        if (bloomBlurProgram_) glDeleteProgram(bloomBlurProgram_);
        if (bloomCombineProgram_) glDeleteProgram(bloomCombineProgram_);

        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        if (colorTex_) glDeleteTextures(1, &colorTex_);
        if (rboDepthStencil_) glDeleteRenderbuffers(1, &rboDepthStencil_);

        for (int i = 0; i < 2; i++) {
            if (pingpongFBO_[i]) glDeleteFramebuffers(1, &pingpongFBO_[i]);
            if (pingpongTex_[i]) glDeleteTextures(1, &pingpongTex_[i]);
            if (bloomFBO_[i]) glDeleteFramebuffers(1, &bloomFBO_[i]);
            if (bloomTex_[i]) glDeleteTextures(1, &bloomTex_[i]);
        }

        if (quadVAO_) glDeleteVertexArrays(1, &quadVAO_);
        if (quadVBO_) glDeleteBuffers(1, &quadVBO_);

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
        std::cout << "FrameBuffer DEMO - Post-Processing Avancé" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Contrôles:" << std::endl;
        std::cout << "  TAB: Toggle camera control" << std::endl;
        std::cout << "  0-9: Changer effet de post-processing" << std::endl;
        std::cout << "  ESPACE: Toggle bloom effect" << std::endl;
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
