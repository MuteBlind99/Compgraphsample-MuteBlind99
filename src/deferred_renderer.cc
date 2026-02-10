//
// Created by forna on 07.02.2026.
//

#include "deferred_renderer.h"
#include <iostream>
#include <cstring>
#include <cmath>

// ==================== CONSTRUCTEUR/DESTRUCTEUR ====================
DeferredRenderer::DeferredRenderer()
    : gBuffer_(0), gPosition_(0), gNormal_(0), gAlbedo_(0), gDepth_(0),
      geometryShader_(0), lightingShader_(0),
      screenWidth_(800), screenHeight_(600), initialized_(false) {
    // Initialiser les locations à -1
    geomModelLoc_ = -1;
    geomViewLoc_ = -1;
    geomProjLoc_ = -1;
    lightCamPosLoc_ = -1;
    lightDirLoc_ = -1;
    lightColorLoc_ = -1;
    lightIntensityLoc_ = -1;
    lightSpaceMatLoc_ = -1;
    lightPointCountLoc_ = -1;
    lightShadowMapLoc_ = -1;

    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        lightPointPosLoc_[i] = -1;
        lightPointColorLoc_[i] = -1;
        lightPointIntensityLoc_[i] = -1;
        lightPointRadiusLoc_[i] = -1;
    }
}

DeferredRenderer::~DeferredRenderer() {
    cleanup();
}

// ==================== INITIALISATION ====================
void DeferredRenderer::initialize(int screenWidth, int screenHeight) {
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    createGBuffers();
    initialized_ = true;
}

bool DeferredRenderer::loadDefaultShaders() {
    return loadShaders(getDefaultGeometryVS(), getDefaultGeometryFS(),
                       getDefaultLightingVS(), getDefaultLightingFS());
}

bool DeferredRenderer::loadShaders(const std::string &geomVS,
                                   const std::string &geomFS,
                                   const std::string &lightVS,
                                   const std::string &lightFS) {
    // ===== GEOMETRY PROGRAM =====
    GLuint gVS = compileShader(GL_VERTEX_SHADER, geomVS);
    GLuint gFS = compileShader(GL_FRAGMENT_SHADER, geomFS);
    if (!gVS || !gFS) return false;

    geometryShader_ = glCreateProgram();
    glAttachShader(geometryShader_, gVS);
    glAttachShader(geometryShader_, gFS);
    glLinkProgram(geometryShader_);

    GLint ok = 0;
    glGetProgramiv(geometryShader_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(geometryShader_, sizeof(log), nullptr, log);
        std::cerr << "[Geometry LINK ERROR]\n" << log << std::endl;
        return false;
    }

    glDeleteShader(gVS);
    glDeleteShader(gFS);

    // ===== LIGHTING PROGRAM =====
    GLuint lVS = compileShader(GL_VERTEX_SHADER, lightVS);
    GLuint lFS = compileShader(GL_FRAGMENT_SHADER, lightFS);
    if (!lVS || !lFS) return false;

    lightingShader_ = glCreateProgram();
    glAttachShader(lightingShader_, lVS);
    glAttachShader(lightingShader_, lFS);
    glLinkProgram(lightingShader_);
    glGetProgramiv(lightingShader_, GL_LINK_STATUS, &ok);
    std::cout << "[Lighting link status] = " << ok << std::endl;
    glGetProgramiv(lightingShader_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(lightingShader_, sizeof(log), nullptr, log);
        std::cerr << "[Lighting LINK ERROR]\n" << log << std::endl;
        return false;
    }

    glDeleteShader(lVS);
    glDeleteShader(lFS);

    return true;
}

// ==================== RENDU ====================
void DeferredRenderer::beginGeometryPass() const {
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);
    // IMPORTANT : état GL propre (ImGui peut laisser des trucs)
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE); // debug (tu pourras le remettre)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void DeferredRenderer::endGeometryPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredRenderer::beginLightingPass() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedo_);
}

void DeferredRenderer::endLightingPass() {
    glEnable(GL_DEPTH_TEST);
}

