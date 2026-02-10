//
// Created by forna on 10.02.2026.
//

#include "../../include/Refactor/final_scene.h"

#include <corecrt_math_defines.h>
#include <iostream>
#include <random>

#include "stb_image.h"
#include "engine/window.h"
#include "Refactor/shaders.h"

void FinalScene::Begin() {
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
    glGenBuffers(1, &modelInstanceVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, modelInstanceVBO_);
    glBufferData(GL_ARRAY_BUFFER,
                 modelInstanceCount_ * 16 * sizeof(float),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    //Attacher aux VAO de tous les meshes du modèle
    model_->AttachInstanceBuffer(modelInstanceVBO_);

    core::Vec3F c = model_->GetCenter();
    float offsetY = -model_->GetMinY();
    target_[0] = c.x;
    target_[1] = c.y + offsetY;
    target_[2] = c.z;

    orbitRadius_ = std::max(orbitRadius_, 5.0f);
    std::cout << "Center: " << target_[0] << "," << target_[1] << "," << target_[2]
            << "  Radius: " << orbitRadius_ << std::endl;

    // Angle de départ pour bien voir
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

#ifdef IMGUI_ENABLED
    initImGui();
#endif
}

void FinalScene::Update(float dt) {
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

void FinalScene::FixedUpdate() {
}

void FinalScene::End() {
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
}

void FinalScene::PreDraw() {
    DrawInterface::PreDraw();
}

void FinalScene::Draw() {
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

void FinalScene::PostDraw() {
    DrawInterface::PostDraw();
}

void FinalScene::SetMouseLook(bool enabled) {
    mouseLookEnabled_ = enabled;
    SDL_Window *window = common::GetWindow();

    SDL_SetWindowRelativeMouseMode(window, enabled);
    float dx, dy;
    SDL_GetRelativeMouseState(&dx, &dy);
    firstMouse_ = true;
}

void FinalScene::initGL() {
    // 1) créer shader modèle
    modelProgram_ = createModelShaderProgram();

    // 2) binder et set les samplers (IMPORTANT: tant que le program est actif)
    glUseProgram(modelProgram_);
    glUniform1i(glGetUniformLocation(modelProgram_, "texture_diffuse1"), 0);
    // Si ton FS n'utilise pas texture_specular1, tu peux enlever.
    // glUniform1i(glGetUniformLocation(modelProgram_, "texture_specular1"), 1);
    glUseProgram(0);

    // 3) post-processing
    initPostProcessingShaders();

    // 4) état GL de base
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
}

GLuint FinalScene::createModelShaderProgram() {
    return createProgram(ModelVertex, ModelFragment);

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, ModelVertex);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, ModelFragment);

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

void FinalScene::initPostProcessingShaders() {
    postProgram_ = createProgram(PostVertex, PostFragment);
}

GLuint FinalScene::createProgram(const std::string &vertexSrc, const std::string &fragmentSrc) {
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

GLuint FinalScene::compileShader(GLenum type, const std::string &source) {
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

void FinalScene::createFramebuffer() {
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

void FinalScene::createScreenQuad() {
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

void FinalScene::createMultiPassFramebuffers() {
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
void FinalScene::createBloomFramebuffers() {
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

void FinalScene::createSkyboxRessources() {
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


    skyboxProgram_ = createProgram(VertexSkyBox, FragmentSkybox);

    if (skyboxProgram_ == 0) {
        std::cerr << "[ERROR] Skybox program creation failed!\n";
        return;
    }

    glUseProgram(skyboxProgram_);
    glUniform1i(glGetUniformLocation(skyboxProgram_, "uSkybox"), 0);
    glUseProgram(0);

    // 3) chargement cubemap - VÉRIFIER LES CHEMINS !
    std::cout << "[Skybox] Attempting to load cubemap textures...\n";
    cubemapTex_ = loadCubemap({
        "data/assets/right.jpg",
        "data/assets/left.jpg",
        "data/assets/top.jpg",
        "data/assets/bottom.jpg",
        "data/assets/front.jpg",
        "data/assets/back.jpg"
    });

    if (cubemapTex_ == 0) {
        std::cerr << "[ERROR] Failed to load cubemap textures!\n";
        std::cerr << "[INFO] Ensure textures exist at: data/assets/{right,left,top,bottom,front,back}.jpg\n";
    } else {
        std::cout << "[SUCCESS] Cubemap loaded: " << cubemapTex_ << "\n";
    }
}

GLuint FinalScene::loadCubemap(const std::vector<std::string> &path) {
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
    int loadedCount = 0;
    for (int i = 0; i < 6; i++) {
        unsigned char *data = stbi_load(path[i].c_str(), &w, &h, &comp, 0);
        if (!data) {
            std::cerr << "[Skybox] FAILED to load: " << path[i]
                    << " | reason: " << stbi_failure_reason() << "\n";
            continue; // ⚠️ Continue quand même pour voir les autres
        } else {
            std::cout << "[Skybox] ✓ Loaded: " << path[i]
                    << " (" << w << "x" << h << ", comp=" << comp << ")\n";
            loadedCount++;
        }

        GLenum format = GL_RGB;
        if (comp == 1) format = GL_RED;
        else if (comp == 3) format = GL_RGB;
        else if (comp == 4) format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    if (loadedCount < 6) {
        std::cerr << "[ERROR] Only " << loadedCount << "/6 textures loaded!\n";
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return texID;
}

void FinalScene::initBloomShaders() {
    bloomExtractProgram_ = createProgram(bloomVertexShader, bloomExtractFS);
    bloomBlurProgram_ = createProgram(bloomVertexShader, bloomBlurFS);
    bloomCombineProgram_ = createProgram(bloomVertexShader, bloomCombineFS);
}

void FinalScene::initConvolutionKernels() {
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

void FinalScene::initCubeResources() {
    cubeProgram_ = createProgram(ModelCube, ModulFragment);
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

GLuint FinalScene::loadTexture2D(const std::string &path, bool flipY) {
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

void FinalScene::createGBuffer(int width, int height) {
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

void FinalScene::initShadowMapping() {
    glGenFramebuffers(1, &shadowFBO_);

    glGenTextures(1, &shadowDepthTex_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT32F,
                 SHADOW_WIDTH,
                 SHADOW_HEIGHT,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_FLOAT,
                 nullptr); // Utiliser GL_FLOAT

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


    shadowProgram_ = createProgram(shadowVS, shadowFS);

    // DEBUG: Vérifier que le shader est créé
    if (shadowProgram_ == 0) {
        std::cerr << "ERROR: Failed to create shadow program!" << std::endl;
    } else {
        std::cout << "[INFO] Shadow Mapping initialisé ("
                << SHADOW_WIDTH << "x" << SHADOW_HEIGHT << ")" << std::endl;
    }
}

void FinalScene::checkShadowFBO() const {
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    switch (status) {
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

void FinalScene::initSSAO() {
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

    ssaoProgram_ = createProgram(ssaoVS, ssaoFS);
    ssaoBlurProgram_ = createProgram(ssaoVS, ssaoBlurFS);

    std::cout << "[INFO] SSAO initialisé (kernel size: " << ssaoKernelSize_ << ")" << std::endl;
}

void FinalScene::createCubeLine() {
    cubeCenters_.clear();

    // Positionnez les cubes autour du centre de la scène
    for (int i = -20; i <= 20; ++i) {
        cubeCenters_.push_back(core::Vec3F{
            target_[0] + i * 3.0f, // Plus espacés
            target_[1] + 0.5f, // Un peu au-dessus du sol
            target_[2] - 10.0f // Devant la caméra
        });
    }

    std::cout << "Created " << cubeCenters_.size() << " cubes" << std::endl;
}

void FinalScene::initCubeInstancing() {
    glBindVertexArray(cubeVAO_);

    glGenBuffers(1, &cubeInstanceVBO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeInstanceVBO_);
    glBufferData(GL_ARRAY_BUFFER, cubeCenters_.size() * 16 * sizeof(float), nullptr,GL_DYNAMIC_DRAW);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              16 * sizeof(float),
                              (void *) (i * 4 * sizeof(float)));
        glVertexAttribDivisor(4 + i, 1);
    }
    glBindVertexArray(0);
}

void FinalScene::cleanup() {
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

void FinalScene::renderDeferred() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    float view[16], proj[16];
    calculateCameraMatrices(view, proj);

    // Étape 1: Geometry Pass (identique)
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // glEnable(GL_DEPTH_TEST);
    // glDepthMask(GL_TRUE);
    //
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    // glFrontFace(GL_CCW);

    renderGeometryPass(view, proj);

    // Étape 2: Lighting pass AVEC skybox
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    static GLuint debugProgram = 0;
    if (debugProgram == 0) {
        debugProgram = createProgram(debugVS, debugFS);
    }

    // UTILISEZ LE VRAI SHADER DEFERRED (celui qui affiche la skybox)
    glUseProgram(deferredProgram_);

    // Textures du G-buffer
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition_);
    glUniform1i(glGetUniformLocation(deferredProgram_, "gPosition"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal_);
    glUniform1i(glGetUniformLocation(deferredProgram_, "gNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec_);
    glUniform1i(glGetUniformLocation(deferredProgram_, "gAlbedoSpec"), 2);

    // Uniforms pour la skybox
    glUniform3f(glGetUniformLocation(deferredProgram_, "viewPos"),
                camPos_[0], camPos_[1], camPos_[2]);
    glUniform1f(glGetUniformLocation(deferredProgram_, "specularPow"), 32.0f);

    // Lumière (comme dans votre ancien projet)
    glUniform1i(glGetUniformLocation(deferredProgram_, "pointLightCount"), 1);

    // Position de la lumière (animation)
    float lightAngle = time_ * 0.5f;
    float lightX = target_[0] + cos(lightAngle) * 15.0f;
    float lightY = target_[1] + 8.0f;
    float lightZ = target_[2] + sin(lightAngle) * 15.0f;

    glUniform3f(glGetUniformLocation(deferredProgram_, "pointLights[0].position"),
                lightX, lightY, lightZ);
    glUniform3f(glGetUniformLocation(deferredProgram_, "pointLights[0].color"),
                1.2f, 1.1f, 1.0f);
    glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].constant"), 1.0f);
    glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].linear"), 0.09f);
    glUniform1f(glGetUniformLocation(deferredProgram_, "pointLights[0].quadratic"), 0.032f);

    // Skybox texture
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex_);
    glUniform1i(glGetUniformLocation(deferredProgram_, "uSkybox"), 5);

    // Mode normal (pas debug)
    glUniform1i(glGetUniformLocation(deferredProgram_, "debugMode"), 0);

    // Matrices inverses pour le ray marching de la skybox
    float invProj[16], invView[16];

    glUniformMatrix4fv(glGetUniformLocation(deferredProgram_, "invProj"), 1, GL_FALSE, invProj);
    glUniformMatrix4fv(glGetUniformLocation(deferredProgram_, "invViewRot"), 1, GL_FALSE, invView);


    // Rendu du quad plein écran
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUniform1f(glGetUniformLocation(deferredProgram_, "far"), far);
    renderSkybox(view, proj);
    //glDisable(GL_DEPTH_TEST);
}

void FinalScene::calculateCameraMatrices(float *view, float *proj) {
    float camX = target_[0] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::cos(yaw_ * M_PI / 180);
    float camY = target_[1] + orbitRadius_ * std::sin(pitch_ * M_PI / 180);
    float camZ = target_[2] + orbitRadius_ * std::cos(pitch_ * M_PI / 180) * std::sin(yaw_ * M_PI / 180);

    camPos_[0] = camX;
    camPos_[1] = camY;
    camPos_[2] = camZ;

    lookAtMatrix(view, camX, camY, camZ, target_[0], target_[1], target_[2], 0, 1, 0);
    perspectiveMatrix(proj, 60.0f, (float) width_ / height_, 0.1f, far);
}

void FinalScene::renderGeometryPass(float *view, float *proj) {
    //Rendu des cubes
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

    // Mise à jour des instances des cubes
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

    // Rendu du modèle nanosuit avec instanciation


    // ===== Model instanced (nanosuit) =====
    if (model_) {
        modelInstanceMatrices_.clear();
        modelInstanceMatrices_.reserve(modelInstanceCount_ * 16);

        for (int i = 0; i < modelInstanceCount_; ++i) {
            float M[16];
            identityMatrix(M);

            float angle = (2.0f * M_PI / float(modelInstanceCount_)) * float(i);
            float radius = 10.0f;

            // scale
            float s = 0.5f;
            M[0] = s;
            M[5] = s;
            M[10] = s;

            // translation (col-major)
            M[12] = target_[0] + std::cos(angle) * radius;
            M[13] = target_[1] + 0.5f;
            M[14] = target_[2] + std::sin(angle) * radius;

            modelInstanceMatrices_.insert(modelInstanceMatrices_.end(), M, M + 16);
        }

        // upload vers le VBO
        glBindBuffer(GL_ARRAY_BUFFER, modelInstanceVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        modelInstanceMatrices_.size() * sizeof(float),
                        modelInstanceMatrices_.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // draw instanced dans le GBuffer
        glUseProgram(modelProgram_);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uProj"), 1, GL_FALSE, proj);

        model_->DrawInstanced(modelProgram_, modelInstanceCount_);
    }
}

void FinalScene::renderSkybox(float *view, float *proj) {
    if (!skyboxProgram_ || !cubemapTex_) return;
    //glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LEQUAL); // Permettre à la skybox d'être au fond

    glUseProgram(skyboxProgram_);

    // Passer les matrices sans translation
    float viewNoTranslation[16];
    memcpy(viewNoTranslation, view, 16 * sizeof(float));
    viewNoTranslation[12] = 0;
    viewNoTranslation[13] = 0;
    viewNoTranslation[14] = 0;

    glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uView"), 1, GL_FALSE, viewNoTranslation);
    glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uProj"), 1, GL_FALSE, proj);

    //glUniform1f(glGetUniformLocation(skyboxProgram_, "uBrightness"), 2.f); // 1.5 = 50% plus clair

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex_);
    glBindVertexArray(skyboxVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    //glEnable(GL_CULL_FACE);
}

void FinalScene::lookAtMatrix(float *m,
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

void FinalScene::perspectiveMatrix(float *m, float fov, float aspect, float near, float far) {
    float rad = fov * 3.1415926535f / 180.0f;
    float f = 1.0f / std::tan(rad * 0.5f);

    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

void FinalScene::multiplyMat4(float *out, const float *a, const float *b) {
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

void FinalScene::identityMatrix(float *m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void FinalScene::renderShadowMapping() {
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
    // Rendu de la skybox
    renderSkybox(view, proj);
    // Rendu de la géométrie
    renderGeometryPass(view, proj);

    // Lighting pass avec ombres
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    static GLuint shadowSimpleProgram = 0;
    if (shadowSimpleProgram == 0) {
        shadowSimpleProgram = createProgram(SSAOvs, shadowSimpleFS);
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

void FinalScene::renderShadowMap() {
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
    std::cout << "Light Target: " << lightTarget[0] << ", " << lightTarget[1] << ", " << lightTarget[2] <<
            std::endl;
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

void FinalScene::renderSSAO() {
    float view[16], proj[16];
    calculateCameraMatrices(view, proj);

    // 1. Geometry pass
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    renderSkybox(view, proj);

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


    static GLuint ssaoProgram = 0;
    if (ssaoProgram == 0) {
        ssaoProgram = createProgram(SSAOvs2, ssaoFS2);
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

void FinalScene::renderSSAOPass(float *proj) {
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
    glEnable(GL_DEPTH_TEST);
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

void FinalScene::renderDebugShadow() {
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
    std::cout << "  Avg depth (valid): " << (validPixels > 0 ? sumDepth / validPixels : 0) << std::endl;

    // 4. Afficher la texture
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // Shader debug amélioré


    static GLuint debugProgram = 0;
    if (debugProgram == 0) {
        debugProgram = createProgram(debugVS2, debugFS2);
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

void FinalScene::renderDebugSSAO() {
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


    static GLuint debugProgram = 0;
    if (debugProgram == 0) {
        debugProgram = createProgram(debugVS3, debugFS3);
    }

    glUseProgram(debugProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurBuffer_);
    glUniform1i(glGetUniformLocation(debugProgram, "ssaoBuffer"), 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
