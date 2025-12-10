//
// Created by forna on 10.12.2025.
//
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <corecrt_math_defines.h>

// Ajouter les includes pour le chargement d'images
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class AttenuatedPointLightCubes : public common::DrawInterface, public common::SystemInterface {
public:
    struct CubeInstance {
        float position[3];        // Position dans le monde
        float rotationAngleX;
        float rotationAngleY;
        float rotationAngleZ;
        float rotationSpeedX;
        float rotationSpeedY;
        float rotationSpeedZ;
        float scale;
    };

    void Begin() override {
        std::cout << "AttenuatedPointLightCubes::Begin() - Étape 4: Point Light with Attenuation" << std::endl;
        initGL();
        createInstances();
    }

    void Update(float dt) override {
        time += dt;

        // Mettre à jour la rotation de chaque cube
        for (auto& cube : cubeInstances) {
            cube.rotationAngleX += cube.rotationSpeedX * dt;
            cube.rotationAngleY += cube.rotationSpeedY * dt;
            cube.rotationAngleZ += cube.rotationSpeedZ * dt;

            // Garder les angles dans [0, 360]
            cube.rotationAngleX = fmodf(cube.rotationAngleX, 360.0f);
            cube.rotationAngleY = fmodf(cube.rotationAngleY, 360.0f);
            cube.rotationAngleZ = fmodf(cube.rotationAngleZ, 360.0f);
        }

        // Animation de la lumière ponctuelle avec atténuation
        pointLightAngle += 20.0f * dt;
        if (pointLightAngle > 360.0f) pointLightAngle -= 360.0f;

        // Mettre à jour la position de la lumière (trajectoire intéressante)
        pointLightPos[0] = 4.0f * sinf(pointLightAngle * M_PI / 180.0f);
        pointLightPos[1] = 1.0f + sinf(pointLightAngle * 2.0f * M_PI / 180.0f) * 0.5f;  // Mouvement vertical
        pointLightPos[2] = 4.0f * cosf(pointLightAngle * M_PI / 180.0f);

        // Animation de l'atténuation (pour montrer l'effet)
        attenuationDistance += dt * 0.5f;
        if (attenuationDistance > 10.0f) attenuationDistance = 3.0f;

        // Changer la couleur de la lumière pour plus d'effet visuel
        lightColor[0] = 0.8f + 0.2f * sinf(time * 1.5f);
        lightColor[1] = 0.7f + 0.3f * sinf(time * 2.0f);
        lightColor[2] = 0.9f + 0.1f * sinf(time * 1.0f);

        // Mettre à jour la position de la caméra
        cameraAngle += 10.0f * dt;
        if (cameraAngle > 360.0f) cameraAngle -= 360.0f;

        viewPos[0] = 8.0f * sinf(cameraAngle * M_PI / 180.0f);
        viewPos[1] = 4.0f;
        viewPos[2] = 8.0f * cosf(cameraAngle * M_PI / 180.0f);

        // Affichage périodique des informations
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 2.5f) {
            infoTimer = 0.0f;

            // Calculer la distance de la lumière au cube central
            float distanceToCenter = sqrtf(
                pointLightPos[0]*pointLightPos[0] +
                pointLightPos[1]*pointLightPos[1] +
                pointLightPos[2]*pointLightPos[2]
            );

            // Calculer l'atténuation actuelle
            float attenuation = 1.0f / (constant + linear * distanceToCenter + quadratic * distanceToCenter * distanceToCenter);

            std::cout << "Point Light:" << std::endl;
            std::cout << "  Position: [" << pointLightPos[0] << ", "
                      << pointLightPos[1] << ", " << pointLightPos[2] << "]" << std::endl;
            std::cout << "  Distance au centre: " << distanceToCenter << std::endl;
            std::cout << "  Atténuation: " << attenuation << " (C=" << constant
                      << ", L=" << linear << ", Q=" << quadratic << ")" << std::endl;
            std::cout << "  Couleur: [" << lightColor[0] << ", "
                      << lightColor[1] << ", " << lightColor[2] << "]" << std::endl;
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "AttenuatedPointLightCubes::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        // Effacer les buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Activer les textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glUniform1i(diffuseMapUniformLoc, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);
        glUniform1i(specularMapUniformLoc, 1);

        // Calculer et passer les matrices
        updateViewProjectionMatrices();

        // Passer les matrices au shader
        glUniformMatrix4fv(viewMatrixUniformLoc, 1, GL_FALSE, viewMatrix);
        glUniformMatrix4fv(projectionMatrixUniformLoc, 1, GL_FALSE, projectionMatrix);
        glUniform1f(timeUniformLoc, time);
        glUniform3fv(viewPosUniformLoc, 1, viewPos);

        // Passer les propriétés de la lumière avec atténuation
        glUniform3fv(lightPosUniformLoc, 1, pointLightPos);
        glUniform3fv(lightColorUniformLoc, 1, lightColor);
        glUniform1f(constantUniformLoc, constant);
        glUniform1f(linearUniformLoc, linear);
        glUniform1f(quadraticUniformLoc, quadratic);

        // Rendre chaque cube
        glBindVertexArray(VAO);
        for (const auto& cube : cubeInstances) {
            updateModelMatrix(cube);
            glUniformMatrix4fv(modelMatrixUniformLoc, 1, GL_FALSE, modelMatrix);
            glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);

        // Dessiner une sphère à la position de la lumière (visuel)
        drawLightSphere();
    }

    void PostDraw() override {}

private:
    // Shader et buffers
    GLuint shaderProgram = 0;
    GLuint lightShaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLuint lightVAO = 0;
    GLuint lightVBO = 0;

    // Textures
    GLuint diffuseMap = 0;
    GLuint specularMap = 0;

    GLsizei verticesCount = 0;
    GLsizei indicesCount = 0;

    // Variables d'animation
    float time = 0.0f;
    float cameraAngle = 0.0f;
    float pointLightAngle = 0.0f;
    float attenuationDistance = 3.0f;

    // Lumière ponctuelle avec atténuation
    float pointLightPos[3] = {2.0f, 1.5f, 2.0f};
    float lightColor[3] = {1.0f, 0.8f, 0.6f};

    // Coefficients d'atténuation (constant, linear, quadratic)
    // Ces valeurs contrôlent comment la lumière diminue avec la distance
    float constant = 1.0f;    // Composante constante
    float linear = 0.09f;     // Composante linéaire
    float quadratic = 0.032f; // Composante quadratique

    // Pour démonstration, nous allons faire varier ces valeurs
    float constantValues[3] = {1.0f, 1.0f, 1.0f};
    float linearValues[3] = {0.7f, 0.35f, 0.14f};
    float quadraticValues[3] = {1.8f, 0.44f, 0.07f};

    // Caméra
    float viewPos[3] = {0.0f, 3.0f, 8.0f};

    // Matrices
    float modelMatrix[16];
    float viewMatrix[16];
    float projectionMatrix[16];

    // Locations des uniforms
    GLint modelMatrixUniformLoc = -1;
    GLint viewMatrixUniformLoc = -1;
    GLint projectionMatrixUniformLoc = -1;
    GLint timeUniformLoc = -1;
    GLint diffuseMapUniformLoc = -1;
    GLint specularMapUniformLoc = -1;
    GLint viewPosUniformLoc = -1;

    // Point Light with attenuation uniforms
    GLint lightPosUniformLoc = -1;
    GLint lightColorUniformLoc = -1;
    GLint constantUniformLoc = -1;
    GLint linearUniformLoc = -1;
    GLint quadraticUniformLoc = -1;

    struct Vertex {
        float position[3];    // x, y, z
        float normal[3];      // nx, ny, nz
        float texCoord[2];    // u, v
    };

    // Instances de cubes
    std::vector<CubeInstance> cubeInstances;

    // ==================== FONCTION DE CHARGEMENT DE TEXTURE ====================
    GLuint loadTextureFromFile(const std::string& filename, bool gammaCorrection = false) {
        GLuint textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;

        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

        if (data) {
            GLenum format;
            GLenum internalFormat;

            if (nrComponents == 1) {
                format = GL_RED;
                internalFormat = GL_RED;
            }
            else if (nrComponents == 3) {
                format = GL_RGB;
                internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            }
            else if (nrComponents == 4) {
                format = GL_RGBA;
                internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            }
            else {
                std::cerr << "Format d'image non supporté: " << filename << std::endl;
                stbi_image_free(data);
                return 0;
            }

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                         format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            std::cout << "✓ Texture chargée: " << filename
                      << " (" << width << "x" << height << ")" << std::endl;

            stbi_image_free(data);
        } else {
            std::cerr << "✗ Échec du chargement: " << filename << std::endl;
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        return textureID;
    }

    // ==================== FONCTION POUR SPECULAR MAP ====================
    GLuint createProceduralSpecularMap() {
        const int texWidth = 256;
        const int texHeight = 256;
        std::vector<unsigned char> textureData(texWidth * texHeight * 3);

        for (int y = 0; y < texHeight; ++y) {
            for (int x = 0; x < texWidth; ++x) {
                int idx = (y * texWidth + x) * 3;

                int brickWidth = texWidth / 8;
                int brickHeight = texHeight / 4;
                int mortarThickness = brickHeight / 8;

                int brickRow = y / (brickHeight + mortarThickness);
                int brickCol = x / (brickWidth + mortarThickness);
                int localY = y % (brickHeight + mortarThickness);
                int localX = x % (brickWidth + mortarThickness);

                if (brickRow % 2 == 1) {
                    localX = (localX + brickWidth / 2) % (brickWidth + mortarThickness);
                }

                if (localY < brickHeight && localX < brickWidth) {
                    // Brique - peu brillante
                    textureData[idx] = 30 + rand() % 20;
                    textureData[idx + 1] = 30 + rand() % 20;
                    textureData[idx + 2] = 30 + rand() % 20;
                } else {
                    // Mortier - brillant
                    textureData[idx] = 180 + rand() % 40;
                    textureData[idx + 1] = 180 + rand() % 40;
                    textureData[idx + 2] = 180 + rand() % 40;
                }
            }
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight,
                     0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        return textureID;
    }

    // ==================== FONCTIONS MATHÉMATIQUES POUR MATRICES ====================
    void identityMatrix(float* m) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    void rotateXMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
    }

    void rotateYMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
    }

    void rotateZMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        m[0] = c; m[1] = s; m[4] = -s; m[5] = c;
    }

    void translateMatrix(float* m, float x, float y, float z) {
        identityMatrix(m);
        m[12] = x; m[13] = y; m[14] = z;
    }

    void scaleMatrix(float* m, float sx, float sy, float sz) {
        identityMatrix(m);
        m[0] = sx; m[5] = sy; m[10] = sz;
    }

    void perspectiveMatrix(float* m, float fov, float aspect, float near, float far) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        float f = 1.0f / tanf(fov * M_PI / 360.0f);
        m[0] = f / aspect; m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    void lookAtMatrix(float* m, float eyeX, float eyeY, float eyeZ,
                     float centerX, float centerY, float centerZ,
                     float upX, float upY, float upZ) {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float fl = sqrtf(fx*fx + fy*fy + fz*fz);
        fx /= fl; fy /= fl; fz /= fl;

        float ux = upX, uy = upY, uz = upZ;
        float ul = sqrtf(ux*ux + uy*uy + uz*uz);
        ux /= ul; uy /= ul; uz /= ul;

        float rx = fy * uz - fz * uy;
        float ry = fz * ux - fx * uz;
        float rz = fx * uy - fy * ux;

        ux = ry * fz - rz * fy;
        uy = rz * fx - rx * fz;
        uz = rx * fy - ry * fx;

        m[0] = rx; m[4] = ry; m[8] = rz; m[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
        m[1] = ux; m[5] = uy; m[9] = uz; m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
        m[2] = -fx; m[6] = -fy; m[10] = -fz; m[14] = (fx*eyeX + fy*eyeY + fz*eyeZ);
        m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1.0f;
    }

    void multiplyMatrices(float* result, const float* a, const float* b) {
        float temp[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                temp[i*4 + j] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    temp[i*4 + j] += a[i*4 + k] * b[k*4 + j];
                }
            }
        }
        memcpy(result, temp, 16 * sizeof(float));
    }

    void updateModelMatrix(const CubeInstance& cube) {
        float translate[16], scaleMat[16];
        float rotX[16], rotY[16], rotZ[16];
        float temp1[16], temp2[16];

        translateMatrix(translate, cube.position[0], cube.position[1], cube.position[2]);
        scaleMatrix(scaleMat, cube.scale, cube.scale, cube.scale);
        rotateXMatrix(rotX, cube.rotationAngleX);
        rotateYMatrix(rotY, cube.rotationAngleY);
        rotateZMatrix(rotZ, cube.rotationAngleZ);

        multiplyMatrices(temp1, scaleMat, rotX);
        multiplyMatrices(temp2, rotY, temp1);
        multiplyMatrices(temp1, rotZ, temp2);
        multiplyMatrices(modelMatrix, translate, temp1);
    }

    void updateViewProjectionMatrices() {
        float eyeX = viewPos[0];
        float eyeY = viewPos[1];
        float eyeZ = viewPos[2];

        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;

        lookAtMatrix(viewMatrix,
                    eyeX, eyeY, eyeZ,
                    centerX, centerY, centerZ,
                    0.0f, 1.0f, 0.0f);

        perspectiveMatrix(projectionMatrix, 45.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    }

    void createInstances() {
        cubeInstances.clear();

        // Créer des cubes en cercle pour mieux voir l'atténuation
        int cubeCount = 12;
        float radius = 5.0f;

        for (int i = 0; i < cubeCount; i++) {
            CubeInstance cube;

            float angle = (float)i / cubeCount * 2.0f * M_PI;
            cube.position[0] = radius * cosf(angle);
            cube.position[1] = 0.0f;
            cube.position[2] = radius * sinf(angle);

            cube.rotationAngleX = static_cast<float>(rand() % 360);
            cube.rotationAngleY = static_cast<float>(rand() % 360);
            cube.rotationAngleZ = static_cast<float>(rand() % 360);

            cube.rotationSpeedX = 5.0f + static_cast<float>(rand() % 10);
            cube.rotationSpeedY = 5.0f + static_cast<float>(rand() % 10);
            cube.rotationSpeedZ = 5.0f + static_cast<float>(rand() % 10);

            cube.scale = 0.6f;

            cubeInstances.push_back(cube);
        }

        std::cout << "✓ Créé " << cubeInstances.size() << " cubes en cercle" << std::endl;
    }

    void createLightSphere() {
        // Créer une sphère simple pour représenter la lumière
        std::vector<float> sphereVertices;

        const int stacks = 16;
        const int slices = 16;
        const float radius = 0.2f;

        for (int i = 0; i <= stacks; ++i) {
            float lat0 = M_PI * (-0.5f + (float)(i - 1) / stacks);
            float z0 = sinf(lat0);
            float zr0 = cosf(lat0);

            float lat1 = M_PI * (-0.5f + (float)i / stacks);
            float z1 = sinf(lat1);
            float zr1 = cosf(lat1);

            for (int j = 0; j <= slices; ++j) {
                float lng = 2.0f * M_PI * (float)(j - 1) / slices;
                float x = cosf(lng);
                float y = sinf(lng);

                sphereVertices.push_back(x * zr0 * radius);
                sphereVertices.push_back(y * zr0 * radius);
                sphereVertices.push_back(z0 * radius);

                sphereVertices.push_back(x * zr1 * radius);
                sphereVertices.push_back(y * zr1 * radius);
                sphereVertices.push_back(z1 * radius);
            }
        }

        glGenVertexArrays(1, &lightVAO);
        glGenBuffers(1, &lightVBO);

        glBindVertexArray(lightVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
        glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float),
                    sphereVertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void drawLightSphere() {
        // Dessiner une sphère à la position de la lumière
        glUseProgram(lightShaderProgram);

        // Matrice modèle pour la sphère de lumière
        float lightModelMatrix[16];
        identityMatrix(lightModelMatrix);

        // Translation à la position de la lumière
        lightModelMatrix[12] = pointLightPos[0];
        lightModelMatrix[13] = pointLightPos[1];
        lightModelMatrix[14] = pointLightPos[2];

        // Petite échelle
        lightModelMatrix[0] = 0.2f;
        lightModelMatrix[5] = 0.2f;
        lightModelMatrix[10] = 0.2f;

        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "uModel"), 1, GL_FALSE, lightModelMatrix);
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "uView"), 1, GL_FALSE, viewMatrix);
        glUniformMatrix4fv(glGetUniformLocation(lightShaderProgram, "uProjection"), 1, GL_FALSE, projectionMatrix);
        glUniform3fv(glGetUniformLocation(lightShaderProgram, "uLightColor"), 1, lightColor);

        glBindVertexArray(lightVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * (16 + 1) * 16);
        glBindVertexArray(0);
    }

    GLuint createLightShaderProgram() {
        // Shader simple pour la sphère de lumière
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec3 aPosition;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;

            void main() {
                gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
            }
        )";

        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            out vec4 FragColor;
            uniform vec3 uLightColor;

            void main() {
                FragColor = vec4(uLightColor, 1.0);
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error(std::string("Erreur linkage light shader:\n") + std::string(infoLog));
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
    }

    void initGL() {
        std::cout << "Initializing Attenuated Point Light Cubes..." << std::endl;
        std::cout << "Étape 4: Point Light with Attenuation" << std::endl;
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

        // ==================== CHARGEMENT DES TEXTURES ====================
        std::cout << "\n--- CHARGEMENT DES TEXTURES ---" << std::endl;

        // Charger votre texture brickwall.jpg
        std::cout << "1. Chargement de la diffuse map..." << std::endl;
        diffuseMap = loadTextureFromFile("data/textures/brickwall.jpg", true);

        if (diffuseMap == 0) {
            // Essayer d'autres chemins
            const char* paths[] = {
                "data/textures/brickwall.jpg",
                "../textures/brickwall.jpg",
                "textures/brickwall.jpg",
                "brickwall.jpg"
            };

            for (const auto& path : paths) {
                diffuseMap = loadTextureFromFile(path, true);
                if (diffuseMap != 0) break;
            }
        }

        if (diffuseMap == 0) {
            std::cout << "   → Création texture de secours..." << std::endl;
            // Créer texture procédurale...
        }

        std::cout << "\n2. Création de la specular map..." << std::endl;
        specularMap = createProceduralSpecularMap();

        // Création des shaders
        shaderProgram = createShaderProgram();
        lightShaderProgram = createLightShaderProgram();

        // Créer la sphère pour la lumière
        createLightSphere();

        // Création des données du cube
        createCubeData();

        // Création du VAO/VBO/EBO
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     cubeVertices.size() * sizeof(Vertex),
                     cubeVertices.data(),
                     GL_STATIC_DRAW);

        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     cubeIndices.size() * sizeof(GLuint),
                     cubeIndices.data(),
                     GL_STATIC_DRAW);

        // Configuration des attributs
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Configuration OpenGL
        glClearColor(0.03f, 0.03f, 0.05f, 1.0f);  // Fond très sombre
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        std::cout << "\n✓ Attenuated Point Light Cubes initialized successfully!" << std::endl;

        // Afficher les informations sur l'atténuation
        std::cout << "\n--- COEFFICIENTS D'ATTÉNUATION ---" << std::endl;
        std::cout << "Formule: attenuation = 1.0 / (constant + linear*distance + quadratic*distance²)" << std::endl;
        std::cout << "Valeurs actuelles:" << std::endl;
        std::cout << "  Constant: " << constant << std::endl;
        std::cout << "  Linear: " << linear << std::endl;
        std::cout << "  Quadratic: " << quadratic << std::endl;
        std::cout << "\nObservez comment la lumière s'atténue avec la distance!" << std::endl;
    }

    void createCubeData() {
        float half = 0.5f;

        // Cube avec 6 faces
        // Face avant
        cubeVertices.push_back({{-half, -half,  half}, {0, 0, 1}, {0, 0}});
        cubeVertices.push_back({{ half, -half,  half}, {0, 0, 1}, {1, 0}});
        cubeVertices.push_back({{ half,  half,  half}, {0, 0, 1}, {1, 1}});
        cubeVertices.push_back({{-half,  half,  half}, {0, 0, 1}, {0, 1}});

        // Face arrière
        cubeVertices.push_back({{ half, -half, -half}, {0, 0, -1}, {0, 0}});
        cubeVertices.push_back({{-half, -half, -half}, {0, 0, -1}, {1, 0}});
        cubeVertices.push_back({{-half,  half, -half}, {0, 0, -1}, {1, 1}});
        cubeVertices.push_back({{ half,  half, -half}, {0, 0, -1}, {0, 1}});

        // Face droite
        cubeVertices.push_back({{ half, -half,  half}, {1, 0, 0}, {0, 0}});
        cubeVertices.push_back({{ half, -half, -half}, {1, 0, 0}, {1, 0}});
        cubeVertices.push_back({{ half,  half, -half}, {1, 0, 0}, {1, 1}});
        cubeVertices.push_back({{ half,  half,  half}, {1, 0, 0}, {0, 1}});

        // Face gauche
        cubeVertices.push_back({{-half, -half, -half}, {-1, 0, 0}, {0, 0}});
        cubeVertices.push_back({{-half, -half,  half}, {-1, 0, 0}, {1, 0}});
        cubeVertices.push_back({{-half,  half,  half}, {-1, 0, 0}, {1, 1}});
        cubeVertices.push_back({{-half,  half, -half}, {-1, 0, 0}, {0, 1}});

        // Face supérieure
        cubeVertices.push_back({{-half,  half,  half}, {0, 1, 0}, {0, 0}});
        cubeVertices.push_back({{ half,  half,  half}, {0, 1, 0}, {1, 0}});
        cubeVertices.push_back({{ half,  half, -half}, {0, 1, 0}, {1, 1}});
        cubeVertices.push_back({{-half,  half, -half}, {0, 1, 0}, {0, 1}});

        // Face inférieure
        cubeVertices.push_back({{-half, -half, -half}, {0, -1, 0}, {0, 0}});
        cubeVertices.push_back({{ half, -half, -half}, {0, -1, 0}, {1, 0}});
        cubeVertices.push_back({{ half, -half,  half}, {0, -1, 0}, {1, 1}});
        cubeVertices.push_back({{-half, -half,  half}, {0, -1, 0}, {0, 1}});

        // Indices
        for (int face = 0; face < 6; face++) {
            int base = face * 4;
            cubeIndices.push_back(base);
            cubeIndices.push_back(base + 1);
            cubeIndices.push_back(base + 2);
            cubeIndices.push_back(base);
            cubeIndices.push_back(base + 2);
            cubeIndices.push_back(base + 3);
        }

        verticesCount = static_cast<GLsizei>(cubeVertices.size());
        indicesCount = static_cast<GLsizei>(cubeIndices.size());
    }

    void cleanup() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (EBO != 0) glDeleteBuffers(1, &EBO);
        if (lightVAO != 0) glDeleteVertexArrays(1, &lightVAO);
        if (lightVBO != 0) glDeleteBuffers(1, &lightVBO);
        if (diffuseMap != 0) glDeleteTextures(1, &diffuseMap);
        if (specularMap != 0) glDeleteTextures(1, &specularMap);
        if (shaderProgram != 0) glDeleteProgram(shaderProgram);
        if (lightShaderProgram != 0) glDeleteProgram(lightShaderProgram);
    }

    GLuint compileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* sourceCStr = source.c_str();
        glShaderSource(shader, 1, &sourceCStr, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::string shaderType = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            std::string errorMsg = "Erreur compilation " + shaderType + ":\n" + std::string(infoLog);
            throw std::runtime_error(errorMsg);
        }

        return shader;
    }

    GLuint createShaderProgram() {
        std::cout << "\n--- CRÉATION DU SHADER AVEC ATTÉNUATION ---" << std::endl;

        // Vertex Shader
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoord;

            out vec3 FragPos;
            out vec3 Normal;
            out vec2 TexCoord;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;

            void main() {
                FragPos = vec3(uModel * vec4(aPosition, 1.0));
                Normal = mat3(transpose(inverse(uModel))) * aNormal;
                TexCoord = aTexCoord;

                gl_Position = uProjection * uView * vec4(FragPos, 1.0);
            }
        )";

        // Fragment Shader avec ATTÉNUATION RÉALISTE
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 FragPos;
            in vec3 Normal;
            in vec2 TexCoord;

            out vec4 FragColor;

            uniform float uTime;
            uniform vec3 uViewPos;
            uniform sampler2D uDiffuseMap;
            uniform sampler2D uSpecularMap;

            // Point Light with attenuation
            uniform vec3 uLightPos;
            uniform vec3 uLightColor;
            uniform float uConstant;
            uniform float uLinear;
            uniform float uQuadratic;

            void main() {
                // Propriétés des matériaux
                vec3 diffuseColor = texture(uDiffuseMap, TexCoord).rgb;
                vec3 specularColor = texture(uSpecularMap, TexCoord).rgb;

                vec3 norm = normalize(Normal);
                vec3 viewDir = normalize(uViewPos - FragPos);

                // ========== CALCUL DE L'ATTÉNUATION ==========
                float distance = length(uLightPos - FragPos);
                float attenuation = 1.0 / (uConstant + uLinear * distance + uQuadratic * distance * distance);

                // ========== ÉCLAIRAGE AVEC ATTÉNUATION ==========
                // Ambient
                vec3 ambient = uLightColor * 0.1;

                // Diffuse
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * uLightColor;

                // Specular
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                vec3 specular = spec * uLightColor * specularColor;

                // ========== APPLIQUER L'ATTÉNUATION ==========
                ambient *= attenuation;
                diffuse *= attenuation;
                specular *= attenuation;

                // ========== RÉSULTAT FINAL ==========
                vec3 result = (ambient + diffuse + specular) * diffuseColor;

                // Effet visuel supplémentaire
                float edgeGlow = 1.0 - smoothstep(0.0, 15.0, distance);
                result += edgeGlow * 0.1 * uLightColor;

                // Gamma correction
                result = pow(result, vec3(1.0 / 2.2));

                FragColor = vec4(result, 1.0);
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error(std::string("Erreur linkage shader:\n") + std::string(infoLog));
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Récupérer les locations des uniforms
        modelMatrixUniformLoc = glGetUniformLocation(program, "uModel");
        viewMatrixUniformLoc = glGetUniformLocation(program, "uView");
        projectionMatrixUniformLoc = glGetUniformLocation(program, "uProjection");
        timeUniformLoc = glGetUniformLocation(program, "uTime");
        diffuseMapUniformLoc = glGetUniformLocation(program, "uDiffuseMap");
        specularMapUniformLoc = glGetUniformLocation(program, "uSpecularMap");
        viewPosUniformLoc = glGetUniformLocation(program, "uViewPos");

        // Point Light with attenuation uniforms
        lightPosUniformLoc = glGetUniformLocation(program, "uLightPos");
        lightColorUniformLoc = glGetUniformLocation(program, "uLightColor");
        constantUniformLoc = glGetUniformLocation(program, "uConstant");
        linearUniformLoc = glGetUniformLocation(program, "uLinear");
        quadraticUniformLoc = glGetUniformLocation(program, "uQuadratic");

        std::cout << "✓ Shader avec atténuation créé avec succès!" << std::endl;

        return program;
    }

    // Données du cube
    std::vector<Vertex> cubeVertices;
    std::vector<GLuint> cubeIndices;
};

