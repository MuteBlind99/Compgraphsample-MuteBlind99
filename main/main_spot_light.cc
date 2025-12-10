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

class SpotLightCubes : public common::DrawInterface, public common::SystemInterface {
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
        std::cout << "SpotLightCubes::Begin() - Étape 6: Spot Light" << std::endl;
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

        // ==================== ANIMATION DES SPOT LIGHTS ====================

        // Spot Light 1: Projecteur qui tourne autour de la scène
        spotLight1Angle += 15.0f * dt;
        if (spotLight1Angle > 360.0f) spotLight1Angle -= 360.0f;

        // Position circulaire
        spotLight1Pos[0] = 5.0f * sinf(spotLight1Angle * M_PI / 180.0f);
        spotLight1Pos[1] = 4.0f;  // En hauteur
        spotLight1Pos[2] = 5.0f * cosf(spotLight1Angle * M_PI / 180.0f);

        // Dirige vers le centre de la scène
        spotLight1Dir[0] = -spotLight1Pos[0];
        spotLight1Dir[1] = -spotLight1Pos[1] + 1.0f;  // Un peu vers le bas
        spotLight1Dir[2] = -spotLight1Pos[2];

        // Normaliser la direction
        normalizeVector(spotLight1Dir);

        // Animation de la couleur
        spotLight1Color[0] = 0.8f + 0.2f * sinf(time * 1.5f);
        spotLight1Color[1] = 0.6f + 0.4f * sinf(time * 2.0f);
        spotLight1Color[2] = 0.7f + 0.3f * sinf(time * 1.0f);

        // Spot Light 2: Projecteur fixe avec mouvement de balayage
        spotLight2Angle += 25.0f * dt;
        if (spotLight2Angle > 360.0f) spotLight2Angle -= 360.0f;

        // Position fixe en hauteur
        spotLight2Pos[0] = -6.0f;
        spotLight2Pos[1] = 5.0f;
        spotLight2Pos[2] = 0.0f;

        // Direction qui balaie de gauche à droite
        spotLight2Dir[0] = cosf(spotLight2Angle * M_PI / 180.0f);
        spotLight2Dir[1] = -0.5f;  // Un peu vers le bas
        spotLight2Dir[2] = sinf(spotLight2Angle * M_PI / 180.0f);

        normalizeVector(spotLight2Dir);

        // Couleur fixe (blanc bleuté)
        spotLight2Color[0] = 0.7f;
        spotLight2Color[1] = 0.8f;
        spotLight2Color[2] = 1.0f;

        // Spot Light 3: Projecteur au sol qui éclaire vers le haut
        spotLight3Angle += 10.0f * dt;
        if (spotLight3Angle > 360.0f) spotLight3Angle -= 360.0f;

        // Position au sol
        spotLight3Pos[0] = 0.0f;
        spotLight3Pos[1] = 0.5f;
        spotLight3Pos[2] = 6.0f;

        // Direction qui tourne vers le haut
        spotLight3Dir[0] = sinf(spotLight3Angle * M_PI / 180.0f) * 0.3f;
        spotLight3Dir[1] = 1.0f;  // Principalement vers le haut
        spotLight3Dir[2] = cosf(spotLight3Angle * M_PI / 180.0f) * 0.3f;

        normalizeVector(spotLight3Dir);

        // Couleur chaude (jaune/orange)
        spotLight3Color[0] = 1.0f;
        spotLight3Color[1] = 0.6f + 0.2f * sinf(time * 3.0f);
        spotLight3Color[2] = 0.2f;

        // Animation des angles des cônes (pour effet visuel)
        spotLight1CutOff = 10.0f + 2.0f * sinf(time * 2.0f);
        spotLight1OuterCutOff = spotLight1CutOff + 5.0f;

        spotLight2CutOff = 8.0f;
        spotLight2OuterCutOff = 15.0f;

        spotLight3CutOff = 15.0f;
        spotLight3OuterCutOff = 25.0f;

        // Animation de la caméra
        cameraAngle += 8.0f * dt;
        if (cameraAngle > 360.0f) cameraAngle -= 360.0f;

