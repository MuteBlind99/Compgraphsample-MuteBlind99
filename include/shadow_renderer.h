//
// Created by forna on 07.02.2026.
//

#ifndef SHADOW_RENDERER_H
#define SHADOW_RENDERER_H
#include "third_party/gl_include.h"
#include "light_manager.h"
#include <string>
#include <memory>



class ShadowRenderer {
public:
    // ==================== CONSTRUCTEURS ====================
    ShadowRenderer();
    ~ShadowRenderer();

    // Empêcher les copies
    ShadowRenderer(const ShadowRenderer&) = delete;
    ShadowRenderer& operator=(const ShadowRenderer&) = delete;

    // ==================== INITIALISATION ====================
    void initialize(LightManager& lightManager);
    bool loadShaders(const std::string& vertexShaderSource,
                    const std::string& fragmentShaderSource);
    bool loadDefaultShaders();

    // ==================== RENDU ====================
    void beginShadowPass();
    void endShadowPass();
    void bindShader() const;

    static void unbindShader();
    void setModelMatrix(const float* modelMatrix) const;
    void setProjectionMatrix(const float* projMatrix) const;
    void setViewMatrix(const float* viewMatrix) const;
    void setLightSpaceMatrix(const float* lightSpaceMatrix) const;

    // ==================== GETTERS ====================
    [[nodiscard]] GLuint getShaderProgram() const { return shaderProgram_; }
    [[nodiscard]] bool isInitialized() const { return initialized_; }
    [[nodiscard]] bool hasShaders() const { return shaderProgram_ != 0; }

    // ==================== NETTOYAGE ====================
    void cleanup();

private:
    // ==================== RESSOURCES OPENGL ====================
    GLuint shaderProgram_;
    GLint modelMatrixLoc_;
    GLint projMatrixLoc_;
    GLint viewMatrixLoc_;
    GLint lightSpaceMatrixLoc_;

    // ==================== RÉFÉRENCES ====================
    LightManager* lightManager_;
    bool initialized_;

    // ==================== MÉTHODES PRIVÉES ====================
    static GLuint compileShader(GLenum type, const std::string& source);
    static GLuint createProgram(GLuint vertexShader, GLuint fragmentShader);
    void initializeUniformLocations();
    static void printShaderError(GLuint shader, GLenum type);
    static void printProgramError(GLuint program);

    // ==================== SHADERS PAR DÉFAUT ====================
    static std::string getDefaultVertexShader();
    static std::string getDefaultFragmentShader();
};



#endif //SHADOW_RENDERER_H
