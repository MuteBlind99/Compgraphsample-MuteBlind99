//
// Created by forna on 16.12.2025.
//
#pragma once


#include <vector>
#include <string>
#include "third_party/gl_include.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include "maths/vec3.h"

struct Vertex {
    float position[3];
    float normal[3];
    float texCoords[2];
};

struct Texture {
    GLuint id;
    std::string type;
    aiString path;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    GLuint VAO=0, VBO=0, EBO=0;

    void setupMesh();
    void Draw(GLuint shaderProgram);
    void AttachInstancBuffer(GLuint instanceVBO);
    void DrawInstanced(GLuint shaderProgram, int instanceCount);
};

class Model {
public:
    explicit Model(const std::string& path) {
        loadModel(path);
    }

    void Draw(GLuint shaderProgram);
    void AttachInstanceBuffer(GLuint instanceVBO);
    void DrawInstanced(GLuint shaderProgram, int instanceCount);


    core::Vec3F aabbMin;
    core::Vec3F aabbMax;

    [[nodiscard]] core::Vec3F GetCenter() const;
    [[nodiscard]] float GetBoundingRadius() const;

    float GetMinY() const{return aabbMin.y;}

private:
    std::vector<Mesh> meshes;
    std::string directory;
    std::vector<Texture> textures_loaded;
    static GLuint gWhiteTex;

    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat,
                                             aiTextureType type,
                                             const std::string& typeName);

    static GLuint TextureFromFile(const char* path, const std::string& directory);
};