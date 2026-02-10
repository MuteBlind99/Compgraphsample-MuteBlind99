//
// Created by forna on 07.02.2026.
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
#include <filesystem>
#include "camera.h"
#include "deferred_renderer.h"
#include "light_manager.h"
#include "model_loader.h"
#include "scene_manager.h"
#include "shadow_renderer.h"
#include "ssao_renderer.h"
#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

#include "stb_image.h"

// ==================== ÉNUMÉRATION DES MODES ====================
enum class RenderMode {
    FORWARD,
    DEFERRED,
    DEFERRED_SSAO,
    SHADOW_MAPPING,
    DEFERRED_SHADOWS
};
struct RenderSettings {
    bool shadows = true;
    bool ssao = true;
    RenderMode mode = RenderMode::DEFERRED_SHADOWS;
};

class RenderPipeline {
public:
    void render(float dt);
private:
    void shadowPass();
    void geometryPass();
    void ssaoPass(const float* proj);
    void lightingPass();
};
// ==================== CLASSE PRINCIPALE ====================
class MainScene : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "MainScene::Begin() - Initialisation" << std::endl;
        init();
    }

    void Update(float dt) override {
        update(dt);
    }

    void FixedUpdate() override {
    }

    void End() override {
        std::cout << "MainScene::End() - Nettoyage" << std::endl;
        cleanup();
    }

    void PreDraw() override {
    }

    void Draw() override {
        render(0.016f); // ~60 FPS
    }

    void PostDraw() override {
    }

