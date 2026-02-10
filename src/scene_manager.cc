//
// Created by forna on 07.02.2026.
//

#include "../include/scene_manager.h"

#include <iostream>
// ==================== ModelInstance ====================
void ModelInstance::updateModelMatrix() {
    SceneManager::createTransformMatrix(position, rotation, scale, modelMatrix.data());
}

// ==================== CONSTRUCTEUR/DESTRUCTEUR ====================
SceneManager::SceneManager() = default;

SceneManager::~SceneManager() {
    cleanup();
}

// ==================== GESTION DES MODÈLES ====================
int SceneManager::loadModel(const std::string& path) {
    try {
        auto newModel = std::make_unique<Model>(path);
        int index = models_.size();
        models_.push_back(std::move(newModel));
        return index;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load model: " << path << std::endl;
        std::cerr << "Details: " << e.what() << std::endl;
        return -1;
    }
}

Model* SceneManager::getModel(int modelIndex) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models_.size())) {
        return nullptr;
    }
    return models_[modelIndex].get();
}

const Model* SceneManager::getModel(int modelIndex) const {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models_.size())) {
        return nullptr;
    }
    return models_[modelIndex].get();
}

// ==================== GESTION DES INSTANCES ====================
int SceneManager::addModelInstance(int modelIndex,
                                  const core::Vec3F& position,
                                  const core::Vec3F& rotation,
                                  const core::Vec3F& scale) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(models_.size())) {
        std::cerr << "ERROR: Invalid model index: " << modelIndex << std::endl;
        return -1;
    }

    ModelInstance instance;
    instance.model = models_[modelIndex].get();
    instance.position = position;
    instance.rotation = rotation;
    instance.scale = scale;
    instance.modelIndex = modelIndex;
    instance.updateModelMatrix();

    int index = instances_.size();
    instances_.push_back(instance);
    return index;
}

void SceneManager::removeModelInstance(int instanceIndex) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }
    instances_.erase(instances_.begin() + instanceIndex);
}

ModelInstance* SceneManager::getInstance(int instanceIndex) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return nullptr;
    }
    return &instances_[instanceIndex];
}

const ModelInstance* SceneManager::getInstance(int instanceIndex) const {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return nullptr;
    }
    return &instances_[instanceIndex];
}

int SceneManager::getVisibleInstanceCount() const {
    int count = 0;
    for (const auto& instance : instances_) {
        if (instance.visible) {
            count++;
        }
    }
    return count;
}

// ==================== TRANSFORMATION DES INSTANCES ====================
void SceneManager::setInstancePosition(int instanceIndex, const core::Vec3F& position) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }
    instances_[instanceIndex].position = position;
    instances_[instanceIndex].updateModelMatrix();
}

const core::Vec3F& SceneManager::getInstancePosition(int instanceIndex) const {
    static constexpr core::Vec3F defaultPos{0.0f, 0.0f, 0.0f};
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return defaultPos;
    }
    return instances_[instanceIndex].position;
}

void SceneManager::setInstanceRotation(int instanceIndex, const core::Vec3F& rotation) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }
    instances_[instanceIndex].rotation = rotation;
    instances_[instanceIndex].updateModelMatrix();
}

void SceneManager::setInstanceScale(int instanceIndex, const core::Vec3F& scale) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }
    instances_[instanceIndex].scale = scale;
    instances_[instanceIndex].updateModelMatrix();
}

void SceneManager::setInstanceVisible(int instanceIndex, bool visible) {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }
    instances_[instanceIndex].visible = visible;
}

// ==================== MATRICES DE TRANSFORMATION ====================
void SceneManager::getInstanceModelMatrix(int instanceIndex, float* outMatrix) const {
    if (outMatrix == nullptr) {
        return;
    }

    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        // Retourner la matrice identité en cas d'erreur
        identityMatrix(outMatrix);
        return;
    }

    std::memcpy(outMatrix, instances_[instanceIndex].modelMatrix.data(), sizeof(float) * 16);
}

void SceneManager::updateAllMatrices() {
    for (auto& instance : instances_) {
        instance.updateModelMatrix();
    }
}

// ==================== RENDU ====================
void SceneManager::drawInstance(int instanceIndex, GLuint shaderProgram) const {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }

    const ModelInstance& instance = instances_[instanceIndex];
    if (!instance.visible || instance.model == nullptr) {
        return;
    }

    instance.model->Draw(shaderProgram);
}

void SceneManager::drawAllInstances(GLuint shaderProgram) const {
    for (int i = 0; i < static_cast<int>(instances_.size()); ++i) {
        drawInstance(i, shaderProgram);
    }
}