void DeferredRenderer::bindGeometryShader() const {
    glUseProgram(geometryShader_);
}

void DeferredRenderer::bindLightingShader() const {
    glUseProgram(lightingShader_);
    if (lightingShader_ != 0) {
        GLint loc;

        loc = glGetUniformLocation(lightingShader_, "gPosition");
        if (loc >= 0)
            glUniform1i(loc, 0);

        loc = glGetUniformLocation(lightingShader_, "gNormal");
        if (loc >= 0)
            glUniform1i(loc, 1);

        loc = glGetUniformLocation(lightingShader_, "gAlbedo");
        if (loc >= 0)
            glUniform1i(loc, 2);

        // SSAO unit 3 (main_scene bind la texture si activé)
        loc = glGetUniformLocation(lightingShader_, "gSSAO");
        if (loc >= 0)
            glUniform1i(loc, 3);

        loc = glGetUniformLocation(lightingShader_, "uUseSSAO");
        if (loc >= 0)
            glUniform1i(loc, 0);
    }
}

void DeferredRenderer::unbindShader() {
    glUseProgram(0);
}

// ==================== MATRICES ====================
void DeferredRenderer::setModelMatrix(const float *modelMatrix) const {
    if (geomModelLoc_ >= 0 && modelMatrix != nullptr) {
        glUniformMatrix4fv(geomModelLoc_, 1, GL_FALSE, modelMatrix);
    }
}

void DeferredRenderer::setViewMatrix(const float *viewMatrix) const {
    if (geomViewLoc_ >= 0 && viewMatrix != nullptr) {
        glUniformMatrix4fv(geomViewLoc_, 1, GL_FALSE, viewMatrix);
    }
}

void DeferredRenderer::setProjectionMatrix(const float *projMatrix) const {
    if (geomProjLoc_ >= 0 && projMatrix != nullptr) {
        glUniformMatrix4fv(geomProjLoc_, 1, GL_FALSE, projMatrix);
    }
}

void DeferredRenderer::setLightSpaceMatrix(const float *lightSpaceMatrix) const {
    if (lightSpaceMatLoc_ >= 0 && lightSpaceMatrix != nullptr) {
        glUniformMatrix4fv(lightSpaceMatLoc_, 1, GL_FALSE, lightSpaceMatrix);
    }
}

// ==================== UNIFORMS DE LIGHTING ====================
void DeferredRenderer::setCameraPosition(const core::Vec3F &camPos) const {
    if (lightCamPosLoc_ >= 0) {
        glUniform3f(lightCamPosLoc_, camPos.x, camPos.y, camPos.z);
    }
}

void DeferredRenderer::setPointLight(int index, const core::Vec3F &position,
                                     const core::Vec3F &color, float intensity, float radius) {
    if (index < 0 || index >= MAX_POINT_LIGHTS) {
        return;
    }

    if (lightPointPosLoc_[index] >= 0) {
        glUniform3f(lightPointPosLoc_[index], position.x, position.y, position.z);
    }
    if (lightPointColorLoc_[index] >= 0) {
        glUniform3f(lightPointColorLoc_[index], color.x, color.y, color.z);
    }
    if (lightPointIntensityLoc_[index] >= 0) {
        glUniform1f(lightPointIntensityLoc_[index], intensity);
    }
    if (lightPointRadiusLoc_[index] >= 0) {
        glUniform1f(lightPointRadiusLoc_[index], radius);
    }
}

void DeferredRenderer::setDirectionalLight(const core::Vec3F &direction,
                                           const core::Vec3F &color, float intensity) const {
    if (lightDirLoc_ >= 0) {
        glUniform3f(lightDirLoc_, direction.x, direction.y, direction.z);
    }
    if (lightColorLoc_ >= 0) {
        glUniform3f(lightColorLoc_, color.x, color.y, color.z);
    }
    if (lightIntensityLoc_ >= 0) {
        glUniform1f(lightIntensityLoc_, intensity);
    }
}