private:
    // ==================== VARIABLES MEMBRES ====================
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint modelProgram_ = 0;
    Camera g_camera;
    LightManager g_lightManager;
    DeferredRenderer g_deferredRenderer;
    ShadowRenderer g_shadowRenderer;
    SSAORenderer g_ssaoRenderer;
    SceneManager g_sceneManager;

    // Paramètres de rendu
    RenderMode g_renderMode = RenderMode::DEFERRED_SSAO;
    bool enableSSAO = true;
    bool enableShadows = true;
    float ssaoRadius = 0.5f;
    float ssaoBias = 0.025f;
    float deltaTimeAccum = 0.0f;
    const int W = 1600;
    const int H = 1024;

    // ==================== INITIALISATION ====================
    void init() {
        namespace fs = std::filesystem;
        // Initialiser la caméra
        g_camera.initialize(W, H, 45.0f);
        g_camera.setPosition({0.0f, 1.0f, 5.0f}); // Plus près
        g_camera.setYaw(0.0f); // Regarde vers -Z
        g_camera.setPitch(0.0f); // Horizontal

        // Initialiser le gestionnaire de lumière
        g_lightManager.initialize(W, H);
        g_lightManager.initializeShadowMapping(2048, 2048);
        g_lightManager.setShadowViewDistance(50.0f);

        // Ajouter une lumière directionnelle
        DirectionalLight sunLight;
        sunLight.position = {10.0f, 20.0f, 10.0f};
        sunLight.direction = {-1.0f, -1.0f, -1.0f};
        sunLight.direction = sunLight.direction.Normalize();
        sunLight.color = {1.0f, 1.0f, 1.0f};
        sunLight.intensity = 1.0f;

        int mainLightIndex = g_lightManager.addDirectionalLight(sunLight);
        g_lightManager.setMainDirectionalLight(mainLightIndex);

        // Initialiser le deferred renderer
        g_deferredRenderer.initialize(W, H);
        g_deferredRenderer.loadDefaultShaders();

        // Initialiser le SSAO renderer
        g_ssaoRenderer.initialize(W, H, 64, 0.5f, 0.025f);
        g_ssaoRenderer.loadDefaultShaders();

        // Initialiser le renderer d'ombres
        g_shadowRenderer.initialize(g_lightManager);
        g_shadowRenderer.loadDefaultShaders();


        // Charger les modèles
        // Exemple : charger un modèle de cube ou autre
        std::cout << "CWD = " << fs::current_path() << std::endl;

        const std::string modelPath = "data/model/nanosuit2/nanosuit.obj";
        std::cout << "Model path = " << modelPath
                << " exists=" << fs::exists(modelPath) << std::endl;

        checkModelLoading();

        int modelIndex = g_sceneManager.loadModel(modelPath);
        std::cout << "loadModel returned = " << modelIndex << std::endl;

        if (modelIndex >= 0) {
            g_sceneManager.addModelInstance(modelIndex, {0.0f, 0.0f, 0.0f});
            std::cout << "Model instance added successfully!" << std::endl;

            // Vérifier les détails du modèle
            const Model* model = g_sceneManager.getModel(modelIndex);
            if (model) {
                std::cout << "Model loaded with " << model << " meshes" << std::endl;
            }
        } else {
            std::cerr << "ERROR: Failed to load model! Model index: " << modelIndex << std::endl;
        std::cout << "Scene initialized successfully!" << std::endl;
            // Alternative: créer un cube de test
            createTestCube();
            std::cout << "Created test cube as fallback" << std::endl;
        }

        // // Position initiale de la caméra - S'assurer qu'elle voit l'origine
        // g_camera.setPosition({0.0f, 5.0f, 10.0f});
        // g_camera.setYaw(-90.0f); // Regarde vers -Z
        // g_camera.setPitch(0.0f);

        std::cout << "Camera position: " << g_camera.getPosition().x << ", "
                << g_camera.getPosition().y << ", " << g_camera.getPosition().z << std::endl;
        std::cout << "Camera front: " << g_camera.getFront().x << ", "
                << g_camera.getFront().y << ", " << g_camera.getFront().z << std::endl;
        if (!g_deferredRenderer.loadDefaultShaders()) {
            std::cerr << "Deferred shaders failed to load/compile\n";
        }
        // Initialiser le quad
        initFullscreenQuad();

        std::cout << "Scene initialized successfully!" << std::endl;
    }

    void createModelShader() {
        const std::string vs = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoord;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;

            out vec3 FragPos;
            out vec3 Normal;
            out vec2 TexCoord;

            void main() {
                FragPos = vec3(uModel * vec4(aPos, 1.0));
                Normal = mat3(transpose(inverse(uModel))) * aNormal;
                TexCoord = aTexCoord;
                gl_Position = uProjection * uView * vec4(FragPos, 1.0);
            }
        )";

        const std::string fs = R"(
            #version 330 core
            layout(location = 0) out vec3 gPosition;
            layout(location = 1) out vec3 gNormal;
            layout(location = 2) out vec4 gAlbedo;

            in vec3 FragPos;
            in vec3 Normal;
            in vec2 TexCoord;

            uniform sampler2D texture_diffuse1;

            void main() {
                gPosition = FragPos;
                gNormal = normalize(Normal);

                // Utilisez une couleur par défaut si la texture n'est pas chargée
                vec4 texColor = texture(texture_diffuse1, TexCoord);
                if(texColor.a < 0.1) {
                    gAlbedo = vec4(0.8, 0.6, 0.4, 1.0); // Couleur de fallback
                } else {
                    gAlbedo = texColor;
                }
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vs);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fs);

        modelProgram_ = glCreateProgram();
        glAttachShader(modelProgram_, vertexShader);
        glAttachShader(modelProgram_, fragmentShader);
        glLinkProgram(modelProgram_);

        // Vérifier les erreurs
        GLint success;
        glGetProgramiv(modelProgram_, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(modelProgram_, 512, nullptr, infoLog);
            std::cerr << "ERROR: Model program linking failed:\n" << infoLog << std::endl;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    static GLuint compileShader(const GLenum type, const std::string &source) {
        GLuint shader = glCreateShader(type);
        const char *src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "ERROR: Shader compilation failed:\n" << infoLog << std::endl;
            return 0;
        }
        return shader;
    }

    // ==================== RENDU SHADOW PASS ====================
    void renderShadowPass() {
        if (!enableShadows) {
            return;
        }

        // Calculer les matrices de lumière basées sur la scène
        core::Vec3F sceneCenter = g_sceneManager.getSceneCenter();
        float sceneRadius = g_sceneManager.getSceneRadius();
        g_lightManager.calculateLightMatrices(sceneCenter, sceneRadius);

        // Commencer le rendu de la carte d'ombres
        g_shadowRenderer.beginShadowPass();
        g_shadowRenderer.bindShader();

        // Obtenir les matrices de lumière
        float lightProj[16], lightView[16];
        g_lightManager.getLightProjectionMatrix(lightProj);
        g_lightManager.getLightViewMatrix(lightView);

        // Envoyer les matrices au shader
        g_shadowRenderer.setProjectionMatrix(lightProj);
        g_shadowRenderer.setViewMatrix(lightView);

        // Rendu de la géométrie depuis la vue de la lumière
        for (int i = 0; i < g_sceneManager.getInstanceCount(); ++i) {
            const ModelInstance *instance = g_sceneManager.getInstance(i);
            if (instance != nullptr && instance->visible && instance->model != nullptr) {
                float modelMatrix[16];
                g_sceneManager.getInstanceModelMatrix(i, modelMatrix);

                g_shadowRenderer.setModelMatrix(modelMatrix);
                g_sceneManager.drawInstance(i, g_shadowRenderer.getShaderProgram());
            }
        }

        ShadowRenderer::unbindShader();
        g_shadowRenderer.endShadowPass();
    }

    // ==================== RENDU GEOMETRY PASS (DEFERRED) ====================
    void renderGeometryPass() {
        // Passe géométrique : remplir les G-Buffers
        g_deferredRenderer.beginGeometryPass();
        g_deferredRenderer.bindGeometryShader();

        float view[16], proj[16];
        g_camera.getViewMatrix(view);
        g_camera.getProjectionMatrix(proj);

        g_deferredRenderer.setViewMatrix(view);
        g_deferredRenderer.setProjectionMatrix(proj);

        // --- CHANGED (FIX): garder un state GL "normal" (depth + cull) ---
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        for (int i = 0; i < g_sceneManager.getInstanceCount(); ++i) {
            const ModelInstance* instance = g_sceneManager.getInstance(i);
            if (instance != nullptr && instance->visible && instance->model != nullptr) {
                float modelMatrix[16];
                g_sceneManager.getInstanceModelMatrix(i, modelMatrix);

                g_deferredRenderer.setModelMatrix(modelMatrix);
                g_sceneManager.drawInstance(i, g_deferredRenderer.getGeometryShader());
            }
        }

        DeferredRenderer::unbindShader();
        g_deferredRenderer.endGeometryPass(); // FIX: instance
    }

    // ==================== RENDU SSAO ====================
    void renderSSAO(const float *projMatrix) {
        if (!enableSSAO) {
            return;
        }

        // Calculer le SSAO
        g_ssaoRenderer.compute(g_deferredRenderer.getPositionTexture(),
                               g_deferredRenderer.getNormalTexture(),
                               projMatrix, quadVAO, quadVBO);

        // Appliquer le blur
        g_ssaoRenderer.blur(quadVAO, quadVBO);
    }

    // ==================== RENDU LIGHTING PASS (DEFERRED) ====================
    void renderLightingPass() {
        g_deferredRenderer.beginLightingPass();
        g_deferredRenderer.bindLightingShader();

        g_deferredRenderer.setCameraPosition(g_camera.getPosition());

        const Light* mainLight = g_lightManager.getMainDirectionalLight();
        if (mainLight != nullptr) {
            g_deferredRenderer.setDirectionalLight(mainLight->direction, mainLight->color, mainLight->intensity);
        }

        // --- CHANGED (FIX): SSAO optionnel (unit 3 + uniform uUseSSAO) ---
        const GLuint sh = g_deferredRenderer.getLightingShader();
        const bool useSSAO = enableSSAO && (g_renderMode == RenderMode::DEFERRED_SSAO || g_renderMode == RenderMode::DEFERRED_SHADOWS);

        GLint useLoc = glGetUniformLocation(sh, "uUseSSAO");
        if (useLoc >= 0) glUniform1i(useLoc, useSSAO ? 1 : 0);

        if (useSSAO) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, g_ssaoRenderer.getBlurredSSAOTexture());
            GLint ssaoLoc = glGetUniformLocation(sh, "gSSAO");
            if (ssaoLoc >= 0) glUniform1i(ssaoLoc, 3);
        }

        initFullscreenQuad();

        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);

        DeferredRenderer::unbindShader();
        g_deferredRenderer.endLightingPass();
    }


    // ==================== RENDU FORWARD PASS ====================
    void renderForwardPass() {
        // Rendu forward classique (sans déferred)
        float view[16], proj[16];
        g_camera.getViewMatrix(view);
        g_camera.getProjectionMatrix(proj);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        static GLuint forwardShader = 0;
        if (forwardShader == 0) {
            // Créer un shader forward simple
            const std::string vsSource = R"(
            #version 330 core
            layout(location = 0) in vec3 aPosition;
            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            void main() {
                gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
            }
        )";

            std::string fsSource = R"(
            #version 330 core
            out vec4 FragColor;
            void main() {
                FragColor = vec4(0.7, 0.7, 0.7, 1.0); // Couleur grise par défaut
            }
        )";
            GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
            GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);

            forwardShader = glCreateProgram();
            glAttachShader(forwardShader, vs);
            glAttachShader(forwardShader, fs);
            glLinkProgram(forwardShader);

            GLint success;
            glGetProgramiv(forwardShader, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetProgramInfoLog(forwardShader, 512, nullptr, infoLog);
                std::cerr << "ERROR: Forward shader link failed:\n" << infoLog << std::endl;
                glDeleteProgram(forwardShader);
                forwardShader = 0;
            }

            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        if (forwardShader == 0) {
            return;
        }

        glUseProgram(forwardShader);
        glUniformMatrix4fv(glGetUniformLocation(forwardShader, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(forwardShader, "uProjection"), 1, GL_FALSE, proj);

        for (int i = 0; i < g_sceneManager.getInstanceCount(); ++i) {
            const ModelInstance* instance = g_sceneManager.getInstance(i);
            if (instance != nullptr && instance->visible && instance->model != nullptr) {
                float modelMatrix[16];
                g_sceneManager.getInstanceModelMatrix(i, modelMatrix);
                glUniformMatrix4fv(glGetUniformLocation(forwardShader, "uModel"), 1, GL_FALSE, modelMatrix);

                g_sceneManager.drawInstance(i, forwardShader);
            }
        }

        glUseProgram(0);
    }

    // ==================== RENDU SHADOW MAP DEBUG ====================
    void renderShadowDebug() const {
        // Afficher la shadow map (debug)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // Créer un quad pour afficher la shadow map
        static GLuint debugQuadVAO = 0;
        if (debugQuadVAO == 0) {
            float quadVertices[] = {
                -1.0f, 1.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f,
                1.0f, 1.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 1.0f, 0.0f,
            };

            GLuint quadVBO;
            glGenVertexArrays(1, &debugQuadVAO);
            glGenBuffers(1, &quadVBO);

            glBindVertexArray(debugQuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void *>(nullptr));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                                  reinterpret_cast<void *>(2 * sizeof(float)));

            glBindVertexArray(0);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_lightManager.getShadowDepthTexture());

        glBindVertexArray(debugQuadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
    }

    // ==================== MISE À JOUR ====================
    void update(float deltaTime) {
        g_camera.update(deltaTime);
        g_sceneManager.updateAllMatrices();
    }


    void setImGUI();

    // ==================== RENDU PRINCIPAL ====================
    void render(float deltaTime);

    static void identityMatrix(float *m) {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static void multiplyMat4(float* out, const float* a, const float* b) {
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

    // void renderDebugSimple() {
    //     std::cout << "=== DEBUG SIMPLE RENDER ===" << std::endl;
    //
    //     // 1. Nettoyer l'écran avec une couleur claire
    //     glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    //     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //     glEnable(GL_DEPTH_TEST);
    //
    //     // 2. Créer un shader ultra simple
    //     const char *vs = R"(
    //     #version 330 core
    //     layout(location = 0) in vec3 aPos;
    //     uniform mat4 mvp;
    //     void main() {
    //         gl_Position = mvp * vec4(aPos, 1.0);
    //     }
    // )";
    //
    //     const char *fs = R"(
    //     #version 330 core
    //     out vec4 FragColor;
    //     void main() {
    //         FragColor = vec4(1.0, 0.0, 0.0, 1.0); // ROUGE
    //     }
    // )";
    //
    //     static GLuint shader = 0;
    //     if (shader == 0) {
    //         GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    //         glShaderSource(vertex, 1, &vs, NULL);
    //         glCompileShader(vertex);
    //
    //         GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    //         glShaderSource(fragment, 1, &fs, NULL);
    //         glCompileShader(fragment);
    //
    //         shader = glCreateProgram();
    //         glAttachShader(shader, vertex);
    //         glAttachShader(shader, fragment);
    //         glLinkProgram(shader);
    //
    //         // Vérifier les erreurs
    //         GLint success;
    //         glGetProgramiv(shader, GL_LINK_STATUS, &success);
    //         if (!success) {
    //             char infoLog[512];
    //             glGetProgramInfoLog(shader, 512, NULL, infoLog);
    //             std::cout << "Shader error: " << infoLog << std::endl;
    //         }
    //
    //         glDeleteShader(vertex);
    //         glDeleteShader(fragment);
    //         std::cout << "Debug shader created: " << shader << std::endl;
    //     }
    //
    //     // 3. Calculer les matrices
    //     float view[16], proj[16];
    //     g_camera.getViewMatrix(view);
    //     g_camera.getProjectionMatrix(proj);
    //
    //     std::cout << "Camera matrices calculated" << std::endl;
    //
    //     // 4. Rendre TOUS les modèles
    //     glUseProgram(shader);
    //
    //     for (int i = 0; i < g_sceneManager.getInstanceCount(); ++i) {
    //         const ModelInstance *instance = g_sceneManager.getInstance(i);
    //         if (instance && instance->model) {
    //             std::cout << "Drawing instance " << i << std::endl;
    //
    //             // Obtenir la matrice modèle
    //             float model[16];
    //             g_sceneManager.getInstanceModelMatrix(i, model);
    //
    //             // Multiplier les matrices: mvp = proj * view * model
    //             float mvp[16];
    //             identityMatrix(mvp);
    //
    //             // Simple multiplication (vous devez avoir multiplyMat4)
    //             float viewProj[16];
    //             multiplyMat4(viewProj, proj, view);
    //             multiplyMat4(mvp, viewProj, model);
    //
    //             // Passer la matrice
    //             GLint loc = glGetUniformLocation(shader, "mvp");
    //             if (loc != -1) {
    //                 glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);
    //             } else {
    //                 std::cout << "Uniform 'mvp' not found!" << std::endl;
    //             }
    //
    //             // 5. Désactiver le culling pour voir tout
    //             glDisable(GL_CULL_FACE);
    //
    //             // 6. Dessiner en wireframe pour mieux voir
    //             glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //
    //             // 7. Dessiner le modèle
    //             instance->model->Draw(shader);
    //
    //             // Restaurer
    //             glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    //             glEnable(GL_CULL_FACE);
    //         }
    //     }
    //
    //     glUseProgram(0);
    //     std::cout << "=== DEBUG RENDER COMPLETE ===" << std::endl;
    // }

    void createTestCube() {
        // Supprimez les instances existantes
        g_sceneManager.cleanup();

        // Créez un modèle de cube simple programmatiquement
        // ou utilisez un cube prédéfini

        std::cout << "Creating test cube..." << std::endl;

        // Créez un VBO/VAO simple pour un cube
        std::cout << "Creating test cube..." << std::endl;

        // Créez un VBO/VAO simple pour un cube
        float vertices[] = {
            -0.5f, -0.5f, -0.5f,
             0.5f, -0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
             0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,
            // ... (toutes les faces du cube)
        };

        // Créer un VAO/VBO pour le cube
        GLuint cubeVAO, cubeVBO;
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);

        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glBindVertexArray(0);

        std::cout << "Test cube VAO created: " << cubeVAO << std::endl;

        // Vous pouvez maintenant utiliser drawCube dans votre renderDebugSimple
    }

    static void renderSingleTriangle() {
        static GLuint shader = 0;
        static GLuint VAO = 0;

        if (shader == 0) {
            // Créer et compiler le vertex shader
            const char *vsSource = R"(#version 330 core
void main() {
    const vec2 positions[3] = vec2[3](
        vec2(-0.5, -0.5),
        vec2(0.5, -0.5),
        vec2(0.0, 0.5)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";

            const char *fsSource = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Rouge
})";

            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vsSource, NULL);
            glCompileShader(vertexShader);

            // Vérifier la compilation
            GLint success;
            glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
                std::cout << "Vertex shader compilation failed: " << infoLog << std::endl;
            }

            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fsSource, NULL);
            glCompileShader(fragmentShader);

            glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
                std::cout << "Fragment shader compilation failed: " << infoLog << std::endl;
            }

            shader = glCreateProgram();
            glAttachShader(shader, vertexShader);
            glAttachShader(shader, fragmentShader);
            glLinkProgram(shader);

            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetProgramInfoLog(shader, 512, NULL, infoLog);
                std::cout << "Shader program linking failed: " << infoLog << std::endl;
            }

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            std::cout << "Triangle shader created: " << shader << std::endl;

            // Pour un triangle simple avec gl_VertexID, on n'a pas besoin de VAO/VBO
            // mais OpenGL moderne nécessite un VAO valide
            glGenVertexArrays(1, &VAO);
            glBindVertexArray(VAO);
            // Pas besoin de VBO car on utilise gl_VertexID
            glBindVertexArray(0);
        }

        // Nettoyer l'écran
        glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Désactiver le depth test pour le triangle
        glDisable(GL_DEPTH_TEST);

        // Utiliser le shader
        glUseProgram(shader);

        // Binder le VAO (nécessaire même s'il est vide)
        glBindVertexArray(VAO);

        // Dessiner le triangle
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Nettoyer
        glBindVertexArray(0);
        glUseProgram(0);
        glEnable(GL_DEPTH_TEST);

        std::cout << "Triangle drawn" << std::endl;
    }

    void renderModelTest() {
        glViewport(0, 0, W, H);
        // Créer un shader simple pour le modèle
        const char *vs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
})";

        const char *fs = R"(#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.8, 0.6, 0.4, 1.0); // Couleur bronze
})";

        static GLuint modelShader = 0;
        if (modelShader == 0) {
            // Compiler le vertex shader
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vs, NULL);
            glCompileShader(vertexShader);

            // Vérifier la compilation
            GLint success;
            glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
                std::cout << "Vertex shader compilation failed: " << infoLog << std::endl;
            }

            // Compiler le fragment shader
            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fs, NULL);
            glCompileShader(fragmentShader);

            glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
                std::cout << "Fragment shader compilation failed: " << infoLog << std::endl;
            }

            // Créer le programme shader
            modelShader = glCreateProgram();
            glAttachShader(modelShader, vertexShader);
            glAttachShader(modelShader, fragmentShader);
            glLinkProgram(modelShader);

            glGetProgramiv(modelShader, GL_LINK_STATUS, &success);
            if (!success) {
                char infoLog[512];
                glGetProgramInfoLog(modelShader, 512, NULL, infoLog);
                std::cout << "Shader program linking failed: " << infoLog << std::endl;
            }

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            std::cout << "Model test shader created: " << modelShader << std::endl;
        }

        // Nettoyer l'écran
        glClearColor(0.3f, 0.4f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glUseProgram(modelShader);

        // Calculer les matrices
        float view[16], proj[16];
        g_camera.setPosition({0.0f, 2.0f, 5.0f}); // Plus bas et plus près
        g_camera.getViewMatrix(view);
        g_camera.getProjectionMatrix(proj);

        // Afficher les informations de debug
        std::cout << "=== RENDER MODEL TEST ===" << std::endl;
        std::cout << "Instance count: " << g_sceneManager.getInstanceCount() << std::endl;
glDisable(GL_DEPTH_TEST);
        // Pour chaque instance
        for (int i = 0; i < g_sceneManager.getInstanceCount(); ++i) {
            const ModelInstance *instance = g_sceneManager.getInstance(i);
            if (instance && instance->model) {
                std::cout << "Drawing model instance " << i << std::endl;
                std::cout << "Model pointer: " << instance->model << std::endl;

                float model[16];
                identityMatrix(model);
                // Translation
                model[12] = 0.0f; // X
                model[13] = 0.0f; // Y
                model[14] = -2.0f; // Z - déplacer vers la caméra

                float scale = 0.1f; // Réduire l'échelle
                float scaledModel[16];
                identityMatrix(scaledModel);
                scaledModel[0] = scale;  // X scale
                scaledModel[5] = scale;  // Y scale
                scaledModel[10] = scale; // Z scale

                // Multiplier avec la matrice de position
                multiplyMat4(model, scaledModel, model); // model = scale * position
                g_sceneManager.getInstanceModelMatrix(i, model);


                // Calculer MVP
                float viewProj[16], mvp[16];
                multiplyMat4(viewProj, proj, view);
                multiplyMat4(mvp, viewProj, model);

                // Passer la matrice au shader
                GLint mvpLoc = glGetUniformLocation(modelShader, "uMVP");
                if (mvpLoc != -1) {
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);
                    std::cout << "MVP matrix sent to shader" << std::endl;
                } else {
                    std::cout << "ERROR: uMVP uniform not found in shader!" << std::endl;
                }

                // Dessiner en wireframe pour voir la géométrie
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDisable(GL_CULL_FACE);

                // Dessiner le modèle
                std::cout << "Calling model->Draw()..." << std::endl;
                instance->model->Draw(modelShader);
                std::cout << "Model drawn" << std::endl;

                // Restaurer
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glEnable(GL_CULL_FACE);
            } else {
                std::cout << "Instance " << i << " is null or has null model" << std::endl;
            }
        }
        glEnable(GL_DEPTH_TEST);
        glUseProgram(0);
        std::cout << "=== RENDER MODEL TEST COMPLETE ===" << std::endl;
    }

    static void checkModelLoading() {
        std::cout << "\n=== MODEL LOADING CHECK ===" << std::endl;

        // Vérifier le chemin
        const std::string modelPath = "data/model/nanosuit2/nanosuit.obj";
        std::filesystem::path fullPath = std::filesystem::absolute(modelPath);
        std::cout << "Model path: " << modelPath << std::endl;
        std::cout << "Full path: " << fullPath << std::endl;
        std::cout << "Exists: " << std::filesystem::exists(fullPath) << std::endl;

        // Vérifier si le dossier existe
        std::filesystem::path parentPath = fullPath.parent_path();
        std::cout << "Parent path: " << parentPath << std::endl;
        std::cout << "Parent exists: " << std::filesystem::exists(parentPath) << std::endl;

        // Lister les fichiers dans le dossier
        if (std::filesystem::exists(parentPath)) {
            std::cout << "Files in directory:" << std::endl;
            for (const auto &entry: std::filesystem::directory_iterator(parentPath)) {
                std::cout << "  " << entry.path().filename() << std::endl;
            }
        }

        std::cout << "=== END CHECK ===\n" << std::endl;
    }
    void initFullscreenQuad() {
        if (quadVAO == 0) {
            float quadVertices[] = {
                // positions   // texCoords
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                 1.0f,  1.0f,  1.0f, 1.0f,
                 1.0f, -1.0f,  1.0f, 0.0f,
            };

            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glBindVertexArray(0);
        }
    }
    // ==================== NETTOYAGE ====================
    void cleanup() {
        g_deferredRenderer.cleanup();
        g_ssaoRenderer.cleanup();
        g_shadowRenderer.cleanup();
        g_lightManager.cleanup();
        g_sceneManager.cleanup();
    }
};

