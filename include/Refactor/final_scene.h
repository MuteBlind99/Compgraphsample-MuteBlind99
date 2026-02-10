//
// Created by forna on 10.02.2026.
//

#ifndef FINAL_SCENE_H
#define FINAL_SCENE_H
#include <memory>
#include <GL/glew.h>

#include "model_loader.h"
#include "engine/renderer.h"
#include "engine/system.h"

// Début de la classe principale SceneFinal
class FinalScene : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override;

    void Update(float dt) override;

    void FixedUpdate() override;

    void End() override;;

    void PreDraw() override;;

    void Draw() override;;

    void PostDraw() override;;

private:
    // Groupement : Énumérations pour les effets post-processing et modes de rendu
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

    enum RenderMode {
        RENDER_DEFERRED = 0,
        RENDER_SHADOW_MAPPING,
        RENDER_SSAO,
        RENDER_DEBUG_SHADOW,
        RENDER_DEBUG_SSAO,
        RENDER_MODE_COUNT
    };

    // Groupement : Variables générales pour la caméra et l'interaction utilisateur
    bool mouseLookEnabled_ = true;
    bool firstMouse_ = true;
    int screenWidth_ = 800;
    int screenHeight_ = 600;
    int width_ = 800;
    int height_ = 600;
    GLuint modelProgram_ = 0;
    GLuint postProgram_ = 0;
    GLuint fbo_ = 0;
    GLuint colorTex_ = 0;
    GLuint rboDepthStencil_ = 0;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    std::unique_ptr<Model> model_;
    GLuint modelInstanceVBO_ = 0;
    int modelInstanceCount_ = 5;
    float target_[3] = {0.0f, 0.0f, 0.0f};
    float orbitRadius_ = 20.0f;
    float yaw_ = -90.0f; // regarde vers -Z
    float pitch_ = 0.0f;
    bool tabWasDown_ = false;
    bool keyWasDown_[512] = {false};
    PostEffect currentEffect_ = EFFECT_NONE;
    bool useBloom_ = false;
    RenderMode currentRenderMode_ = RENDER_DEFERRED;
    float mouseSensitivity_ = 0.10f;
    float moveSpeed_ = 3.5f;
    float minPitch_ = -89.0f;
    float maxPitch_ = 89.0f;
    float camPos_[3] = {0.0f, 0.0f, 3.0f};
    float time_ = 0.0f;
    // Groupement : Variables liées au Bloom (effet post-processing avancé)
    GLuint bloomFBO_[2] = {0, 0};
    GLuint bloomTex_[2] = {0, 0};
    GLuint pingpongFBO_[2] = {0, 0};
    GLuint pingpongTex_[2] = {0, 0};
    GLuint bloomExtractProgram_ = 0;
    GLuint bloomBlurProgram_ = 0;
    GLuint bloomCombineProgram_ = 0;
    // Groupement : Variables liées au Skybox
    GLuint skyboxVAO_ = 0;
    GLuint skyboxVBO_ = 0;
    GLuint cubemapTex_ = 0;
    GLuint skyboxProgram_ = 0;

    // Groupement : Variables liées aux noyaux de convolution (pour effets comme blur, sharpen, etc.)
    std::vector<std::vector<float> > convolutionKernels_;

    // Groupement : Variables liées aux cubes et au rendu deferred
    GLuint cubeProgram_ = 0;
    GLuint deferredProgram_ = 0;
    GLsizei cubeIndexCount_ = 0;
    GLuint cubeVAO_ = 0, cubeVBO_ = 0, cubeEBO_ = 0;
    GLuint cubeDiffuseTex_ = 0;
    GLuint cubeNormalTex_ = 0;
    GLuint gBuffer_ = 0, gPosition_ = 0, gNormal_ = 0, gAlbedoSpec_ = 0;
    GLuint rboDepth_ = 0;
    std::vector<core::Vec3F> cubeCenters_;
    GLuint cubeInstanceVBO_ = 0;
    std::vector<float> modelInstanceMatrices_;
    std::vector<float> cubeInstanceMatrices_;

    // Groupement : Variables liées au Shadow Mapping
    GLuint shadowProgram_ = 0;
    GLuint shadowFBO_ = 0;
    GLuint shadowDepthTex_ = 0;
    const int SHADOW_WIDTH = 2048;
    const int SHADOW_HEIGHT = 2048;
    float lightSpaceMatrix_[16] = {};
    float lightProjection_[16] = {};
    float lightView_[16] = {};
    float far = 500.f;

    // Groupement : Variables liées au SSAO (Screen Space Ambient Occlusion)
    int ssaoKernelSize_ = 64;
    std::vector<core::Vec3F> ssaoKernel_;
    GLuint noiseTexture_ = 0;
    GLuint ssaoFBO_ = 0;
    GLuint ssaoColorBuffer_ = 0;
    GLuint ssaoBlurFBO_ = 0;
    GLuint ssaoBlurBuffer_ = 0;
    GLuint ssaoProgram_ = 0;
    GLuint ssaoBlurProgram_ = 0;
    float ssaoRadius_ = 0.5f;
    float ssaoBias_ = 0.025f;

    // Groupement : Fonctions générales d'initialisation et de gestion
    void SetMouseLook(bool enabled);

    void initGL();

    static GLuint createModelShaderProgram();

    void initPostProcessingShaders();

    static GLuint createProgram(const std::string &vertexSrc, const std::string &fragmentSrc);

    static GLuint compileShader(GLenum type, const std::string &source);

    void createFramebuffer();

    void createScreenQuad();

    void createMultiPassFramebuffers();

    void createBloomFramebuffers();

    void createSkyboxRessources();

    static GLuint loadCubemap(const std::vector<std::string> &path);

    void initBloomShaders();

    void initConvolutionKernels();

    void initCubeResources();

    static GLuint loadTexture2D(const std::string &path, bool flipY);

    void createGBuffer(int width, int height);

    void cleanup();

    void calculateCameraMatrices(float *view, float *proj);

    static void lookAtMatrix(float *m, float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ,
                             float upX, float upY, float upZ);

    static void perspectiveMatrix(float *m, float fov, float aspect, float near, float far);

    static void multiplyMat4(float *out, const float *a, const float *b);

    static void identityMatrix(float *m);

    // Groupement : Fonctions liées au rendu Deferred Shading
    void renderDeferred();

    void renderGeometryPass(float *view, float *proj);

    void renderSkybox(float *view, float *proj);

    // Groupement : Fonctions liées au Shadow Mapping
    void initShadowMapping();

    void checkShadowFBO() const;

    void renderShadowMapping();

    void renderShadowMap();

    void renderDebugShadow();

    // Groupement : Fonctions liées au SSAO
    void initSSAO();

    void createCubeLine();

    void initCubeInstancing();

    void renderSSAO();

    void renderSSAOPass(float *proj);

    void renderDebugSSAO();
};


#endif //FINAL_SCENE_H