void DeferredRenderer::bindShadowMap(GLuint shadowMap) const {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowMap);
    if (lightShadowMapLoc_ >= 0) {
        glUniform1i(lightShadowMapLoc_, 3);
    }
}

void DeferredRenderer::setPointLightCount(int count) const {
    if (lightPointCountLoc_ >= 0) {
        glUniform1i(lightPointCountLoc_, count);
    }
}

// ==================== NETTOYAGE ====================
void DeferredRenderer::cleanup() {
    if (gBuffer_ != 0) {
        glDeleteFramebuffers(1, &gBuffer_);
        gBuffer_ = 0;
    }
    if (gPosition_ != 0) {
        glDeleteTextures(1, &gPosition_);
        gPosition_ = 0;
    }
    if (gNormal_ != 0) {
        glDeleteTextures(1, &gNormal_);
        gNormal_ = 0;
    }
    if (gAlbedo_ != 0) {
        glDeleteTextures(1, &gAlbedo_);
        gAlbedo_ = 0;
    }
    if (gDepth_ != 0) {
        glDeleteRenderbuffers(1, &gDepth_);
        gDepth_ = 0;
    }
    if (geometryShader_ != 0) {
        glDeleteProgram(geometryShader_);
        geometryShader_ = 0;
    }
    if (lightingShader_ != 0) {
        glDeleteProgram(lightingShader_);
        lightingShader_ = 0;
    }
}

// ==================== MÉTHODES PRIVÉES ====================
void DeferredRenderer::createGBuffers() {
    // Créer le framebuffer
    glGenFramebuffers(1, &gBuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer_);

    // Position buffer (RGB16F)
    glGenTextures(1, &gPosition_);
    glBindTexture(GL_TEXTURE_2D, gPosition_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth_, screenHeight_, 0,
                 GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition_, 0);

    // Normal buffer (RGB16F)
    glGenTextures(1, &gNormal_);
    glBindTexture(GL_TEXTURE_2D, gNormal_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth_, screenHeight_, 0,
                 GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal_, 0);

    // Albedo buffer (RGBA8)
    glGenTextures(1, &gAlbedo_);
    glBindTexture(GL_TEXTURE_2D, gAlbedo_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth_, screenHeight_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo_, 0);

    // Depth buffer (RBO)
    glGenRenderbuffers(1, &gDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, gDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth_, screenHeight_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gDepth_);

    // Spécifier les attachments de couleur
    unsigned int attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: G-Buffer framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint DeferredRenderer::compileShader(GLenum type, const std::string &source) {
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        printShaderError(shader, type);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint DeferredRenderer::createProgram(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        printProgramError(program);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void DeferredRenderer::initializeUniformLocations() {
    // Géométrie
    geomModelLoc_ = glGetUniformLocation(geometryShader_, "uModel");
    geomViewLoc_ = glGetUniformLocation(geometryShader_, "uView");
    geomProjLoc_ = glGetUniformLocation(geometryShader_, "uProjection");

    // Lighting
    lightCamPosLoc_ = glGetUniformLocation(lightingShader_, "uCameraPos");
    lightDirLoc_ = glGetUniformLocation(lightingShader_, "uDirLight.direction");
    lightColorLoc_ = glGetUniformLocation(lightingShader_, "uDirLight.color");
    lightIntensityLoc_ = glGetUniformLocation(lightingShader_, "uDirLight.intensity");
    lightSpaceMatLoc_ = glGetUniformLocation(lightingShader_, "uLightSpaceMatrix");
    lightPointCountLoc_ = glGetUniformLocation(lightingShader_, "uPointLightCount");
    lightShadowMapLoc_ = glGetUniformLocation(lightingShader_, "uShadowMap");

    // Point lights
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        std::string base = "uPointLights[" + std::to_string(i) + "]";
        lightPointPosLoc_[i] = glGetUniformLocation(lightingShader_, (base + ".position").c_str());
        lightPointColorLoc_[i] = glGetUniformLocation(lightingShader_, (base + ".color").c_str());
        lightPointIntensityLoc_[i] = glGetUniformLocation(lightingShader_, (base + ".intensity").c_str());
        lightPointRadiusLoc_[i] = glGetUniformLocation(lightingShader_, (base + ".radius").c_str());
    }

    // Bind textures
    GLint posLoc = glGetUniformLocation(lightingShader_, "gPosition");
    GLint normLoc = glGetUniformLocation(lightingShader_, "gNormal");
    GLint albLoc = glGetUniformLocation(lightingShader_, "gAlbedo");

    glUseProgram(lightingShader_);
    glUniform1i(posLoc, 0);
    glUniform1i(normLoc, 1);
    glUniform1i(albLoc, 2);
    glUseProgram(0);
}

void DeferredRenderer::printShaderError(GLuint shader, GLenum type) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        char *log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, nullptr, log);

        const char *typeStr = (type == GL_VERTEX_SHADER)
                                  ? "VERTEX"
                                  : (type == GL_FRAGMENT_SHADER)
                                        ? "FRAGMENT"
                                        : "UNKNOWN";
        std::cerr << "ERROR: " << typeStr << " shader compilation failed:" << std::endl;
        std::cerr << log << std::endl;

        delete[] log;
    }
}