void MainScene::setImGUI() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Renderer Settings");

    // Mode de rendu
    ImGui::Text("Render Mode");
    int renderModeInt = static_cast<int>(g_renderMode);
    if (ImGui::RadioButton("Forward Rendering", &renderModeInt, static_cast<int>(RenderMode::FORWARD))) {
        g_renderMode = static_cast<RenderMode>(renderModeInt);
    }
    if (ImGui::RadioButton("Deferred Rendering", &renderModeInt, static_cast<int>(RenderMode::DEFERRED))) {
        g_renderMode = static_cast<RenderMode>(renderModeInt);
    }
    if (ImGui::RadioButton("Deferred + SSAO", &renderModeInt, static_cast<int>(RenderMode::DEFERRED_SSAO))) {
        g_renderMode = static_cast<RenderMode>(renderModeInt);
    }
    if (ImGui::RadioButton("Shadow Mapping (Debug)", &renderModeInt,
                           static_cast<int>(RenderMode::SHADOW_MAPPING))) {
        g_renderMode = static_cast<RenderMode>(renderModeInt);
    }
    if (ImGui::RadioButton("Deferred + Shadows + SSAO", &renderModeInt,
                           static_cast<int>(RenderMode::DEFERRED_SHADOWS))) {
        g_renderMode = static_cast<RenderMode>(renderModeInt);
    }


    ImGui::Separator();

    // Paramètres de shadow mapping
    ImGui::Text("Shadow Mapping");
    ImGui::Checkbox("Enable Shadows", &enableShadows);

    ImGui::Separator();

    // Paramètres SSAO
    if (g_renderMode == RenderMode::DEFERRED_SSAO || g_renderMode == RenderMode::DEFERRED_SHADOWS) {
        ImGui::Text("SSAO Settings");
        ImGui::Checkbox("Enable SSAO##ssao", &enableSSAO);

        if (enableSSAO) {
            ImGui::SliderFloat("SSAO Radius", &ssaoRadius, 0.0f, 2.0f);
            ImGui::SliderFloat("SSAO Bias", &ssaoBias, 0.0f, 0.1f);

            g_ssaoRenderer.setRadius(ssaoRadius);
            g_ssaoRenderer.setBias(ssaoBias);
        }

        ImGui::Separator();
    }

    // Informations de caméra
    ImGui::Text("Camera");
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", g_camera.getPosition().x, g_camera.getPosition().y,
                g_camera.getPosition().z);
    ImGui::Text("FOV: %.1f°", g_camera.getFOV());

    ImGui::Separator();

    // Statistiques de scène
    ImGui::Text("Scene Statistics");
    ImGui::Text("Total Instances: %d", g_sceneManager.getInstanceCount());
    ImGui::Text("Visible Instances: %d", g_sceneManager.getVisibleInstanceCount());
    ImGui::Text("Model Count: %d", g_sceneManager.getModelCount());

    ImGui::Separator();

    // Debug info
    if (g_sceneManager.getInstanceCount() == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠ No models loaded!");
        ImGui::Text("Check model path:");
        ImGui::TextWrapped("data/model/nanosuit2/nanosuit.obj");
    }
    // Dans la fenêtre ImGui
    ImGui::Separator();
    ImGui::Text("Debug Options");
    static bool showSimpleTriangle = false;
    static bool showModelTest = true;
    static bool showWireframe = true;

    ImGui::Checkbox("Show Simple Triangle", &showSimpleTriangle);
    ImGui::Checkbox("Show Model Test", &showModelTest);
    ImGui::Checkbox("Wireframe Mode", &showWireframe);

    if (ImGui::Button("Reload Model")) {
        g_sceneManager.cleanup();
        //initModels(); // Fonction à créer pour recharger
    }
    ImGui::End();
}

