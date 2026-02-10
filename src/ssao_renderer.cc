//
// Created by forna on 07.02.2026.
//

#include "../include/ssao_renderer.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <random>

// ==================== CONSTRUCTEUR/DESTRUCTEUR ====================
SSAORenderer::SSAORenderer()
    : ssaoFBO_(0), ssaoColorBuffer_(0), blurFBO_(0), ssaoBlur_(0),
      noiseTex_(0), ssaoShader_(0), blurShader_(0),
      screenWidth_(800), screenHeight_(600),
      kernelSize_(64), radius_(0.5f), bias_(0.025f),
      initialized_(false) {

    ssaoProjLoc_ = -1;
    ssaoRadiusLoc_ = -1;
    ssaoBiasLoc_ = -1;
    ssaoKernelSizeLoc_ = -1;
    ssaoNoiseLoc_ = -1;
    blurTexLoc_ = -1;
}

SSAORenderer::~SSAORenderer() {
    cleanup();
}

// ==================== INITIALISATION ====================
void SSAORenderer::initialize(int screenWidth, int screenHeight,
                             int kernelSize, float radius, float bias) {
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    kernelSize_ = kernelSize;
    radius_ = radius;
    bias_ = bias;

    generateSSAOKernel();
    generateNoiseTexture();
    createFramebuffers();

    initialized_ = true;
}

bool SSAORenderer::loadDefaultShaders() {
    return loadShaders(getDefaultSSAOVS(), getDefaultSSAOFS(),
                      getDefaultBlurVS(), getDefaultBlurFS());
}

bool SSAORenderer::loadShaders(const std::string& ssaoVS,
                              const std::string& ssaoFS,
                              const std::string& blurVS,
                              const std::string& blurFS) {
    if (!initialized_) {
        std::cerr << "ERROR: SSAORenderer not initialized!" << std::endl;
        return false;
    }

    // Compiler shader SSAO
    GLuint sVS = compileShader(GL_VERTEX_SHADER, ssaoVS);
    GLuint sFS = compileShader(GL_FRAGMENT_SHADER, ssaoFS);
    if (sVS == 0 || sFS == 0) {
        glDeleteShader(sVS);
        glDeleteShader(sFS);
        return false;
    }

    ssaoShader_ = createProgram(sVS, sFS);
    glDeleteShader(sVS);
    glDeleteShader(sFS);

    if (ssaoShader_ == 0) {
        return false;
    }

    // Compiler shader blur
    GLuint bVS = compileShader(GL_VERTEX_SHADER, blurVS);
    GLuint bFS = compileShader(GL_FRAGMENT_SHADER, blurFS);
    if (bVS == 0 || bFS == 0) {
        glDeleteShader(bVS);
        glDeleteShader(bFS);
        return false;
    }

    blurShader_ = createProgram(bVS, bFS);
    glDeleteShader(bVS);
    glDeleteShader(bFS);

    if (blurShader_ == 0) {
        return false;
    }

    initializeUniformLocations();
    return true;
}

