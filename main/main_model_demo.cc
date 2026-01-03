//
// Created by forna on 16.12.2025.
//
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

// Pour le chargement des images
//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Pour Assimp
#include <corecrt_math_defines.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Model3DRenderer : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "Model3DRenderer::Begin() - Chargement du modèle 3D" << std::endl;
        initGL();
        loadNanosuitModel();
    }

    void Update(float dt) override {
        time += dt;

        // Animation automatique de rotation
        modelRotationAngle += 20.0f * dt; // 20 degrés par seconde
        if (modelRotationAngle > 360.0f) modelRotationAngle -= 360.0f;

        // Animation de la lumière
        lightAngle += 15.0f * dt;
        if (lightAngle > 360.0f) lightAngle -= 360.0f;

        // Position de la lumière qui tourne
        lightPos[0] = 5.0f * std::sin(lightAngle * std::numbers::pi_v<float> / 180.0f);
        lightPos[1] = 3.0f;
        lightPos[2] = 5.0f * cosf(lightAngle * M_PI / 180.0f);//*

        // Animation de la caméra
        cameraAngle += 5.0f * dt;
        if (cameraAngle > 360.0f) cameraAngle -= 360.0f;

        cameraPos[0] = 8.0f * sinf(cameraAngle * M_PI / 180.0f);
        cameraPos[1] = 4.0f;
        cameraPos[2] = 8.0f * cosf(cameraAngle * M_PI / 180.0f);

        // Afficher des infos périodiquement
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 2.0f) {
            infoTimer = 0.0f;
            std::cout << "Modèle: " << meshCount << " meshes, "
                      << "Rotation: " << modelRotationAngle << "°" << std::endl;
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "Model3DRenderer::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        // Effacer les buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Calculer les matrices
        updateMatrices();

        // Passer les matrices au shader
        glUniformMatrix4fv(modelMatrixUniformLoc, 1, GL_FALSE, modelMatrix);
        glUniformMatrix4fv(viewMatrixUniformLoc, 1, GL_FALSE, viewMatrix);
        glUniformMatrix4fv(projectionMatrixUniformLoc, 1, GL_FALSE, projectionMatrix);

        // Passer les uniformes
        glUniform3fv(viewPosUniformLoc, 1, cameraPos);
        glUniform3fv(lightPosUniformLoc, 1, lightPos);
        glUniform3f(lightColorUniformLoc, 1.0f, 1.0f, 1.0f);
        glUniform1f(materialShininessUniformLoc, 32.0f);

        // Dessiner tous les meshes
        for (const auto& mesh : meshes) {
            drawMesh(mesh);
        }

        // Optionnel: Dessiner la source de lumière
        drawLightSource();
    }

    void PostDraw() override {}

