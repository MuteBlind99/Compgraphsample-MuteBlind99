#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
#include <corecrt_math_defines.h>

#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class MultiCube3D : public common::DrawInterface, public common::SystemInterface {
public:
    struct CubeInstance {
        float position[3];        // Position dans le monde
        float rotationAngleX;     // Rotation individuelle
        float rotationAngleY;
        float rotationAngleZ;
        float rotationSpeedX;
        float rotationSpeedY;
        float rotationSpeedZ;
        float scale;              // Échelle individuelle
        float color[3];           // Couleur de base
    };

    void Begin() override {
        std::cout << "MultiCube3D::Begin()" << std::endl;
        initGL();
        createInstances();  // Créer plusieurs instances de cubes
    }

    void Update(float dt) override {
        time += dt;

        // Mettre à jour chaque instance de cube
        for (auto& cube : cubeInstances) {
            cube.rotationAngleX += cube.rotationSpeedX * dt;
            cube.rotationAngleY += cube.rotationSpeedY * dt;
            cube.rotationAngleZ += cube.rotationSpeedZ * dt;

            // Garder les angles dans [0, 360]
            cube.rotationAngleX = fmodf(cube.rotationAngleX, 360.0f);
            cube.rotationAngleY = fmodf(cube.rotationAngleY, 360.0f);
            cube.rotationAngleZ = fmodf(cube.rotationAngleZ, 360.0f);
        }

        // Animation de la caméra orbitale
        cameraAngle += cameraOrbitSpeed * dt;
        if (cameraAngle > 360.0f) cameraAngle -= 360.0f;

        // Affichage périodique des informations
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 2.0f) {
            infoTimer = 0.0f;
            std::cout << "Nombre de cubes: " << cubeInstances.size()
                      << ", Temps: " << (int)time << "s" << std::endl;
            if (!cubeInstances.empty()) {
                std::cout << "Cube 1 - Pos: ["
                          << cubeInstances[0].position[0] << ", "
                          << cubeInstances[0].position[1] << ", "
                          << cubeInstances[0].position[2] << "]" << std::endl;
            }
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "MultiCube3D::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        // Effacer les buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Calculer et passer les matrices de vue et projection (communes à tous les cubes)
        updateViewProjectionMatrices();

        // Passer les matrices de vue et projection au shader
        if (viewMatrixUniformLoc != -1) {
            glUniformMatrix4fv(viewMatrixUniformLoc, 1, GL_FALSE, viewMatrix);
        }

        if (projectionMatrixUniformLoc != -1) {
            glUniformMatrix4fv(projectionMatrixUniformLoc, 1, GL_FALSE, projectionMatrix);
        }

        if (timeUniformLoc != -1) {
            glUniform1f(timeUniformLoc, time);
        }

        // Rendre chaque cube
        glBindVertexArray(VAO);
        for (const auto& cube : cubeInstances) {
            // Calculer la matrice modèle pour ce cube
            updateModelMatrix(cube);

            // Passer la matrice modèle au shader
            if (modelMatrixUniformLoc != -1) {
                glUniformMatrix4fv(modelMatrixUniformLoc, 1, GL_FALSE, modelMatrix);
            }

            // Passer la couleur individuelle au shader
            if (colorUniformLoc != -1) {
                glUniform3fv(colorUniformLoc, 1, cube.color);
            }

            // Dessiner le cube
            glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei verticesCount = 0;
    GLsizei indicesCount = 0;

    // Variables d'animation
    float time = 0.0f;

    // Caméra
    float cameraAngle = 0.0f;
    float cameraOrbitSpeed = 15.0f;
    float cameraDistance = 8.0f;  // Plus loin pour voir tous les cubes
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
    GLint colorUniformLoc = -1;  // Pour la couleur individuelle

    struct Vertex {
        float position[3];    // x, y, z
        float normal[3];      // nx, ny, nz
    };

    // Instances de cubes
    std::vector<CubeInstance> cubeInstances;

    // Fonctions mathématiques pour matrices (inchangées)
    void identityMatrix(float* m) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    void rotateXMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        m[5] = c;
        m[6] = s;
        m[9] = -s;
        m[10] = c;
    }

    void rotateYMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        m[0] = c;
        m[2] = -s;
        m[8] = s;
        m[10] = c;
    }

    void rotateZMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        m[0] = c;
        m[1] = s;
        m[4] = -s;
        m[5] = c;
    }

    void translateMatrix(float* m, float x, float y, float z) {
        identityMatrix(m);
        m[12] = x;
        m[13] = y;
        m[14] = z;
    }

    void scaleMatrix(float* m, float sx, float sy, float sz) {
        identityMatrix(m);
        m[0] = sx;
        m[5] = sy;
        m[10] = sz;
    }

    void perspectiveMatrix(float* m, float fov, float aspect, float near, float far) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;

        float f = 1.0f / tanf(fov * M_PI / 360.0f);

        m[0] = f / aspect;
        m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    void lookAtMatrix(float* m,
                     float eyeX, float eyeY, float eyeZ,
                     float centerX, float centerY, float centerZ,
                     float upX, float upY, float upZ) {
        // Forward vector
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;

        // Normalize forward
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
        // Matrices de transformation
        float translate[16], scaleMat[16];
        float rotX[16], rotY[16], rotZ[16];
        float temp1[16], temp2[16];

        // Créer les matrices individuelles
        translateMatrix(translate, cube.position[0], cube.position[1], cube.position[2]);
        scaleMatrix(scaleMat, cube.scale, cube.scale, cube.scale);
        rotateXMatrix(rotX, cube.rotationAngleX);
        rotateYMatrix(rotY, cube.rotationAngleY);
        rotateZMatrix(rotZ, cube.rotationAngleZ);

        // Combinaison: Modèle = Translation * RotationZ * RotationY * RotationX * Scale
        multiplyMatrices(temp1, scaleMat, rotX);
        multiplyMatrices(temp2, rotY, temp1);
        multiplyMatrices(temp1, rotZ, temp2);
        multiplyMatrices(modelMatrix, translate, temp1);
    }

    void updateViewProjectionMatrices() {
        // Matrice vue : caméra orbitale
        float eyeX = cameraDistance * sinf(cameraAngle * M_PI / 180.0f);
        float eyeY = cameraHeight;
        float eyeZ = cameraDistance * cosf(cameraAngle * M_PI / 180.0f);

        lookAtMatrix(viewMatrix,
                    eyeX, eyeY, eyeZ,    // Position caméra
                    0.0f, 0.0f, 0.0f,    // Regarde vers le centre
                    0.0f, 1.0f, 0.0f);   // Up vector

        // Matrice projection : perspective
        perspectiveMatrix(projectionMatrix, 45.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    }

    void createInstances() {
        // Créer 12 cubes à différentes positions
        cubeInstances.clear();

        // Configuration pour une grille de cubes
        int gridSize = 3;  // 3x3x3 = 27 cubes max
        float spacing = 2.5f;

        // Couleurs différentes pour chaque cube
        float colors[][3] = {
            {1.0f, 0.0f, 0.0f},   // Rouge
            {0.0f, 1.0f, 0.0f},   // Vert
            {0.0f, 0.0f, 1.0f},   // Bleu
            {1.0f, 1.0f, 0.0f},   // Jaune
            {1.0f, 0.0f, 1.0f},   // Magenta
            {0.0f, 1.0f, 1.0f},   // Cyan
            {0.5f, 0.5f, 1.0f},   // Bleu clair
            {1.0f, 0.5f, 0.0f},   // Orange
            {0.5f, 1.0f, 0.5f},   // Vert clair
            {0.8f, 0.2f, 0.8f},   // Violet
            {0.2f, 0.8f, 0.2f},   // Vert foncé
            {0.9f, 0.9f, 0.3f}    // Jaune clair
        };

        int colorIndex = 0;
        int cubeCount = 0;
        const int maxCubes = 12;  // Nombre total de cubes à créer

        for (int x = -gridSize; x <= gridSize && cubeCount < maxCubes; x++) {
            for (int y = -gridSize; y <= gridSize && cubeCount < maxCubes; y++) {
                for (int z = -gridSize; z <= gridSize && cubeCount < maxCubes; z++) {
                    // Ne pas mettre de cube au centre (pour la visibilité)
                    if (x == 0 && y == 0 && z == 0) continue;

                    CubeInstance cube;

                    // Position dans une grille
                    cube.position[0] = x * spacing;
                    cube.position[1] = y * spacing;
                    cube.position[2] = z * spacing;

                    // Rotations initiales aléatoires
                    cube.rotationAngleX = static_cast<float>(rand() % 360);
                    cube.rotationAngleY = static_cast<float>(rand() % 360);
                    cube.rotationAngleZ = static_cast<float>(rand() % 360);

                    // Vitesses de rotation différentes pour chaque cube
                    cube.rotationSpeedX = 10.0f + static_cast<float>(rand() % 40);
                    cube.rotationSpeedY = 10.0f + static_cast<float>(rand() % 40);
                    cube.rotationSpeedZ = 10.0f + static_cast<float>(rand() % 40);

                    // Échelle variée
                    cube.scale = 0.3f + static_cast<float>(rand() % 70) / 100.0f;  // Entre 0.3 et 1.0

                    // Couleur
                    memcpy(cube.color, colors[colorIndex % 12], sizeof(float) * 3);
                    colorIndex++;

                    cubeInstances.push_back(cube);
                    cubeCount++;

                    if (cubeCount >= maxCubes) break;
                }
                if (cubeCount >= maxCubes) break;
            }
            if (cubeCount >= maxCubes) break;
        }

        std::cout << "Créé " << cubeInstances.size() << " cubes à différentes positions" << std::endl;
    }

    void initGL() {
        std::cout << "Initializing MultiCube 3D..." << std::endl;
        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // Création des données du cube (un seul cube de base)
        createCubeData();

        // Création du VAO
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // Création et configuration du VBO
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     cubeVertices.size() * sizeof(Vertex),
                     cubeVertices.data(),
                     GL_STATIC_DRAW);

        // Création et configuration de l'EBO
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     cubeIndices.size() * sizeof(GLuint),
                     cubeIndices.data(),
                     GL_STATIC_DRAW);

        // Configuration des attributs
        // Position - location 0
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        // Normale - location 1
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        // Déliaison
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0); // L'EBO reste attaché

        // Configuration OpenGL 3D
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);  // Fond légèrement bleuté

        // Activer le test de profondeur
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Activer le face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        std::cout << "MultiCube 3D initialized successfully!" << std::endl;
        std::cout << "Vertices par cube: " << cubeVertices.size() << std::endl;
        std::cout << "Indices par cube: " << cubeIndices.size() << std::endl;
    }

    void createCubeData() {
        // Un cube unitaire centré à l'origine
        float half = 0.5f;

        // 8 positions uniques
        float positions[8][3] = {
            {-half, -half,  half}, // 0: bas-gauche avant
            { half, -half,  half}, // 1: bas-droite avant
            { half,  half,  half}, // 2: haut-droite avant
            {-half,  half,  half}, // 3: haut-gauche avant

            {-half, -half, -half}, // 4: bas-gauche arrière
            { half, -half, -half}, // 5: bas-droite arrière
            { half,  half, -half}, // 6: haut-droite arrière
            {-half,  half, -half}  // 7: haut-gauche arrière
        };

        // Initialiser tous les sommets
        for (int i = 0; i < 8; ++i) {
            Vertex v;
            v.position[0] = positions[i][0];
            v.position[1] = positions[i][1];
            v.position[2] = positions[i][2];

            // Normales par défaut
            v.normal[0] = 0.0f;
            v.normal[1] = 0.0f;
            v.normal[2] = 0.0f;

            cubeVertices.push_back(v);
        }

        // Indices pour 12 triangles (2 triangles par face × 6 faces)
        cubeIndices = {
            // Face avant
            0, 1, 2,
            0, 2, 3,

            // Face arrière
            5, 4, 7,
            5, 7, 6,

            // Face droite
            1, 5, 6,
            1, 6, 2,

            // Face gauche
            4, 0, 3,
            4, 3, 7,

            // Face supérieure
            3, 2, 6,
            3, 6, 7,

            // Face inférieure
            4, 5, 1,
            4, 1, 0
        };

        // Calculer les normales
        calculateNormals();

        verticesCount = static_cast<GLsizei>(cubeVertices.size());
        indicesCount = static_cast<GLsizei>(cubeIndices.size());
    }

    void calculateNormals() {
        // Réinitialiser toutes les normales
        for (auto& vertex : cubeVertices) {
            vertex.normal[0] = 0.0f;
            vertex.normal[1] = 0.0f;
            vertex.normal[2] = 0.0f;
        }

        // Pour chaque triangle, calculer sa normale et l'ajouter à ses sommets
        for (size_t i = 0; i < cubeIndices.size(); i += 3) {
            GLuint i0 = cubeIndices[i];
            GLuint i1 = cubeIndices[i + 1];
            GLuint i2 = cubeIndices[i + 2];

            Vertex& v0 = cubeVertices[i0];
            Vertex& v1 = cubeVertices[i1];
            Vertex& v2 = cubeVertices[i2];

            // Vecteurs du triangle
            float edge1[3] = {
                v1.position[0] - v0.position[0],
                v1.position[1] - v0.position[1],
                v1.position[2] - v0.position[2]
            };

            float edge2[3] = {
                v2.position[0] - v0.position[0],
                v2.position[1] - v0.position[1],
                v2.position[2] - v0.position[2]
            };

            // Produit vectoriel
            float normal[3] = {
                edge1[1] * edge2[2] - edge1[2] * edge2[1],
                edge1[2] * edge2[0] - edge1[0] * edge2[2],
                edge1[0] * edge2[1] - edge1[1] * edge2[0]
            };

            // Normaliser
            float length = sqrtf(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
            if (length > 0.0001f) {
                normal[0] /= length;
                normal[1] /= length;
                normal[2] /= length;
            }

            // Ajouter aux trois sommets (normales moyennes)
            for (int j = 0; j < 3; ++j) {
                cubeVertices[cubeIndices[i + j]].normal[0] += normal[0];
                cubeVertices[cubeIndices[i + j]].normal[1] += normal[1];
                cubeVertices[cubeIndices[i + j]].normal[2] += normal[2];
            }
        }

        // Normaliser toutes les normales des sommets
        for (auto& vertex : cubeVertices) {
            float length = sqrtf(vertex.normal[0]*vertex.normal[0] +
                                vertex.normal[1]*vertex.normal[1] +
                                vertex.normal[2]*vertex.normal[2]);
            if (length > 0.0001f) {
                vertex.normal[0] /= length;
                vertex.normal[1] /= length;
                vertex.normal[2] /= length;
            }
        }
    }

    void cleanup() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (EBO != 0) glDeleteBuffers(1, &EBO);
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
        std::cout << "Creating MultiCube shader program..." << std::endl;

        // Vertex Shader
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;

            out vec3 vNormal;
            out vec3 vFragPos;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;
            uniform float uTime;

            void main() {
                // Transformation par la matrice modèle
                vec4 worldPos = uModel * vec4(aPosition, 1.0);
                vFragPos = worldPos.xyz;

                // Transformation finale (espace clip)
                gl_Position = uProjection * uView * worldPos;

                // Transformation de la normale
                vNormal = mat3(uModel) * aNormal;
            }
        )";

        // Fragment Shader avec couleur uniforme
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 vNormal;
            in vec3 vFragPos;

            layout(location = 0) out vec4 outColor;

            uniform float uTime;
            uniform vec3 uColor;

            void main() {
                // Normaliser la normale
                vec3 normal = normalize(vNormal);

                // Direction de la lumière principale
                vec3 lightDir1 = normalize(vec3(1.0, 1.0, 0.5));

                // Deuxième source de lumière pour un effet plus intéressant
                vec3 lightDir2 = normalize(vec3(-0.5, 0.8, -0.3));

                // Calcul des éclairages diffus
                float diff1 = max(dot(normal, lightDir1), 0.0);
                float diff2 = max(dot(normal, lightDir2), 0.0) * 0.5;

                // Éclairage ambiant
                float ambient = 0.2;

                // Combinaison des éclairages
                float lighting = ambient + diff1 + diff2;

                // Appliquer l'éclairage à la couleur du cube
                vec3 result = uColor * lighting;

                // Effet de pulsation subtil
                float pulse = 0.9 + 0.1 * sin(uTime * 1.5);
                result *= pulse;

                // Gamma correction
                result = pow(result, vec3(1.0 / 2.2));

                outColor = vec4(result, 1.0);
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
        colorUniformLoc = glGetUniformLocation(program, "uColor");

        std::cout << "Shader MultiCube créé avec succès!" << std::endl;

        return program;
    }

    // Données du cube de base
    std::vector<Vertex> cubeVertices;
    std::vector<GLuint> cubeIndices;
};