// ==================== RENDU ====================
void SSAORenderer::compute(GLuint gPositionTex, GLuint gNormalTex, const float* projMatrix, GLuint quadVAO,GLuint quadVBO) {
    if (!initialized_ || ssaoShader_ == 0) {
        return;
    }

    // Binder le framebuffer SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(ssaoShader_);

    // Binder les G-Buffers
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPositionTex);
    glUniform1i(glGetUniformLocation(ssaoShader_, "gPosition"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormalTex);
    glUniform1i(glGetUniformLocation(ssaoShader_, "gNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    glUniform1i(glGetUniformLocation(ssaoShader_, "noiseTexture"), 2);

    // Envoyer les uniforms
    if (ssaoProjLoc_ >= 0) {
        glUniformMatrix4fv(ssaoProjLoc_, 1, GL_FALSE, projMatrix);
    }
    if (ssaoRadiusLoc_ >= 0) {
        glUniform1f(ssaoRadiusLoc_, radius_);
    }
    if (ssaoBiasLoc_ >= 0) {
        glUniform1f(ssaoBiasLoc_, bias_);
    }
    if (ssaoKernelSizeLoc_ >= 0) {
        glUniform1i(ssaoKernelSizeLoc_, kernelSize_);
    }

    // Envoyer le kernel SSAO
    GLint kernelLoc = glGetUniformLocation(ssaoShader_, "samples");
    for (int i = 0; i < kernelSize_; ++i) {
        std::string uniform = "samples[" + std::to_string(i) + "]";
        GLint loc = glGetUniformLocation(ssaoShader_, uniform.c_str());
        if (loc >= 0) {
            glUniform3f(loc, ssaoKernel_[i].x, ssaoKernel_[i].y, ssaoKernel_[i].z);
        }
    }

    // Dessiner un quad plein écran
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth_, screenHeight_);

    // Créer et dessiner un quad simple

    if (quadVAO == 0) {
        float quadVertices[] = {
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

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAORenderer::blur(GLuint quadVAO,GLuint quadVBO) const {
    if (!initialized_ || blurShader_ == 0) {
        return;
    }

    // Binder le framebuffer blur
    glBindFramebuffer(GL_FRAMEBUFFER, blurFBO_);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(blurShader_);

    // Binder la texture SSAO
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
    glUniform1i(glGetUniformLocation(blurShader_, "ssaoInput"), 0);

    // Dessiner le quad
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth_, screenHeight_);


    if (quadVAO == 0) {
        float quadVertices[] = {
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

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ==================== PARAMÈTRES ====================
void SSAORenderer::setRadius(float radius) {
    radius_ = std::max(0.0f, std::min(2.0f, radius));
    if (ssaoRadiusLoc_ >= 0) {
        glUseProgram(ssaoShader_);
        glUniform1f(ssaoRadiusLoc_, radius_);
        glUseProgram(0);
    }
}

void SSAORenderer::setBias(float bias) {
    bias_ = std::max(0.0f, std::min(0.1f, bias));
    if (ssaoBiasLoc_ >= 0) {
        glUseProgram(ssaoShader_);
        glUniform1f(ssaoBiasLoc_, bias_);
        glUseProgram(0);
    }
}

void SSAORenderer::setKernelSize(int size) {
    if (size != 16 && size != 32 && size != 64 && size != 128) {
        return; // Taille invalide
    }
    kernelSize_ = size;
    generateSSAOKernel();
}

// ==================== NETTOYAGE ====================
void SSAORenderer::cleanup() {
    if (ssaoFBO_ != 0) {
        glDeleteFramebuffers(1, &ssaoFBO_);
        ssaoFBO_ = 0;
    }
    if (ssaoColorBuffer_ != 0) {
        glDeleteTextures(1, &ssaoColorBuffer_);
        ssaoColorBuffer_ = 0;
    }
    if (blurFBO_ != 0) {
        glDeleteFramebuffers(1, &blurFBO_);
        blurFBO_ = 0;
    }
    if (ssaoBlur_ != 0) {
        glDeleteTextures(1, &ssaoBlur_);
        ssaoBlur_ = 0;
    }
    if (noiseTex_ != 0) {
        glDeleteTextures(1, &noiseTex_);
        noiseTex_ = 0;
    }
    if (ssaoShader_ != 0) {
        glDeleteProgram(ssaoShader_);
        ssaoShader_ = 0;
    }
    if (blurShader_ != 0) {
        glDeleteProgram(blurShader_);
        blurShader_ = 0;
    }
}

// ==================== MÉTHODES PRIVÉES ====================
void SSAORenderer::generateSSAOKernel() {
    ssaoKernel_.clear();
    ssaoKernel_.reserve(kernelSize_);

    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::mt19937 generator(std::random_device{}());

    for (int i = 0; i < kernelSize_; ++i) {
        core::Vec3F sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = sample.Normalize();

        // Augmenter progressivement les échantillons vers la surface
        float scale = static_cast<float>(i) / kernelSize_;
        scale = 0.1f + scale * 0.9f;
        sample = sample * scale;

        ssaoKernel_.push_back(sample);
    }
}

void SSAORenderer::generateNoiseTexture() {
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::mt19937 generator(std::random_device{}());

    int noiseSize = 4;
    std::vector<core::Vec3F> ssaoNoise;
    for (int i = 0; i < noiseSize * noiseSize; ++i) {
        core::Vec3F noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }

    glGenTextures(1, &noiseTex_);
    glBindTexture(GL_TEXTURE_2D, noiseTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, noiseSize, noiseSize, 0,
                 GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void SSAORenderer::createFramebuffers() {
    // SSAO FBO
    glGenFramebuffers(1, &ssaoFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO_);

    glGenTextures(1, &ssaoColorBuffer_);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, screenWidth_, screenHeight_, 0,
                 GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, ssaoColorBuffer_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: SSAO FBO is not complete!" << std::endl;
    }

    // Blur FBO
    glGenFramebuffers(1, &blurFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFBO_);

    glGenTextures(1, &ssaoBlur_);
    glBindTexture(GL_TEXTURE_2D, ssaoBlur_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, screenWidth_, screenHeight_, 0,
                 GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, ssaoBlur_, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR: Blur FBO is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint SSAORenderer::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
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

GLuint SSAORenderer::createProgram(GLuint vs, GLuint fs) {
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

void SSAORenderer::initializeUniformLocations() {
    ssaoProjLoc_ = glGetUniformLocation(ssaoShader_, "projection");
    ssaoRadiusLoc_ = glGetUniformLocation(ssaoShader_, "radius");
    ssaoBiasLoc_ = glGetUniformLocation(ssaoShader_, "bias");
    ssaoKernelSizeLoc_ = glGetUniformLocation(ssaoShader_, "kernelSize");
    ssaoNoiseLoc_ = glGetUniformLocation(ssaoShader_, "noiseTexture");

    blurTexLoc_ = glGetUniformLocation(blurShader_, "ssaoInput");
}

void SSAORenderer::printShaderError(GLuint shader, GLenum type) {
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

void SSAORenderer::printProgramError(GLuint program) {
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
std::string SSAORenderer::getDefaultSSAOVS() {
    return R"(
#version 430 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

out VS_OUT {
    vec2 TexCoord;
} vs_out;

void main()
{
    vs_out.TexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
    )";
}

std::string SSAORenderer::getDefaultSSAOFS() {
    return R"(
#version 430 core

in VS_OUT {
    vec2 TexCoord;
} fs_in;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D noiseTexture;

uniform mat4 projection;
uniform float radius;
uniform float bias;
uniform int kernelSize;

uniform vec3 samples[128];

out float FragColor;

void main()
{
    vec3 fragPos = texture(gPosition, fs_in.TexCoord).xyz;
    vec3 normal = normalize(texture(gNormal, fs_in.TexCoord).xyz);
    vec3 randomVec = normalize(texture(noiseTexture, fs_in.TexCoord * 50.0).xyz);

    // Créer une matrice TBN
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // Obtenir position d'échantillon
        vec3 samplePos = fragPos + TBN * samples[i] * radius;

        // Projeter dans l'espace écran
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Lire la profondeur d'écran
        float sampleDepth = texture(gPosition, offset.xy).z;

        // Appliquer un falloff
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth, samplePos.z - bias);
    }

    occlusion = 1.0 - (occlusion / kernelSize);
    FragColor = occlusion;
}
    )";
}

std::string SSAORenderer::getDefaultBlurVS() {
    return R"(
#version 430 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

out VS_OUT {
    vec2 TexCoord;
} vs_out;

void main()
{
    vs_out.TexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
    )";
}

std::string SSAORenderer::getDefaultBlurFS() {
    return R"(
#version 430 core

in VS_OUT {
    vec2 TexCoord;
} fs_in;

uniform sampler2D ssaoInput;

out float FragColor;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;

    for(int x = -2; x < 2; ++x)
    {
        for(int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, fs_in.TexCoord + offset).r;
        }
    }

    FragColor = result / 16.0;
}
    )";
}
