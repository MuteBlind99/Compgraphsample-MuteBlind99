//
// Created by forna on 07.02.2026.
//

#include "../include/light_manager.h"
#include <cmath>
#include <cstring>
#include <iostream>
// ==================== CONSTRUCTEUR/DESTRUCTEUR ====================
LightManager::LightManager()
    : mainLightIndex_(-1),
      shadowFramebuffer_(0),
      shadowDepthTexture_(0),
      shadowMapWidth_(2048),
      shadowMapHeight_(2048),
      shadowViewDistance_(50.0f),
      shadowMappingEnabled_(true),
      screenWidth_(800),
      screenHeight_(600) {
    // Initialiser les matrices à l'identité
    for (int i = 0; i < 16; ++i) {
        lightProjectionMatrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        lightViewMatrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        lightSpaceMatrix_[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

LightManager::~LightManager() {
    cleanup();
}
// ==================== INITIALISATION ====================
void LightManager::initialize(int screenWidth, int screenHeight) {
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
}

void LightManager::initializeShadowMapping(int shadowMapWidth, int shadowMapHeight) {
    shadowMapWidth_ = shadowMapWidth;
    shadowMapHeight_ = shadowMapHeight;
    createShadowResources();
}

// ==================== GESTION DES LUMIÈRES ====================
int LightManager::addDirectionalLight(const DirectionalLight& light) {
    auto newLight = std::make_unique<DirectionalLight>(light);
    int index = lights_.size();
    lights_.push_back(std::move(newLight));

    // La première lumière directionnelle devient la lumière principale
    if (mainLightIndex_ == -1) {
        mainLightIndex_ = index;
    }

    return index;
}

int LightManager::addPointLight(const PointLight& light) {
    auto newLight = std::make_unique<PointLight>(light);
    int index = lights_.size();
    lights_.push_back(std::move(newLight));
    return index;
}

int LightManager::addSpotLight(const SpotLight& light) {
    auto newLight = std::make_unique<SpotLight>(light);
    int index = lights_.size();
    lights_.push_back(std::move(newLight));
    return index;
}

void LightManager::removeLight(int index) {
    if (index < 0 || index >= static_cast<int>(lights_.size())) {
        return;
    }

    lights_.erase(lights_.begin() + index);

    // Ajuster l'index de la lumière principale
    if (mainLightIndex_ == index) {
        mainLightIndex_ = -1;
    } else if (mainLightIndex_ > index) {
        mainLightIndex_--;
    }
}

void LightManager::updateLight(int index, const Light& light) const {
    if (index < 0 || index >= static_cast<int>(lights_.size())) {
        return;
    }

    Light* targetLight = lights_[index].get();
    targetLight->position = light.position;
    targetLight->direction = light.direction;
    targetLight->color = light.color;
    targetLight->intensity = light.intensity;
    targetLight->enabled = light.enabled;
}

Light* LightManager::getLight(int index) {
    if (index < 0 || index >= static_cast<int>(lights_.size())) {
        return nullptr;
    }
    return lights_[index].get();
}

const Light* LightManager::getLight(int index) const {
    if (index < 0 || index >= static_cast<int>(lights_.size())) {
        return nullptr;
    }
    return lights_[index].get();
}

// ==================== LUMIÈRE DIRECTIONNELLE PRINCIPALE ====================
void LightManager::setMainDirectionalLight(int index) {
    if (index < 0 || index >= static_cast<int>(lights_.size())) {
        return;
    }

    if (lights_[index]->type == LightType::DIRECTIONAL) {
        mainLightIndex_ = index;
    }
}

Light* LightManager::getMainDirectionalLight() {
    if (mainLightIndex_ < 0 || mainLightIndex_ >= static_cast<int>(lights_.size())) {
        return nullptr;
    }
    return lights_[mainLightIndex_].get();
}

const Light* LightManager::getMainDirectionalLight() const {
    if (mainLightIndex_ < 0 || mainLightIndex_ >= static_cast<int>(lights_.size())) {
        return nullptr;
    }
    return lights_[mainLightIndex_].get();
}

// ==================== MATRICES DE LUMIÈRE ====================
void LightManager::calculateLightMatrices(const core::Vec3F& sceneCenter,
                                         float sceneRadius,
                                         int lightIndex) {
    int useIndex = (lightIndex >= 0) ? lightIndex : mainLightIndex_;
    if (useIndex < 0 || useIndex >= static_cast<int>(lights_.size())) {
        return;
    }

    const Light* light = lights_[useIndex].get();

    // Position de la caméra de lumière
    core::Vec3F lightPos = sceneCenter - (light->direction * sceneRadius);

    // Créer la matrice de vue de la lumière
    lookAtMatrix(lightViewMatrix_.data(), lightPos, sceneCenter, {0.0f, 1.0f, 0.0f});

    // Créer la matrice de projection orthographique
    float distance = shadowViewDistance_;
    orthographicMatrix(lightProjectionMatrix_.data(),
                       -distance, distance,
                       -distance, distance,
                       0.1f, distance * 2.0f);

    // Multiplier pour obtenir la matrice d'espace de lumière
    multiplyMat4(lightSpaceMatrix_.data(),
                lightProjectionMatrix_.data(),
                lightViewMatrix_.data());
}

void LightManager::getLightProjectionMatrix(float* out) const {
    std::memcpy(out, lightProjectionMatrix_.data(), sizeof(float) * 16);
}

void LightManager::getLightViewMatrix(float* out) const {
    std::memcpy(out, lightViewMatrix_.data(), sizeof(float) * 16);
}

void LightManager::getLightSpaceMatrix(float* out) const {
    std::memcpy(out, lightSpaceMatrix_.data(), sizeof(float) * 16);
}

const core::Vec3F& LightManager::getMainLightPosition() const {
    static const core::Vec3F defaultPos{0.0f, 0.0f, 0.0f};
    const Light* light = getMainDirectionalLight();
    return light ? light->position : defaultPos;
}

const core::Vec3F& LightManager::getMainLightDirection() const {
    static constexpr core::Vec3F defaultDir{0.0f, -1.0f, 0.0f};
    const Light* light = getMainDirectionalLight();
    return light ? light->direction : defaultDir;
}

// ==================== SHADOW MAPPING ====================
void LightManager::bindShadowFramebuffer() const {
    if (shadowFramebuffer_ == 0) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
    glViewport(0, 0, shadowMapWidth_, shadowMapHeight_);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void LightManager::unbindShadowFramebuffer() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth_, screenHeight_);
}

void LightManager::cleanup() {
    if (shadowFramebuffer_ != 0) {
        glDeleteFramebuffers(1, &shadowFramebuffer_);
        shadowFramebuffer_ = 0;
    }
    if (shadowDepthTexture_ != 0) {
        glDeleteTextures(1, &shadowDepthTexture_);
        shadowDepthTexture_ = 0;
    }
    lights_.clear();
}

// ==================== MÉTHODES PRIVÉES ====================
void LightManager::createShadowResources() {
    // Créer la texture de profondeur
    glGenTextures(1, &shadowDepthTexture_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                shadowMapWidth_, shadowMapHeight_, 0,
                GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // Créer le framebuffer
    glGenFramebuffers(1, &shadowFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_TEXTURE_2D, shadowDepthTexture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: Shadow framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void LightManager::multiplyMat4(float* out, const float* a, const float* b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                out[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
}

void LightManager::orthographicMatrix(float* out,
                                     float left, float right,
                                     float bottom, float top,
                                     float near, float far) {
    // Initialiser à zéro
    for (int i = 0; i < 16; ++i) {
        out[i] = 0.0f;
    }

    float width = right - left;
    float height = top - bottom;
    float depth = far - near;

    out[0] = 2.0f / width;
    out[5] = 2.0f / height;
    out[10] = -2.0f / depth;
    out[12] = -(right + left) / width;
    out[13] = -(top + bottom) / height;
    out[14] = -(far + near) / depth;
    out[15] = 1.0f;
}

void LightManager::lookAtMatrix(float* out,
                               const core::Vec3F& eye,
                               const core::Vec3F& center,
                               const core::Vec3F& up) {
    // Calculer les vecteurs de la base
    core::Vec3F f = (center - eye).Normalize();
    core::Vec3F s = f.Cross(up).Normalize();
    core::Vec3F u = s.Cross(f).Normalize();

    // Initialiser la matrice
    for (int i = 0; i < 16; ++i) {
        out[i] = 0.0f;
    }

    out[0] = s.x;
    out[4] = s.y;
    out[8] = s.z;
    out[1] = u.x;
    out[5] = u.y;
    out[9] = u.z;
    out[2] = -f.x;
    out[6] = -f.y;
    out[10] = -f.z;
    out[15] = 1.0f;

    out[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    out[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    out[14] = f.x * eye.x + f.y * eye.y + f.z * eye.z;
}