void DeferredRenderer::printProgramError(GLuint program) {
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        char *log = new char[logLength];
        glGetProgramInfoLog(program, logLength, nullptr, log);

        std::cerr << "ERROR: Program linking failed:" << std::endl;
        std::cerr << log << std::endl;

        delete[] log;
    }
}

// ==================== SHADERS PAR DÉFAUT ====================
std::string DeferredRenderer::getDefaultGeometryVS() {
    return R"(
#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} vs_out;

void main()
{
    vs_out.FragPos = vec3(uModel * vec4(aPosition, 1.0));
    vs_out.Normal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vs_out.TexCoord = aTexCoord;

    gl_Position = uProjection * uView * vec4(vs_out.FragPos, 1.0);
}
    )";
}

std::string DeferredRenderer::getDefaultGeometryFS() {
    return R"(
#version 330 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gAlbedo;

void main()
{
    gPosition = fs_in.FragPos;
    gNormal = normalize(fs_in.Normal);
    gAlbedo = vec4(0.5, 0.5, 0.5, 1.0); // Albédo par défaut gris
}
    )";
}

std::string DeferredRenderer::getDefaultLightingVS() {
    return R"(
#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
    )";
}

std::string DeferredRenderer::getDefaultLightingFS() {
    return R"(
 #version 330 core
    out vec4 FragColor;

    in vec2 vTexCoord;

    uniform sampler2D gPosition;
    uniform sampler2D gNormal;
    uniform sampler2D gAlbedo;  // FIX: name matches G-buffer attachment

    // SSAO (optional)
    uniform sampler2D gSSAO;
    uniform int uUseSSAO;       // 0/1

    uniform vec3 lightDirection;
    uniform vec3 lightColor;
    uniform float lightIntensity;
    uniform vec3 viewPos;

    void main() {
        vec3 FragPos = texture(gPosition, vTexCoord).rgb;
        vec3 Normal  = normalize(texture(gNormal, vTexCoord).rgb);
        vec3 Albedo  = texture(gAlbedo, vTexCoord).rgb;

        float ao = 1.0;
        if (uUseSSAO == 1) {
            ao = texture(gSSAO, vTexCoord).r;
        }

        vec3 ambient = 0.15 * Albedo * ao;

        vec3 lightDir = normalize(-lightDirection);
        float diff = max(dot(Normal, lightDir), 0.0);
        vec3 diffuse = diff * lightColor * lightIntensity * Albedo;

        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(Normal, halfDir), 0.0), 32.0);
        vec3 specular = spec * lightColor * lightIntensity * 0.2;

        vec3 lighting = ambient + diffuse + specular;
        FragColor = vec4(lighting, 1.0);
    }
    )";
}