void MainScene::render(float deltaTime) {
    update(deltaTime);
    // DEBUG: Vérifier l'état
    std::cout << "Render Mode: " << static_cast<int>(g_renderMode) << std::endl;
    std::cout << "Instance count: " << g_sceneManager.getInstanceCount() << std::endl;
    std::cout << "SSAO enabled: " << enableSSAO << std::endl;

    std::cout << "=== RENDER START ===" << std::endl;
    std::cout << "Instances: " << g_sceneManager.getInstanceCount() << std::endl;

    // Test simple pour vérifier que le rendu fonctionne
    glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    if (g_sceneManager.getInstanceCount() > 0) {
        const ModelInstance *inst = g_sceneManager.getInstance(0);
        if (inst) {
            std::cout << "First instance visible: " << inst->visible << std::endl;
            std::cout << "First instance model ptr: " << inst->model << std::endl;
        }
    }
    switch (g_renderMode) {
        case RenderMode::FORWARD: {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderForwardPass();
            break;
        }

        case RenderMode::DEFERRED: {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderGeometryPass();

            float proj[16];
            g_camera.getProjectionMatrix(proj);

            renderLightingPass();
            break;
        }

        case RenderMode::DEFERRED_SSAO: {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderGeometryPass();

            float proj[16];
            g_camera.getProjectionMatrix(proj);
            renderSSAO(proj);

            renderLightingPass();
            break;
        }

        case RenderMode::SHADOW_MAPPING: {
            renderShadowPass();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderShadowDebug();
            break;
        }

        case RenderMode::DEFERRED_SHADOWS: {
            if (enableShadows) {
                renderShadowPass();
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderGeometryPass();

            float proj[16];
            g_camera.getProjectionMatrix(proj);
            renderSSAO(proj);

            renderLightingPass();
            break;
        }

        default:
            break;
    }

    // ==================== IMGUI UI ====================
    setImGUI();
}

// ==================== VARIABLE GLOBALE ====================
std::unique_ptr<MainScene> g_mainScene;

// ==================== FONCTION PRINCIPALE ====================
int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "   GPR924 - 3D Scene Renderer" << std::endl;
        std::cout << "   Deferred Rendering + SSAO + Shadows" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 1600;
        config.height = 1024;
        config.title = "GPR924 - 3D Scene";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de la scène
        g_mainScene = std::make_unique<MainScene>();

        // Enregistrement dans le moteur
        common::SystemObserverSubject::AddObserver(g_mainScene.get());
        common::DrawObserverSubject::AddObserver(g_mainScene.get());

        // Lancement du moteur
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_mainScene.get());
        common::DrawObserverSubject::RemoveObserver(g_mainScene.get());
        g_mainScene.reset();

        std::cout << "\n✓ Application terminated successfully!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_mainScene) {
            common::SystemObserverSubject::RemoveObserver(g_mainScene.get());
            common::DrawObserverSubject::RemoveObserver(g_mainScene.get());
            g_mainScene.reset();
        }
        return -1;
    }

    return 0;
}
