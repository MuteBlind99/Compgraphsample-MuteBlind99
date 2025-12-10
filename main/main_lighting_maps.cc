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

class AdvancedLightingCubes : public common::DrawInterface, public common::SystemInterface {
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
        std::cout << "AdvancedLightingCubes::Begin() - Étape 2: Lighting Maps" << std::endl;
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

        // Animation de la lumière
        lightAngle += lightRotationSpeed * dt;
        if (lightAngle > 360.0f) lightAngle -= 360.0f;

        // Mettre à jour la position de la lumière (circulaire)
        lightPos[0] = 3.0f * sinf(lightAngle * M_PI / 180.0f);
        lightPos[1] = 2.0f;
        lightPos[2] = 3.0f * cosf(lightAngle * M_PI / 180.0f);

        // Mettre à jour la position de la caméra (vue)
        viewPos[0] = 5.0f * sinf(cameraAngle * M_PI / 180.0f);
        viewPos[1] = 2.0f;
        viewPos[2] = 5.0f * cosf(cameraAngle * M_PI / 180.0f);
        cameraAngle += cameraRotationSpeed * dt;
        if (cameraAngle > 360.0f) cameraAngle -= 360.0f;

        // Affichage périodique
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 3.0f) {
            infoTimer = 0.0f;
            std::cout << "Light Position: ["
                      << lightPos[0] << ", " << lightPos[1] << ", " << lightPos[2] << "]" << std::endl;
            std::cout << "Camera Position: ["
                      << viewPos[0] << ", " << viewPos[1] << ", " << viewPos[2] << "]" << std::endl;
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "AdvancedLightingCubes::End()" << std::endl;
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

        // Passer les propriétés de la lumière
        glUniform3fv(lightPosUniformLoc, 1, lightPos);
        glUniform3fv(lightColorUniformLoc, 1, lightColor);
        glUniform3fv(viewPosUniformLoc, 1, viewPos);

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
    float cameraRotationSpeed = 15.0f;

    // Lumière
    float lightPos[3] = {2.0f, 2.0f, 2.0f};
    float lightColor[3] = {1.0f, 1.0f, 1.0f};
    float lightAngle = 0.0f;
    float lightRotationSpeed = 30.0f;
    float viewPos[3] = {0.0f, 2.0f, 5.0f};

    // Caméra
    float cameraDistance = 8.0f;
    float cameraHeight = 2.0f;

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
    GLint lightPosUniformLoc = -1;
    GLint lightColorUniformLoc = -1;
    GLint viewPosUniformLoc = -1;

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

        // Charger l'image avec stb_image
        stbi_set_flip_vertically_on_load(true);  // OpenGL attend l'origine en bas
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
                std::cerr << "Format d'image non supporté: " << filename
                          << " (" << nrComponents << " composants)" << std::endl;
                stbi_image_free(data);
                return 0;
            }

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                         format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            // Configuration du filtrage
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            std::cout << "✓ Texture chargée: " << filename
                      << " (" << width << "x" << height
                      << ", " << nrComponents << " composants)" << std::endl;

            stbi_image_free(data);
        } else {
            std::cerr << "✗ Échec du chargement de la texture: " << filename << std::endl;
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

                // Créer un motif de briques pour la specular map
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
                    // Brique - peu brillante (sombre)
                    textureData[idx] = 30 + rand() % 20;      // R
                    textureData[idx + 1] = 30 + rand() % 20;  // G
                    textureData[idx + 2] = 30 + rand() % 20;  // B
                } else {
                    // Mortier - plus brillant (clair)
                    textureData[idx] = 180 + rand() % 40;     // R
                    textureData[idx + 1] = 180 + rand() % 40; // G
                    textureData[idx + 2] = 180 + rand() % 40; // B
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

        std::cout << "✓ Specular map procédurale créée (256x256)" << std::endl;
        return textureID;
    }

    // ==================== FONCTION POUR TEXTURE UNIFORME GRISE ====================
    GLuint createGreyTexture(unsigned char greyValue) {
        const int texWidth = 256;
        const int texHeight = 256;
        std::vector<unsigned char> textureData(texWidth * texHeight * 3, greyValue);

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight,
                     0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "✓ Texture grise uniforme créée (valeur: " << (int)greyValue << ")" << std::endl;
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
        // Forward vector
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float fl = sqrtf(fx*fx + fy*fy + fz*fz);
        fx /= fl; fy /= fl; fz /= fl;

        // Up vector
        float ux = upX, uy = upY, uz = upZ;
        float ul = sqrtf(ux*ux + uy*uy + uz*uz);
        ux /= ul; uy /= ul; uz /= ul;

        // Right vector = forward x up
        float rx = fy * uz - fz * uy;
        float ry = fz * ux - fx * uz;
        float rz = fx * uy - fy * ux;

        // Recompute up = right x forward
        ux = ry * fz - rz * fy;
        uy = rz * fx - rx * fz;
        uz = rx * fy - ry * fx;

        // Fill matrix
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
        // Caméra qui tourne autour de la scène
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
        int cubeCount = 4;  // 4 cubes pour démonstration

        // Positions en ligne
        for (int i = 0; i < cubeCount; i++) {
            CubeInstance cube;

            cube.position[0] = (i - cubeCount/2.0f + 0.5f) * 3.0f;
            cube.position[1] = 0.0f;
            cube.position[2] = 0.0f;

            cube.rotationAngleX = static_cast<float>(rand() % 360);
            cube.rotationAngleY = static_cast<float>(rand() % 360);
            cube.rotationAngleZ = static_cast<float>(rand() % 360);

            cube.rotationSpeedX = 10.0f + static_cast<float>(rand() % 20);
            cube.rotationSpeedY = 10.0f + static_cast<float>(rand() % 20);
            cube.rotationSpeedZ = 10.0f + static_cast<float>(rand() % 20);

            cube.scale = 0.8f;

            cubeInstances.push_back(cube);
        }

        std::cout << "✓ Créé " << cubeInstances.size() << " cubes avec lighting maps" << std::endl;
    }

    void initGL() {
        std::cout << "Initializing Advanced Lighting Cubes..." << std::endl;
        std::cout << "Étape 2: Lighting Maps (Diffuse + Specular)" << std::endl;
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

        // ==================== CHARGEMENT DE VOTRE TEXTURE ====================
        std::cout << "\n--- CHARGEMENT DES TEXTURES ---" << std::endl;

        // 1. Charger VOTRE texture brickwall.jpg comme diffuse map
        std::cout << "1. Chargement de brickwall.jpg comme diffuse map..." << std::endl;

        // Liste des chemins possibles (essayez dans cet ordre)
        const char* possiblePaths[] = {
            "../data/textures/brickwall.jpg",  // VOTRE STRUCTURE
            "data/textures/brickwall.jpg",
            "../textures/brickwall.jpg",
            "textures/brickwall.jpg",
            "brickwall.jpg",
            "./brickwall.jpg",
            "assets/brickwall.jpg",
            "resources/brickwall.jpg",
            "brickwall.png",
            "brick_wall.jpg",
            "brick_wall.png"
        };

        bool textureLoaded = false;
        for (const auto& path : possiblePaths) {
            std::cout << "   Essai: " << path << "..." << std::endl;
            diffuseMap = loadTextureFromFile(path, true);  // true = gamma correction
            if (diffuseMap != 0) {
                textureLoaded = true;
                break;
            }
        }

        if (!textureLoaded) {
            std::cout << "   ✗ Aucun fichier brickwall.jpg trouvé" << std::endl;
            std::cout << "   → Création d'une texture de secours..." << std::endl;

            // Créer une texture procédurale de briques comme secours
            const int texWidth = 256;
            const int texHeight = 256;
            std::vector<unsigned char> textureData(texWidth * texHeight * 3);

            for (int y = 0; y < texHeight; ++y) {
                for (int x = 0; x < texWidth; ++x) {
                    int idx = (y * texWidth + x) * 3;

                    // Texture de briques de secours
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
                        // Couleur de brique rouge/brun
                        textureData[idx] = 180 + rand() % 40;
                        textureData[idx + 1] = 70 + rand() % 40;
                        textureData[idx + 2] = 50 + rand() % 30;
                    } else {
                        // Mortier gris
                        textureData[idx] = 200;
                        textureData[idx + 1] = 200;
                        textureData[idx + 2] = 200;
                    }
                }
            }

            glGenTextures(1, &diffuseMap);
            glBindTexture(GL_TEXTURE_2D, diffuseMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight,
                         0, GL_RGB, GL_UNSIGNED_BYTE, textureData.data());
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, 0);
            std::cout << "   ✓ Texture de secours créée (256x256)" << std::endl;
        }

        // 2. Créer la specular map
        std::cout << "\n2. Création de la specular map..." << std::endl;
        std::cout << "   Choisissez une option:" << std::endl;
        std::cout << "   [1] Specular map procédurale (mortier brillant, briques mates)" << std::endl;
        std::cout << "   [2] Texture grise uniforme (tout également brillant)" << std::endl;
        std::cout << "   [3] Utiliser la même texture que la diffuse map" << std::endl;

        // Par défaut, on utilise l'option 1
        int specularOption = 1;  // Changez ceci pour tester les différentes options

        switch (specularOption) {
            case 1:
                specularMap = createProceduralSpecularMap();
                break;
            case 2:
                specularMap = createGreyTexture(128);  // Gris moyen
                break;
            case 3:
                specularMap = diffuseMap;  // Même texture
                std::cout << "   ✓ Utilisation de la même texture pour specular map" << std::endl;
                break;
            default:
                specularMap = createProceduralSpecularMap();
        }

        std::cout << "\n--- TEXTURES PRÊTES ---" << std::endl;
        std::cout << "Diffuse Map:  Texture unit 0" << std::endl;
        std::cout << "Specular Map: Texture unit 1" << std::endl;

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
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        std::cout << "\n✓ Advanced Lighting Cubes initialized successfully!" << std::endl;
    }

    void createCubeData() {
        float half = 0.5f;

        // Cube avec 6 faces, 4 sommets par face
        // Face avant (Z positif)
        cubeVertices.push_back({{-half, -half,  half}, {0, 0, 1}, {0, 0}});
        cubeVertices.push_back({{ half, -half,  half}, {0, 0, 1}, {1, 0}});
        cubeVertices.push_back({{ half,  half,  half}, {0, 0, 1}, {1, 1}});
        cubeVertices.push_back({{-half,  half,  half}, {0, 0, 1}, {0, 1}});

        // Face arrière (Z négatif)
        cubeVertices.push_back({{ half, -half, -half}, {0, 0, -1}, {0, 0}});
        cubeVertices.push_back({{-half, -half, -half}, {0, 0, -1}, {1, 0}});
        cubeVertices.push_back({{-half,  half, -half}, {0, 0, -1}, {1, 1}});
        cubeVertices.push_back({{ half,  half, -half}, {0, 0, -1}, {0, 1}});

        // Face droite (X positif)
        cubeVertices.push_back({{ half, -half,  half}, {1, 0, 0}, {0, 0}});
        cubeVertices.push_back({{ half, -half, -half}, {1, 0, 0}, {1, 0}});
        cubeVertices.push_back({{ half,  half, -half}, {1, 0, 0}, {1, 1}});
        cubeVertices.push_back({{ half,  half,  half}, {1, 0, 0}, {0, 1}});

        // Face gauche (X négatif)
        cubeVertices.push_back({{-half, -half, -half}, {-1, 0, 0}, {0, 0}});
        cubeVertices.push_back({{-half, -half,  half}, {-1, 0, 0}, {1, 0}});
        cubeVertices.push_back({{-half,  half,  half}, {-1, 0, 0}, {1, 1}});
        cubeVertices.push_back({{-half,  half, -half}, {-1, 0, 0}, {0, 1}});

        // Face supérieure (Y positif)
        cubeVertices.push_back({{-half,  half,  half}, {0, 1, 0}, {0, 0}});
        cubeVertices.push_back({{ half,  half,  half}, {0, 1, 0}, {1, 0}});
        cubeVertices.push_back({{ half,  half, -half}, {0, 1, 0}, {1, 1}});
        cubeVertices.push_back({{-half,  half, -half}, {0, 1, 0}, {0, 1}});

        // Face inférieure (Y négatif)
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
        if (specularMap != 0 && specularMap != diffuseMap) glDeleteTextures(1, &specularMap);
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
        std::cout << "\n--- CRÉATION DU SHADER ---" << std::endl;
        std::cout << "Création du shader avec lighting maps..." << std::endl;

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

        // Fragment Shader avec lighting maps
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 FragPos;
            in vec3 Normal;
            in vec2 TexCoord;

            out vec4 FragColor;

            uniform float uTime;
            uniform vec3 uLightPos;
            uniform vec3 uLightColor;
            uniform vec3 uViewPos;

            uniform sampler2D uDiffuseMap;
            uniform sampler2D uSpecularMap;

            void main() {
                // Propriétés des matériaux
                vec3 diffuseColor = texture(uDiffuseMap, TexCoord).rgb;
                vec3 specularColor = texture(uSpecularMap, TexCoord).rgb;

                // Éclairage ambiant
                float ambientStrength = 0.2;
                vec3 ambient = ambientStrength * uLightColor;

                // Éclairage diffus
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * uLightColor * diffuseColor;

                // Éclairage spéculaire
                float specularStrength = 0.8;
                vec3 viewDir = normalize(uViewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
                vec3 specular = specularStrength * spec * uLightColor * specularColor;

                // Résultat final
                vec3 result = ambient + diffuse + specular;

                // Effet visuel supplémentaire
                float distance = length(uLightPos - FragPos);
                float attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
                result *= attenuation;

                // Pulsation subtile
                float pulse = 0.95 + 0.05 * sin(uTime * 2.0);
                result *= pulse;

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
        lightPosUniformLoc = glGetUniformLocation(program, "uLightPos");
        lightColorUniformLoc = glGetUniformLocation(program, "uLightColor");
        viewPosUniformLoc = glGetUniformLocation(program, "uViewPos");

        std::cout << "✓ Shader Advanced Lighting créé avec succès!" << std::endl;

        return program;
    }

    // Données du cube
    std::vector<Vertex> cubeVertices;
    std::vector<GLuint> cubeIndices;
};

std::unique_ptr<AdvancedLightingCubes> g_advancedLightingCubes;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "OBJECTIF 2 - ÉTAPE 2: LIGHTING MAPS" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Caractéristiques:" << std::endl;
        std::cout << " - 4 cubes avec rotation individuelle" << std::endl;
        std::cout << " - Lighting Maps: Diffuse + Specular" << std::endl;
        std::cout << " - Diffuse Map: VOTRE texture brickwall.jpg" << std::endl;
        std::cout << " - Specular Map: Procédurale (mortier brillant)" << std::endl;
        std::cout << " - Point Light animée qui tourne" << std::endl;
        std::cout << " - Caméra orbitale" << std::endl;
        std::cout << " - Éclairage: Ambient + Diffuse + Specular" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "INSTRUCTIONS:" << std::endl;
        std::cout << "1. Placez brickwall.jpg dans le même dossier que l'exécutable" << std::endl;
        std::cout << "2. Sinon, modifiez le chemin dans le code (ligne ~340)" << std::endl;
        std::cout << "3. Pour changer la specular map, modifiez 'specularOption' (ligne ~420)" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Objectif 2 - Étape 2: Lighting Maps (Votre texture)";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de l'application
        g_advancedLightingCubes = std::make_unique<AdvancedLightingCubes>();

        // Enregistrement dans le moteur
        common::SystemObserverSubject::AddObserver(g_advancedLightingCubes.get());
        common::DrawObserverSubject::AddObserver(g_advancedLightingCubes.get());

        // Lancement du moteur
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_advancedLightingCubes.get());
        common::DrawObserverSubject::RemoveObserver(g_advancedLightingCubes.get());
        g_advancedLightingCubes.reset();

        std::cout << "\n✓ Étape 2 terminée avec succès!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;
        std::cerr << "Vérifiez que:" << std::endl;
        std::cerr << "1. brickwall.jpg est dans le bon dossier" << std::endl;
        std::cerr << "2. stb_image.h est accessible" << std::endl;
        std::cerr << "3. Votre GPU supporte OpenGL ES 3.0+" << std::endl;

        if (g_advancedLightingCubes) {
            common::SystemObserverSubject::RemoveObserver(g_advancedLightingCubes.get());
            common::DrawObserverSubject::RemoveObserver(g_advancedLightingCubes.get());
            g_advancedLightingCubes.reset();
        }
        return -1;
    }

    return 0;
}