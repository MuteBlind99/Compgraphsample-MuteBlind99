//
// Created by forna on 07.02.2026.
//

#include "shadow_renderer.h"
#include <iostream>
#include <cstring>
// ==================== CONSTRUCTEUR/DESTRUCTEUR ====================
ShadowRenderer::ShadowRenderer()
    : shaderProgram_(0),
      modelMatrixLoc_(-1),
      projMatrixLoc_(-1),
      viewMatrixLoc_(-1),
      lightSpaceMatrixLoc_(-1),
      lightManager_(nullptr),
      initialized_(false) {
}

ShadowRenderer::~ShadowRenderer() {
    cleanup();
}

// ==================== INITIALISATION ====================
void ShadowRenderer::initialize(LightManager& lightManager) {
    lightManager_ = &lightManager;
    initialized_ = true;
}

bool ShadowRenderer::loadShaders(const std::string& vertexShaderSource,
                               const std::string& fragmentShaderSource) {
    if (!initialized_) {
        std::cerr << "ERROR: ShadowRenderer not initialized!" << std::endl;
        return false;
    }

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (vertexShader == 0) {
        return false;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shaderProgram_ = createProgram(vertexShader, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (shaderProgram_ == 0) {
        return false;
    }

    initializeUniformLocations();
    return true;
}

bool ShadowRenderer::loadDefaultShaders() {
    return loadShaders(getDefaultVertexShader(), getDefaultFragmentShader());
}

// ==================== RENDU ====================
void ShadowRenderer::beginShadowPass() {
    if (lightManager_ == nullptr) {
        std::cerr << "ERROR: LightManager not set!" << std::endl;
        return;
    }

    lightManager_->bindShadowFramebuffer();

    // Configuration OpenGL pour le rendu des ombres
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Réduire les artefacts d'ombres
}

void ShadowRenderer::endShadowPass() {
    if (lightManager_ == nullptr) {
        return;
    }

    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    lightManager_->unbindShadowFramebuffer();
}

void ShadowRenderer::bindShader() const {
    glUseProgram(shaderProgram_);
}

void ShadowRenderer::unbindShader() {
    glUseProgram(0);
}

void ShadowRenderer::setModelMatrix(const float* modelMatrix) const {
    if (modelMatrixLoc_ >= 0 && modelMatrix != nullptr) {
        glUniformMatrix4fv(modelMatrixLoc_, 1, GL_FALSE, modelMatrix);
    }
}

void ShadowRenderer::setProjectionMatrix(const float* projMatrix) const {
    if (projMatrixLoc_ >= 0 && projMatrix != nullptr) {
        glUniformMatrix4fv(projMatrixLoc_, 1, GL_FALSE, projMatrix);
    }
}

void ShadowRenderer::setViewMatrix(const float* viewMatrix) const {
    if (viewMatrixLoc_ >= 0 && viewMatrix != nullptr) {
        glUniformMatrix4fv(viewMatrixLoc_, 1, GL_FALSE, viewMatrix);
    }
}

void ShadowRenderer::setLightSpaceMatrix(const float* lightSpaceMatrix) const {
    if (lightSpaceMatrixLoc_ >= 0 && lightSpaceMatrix != nullptr) {
        glUniformMatrix4fv(lightSpaceMatrixLoc_, 1, GL_FALSE, lightSpaceMatrix);
    }
}

void ShadowRenderer::cleanup() {
    if (shaderProgram_ != 0) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }
}

// ==================== MÉTHODES PRIVÉES ====================
GLuint ShadowRenderer::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Vérifier les erreurs de compilation
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        printShaderError(shader, type);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint ShadowRenderer::createProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Vérifier les erreurs de liaison
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        printProgramError(program);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void ShadowRenderer::initializeUniformLocations() {
    if (shaderProgram_ == 0) {
        return;
    }

    modelMatrixLoc_ = glGetUniformLocation(shaderProgram_, "uModel");
    projMatrixLoc_ = glGetUniformLocation(shaderProgram_, "uProjection");
    viewMatrixLoc_ = glGetUniformLocation(shaderProgram_, "uView");
    lightSpaceMatrixLoc_ = glGetUniformLocation(shaderProgram_, "uLightSpaceMatrix");
}

void ShadowRenderer::printShaderError(GLuint shader, GLenum type) {
    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        char* log = new char[logLength];
        glGetShaderInfoLog(shader, logLength, nullptr, log);

        const char* typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" :
                             (type == GL_FRAGMENT_SHADER) ? "FRAGMENT" : "UNKNOWN";
        std::cerr << "ERROR: " << typeStr << " shader compilation failed:" << std::endl;
        std::cerr << log << std::endl;

        delete[] log;
    }
}

void ShadowRenderer::printProgramError(GLuint program) {
    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
        char* log = new char[logLength];
        glGetProgramInfoLog(program, logLength, nullptr, log);

        std::cerr << "ERROR: Program linking failed:" << std::endl;
        std::cerr << log << std::endl;

        delete[] log;
    }
}

// ==================== SHADERS PAR DÉFAUT ====================
std::string ShadowRenderer::getDefaultVertexShader() {
    return R"(
#version 430 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
    )";
}

std::string ShadowRenderer::getDefaultFragmentShader() {
    return R"(
#version 430 core

out vec4 FragColor;

void main()
{
    // Le depth est automatiquement écrit par OpenGL
    // Ce shader ne fait essentiellement rien
    FragColor = vec4(0.0);
}
    )";
}
