//
// Created by forna on 07.02.2026.
//

#ifndef DEFERRED_RENDERER_H
#define DEFERRED_RENDERER_H
#include <string>
#include <GL/glew.h>

#include "maths/vec3.h"


class DeferredRenderer {
public:
    // ==================== CONSTRUCTEURS ====================
    DeferredRenderer();
    ~DeferredRenderer();

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    // ==================== INITIALISATION ====================
    void initialize(int screenWidth, int screenHeight);
    bool loadDefaultShaders();
    bool loadShaders(const std::string& geometryVS,
                     const std::string& geometryFS,
                     const std::string& lightingVS,
                     const std::string& lightingFS);

    // ==================== RENDU ====================
    void beginGeometryPass() const;

    static void endGeometryPass();
    void beginLightingPass() const;

    static void endLightingPass();
    void bindGeometryShader() const;
    void bindLightingShader() const;
    static void unbindShader();

    // ==================== MATRICES ====================
    void setModelMatrix(const float* modelMatrix) const;
    void setViewMatrix(const float* viewMatrix) const;
    void setProjectionMatrix(const float* projMatrix) const;
    void setLightSpaceMatrix(const float* lightSpaceMatrix) const;

    // ==================== UNIFORMS DE LIGHTING ====================
    void setCameraPosition(const core::Vec3F& camPos) const;
    void setPointLight(int index, const core::Vec3F& position,
                      const core::Vec3F& color, float intensity, float radius);
    void setDirectionalLight(const core::Vec3F& direction,
                           const core::Vec3F& color, float intensity) const;
    void bindShadowMap(GLuint shadowMap) const;
    void setPointLightCount(int count) const;

    // ==================== GETTERS ====================
    [[nodiscard]] GLuint getPositionTexture() const { return gPosition_; }
    [[nodiscard]] GLuint getNormalTexture() const { return gNormal_; }
    [[nodiscard]] GLuint getAlbedoTexture() const { return gAlbedo_; }
    [[nodiscard]] GLuint getGeometryShader() const { return geometryShader_; }
    [[nodiscard]] GLuint getLightingShader() const { return lightingShader_; }
    [[nodiscard]] bool isInitialized() const { return initialized_; }
    // ==================== NETTOYAGE ====================
    void cleanup();
private:
    // ==================== RESSOURCES OPENGL ====================
    GLuint gBuffer_;
    GLuint gPosition_;
    GLuint gNormal_;
    GLuint gAlbedo_;
    GLuint gDepth_;
    GLuint geometryShader_;
    GLuint lightingShader_;
    // ==================== UNIFORMS ====================
    GLint geomModelLoc_;
    GLint geomViewLoc_;
    GLint geomProjLoc_;

    GLint lightCamPosLoc_;
    GLint lightDirLoc_;
    GLint lightColorLoc_;
    GLint lightIntensityLoc_;
    GLint lightSpaceMatLoc_;
    GLint lightPointCountLoc_;
    GLint lightShadowMapLoc_;

    // ==================== POSITIONS DE LUMIÈRES ====================
    static constexpr int MAX_POINT_LIGHTS = 32;
    GLint lightPointPosLoc_[MAX_POINT_LIGHTS]{};
    GLint lightPointColorLoc_[MAX_POINT_LIGHTS]{};
    GLint lightPointIntensityLoc_[MAX_POINT_LIGHTS]{};
    GLint lightPointRadiusLoc_[MAX_POINT_LIGHTS]{};

    // ==================== PARAMÈTRES ====================
    int screenWidth_;
    int screenHeight_;
    bool initialized_;

    // ==================== MÉTHODES PRIVÉES ====================
    void createGBuffers();
    static GLuint compileShader(GLenum type, const std::string& source);
    static GLuint createProgram(GLuint vs, GLuint fs);
    void initializeUniformLocations();
    static void printShaderError(GLuint shader, GLenum type);
    static void printProgramError(GLuint program);
    // ==================== SHADERS PAR DÉFAUT ====================
    static std::string getDefaultGeometryVS();
    static std::string getDefaultGeometryFS();
    static std::string getDefaultLightingVS();
    static std::string getDefaultLightingFS();


};



#endif //DEFERRED_RENDERER_H