std::unique_ptr<MultiCube3D> g_multiCube3D;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "MULTI-CUBE 3D - OpenGL" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Caractéristiques:" << std::endl;
        std::cout << " - 12 cubes à différentes positions" << std::endl;
        std::cout << " - Chaque cube a sa propre couleur" << std::endl;
        std::cout << " - Rotations indépendantes sur 3 axes" << std::endl;
        std::cout << " - Échelles variées" << std::endl;
        std::cout << " - Caméra orbitale automatique" << std::endl;
        std::cout << " - Éclairage avec deux sources" << std::endl;
        std::cout << " - Test de profondeur (Z-buffer)" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Multi-Cube 3D - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de l'application multi-cube
        g_multiCube3D = std::make_unique<MultiCube3D>();

        // Enregistrement dans le moteur
        common::SystemObserverSubject::AddObserver(g_multiCube3D.get());
        common::DrawObserverSubject::AddObserver(g_multiCube3D.get());

        // Lancement du moteur
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_multiCube3D.get());
        common::DrawObserverSubject::RemoveObserver(g_multiCube3D.get());
        g_multiCube3D.reset();

        std::cout << "\nProgramme Multi-Cube 3D terminé avec succès!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nERREUR: " << e.what() << std::endl;

        if (g_multiCube3D) {
            common::SystemObserverSubject::RemoveObserver(g_multiCube3D.get());
            common::DrawObserverSubject::RemoveObserver(g_multiCube3D.get());
            g_multiCube3D.reset();
        }
        return -1;
    }

    return 0;
}