private:
    struct Mesh {
        GLuint VAO;
        GLuint VBO;
        GLuint EBO;
        GLuint diffuseTexture;
        GLuint specularTexture;
        GLuint normalTexture;
        unsigned int indexCount;
    };

    struct Vertex {
        float position[3];
        float normal[3];
        float texCoords[2];
    };

    std::vector<Mesh> meshes;
    GLuint shaderProgram = 0;
    GLuint lightShaderProgram = 0;
    int meshCount = 0;

    float time = 0.0f;
    float modelRotationAngle = 0.0f;
    float lightAngle = 0.0f;
    float cameraAngle = 0.0f;

    float cameraPos[3] = {0.0f, 4.0f, 8.0f};
    float lightPos[3] = {2.0f, 3.0f, 3.0f};

    float modelMatrix[16];
    float viewMatrix[16];
    float projectionMatrix[16];

    // Locations des uniforms
    GLint modelMatrixUniformLoc = -1;
    GLint viewMatrixUniformLoc = -1;
    GLint projectionMatrixUniformLoc = -1;
    GLint viewPosUniformLoc = -1;
    GLint lightPosUniformLoc = -1;
    GLint lightColorUniformLoc = -1;
    GLint materialShininessUniformLoc = -1;
    GLint diffuseTextureUniformLoc = -1;
    GLint specularTextureUniformLoc = -1;

    // Pour la source de lumière
    GLint lightModelMatrixUniformLoc = -1;

    void initGL() {
        std::cout << "Initializing Model 3D Renderer..." << std::endl;

        // Créer les shaders
        shaderProgram = createModelShaderProgram();
        lightShaderProgram = createLightShaderProgram();

        // Configuration OpenGL
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Activer le blending pour les textures transparentes
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::cout << "✓ Model Renderer initialized!" << std::endl;
    }

    void loadNanosuitModel() {
        std::cout << "\n--- CHARGEMENT DU MODÈLE NANOSUIT ---" << std::endl;

        const char* modelPaths[] = {
            "data/model/nanosuit2/nanosuit.obj",
            "../data/model/nanosuit2/nanosuit.obj",
            "./data/model/nanosuit2/nanosuit.obj",
            "model/nanosuit2/nanosuit.obj"
        };

        bool loaded = false;
        for (const auto& path : modelPaths) {
            std::cout << "Essai de chargement: " << path << std::endl;
            if (loadModelWithAssimp(path)) {
                std::cout << "✓ Modèle chargé avec succès!" << std::endl;
                std::cout << "  Nombre de meshes: " << meshCount << std::endl;
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            std::cout << "✗ Échec du chargement du modèle" << std::endl;
            std::cout << "Création d'un cube de secours..." << std::endl;
            createFallbackCube();
        }
    }

    bool loadModelWithAssimp(const std::string& path) {
        Assimp::Importer importer;

        // Charger le modèle avec Assimp
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_GenNormals);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cout << "Erreur Assimp: " << importer.GetErrorString() << std::endl;
            return false;
        }

        std::cout << "✓ Fichier chargé par Assimp" << std::endl;
        std::cout << "  Nombre de meshes: " << scene->mNumMeshes << std::endl;
        std::cout << "  Nombre de matériaux: " << scene->mNumMaterials << std::endl;

        // Extraire le chemin du dossier
        std::string directory = path.substr(0, path.find_last_of('/'));

        // Charger tous les meshes
        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            aiMesh* aiMesh = scene->mMeshes[i];
            std::cout << "  Mesh " << i << ": "
                      << aiMesh->mNumVertices << " vertices, "
                      << aiMesh->mNumFaces << " faces" << std::endl;

            Mesh mesh = processAIMesh(aiMesh, scene, directory);
            meshes.push_back(mesh);
            meshCount++;
        }

        return true;
    }

    Mesh processAIMesh(aiMesh* aiMesh, const aiScene* scene, const std::string& directory) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // Extraire les vertices
        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            Vertex vertex;

            // Position
            vertex.position[0] = aiMesh->mVertices[i].x;
            vertex.position[1] = aiMesh->mVertices[i].y;
            vertex.position[2] = aiMesh->mVertices[i].z;

            // Normales
            if (aiMesh->HasNormals()) {
                vertex.normal[0] = aiMesh->mNormals[i].x;
                vertex.normal[1] = aiMesh->mNormals[i].y;
                vertex.normal[2] = aiMesh->mNormals[i].z;
            } else {
                vertex.normal[0] = 0.0f;
                vertex.normal[1] = 1.0f;
                vertex.normal[2] = 0.0f;
            }

            // Coordonnées de texture
            if (aiMesh->mTextureCoords[0]) {
                vertex.texCoords[0] = aiMesh->mTextureCoords[0][i].x;
                vertex.texCoords[1] = aiMesh->mTextureCoords[0][i].y;
            } else {
                vertex.texCoords[0] = 0.0f;
                vertex.texCoords[1] = 0.0f;
            }

            vertices.push_back(vertex);
        }

        // Extraire les indices
        for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
            aiFace face = aiMesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        // Charger les textures du matériau
        GLuint diffuseTexture = 0;
        GLuint specularTexture = 0;
        GLuint normalTexture = 0;

        if (aiMesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[aiMesh->mMaterialIndex];

            // Charger la texture diffuse
            diffuseTexture = loadMaterialTexture(material, aiTextureType_DIFFUSE, directory);

            // Charger la texture specular
            specularTexture = loadMaterialTexture(material, aiTextureType_SPECULAR, directory);

            // Charger la texture normale
            normalTexture = loadMaterialTexture(material, aiTextureType_HEIGHT, directory);
        }

        // Si aucune texture n'est chargée, créer une texture par défaut
        if (diffuseTexture == 0) {
            diffuseTexture = createDefaultTexture();
        }

        // Créer les buffers OpenGL
        Mesh mesh;
        mesh.indexCount = indices.size();
        mesh.diffuseTexture = diffuseTexture;
        mesh.specularTexture = specularTexture;
        mesh.normalTexture = normalTexture;

        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO);
        glGenBuffers(1, &mesh.EBO);

        glBindVertexArray(mesh.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        // Positions
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, position));

        // Normales
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, normal));

        // Coordonnées de texture
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoords));

        glBindVertexArray(0);

        return mesh;
    }

    GLuint loadMaterialTexture(aiMaterial* mat, aiTextureType type, const std::string& directory) {
        if (mat->GetTextureCount(type) > 0) {
            aiString str;
            mat->GetTexture(type, 0, &str);

            std::string filename = std::string(str.C_Str());
            std::string fullPath = directory + "/" + filename;

            std::cout << "  Chargement texture: " << filename << std::endl;

            return loadTexture(fullPath);
        }
        return 0;
    }

    GLuint loadTexture(const std::string& path) {
        GLuint textureID;
        glGenTextures(1, &textureID);

        int width, height, nrComponents;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);

        if (data) {
            GLenum format;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 3)
                format = GL_RGB;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                        format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
            return textureID;
        } else {
            std::cout << "  ✗ Échec chargement texture: " << path << std::endl;
            stbi_image_free(data);
            return 0;
        }
    }

    GLuint createDefaultTexture() {
        const int texSize = 64;
        std::vector<unsigned char> data(texSize * texSize * 3);

        for (int y = 0; y < texSize; y++) {
            for (int x = 0; x < texSize; x++) {
                int idx = (y * texSize + x) * 3;

                // Texture damier gris
                if ((x / 8 + y / 8) % 2 == 0) {
                    data[idx] = 100;
                    data[idx + 1] = 100;
                    data[idx + 2] = 100;
                } else {
                    data[idx] = 150;
                    data[idx + 1] = 150;
                    data[idx + 2] = 150;
                }
            }
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0,
                    GL_RGB, GL_UNSIGNED_BYTE, data.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return textureID;
    }

    void createFallbackCube() {
        // Créer un cube simple si le modèle ne charge pas
        // (implémentation similaire à vos cubes précédents)
        std::cout << "Cube de secours créé" << std::endl;
    }

    void drawMesh(const Mesh& mesh) {
        // Activer les textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.diffuseTexture);
        glUniform1i(diffuseTextureUniformLoc, 0);

        if (mesh.specularTexture != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mesh.specularTexture);
            // Note: vous devrez ajouter un uniform pour la texture spéculaire
        }

        // Dessiner le mesh
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void drawLightSource() {
        // Dessiner une sphère simple à la position de la lumière
        glUseProgram(lightShaderProgram);

        float lightModelMatrix[16];
        identityMatrix(lightModelMatrix);
        translateMatrix(lightModelMatrix, lightPos[0], lightPos[1], lightPos[2]);
        scaleMatrix(lightModelMatrix, 0.2f, 0.2f, 0.2f);

        glUniformMatrix4fv(lightModelMatrixUniformLoc, 1, GL_FALSE, lightModelMatrix);
        glUniformMatrix4fv(viewMatrixUniformLoc, 1, GL_FALSE, viewMatrix);
        glUniformMatrix4fv(projectionMatrixUniformLoc, 1, GL_FALSE, projectionMatrix);

        // Vous devrez créer un mesh pour la sphère de lumière
        // Pour l'instant, on saute cette partie
    }

    void updateMatrices() {
        // Matrice modèle
        identityMatrix(modelMatrix);
        translateMatrix(modelMatrix, 0.0f, -1.0f, 0.0f); // Descendre un peu
        rotateYMatrix(modelMatrix, modelRotationAngle);
        scaleMatrix(modelMatrix, 0.2f, 0.2f, 0.2f); // Nanosuit est grand, on réduit

        // Matrice vue
        float eyeX = cameraPos[0];
        float eyeY = cameraPos[1];
        float eyeZ = cameraPos[2];

        lookAtMatrix(viewMatrix,
                    eyeX, eyeY, eyeZ,
                    0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f);

        // Matrice projection
        perspectiveMatrix(projectionMatrix, 60.0f, 800.0f / 600.0f, 0.1f, 100.0f);
    }

    // Fonctions mathématiques pour les matrices (à copier de votre code existant)
    void identityMatrix(float* m) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    void translateMatrix(float* m, float x, float y, float z) {
        identityMatrix(m);
        m[12] = x; m[13] = y; m[14] = z;
    }

    void rotateYMatrix(float* m, float angle) {
        identityMatrix(m);
        float rad = angle * M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
    }

    void scaleMatrix(float* m, float sx, float sy, float sz) {
        identityMatrix(m);
        m[0] = sx; m[5] = sy; m[10] = sz;
    }

    void lookAtMatrix(float* m, float eyeX, float eyeY, float eyeZ,
                     float centerX, float centerY, float centerZ,
                     float upX, float upY, float upZ) {
        // Implémentation de votre code existant
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

    void perspectiveMatrix(float* m, float fov, float aspect, float near, float far) {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        float f = 1.0f / tanf(fov * M_PI / 360.0f);
        m[0] = f / aspect; m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    void cleanup() {
        // Nettoyer les meshes
        for (auto& mesh : meshes) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
            if (mesh.diffuseTexture != 0) glDeleteTextures(1, &mesh.diffuseTexture);
            if (mesh.specularTexture != 0) glDeleteTextures(1, &mesh.specularTexture);
            if (mesh.normalTexture != 0) glDeleteTextures(1, &mesh.normalTexture);
        }

        if (shaderProgram != 0) glDeleteProgram(shaderProgram);
        if (lightShaderProgram != 0) glDeleteProgram(lightShaderProgram);
    }

    GLuint createModelShaderProgram() {
        std::cout << "Création du shader pour modèles 3D..." << std::endl;

        // Vertex Shader
        const char* vertexSource = R"(
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

        // Fragment Shader
        const char* fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 FragPos;
            in vec3 Normal;
            in vec2 TexCoord;

            out vec4 FragColor;

            uniform sampler2D uDiffuseTexture;
            uniform vec3 uViewPos;
            uniform vec3 uLightPos;
            uniform vec3 uLightColor;
            uniform float uMaterialShininess;

            void main() {
                // Couleur diffuse de la texture
                vec3 diffuseColor = texture(uDiffuseTexture, TexCoord).rgb;

                // Ambient
                vec3 ambient = uLightColor * diffuseColor * 0.1;

                // Diffuse
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = uLightColor * diff * diffuseColor;

                // Specular
                vec3 viewDir = normalize(uViewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterialShininess);
                vec3 specular = uLightColor * spec * 0.5;

                // Résultat final
                vec3 result = ambient + diffuse + specular;
                FragColor = vec4(result, 1.0);
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        // Vérifier les erreurs
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error(std::string("Erreur linkage shader: ") + infoLog);
        }

        // Récupérer les locations
        modelMatrixUniformLoc = glGetUniformLocation(program, "uModel");
        viewMatrixUniformLoc = glGetUniformLocation(program, "uView");
        projectionMatrixUniformLoc = glGetUniformLocation(program, "uProjection");
        viewPosUniformLoc = glGetUniformLocation(program, "uViewPos");
        lightPosUniformLoc = glGetUniformLocation(program, "uLightPos");
        lightColorUniformLoc = glGetUniformLocation(program, "uLightColor");
        materialShininessUniformLoc = glGetUniformLocation(program, "uMaterialShininess");
        diffuseTextureUniformLoc = glGetUniformLocation(program, "uDiffuseTexture");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        std::cout << "✓ Shader modèle créé" << std::endl;

        return program;
    }

    GLuint createLightShaderProgram() {
        // Shader simple pour la source de lumière
        const char* vertexSource = R"(
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

        const char* fragmentSource = R"(
            #version 300 es
            precision mediump float;

            out vec4 FragColor;

            void main() {
                FragColor = vec4(1.0, 1.0, 0.8, 1.0); // Couleur lumière jaune clair
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        lightModelMatrixUniformLoc = glGetUniformLocation(program, "uModel");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return program;
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
            throw std::runtime_error("Erreur compilation " + shaderType + ":\n" + std::string(infoLog));
        }

        return shader;
    }
};

std::unique_ptr<Model3DRenderer> g_modelRenderer;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "CHARGEMENT MODÈLE 3D - NANOSUIT2" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Caractéristiques:" << std::endl;
        std::cout << " - Chargement OBJ avec Assimp" << std::endl;
        std::cout << " - Textures multiples (diffuse, specular)" << std::endl;
        std::cout << " - Éclairage Phong dynamique" << std::endl;
        std::cout << " - Caméra orbitale automatique" << std::endl;
        std::cout << " - Lumière tournante" << std::endl;
        std::cout << " - Rotation automatique du modèle" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Modèle 3D - Nanosuit2 avec Assimp";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création du renderer
        g_modelRenderer = std::make_unique<Model3DRenderer>();

        // Enregistrement
        common::SystemObserverSubject::AddObserver(g_modelRenderer.get());
        common::DrawObserverSubject::AddObserver(g_modelRenderer.get());

        // Lancement
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_modelRenderer.get());
        common::DrawObserverSubject::RemoveObserver(g_modelRenderer.get());
        g_modelRenderer.reset();

        std::cout << "\n✓ Démonstration terminée avec succès!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_modelRenderer) {
            common::SystemObserverSubject::RemoveObserver(g_modelRenderer.get());
            common::DrawObserverSubject::RemoveObserver(g_modelRenderer.get());
            g_modelRenderer.reset();
        }
        return -1;
    }

    return 0;
}


