//
// Created by forna on 07.02.2026.
//

#ifndef SSAO_RENDERER_H
#define SSAO_RENDERER_H
#include "third_party/gl_include.h"
#include "maths/vec3.h"
#include <vector>
#include <string>


class SSAORenderer {
public:
    // ==================== CONSTRUCTEURS ====================
    SSAORenderer();
    ~SSAORenderer();

    SSAORenderer(const SSAORenderer&) = delete;
    SSAORenderer& operator=(const SSAORenderer&) = delete;

    // ==================== INITIALISATION ====================
    void initialize(int screenWidth, int screenHeight,
                   int kernelSize = 64, float radius = 0.5f, float bias = 0.025f);
    bool loadDefaultShaders();
    bool loadShaders(const std::string& ssaoVS,
                    const std::string& ssaoFS,
                    const std::string& blurVS,
                    const std::string& blurFS);
    // ==================== RENDU ====================
    void compute(GLuint gPositionTex, GLuint gNormalTex, const float* projMatrix, GLuint quadVAO,GLuint quadVBO);
    void blur(GLuint quadVAO,GLuint quadVBO) const;
    [[nodiscard]] GLuint getSSAOTexture() const { return ssaoColorBuffer_; }
    [[nodiscard]] GLuint getBlurredSSAOTexture() const { return ssaoBlur_; }
    // ==================== PARAMÈTRES ====================
    void setRadius(float radius);
    void setBias(float bias);
    void setKernelSize(int size);

    [[nodiscard]] float getRadius() const { return radius_; }
    [[nodiscard]] float getBias() const { return bias_; }
    [[nodiscard]] int getKernelSize() const { return kernelSize_; }

    // ==================== NETTOYAGE ====================
    void cleanup();
private:
    // ==================== RESSOURCES OPENGL ====================
    GLuint ssaoFBO_;
    GLuint ssaoColorBuffer_;
    GLuint blurFBO_;
    GLuint ssaoBlur_;
    GLuint noiseTex_;
    GLuint ssaoShader_;
    GLuint blurShader_;

    // ==================== UNIFORMS ====================
    GLint ssaoProjLoc_;
    GLint ssaoRadiusLoc_;
    GLint ssaoBiasLoc_;
    GLint ssaoKernelSizeLoc_;
    GLint ssaoNoiseLoc_;

    GLint blurTexLoc_;

    // ==================== PARAMÈTRES ====================
    int screenWidth_;
    int screenHeight_;
    int kernelSize_;
    float radius_;
    float bias_;
    bool initialized_;

    std::vector<core::Vec3F> ssaoKernel_;

    // ==================== MÉTHODES PRIVÉES ====================
    void generateSSAOKernel();
    void generateNoiseTexture();
    void createFramebuffers();

    static GLuint compileShader(GLenum type, const std::string& source);
    static GLuint createProgram(GLuint vs, GLuint fs);
    void initializeUniformLocations();

    static void printShaderError(GLuint shader, GLenum type);
    static void printProgramError(GLuint program);

    // ==================== SHADERS PAR DÉFAUT ====================
    static std::string getDefaultSSAOVS();
    static std::string getDefaultSSAOFS();
    static std::string getDefaultBlurVS();
    static std::string getDefaultBlurFS();

};



#endif //SSAO_RENDERER_H
