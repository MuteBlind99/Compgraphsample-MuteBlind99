//
// Created by forna on 07.02.2026.
//

#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H
#include <array>
#include <memory>

#include "model_loader.h"
#include "maths/vec3.h"

// Déclaration anticipée
class SceneManager;

struct ModelInstance {
    Model *model;
    core::Vec3F position;
    core::Vec3F rotation;
    core::Vec3F scale;
    std::array<float, 16> modelMatrix{};
    bool visible;
    int modelIndex;

    ModelInstance()
        : model(nullptr),
          position(0.0f, 0.0f, 0.0f),
          rotation(0.0f, 0.0f, 0.0f),
          scale(1.0f, 1.0f, 1.0f),
          visible(true),
          modelIndex(-1) {
        for (int i = 0; i < 16; ++i) {
            modelMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
    }
    void updateModelMatrix();
};
class SceneManager {
public:
    // ==================== CONSTRUCTEURS ====================
    SceneManager();
    ~SceneManager();

    // Empêcher les copies
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // ==================== GESTION DES MODÈLES ====================
    int loadModel(const std::string& path);
    Model* getModel(int modelIndex);
    [[nodiscard]] const Model* getModel(int modelIndex) const;
    [[nodiscard]] int getModelCount() const { return models_.size(); }

    // ==================== GESTION DES INSTANCES ====================
    int addModelInstance(int modelIndex,
                        const core::Vec3F& position = {0.0f, 0.0f, 0.0f},
                        const core::Vec3F& rotation = {0.0f, 0.0f, 0.0f},
                        const core::Vec3F& scale = {1.0f, 1.0f, 1.0f});
    void removeModelInstance(int instanceIndex);
    ModelInstance* getInstance(int instanceIndex);
    [[nodiscard]] const ModelInstance* getInstance(int instanceIndex) const;
    [[nodiscard]] int getVisibleInstanceCount() const;
    [[nodiscard]] int getInstanceCount() const { return instances_.size(); }

    // ==================== TRANSFORMATION DES INSTANCES ====================
    void setInstancePosition(int instanceIndex, const core::Vec3F& position);
    [[nodiscard]] const core::Vec3F& getInstancePosition(int instanceIndex) const;
    void setInstanceRotation(int instanceIndex, const core::Vec3F& rotation);
    void setInstanceScale(int instanceIndex, const core::Vec3F& scale);
    void setInstanceVisible(int instanceIndex, bool visible);

    // ==================== MATRICES DE TRANSFORMATION ====================
    void getInstanceModelMatrix(int instanceIndex, float* outMatrix) const;
    void updateAllMatrices();

    // ==================== RENDU ====================
    void drawInstance(int instanceIndex, GLuint shaderProgram) const;
    void drawAllInstances(GLuint shaderProgram) const;
    void drawInstanceRaw(int instanceIndex, GLuint shaderProgram) const;

    // ==================== BOUNDS ====================
    [[nodiscard]] core::Vec3F getSceneCenter() const;
    [[nodiscard]] float getSceneRadius() const;
    void getSceneBounds(core::Vec3F& outMin, core::Vec3F& outMax) const;

    // ==================== NETTOYAGE ====================
    void cleanup();

    // ==================== MÉTHODES PUBLIQUES STATIQUES (pour ModelInstance) ====================
    static void createTransformMatrix(const core::Vec3F& position,
                                     const core::Vec3F& rotation,
                                     const core::Vec3F& scale,
                                     float* outMatrix);

private:
    // ==================== DONNÉES ====================
    std::vector<std::unique_ptr<Model>> models_;
    std::vector<ModelInstance> instances_;

    // ==================== MÉTHODES PRIVÉES ====================
    static void multiplyMat4(float* out, const float* a, const float* b);
    static void translationMatrix(float* out, float x, float y, float z);
    static void rotationMatrixX(float* out, float angle);
    static void rotationMatrixY(float* out, float angle);
    static void rotationMatrixZ(float* out, float angle);
    static void scaleMatrix(float* out, float x, float y, float z);
    static void identityMatrix(float* out);
};


#endif //SCENE_MANAGER_H