void SceneManager::drawInstanceRaw(int instanceIndex, GLuint shaderProgram) const {
    if (instanceIndex < 0 || instanceIndex >= static_cast<int>(instances_.size())) {
        return;
    }

    const ModelInstance& instance = instances_[instanceIndex];
    if (!instance.visible || instance.model == nullptr) {
        return;
    }

    instance.model->Draw(shaderProgram);
}

// ==================== BOUNDS ====================
core::Vec3F SceneManager::getSceneCenter() const {
    core::Vec3F center{0.0f, 0.0f, 0.0f};
    if (instances_.empty()) {
        return center;
    }

    for (const auto& instance : instances_) {
        center = center + instance.position;
    }

    center = center / static_cast<float>(instances_.size());
    return center;
}

float SceneManager::getSceneRadius() const {
    if (instances_.empty()) {
        return 1.0f;
    }

    core::Vec3F center = getSceneCenter();
    float maxDist = 0.0f;

    for (const auto& instance : instances_) {
        core::Vec3F diff = instance.position - center;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (instance.model != nullptr) {
            dist += instance.model->GetBoundingRadius();
        }
        maxDist = std::max(maxDist, dist);
    }

    return maxDist > 0.0f ? maxDist : 1.0f;
}

void SceneManager::getSceneBounds(core::Vec3F& outMin, core::Vec3F& outMax) const {
    outMin = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    outMax = {-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

    for (const auto& instance : instances_) {
        if (instance.model != nullptr) {
            core::Vec3F min = instance.model->aabbMin;
            core::Vec3F max = instance.model->aabbMax;

            // Appliquer la transformation
            min = min * instance.scale + instance.position;
            max = max * instance.scale + instance.position;

            outMin.x = std::min(outMin.x, min.x);
            outMin.y = std::min(outMin.y, min.y);
            outMin.z = std::min(outMin.z, min.z);

            outMax.x = std::max(outMax.x, max.x);
            outMax.y = std::max(outMax.y, max.y);
            outMax.z = std::max(outMax.z, max.z);
        }
    }
}

void SceneManager::cleanup() {
    instances_.clear();
    models_.clear();
}

// ==================== MÉTHODES PUBLIQUES STATIQUES ====================
void SceneManager::createTransformMatrix(const core::Vec3F& position,
                                        const core::Vec3F& rotation,
                                        const core::Vec3F& scale,
                                        float* outMatrix) {
    float translationMat[16], rotXMat[16], rotYMat[16], rotZMat[16], scaleMat[16];
    float tempMat[16];

    // Créer les matrices individuelles
    translationMatrix(translationMat, position.x, position.y, position.z);
    rotationMatrixX(rotXMat, rotation.x);
    rotationMatrixY(rotYMat, rotation.y);
    rotationMatrixZ(rotZMat, rotation.z);
    scaleMatrix(scaleMat, scale.x, scale.y, scale.z);

    // Combiner les rotations (Z * Y * X)
    multiplyMat4(tempMat, rotYMat, rotXMat);
    float rotCombined[16];
    multiplyMat4(rotCombined, rotZMat, tempMat);

    // Appliquer l'échelle
    multiplyMat4(tempMat, rotCombined, scaleMat);

    // Appliquer la translation
    multiplyMat4(outMatrix, translationMat, tempMat);
}

// ==================== MÉTHODES PRIVÉES ====================
void SceneManager::multiplyMat4(float* out, const float* a, const float* b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                out[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

void SceneManager::translationMatrix(float* out, float x, float y, float z) {
    identityMatrix(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void SceneManager::rotationMatrixX(float* out, float angle) {
    identityMatrix(out);
    float c = std::cos(angle);
    float s = std::sin(angle);
    out[5] = c;
    out[6] = s;
    out[9] = -s;
    out[10] = c;
}

void SceneManager::rotationMatrixY(float* out, float angle) {
    identityMatrix(out);
    float c = std::cos(angle);
    float s = std::sin(angle);
    out[0] = c;
    out[2] = -s;
    out[8] = s;
    out[10] = c;
}

void SceneManager::rotationMatrixZ(float* out, float angle) {
    identityMatrix(out);
    float c = std::cos(angle);
    float s = std::sin(angle);
    out[0] = c;
    out[1] = s;
    out[4] = -s;
    out[5] = c;
}

void SceneManager::scaleMatrix(float* out, float x, float y, float z) {
    identityMatrix(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}

void SceneManager::identityMatrix(float* out) {
    for (int i = 0; i < 16; ++i) {
        out[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