std::unique_ptr<AttenuatedPointLightCubes> g_attenuatedPointLightCubes;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "OBJECTIF 2 - ÉTAPE 4: POINT LIGHT WITH ATTENUATION" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Caractéristiques:" << std::endl;
        std::cout << " - 12 cubes en cercle avec rotation" << std::endl;
        std::cout << " - VOTRE texture brickwall.jpg" << std::endl;
        std::cout << " - POINT LIGHT avec ATTÉNUATION RÉALISTE" << std::endl;
        std::cout << " - Formule: 1.0 / (C + L*d + Q*d²)" << std::endl;
        std::cout << " - Sphère visible pour la source lumineuse" << std::endl;
        std::cout << " - Lumière colorée animée" << std::endl;
        std::cout << " - Caméra orbitale" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "OBSERVEZ:" << std::endl;
        std::cout << " - Les cubes proches de la lumière sont bien éclairés" << std::endl;
        std::cout << " - Les cubes éloignés sont dans l'ombre" << std::endl;
        std::cout << " - La lumière s'atténue RÉALISTEMENT avec la distance" << std::endl;
        std::cout << " - La sphère jaune représente la source lumineuse" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "INFO: L'atténuation est affichée dans la console" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Objectif 2 - Étape 4: Point Light with Attenuation";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de l'application
        g_attenuatedPointLightCubes = std::make_unique<AttenuatedPointLightCubes>();

        // Enregistrement dans le moteur
        common::SystemObserverSubject::AddObserver(g_attenuatedPointLightCubes.get());
        common::DrawObserverSubject::AddObserver(g_attenuatedPointLightCubes.get());

        // Lancement du moteur
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_attenuatedPointLightCubes.get());
        common::DrawObserverSubject::RemoveObserver(g_attenuatedPointLightCubes.get());
        g_attenuatedPointLightCubes.reset();

        std::cout << "\n✓ Étape 4 terminée avec succès!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_attenuatedPointLightCubes) {
            common::SystemObserverSubject::RemoveObserver(g_attenuatedPointLightCubes.get());
            common::DrawObserverSubject::RemoveObserver(g_attenuatedPointLightCubes.get());
            g_attenuatedPointLightCubes.reset();
        }
        return -1;
    }

    return 0;
}