//
// Created by forna on 07.02.2026.
//

#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H
#include "maths/vec3.h"
#include "third_party/gl_include.h"
#include <array>
#include <vector>
#include <memory>


enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOT
};

struct Light {
    core::Vec3F position;
    core::Vec3F direction;
    core::Vec3F color;
    float intensity;
    LightType type;
    bool enabled;
};

struct DirectionalLight : public Light {
    DirectionalLight() {
        type = LightType::DIRECTIONAL;
        position = core::Vec3F{3.0f, 10.0f, 3.0f};
        direction = core::Vec3F{0.0f, -1.0f, 0.0f};
        color = core::Vec3F{1.0f, 1.0f, 1.0f};
        intensity = 1.0f;
        enabled = true;
    }
};

struct PointLight : public Light {
    float constant;
    float linear;
    float quadratic;
    float radius;

    PointLight() {
        type = LightType::POINT;
        position = core::Vec3F{0.0f, 5.0f, 0.0f};
        color = core::Vec3F{1.0f, 1.0f, 1.0f};
        intensity = 1.0f;
        constant = 1.0f;
        linear = 0.09f;
        quadratic = 0.032f;
        radius = 20.0f;
        enabled = true;
    }
};

struct SpotLight : public Light {
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;

    SpotLight() {
        type = LightType::SPOT;
        position = core::Vec3F{0.0f, 5.0f, 0.0f};
        direction = core::Vec3F{0.0f, -1.0f, 0.0f};
        color = core::Vec3F{1.0f, 1.0f, 1.0f};
        intensity = 1.0f;
        cutOff = 12.5f;
        outerCutOff = 17.5f;
        constant = 1.0f;
        linear = 0.09f;
        quadratic = 0.032f;
        enabled = true;
    }
};

class LightManager {
public:
    // ==================== CONSTRUCTEURS ====================
    LightManager();

    ~LightManager();

    // Empêcher les copies
    LightManager(const LightManager&) = delete;

    LightManager &operator=(const LightManager&) = delete;

    // ==================== INITIALISATION ====================
    void initialize(int screenWidth, int screenHeight);

    void initializeShadowMapping(int shadowMapWidth = 2048, int shadowMapHeight = 2048);

    // ==================== GESTION DES LUMIÈRES ====================
    int addDirectionalLight(const DirectionalLight &light);

    int addPointLight(const PointLight& light);


    int addSpotLight(const SpotLight &light);

    void removeLight(int index);

    void updateLight(int index, const Light &light) const;

    Light *getLight(int index);

    [[nodiscard]] const Light *getLight(int index) const;

    [[nodiscard]] int getLightCount() const { return lights_.size(); }

    // ==================== LUMIÈRE DIRECTIONNELLE PRINCIPALE ====================
    void setMainDirectionalLight(int index);

    [[nodiscard]] int getMainDirectionalLightIndex() const { return mainLightIndex_; }

    Light *getMainDirectionalLight();

    [[nodiscard]] const Light *getMainDirectionalLight() const;

    // ==================== MATRICES DE LUMIÈRE ====================
    void calculateLightMatrices(const core::Vec3F &sceneCenter,
                                float sceneRadius,
                                int lightIndex = -1);

    void getLightProjectionMatrix(float *out) const;

    void getLightViewMatrix(float *out) const;

    void getLightSpaceMatrix(float *out) const;

    [[nodiscard]] const core::Vec3F &getMainLightPosition() const;

    [[nodiscard]] const core::Vec3F &getMainLightDirection() const;

    // ==================== SHADOW MAPPING ====================
    void bindShadowFramebuffer() const;

    void unbindShadowFramebuffer() const;

    [[nodiscard]] GLuint getShadowDepthTexture() const { return shadowDepthTexture_; }
    void setShadowMappingEnabled(bool enabled) { shadowMappingEnabled_ = enabled; }
    [[nodiscard]] bool isShadowMappingEnabled() const { return shadowMappingEnabled_; }

    // ==================== PARAMÈTRES DE SHADOW MAPPING ====================
    [[nodiscard]] int getShadowMapWidth() const { return shadowMapWidth_; }
    [[nodiscard]] int getShadowMapHeight() const { return shadowMapHeight_; }
    void setShadowViewDistance(float distance) { shadowViewDistance_ = distance; }
    [[nodiscard]] float getShadowViewDistance() const { return shadowViewDistance_; }

    // ==================== NETTOYAGE ====================
    void cleanup();

private:
    // ==================== DONNÉES DES LUMIÈRES ====================
    std::vector<std::unique_ptr<Light> > lights_;
    int mainLightIndex_;

    // ==================== MATRICES DE LUMIÈRE ====================
    std::array<float, 16> lightProjectionMatrix_;
    std::array<float, 16> lightViewMatrix_;
    std::array<float, 16> lightSpaceMatrix_;

    // ==================== RESSOURCES SHADOW MAPPING ====================
    GLuint shadowFramebuffer_;
    GLuint shadowDepthTexture_;
    int shadowMapWidth_;
    int shadowMapHeight_;
    float shadowViewDistance_;
    bool shadowMappingEnabled_;

    // ==================== PARAMÈTRES D'ÉCRAN ====================
    int screenWidth_;
    int screenHeight_;

    // ==================== MÉTHODES PRIVÉES ====================
    void createShadowResources();

    static void multiplyMat4(float *out, const float *a, const float *b);

    static void orthographicMatrix(float *out,
                                   float left, float right,
                                   float bottom, float top,
                                   float near, float far);

    static void lookAtMatrix(float *out,
                             const core::Vec3F &eye,
                             const core::Vec3F &center,
                             const core::Vec3F &up);
};

#endif //LIGHT_MANAGER_H