        cameraPos[0] = 12.0f * sinf(cameraAngle * M_PI / 180.0f);
        cameraPos[1] = 6.0f;
        cameraPos[2] = 12.0f * cosf(cameraAngle * M_PI / 180.0f);

        // Affichage périodique des informations
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 3.0f) {
            infoTimer = 0.0f;

            std::cout << "=== SPOT LIGHTS INFO ===" << std::endl;
            std::cout << "Spot Light 1 (Tournante):" << std::endl;
            std::cout << "  Position: [" << spotLight1Pos[0] << ", "
                      << spotLight1Pos[1] << ", " << spotLight1Pos[2] << "]" << std::endl;
            std::cout << "  Direction: [" << spotLight1Dir[0] << ", "
                      << spotLight1Dir[1] << ", " << spotLight1Dir[2] << "]" << std::endl;
            std::cout << "  Cône: " << spotLight1CutOff << "° / " << spotLight1OuterCutOff << "°" << std::endl;

            std::cout << "\nSpot Light 2 (Balançante):" << std::endl;
            std::cout << "  Position: [" << spotLight2Pos[0] << ", "
                      << spotLight2Pos[1] << ", " << spotLight2Pos[2] << "]" << std::endl;
            std::cout << "  Direction: [" << spotLight2Dir[0] << ", "
                      << spotLight2Dir[1] << ", " << spotLight2Dir[2] << "]" << std::endl;

            std::cout << "\nSpot Light 3 (Sol):" << std::endl;
            std::cout << "  Position: [" << spotLight3Pos[0] << ", "
                      << spotLight3Pos[1] << ", " << spotLight3Pos[2] << "]" << std::endl;
            std::cout << "  Direction: [" << spotLight3Dir[0] << ", "
                      << spotLight3Dir[1] << ", " << spotLight3Dir[2] << "]" << std::endl;
            std::cout << "========================" << std::endl;
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "SpotLightCubes::End()" << std::endl;
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

        // Passer la position de la caméra
        glUniform3fv(viewPosUniformLoc, 1, cameraPos);

        // ==================== PASSER LES 3 SPOT LIGHTS ====================

        // Spot Light 1
        glUniform3fv(spotLight1PosUniformLoc, 1, spotLight1Pos);
        glUniform3fv(spotLight1DirUniformLoc, 1, spotLight1Dir);
        glUniform3fv(spotLight1ColorUniformLoc, 1, spotLight1Color);
        glUniform1f(spotLight1CutOffUniformLoc, cosf(spotLight1CutOff * M_PI / 180.0f));
        glUniform1f(spotLight1OuterCutOffUniformLoc, cosf(spotLight1OuterCutOff * M_PI / 180.0f));

        // Spot Light 2
        glUniform3fv(spotLight2PosUniformLoc, 1, spotLight2Pos);
        glUniform3fv(spotLight2DirUniformLoc, 1, spotLight2Dir);
        glUniform3fv(spotLight2ColorUniformLoc, 1, spotLight2Color);
        glUniform1f(spotLight2CutOffUniformLoc, cosf(spotLight2CutOff * M_PI / 180.0f));
        glUniform1f(spotLight2OuterCutOffUniformLoc, cosf(spotLight2OuterCutOff * M_PI / 180.0f));

        // Spot Light 3
        glUniform3fv(spotLight3PosUniformLoc, 1, spotLight3Pos);
        glUniform3fv(spotLight3DirUniformLoc, 1, spotLight3Dir);
        glUniform3fv(spotLight3ColorUniformLoc, 1, spotLight3Color);
        glUniform1f(spotLight3CutOffUniformLoc, cosf(spotLight3CutOff * M_PI / 180.0f));
        glUniform1f(spotLight3OuterCutOffUniformLoc, cosf(spotLight3OuterCutOff * M_PI / 180.0f));

        // Coefficients d'atténuation (communs aux 3 spots)
        glUniform1f(constantUniformLoc, spotLightConstant);
        glUniform1f(linearUniformLoc, spotLightLinear);
        glUniform1f(quadraticUniformLoc, spotLightQuadratic);

        // Rendre chaque cube
        glBindVertexArray(VAO);
        for (const auto& cube : cubeInstances) {
            updateModelMatrix(cube);
            glUniformMatrix4fv(modelMatrixUniformLoc, 1, GL_FALSE, modelMatrix);
            glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    // Shader et buffers
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    // Textures
    GLuint diffuseMap = 0;
    GLuint specularMap = 0;

    GLsizei verticesCount = 0;
    GLsizei indicesCount = 0;

    // Variables d'animation
    float time = 0.0f;
    float cameraAngle = 0.0f;
    float spotLight1Angle = 0.0f;
    float spotLight2Angle = 0.0f;
    float spotLight3Angle = 0.0f;

    // Caméra
    float cameraPos[3] = {0.0f, 6.0f, 12.0f};
    float cameraFront[3] = {0.0f, 0.0f, -1.0f};
    float cameraUp[3] = {0.0f, 1.0f, 0.0f};

    // ==================== 3 SPOT LIGHTS DIFFÉRENTES ====================

    // Spot Light 1: Projecteur tournant coloré
    float spotLight1Pos[3] = {0.0f, 4.0f, 5.0f};
    float spotLight1Dir[3] = {0.0f, -0.5f, -1.0f};
    float spotLight1Color[3] = {1.0f, 0.5f, 0.8f};  // Rose
    float spotLight1CutOff = 10.0f;
    float spotLight1OuterCutOff = 15.0f;

    // Spot Light 2: Projecteur fixe balançant
    float spotLight2Pos[3] = {-6.0f, 5.0f, 0.0f};
    float spotLight2Dir[3] = {1.0f, -0.3f, 0.0f};
    float spotLight2Color[3] = {0.7f, 0.8f, 1.0f};  // Bleu clair
    float spotLight2CutOff = 8.0f;
    float spotLight2OuterCutOff = 15.0f;

    // Spot Light 3: Projecteur au sol
    float spotLight3Pos[3] = {0.0f, 0.5f, 6.0f};
    float spotLight3Dir[3] = {0.0f, 1.0f, -0.2f};
    float spotLight3Color[3] = {1.0f, 0.8f, 0.2f};  // Jaune/orange
    float spotLight3CutOff = 15.0f;
    float spotLight3OuterCutOff = 25.0f;

    // Atténuation commune
    float spotLightConstant = 1.0f;
    float spotLightLinear = 0.09f;
    float spotLightQuadratic = 0.032f;

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

    // Spot Light 1 uniforms
    GLint spotLight1PosUniformLoc = -1;
    GLint spotLight1DirUniformLoc = -1;
    GLint spotLight1ColorUniformLoc = -1;
    GLint spotLight1CutOffUniformLoc = -1;
    GLint spotLight1OuterCutOffUniformLoc = -1;

    // Spot Light 2 uniforms
    GLint spotLight2PosUniformLoc = -1;
    GLint spotLight2DirUniformLoc = -1;
    GLint spotLight2ColorUniformLoc = -1;
    GLint spotLight2CutOffUniformLoc = -1;
    GLint spotLight2OuterCutOffUniformLoc = -1;

    // Spot Light 3 uniforms
    GLint spotLight3PosUniformLoc = -1;
    GLint spotLight3DirUniformLoc = -1;
    GLint spotLight3ColorUniformLoc = -1;
    GLint spotLight3CutOffUniformLoc = -1;
    GLint spotLight3OuterCutOffUniformLoc = -1;

    // Atténuation uniforms
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

    // ==================== FONCTIONS UTILITAIRES ====================
    void normalizeVector(float* v) {
        float length = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (length > 0.0001f) {
            v[0] /= length;
            v[1] /= length;
            v[2] /= length;
        }
    }

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
        float eyeX = cameraPos[0];
        float eyeY = cameraPos[1];
        float eyeZ = cameraPos[2];

        // Regarde vers le centre de la scène
        float centerX = 0.0f;
        float centerY = 0.0f;
        float centerZ = 0.0f;

        lookAtMatrix(viewMatrix,
                    eyeX, eyeY, eyeZ,
                    centerX, centerY, centerZ,
                    0.0f, 1.0f, 0.0f);

        perspectiveMatrix(projectionMatrix, 60.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    }

    void createInstances() {
        cubeInstances.clear();

        // Créer une scène avec des cubes en hauteur
        int cubeCount = 20;
        float platformRadius = 6.0f;

        // Cubes sur une plateforme circulaire
        for (int i = 0; i < cubeCount; i++) {
            CubeInstance cube;

            float angle = (float)i / cubeCount * 2.0f * M_PI;
            float radius = platformRadius * (0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f);

            cube.position[0] = cosf(angle) * radius;
            cube.position[1] = static_cast<float>(rand()) / RAND_MAX * 3.0f;  // Hauteur variable
            cube.position[2] = sinf(angle) * radius;

            cube.rotationAngleX = static_cast<float>(rand() % 360);
            cube.rotationAngleY = static_cast<float>(rand() % 360);
            cube.rotationAngleZ = static_cast<float>(rand() % 360);

            cube.rotationSpeedX = 3.0f + static_cast<float>(rand() % 6);
            cube.rotationSpeedY = 3.0f + static_cast<float>(rand() % 6);
            cube.rotationSpeedZ = 3.0f + static_cast<float>(rand() % 6);

            cube.scale = 0.5f + static_cast<float>(rand() % 50) / 100.0f;

            cubeInstances.push_back(cube);
        }

        std::cout << "✓ Créé " << cubeInstances.size() << " cubes sur plateforme" << std::endl;
    }

    void initGL() {
        std::cout << "Initializing Spot Light Cubes..." << std::endl;
        std::cout << "Étape 6: Spot Light (Projecteurs)" << std::endl;
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

        // ==================== CHARGEMENT DES TEXTURES ====================
        std::cout << "\n--- CHARGEMENT DES TEXTURES ---" << std::endl;

        // Charger votre texture brickwall.jpg
        std::cout << "1. Chargement de la diffuse map..." << std::endl;
        diffuseMap = loadTextureFromFile("../data/textures/brickwall.jpg", true);

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
        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);  // Fond sombre
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        std::cout << "\n✓ Spot Light Cubes initialized successfully!" << std::endl;

        // Afficher les informations sur les spot lights
        std::cout << "\n=== SPOT LIGHTS CONFIGURATION ===" << std::endl;
        std::cout << "3 PROJECTEURS DIFFÉRENTS:" << std::endl;
        std::cout << "1. Projecteur tournant coloré (en hauteur)" << std::endl;
        std::cout << "2. Projecteur fixe balançant (côté gauche)" << std::endl;
        std::cout << "3. Projecteur au sol (éclairage vers le haut)" << std::endl;
        std::cout << "\n=== CARACTÉRISTIQUES ===" << std::endl;
        std::cout << "- Chaque spot a sa propre position/direction/couleur" << std::endl;
        std::cout << "- Effets combinés sur les cubes" << std::endl;
        std::cout << "- Cônes de lumière directionnels" << std::endl;
        std::cout << "- Atténuation réaliste" << std::endl;
        std::cout << "- Console: infos mises à jour toutes les 3 secondes" << std::endl;
        std::cout << "==================================" << std::endl;
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
        if (diffuseMap != 0) glDeleteTextures(1, &diffuseMap);
        if (specularMap != 0) glDeleteTextures(1, &specularMap);
        if (shaderProgram != 0) glDeleteProgram(shaderProgram);
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
        std::cout << "\n--- CRÉATION DU SHADER SPOT LIGHT (3 projecteurs) ---" << std::endl;

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

        // Fragment Shader avec 3 SPOT LIGHTS
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

            // ========== SPOT LIGHT STRUCTURE ==========
            struct SpotLight {
                vec3 position;
                vec3 direction;
                vec3 color;
                float cutOff;
                float outerCutOff;
            };

            // ========== 3 SPOT LIGHTS ==========
            uniform SpotLight uSpotLight1;
            uniform SpotLight uSpotLight2;
            uniform SpotLight uSpotLight3;

            // ========== ATTÉNUATION ==========
            uniform float uConstant;
            uniform float uLinear;
            uniform float uQuadratic;

            // ========== FONCTION POUR CALCULER UNE SPOT LIGHT ==========
            vec3 calculateSpotLight(SpotLight light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
                vec3 lightDir = normalize(light.position - FragPos);

                // Vérifier si dans le cône
                float theta = dot(lightDir, normalize(-light.direction));
                float epsilon = light.cutOff - light.outerCutOff;
                float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

                // Si hors du cône, pas d'éclairage
                if (theta < light.outerCutOff) {
                    return vec3(0.0);
                }

                // Atténuation
                float distance = length(light.position - FragPos);
                float attenuation = 1.0 / (uConstant + uLinear * distance + uQuadratic * distance * distance);

                // Ambient (très faible pour des spots)
                vec3 ambient = light.color * 0.05;

                // Diffuse
                float diff = max(dot(normal, lightDir), 0.0);
                vec3 diffuse = diff * light.color;

                // Specular
                vec3 reflectDir = reflect(-lightDir, normal);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                vec3 specular = spec * light.color * specularColor * 0.5;

                // Appliquer intensité et atténuation
                ambient *= attenuation * intensity;
                diffuse *= attenuation * intensity;
                specular *= attenuation * intensity;

                return (ambient + diffuse + specular) * diffuseColor;
            }

            void main() {
                // Propriétés des matériaux
                vec3 diffuseColor = texture(uDiffuseMap, TexCoord).rgb;
                vec3 specularColor = texture(uSpecularMap, TexCoord).rgb;

                vec3 norm = normalize(Normal);
                vec3 viewDir = normalize(uViewPos - FragPos);

                // ========== CALCUL DES 3 SPOT LIGHTS ==========
                vec3 result = vec3(0.0);

                // Spot Light 1
                result += calculateSpotLight(uSpotLight1, norm, viewDir, diffuseColor, specularColor);

                // Spot Light 2
                result += calculateSpotLight(uSpotLight2, norm, viewDir, diffuseColor, specularColor);

                // Spot Light 3
                result += calculateSpotLight(uSpotLight3, norm, viewDir, diffuseColor, specularColor);

                // ========== EFFETS VISUELS SUPPLÉMENTAIRES ==========

                // Lumière ambiante globale très faible
                vec3 globalAmbient = vec3(0.03, 0.03, 0.05);
                result += globalAmbient * diffuseColor;

                // Scintillement subtil
                float flicker = 0.98 + 0.02 * sin(uTime * 10.0);
                result *= flicker;

                // Effet de "bloom" pour les zones très éclairées
                float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
                if (brightness > 0.8) {
                    result = mix(result, vec3(1.0), 0.1);
                }

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

        // Spot Light 1 uniforms
        spotLight1PosUniformLoc = glGetUniformLocation(program, "uSpotLight1.position");
        spotLight1DirUniformLoc = glGetUniformLocation(program, "uSpotLight1.direction");
        spotLight1ColorUniformLoc = glGetUniformLocation(program, "uSpotLight1.color");
        spotLight1CutOffUniformLoc = glGetUniformLocation(program, "uSpotLight1.cutOff");
        spotLight1OuterCutOffUniformLoc = glGetUniformLocation(program, "uSpotLight1.outerCutOff");

        // Spot Light 2 uniforms
        spotLight2PosUniformLoc = glGetUniformLocation(program, "uSpotLight2.position");
        spotLight2DirUniformLoc = glGetUniformLocation(program, "uSpotLight2.direction");
        spotLight2ColorUniformLoc = glGetUniformLocation(program, "uSpotLight2.color");
        spotLight2CutOffUniformLoc = glGetUniformLocation(program, "uSpotLight2.cutOff");
        spotLight2OuterCutOffUniformLoc = glGetUniformLocation(program, "uSpotLight2.outerCutOff");

        // Spot Light 3 uniforms
        spotLight3PosUniformLoc = glGetUniformLocation(program, "uSpotLight3.position");
        spotLight3DirUniformLoc = glGetUniformLocation(program, "uSpotLight3.direction");
        spotLight3ColorUniformLoc = glGetUniformLocation(program, "uSpotLight3.color");
        spotLight3CutOffUniformLoc = glGetUniformLocation(program, "uSpotLight3.cutOff");
        spotLight3OuterCutOffUniformLoc = glGetUniformLocation(program, "uSpotLight3.outerCutOff");

        // Atténuation uniforms
        constantUniformLoc = glGetUniformLocation(program, "uConstant");
        linearUniformLoc = glGetUniformLocation(program, "uLinear");
        quadraticUniformLoc = glGetUniformLocation(program, "uQuadratic");

        std::cout << "✓ Shader Spot Light (3 projecteurs) créé avec succès!" << std::endl;

        return program;
    }

    // Données du cube
    std::vector<Vertex> cubeVertices;
    std::vector<GLuint> cubeIndices;
};

std::unique_ptr<SpotLightCubes> g_spotLightCubes;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "OBJECTIF 2 - ÉTAPE 6: SPOT LIGHT" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Caractéristiques:" << std::endl;
        std::cout << " - 20 cubes sur plateforme avec rotation" << std::endl;
        std::cout << " - VOTRE texture brickwall.jpg" << std::endl;
        std::cout << " - 3 SPOT LIGHTS (Projecteurs) différents:" << std::endl;
        std::cout << "   1. Projecteur tournant coloré (en hauteur)" << std::endl;
        std::cout << "   2. Projecteur fixe balançant (côté gauche)" << std::endl;
        std::cout << "   3. Projecteur au sol (éclairage vers le haut)" << std::endl;
        std::cout << " - Cônes de lumière directionnels" << std::endl;
        std::cout << " - Effets combinés des 3 lumières" << std::endl;
        std::cout << " - Atténuation réaliste" << std::endl;
        std::cout << " - Caméra orbitale haute" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "OBSERVEZ:" << std::endl;
        std::cout << " - Les 3 projecteurs ont des comportements différents" << std::endl;
        std::cout << " - Chaque projecteur crée un cône de lumière distinct" << std::endl;
        std::cout << " - Les cubes sont éclairés par la combinaison des 3 lumières" << std::endl;
        std::cout << " - Effet de scène éclairée (comme un concert ou théâtre)" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "INFO: La console affiche les paramètres toutes les 3 secondes" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Objectif 2 - Étape 6: Spot Light (3 Projecteurs)";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de l'application
        g_spotLightCubes = std::make_unique<SpotLightCubes>();

        // Enregistrement dans le moteur
        common::SystemObserverSubject::AddObserver(g_spotLightCubes.get());
        common::DrawObserverSubject::AddObserver(g_spotLightCubes.get());

        // Lancement du moteur
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_spotLightCubes.get());
        common::DrawObserverSubject::RemoveObserver(g_spotLightCubes.get());
        g_spotLightCubes.reset();

        std::cout << "\n✓ Étape 6 terminée avec succès!" << std::endl;
        std::cout << "\n🎉 FÉLICITATIONS ! 🎉" << std::endl;
        std::cout << "Vous avez complété TOUTES les étapes de l'Objectif 2 !" << std::endl;
        std::cout << "Récapitulatif des 6 étapes implémentées:" << std::endl;
        std::cout << "1. Point Light Infini ✓" << std::endl;
        std::cout << "2. Lighting Maps ✓" << std::endl;
        std::cout << "3. Directional Light ✓" << std::endl;
        std::cout << "4. Point Light with Attenuation ✓" << std::endl;
        std::cout << "5. Flash Light ✓" << std::endl;
        std::cout << "6. Spot Light ✓" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_spotLightCubes) {
            common::SystemObserverSubject::RemoveObserver(g_spotLightCubes.get());
            common::DrawObserverSubject::RemoveObserver(g_spotLightCubes.get());
            g_spotLightCubes.reset();
        }
        return -1;
    }

    return 0;
}