//
// Created by forna on 16.12.2025.
//
#include "model_loader.h"
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cfloat>

void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 &indices[0], GL_STATIC_DRAW);

    // Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, position));

    // Normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, normal));

    // Texture coordinates
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, texCoords));

    glBindVertexArray(0);
}

void Mesh::Draw(GLuint shaderProgram) {
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;

    for (unsigned int i = 0; i < textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string number;
        std::string name = textures[i].type;

        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        // Set the sampler to the correct texture unit
        glUniform1i(glGetUniformLocation(shaderProgram,
                    (name + number).c_str()), i);

        // And finally bind the texture
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }
    for (int u = 2; u < 8; ++u) {
        glActiveTexture(GL_TEXTURE0+u);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    // Draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Set everything back to defaults
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::AttachInstancBuffer(GLuint instanceVBO) {
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);


    constexpr GLsizei vec4Size = sizeof(float) * 4;
    constexpr GLsizei mat4Size = sizeof(float) * 16;

    // mat4 = 4 vec4 : locations 5,6,7,8
    for (int i = 0; i < 4; ++i)
    {
        glEnableVertexAttribArray(5 + i);
        glVertexAttribPointer(5+ i, 4, GL_FLOAT, GL_FALSE, mat4Size, (void*)(i * vec4Size));
        glVertexAttribDivisor(5 + i, 1); // <-- 1 par instance
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::DrawInstanced(GLuint shaderProgram, int instanceCount) {
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;

    for (unsigned int i = 0; i < textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string number;
        std::string name = textures[i].type;

        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        // Set the sampler to the correct texture unit
        glUniform1i(glGetUniformLocation(shaderProgram,
                    (name + number).c_str()), i);

        // And finally bind the texture
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }
    for (int u = 2; u < 8; ++u) {
        glActiveTexture(GL_TEXTURE0+u);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    // Draw mesh
    glBindVertexArray(VAO);
    glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0,instanceCount);
    glBindVertexArray(0);

    // Set everything back to defaults
    glActiveTexture(GL_TEXTURE0);
}

void Model::Draw(GLuint shaderProgram) {
    for (unsigned int i = 0; i < meshes.size(); i++) {
        meshes[i].Draw(shaderProgram);
    }
}

void Model::AttachInstanceBuffer(GLuint instanceVBO) {
    for (auto& m : meshes) {
        m.AttachInstancBuffer(instanceVBO);
    }
}

void Model::DrawInstanced(GLuint shaderProgram, int instanceCount) {
    for (auto& m : meshes) {
        m.DrawInstanced(shaderProgram, instanceCount);
    }
}

core::Vec3F Model::GetCenter() const {
    return (aabbMin+aabbMax)*0.5f;
}

float Model::GetBoundingRadius() const {
    core::Vec3F size = aabbMax - aabbMin;
    return 0.5f * size.magnitude();
}

void Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    aabbMin = core::Vec3F( FLT_MAX, FLT_MAX, FLT_MAX);
    aabbMax = core::Vec3F( -FLT_MAX, -FLT_MAX, -FLT_MAX);
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    directory = path.substr(0, path.find_last_of('/'));
    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Process all the node's meshes
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    // Then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        // Positions
        vertex.position[0] = mesh->mVertices[i].x;
        vertex.position[1] = mesh->mVertices[i].y;
        vertex.position[2] = mesh->mVertices[i].z;

        aabbMin.x = std::min(aabbMin.x, vertex.position[0]);
        aabbMin.y = std::min(aabbMin.y, vertex.position[1]);
        aabbMin.z = std::min(aabbMin.z, vertex.position[2]);

        aabbMax.x = std::max(aabbMax.x, vertex.position[0]);
        aabbMax.y = std::max(aabbMax.y, vertex.position[1]);
        aabbMax.z = std::max(aabbMax.z, vertex.position[2]);

        // Normals
        if (mesh->HasNormals()) {
            vertex.normal[0] = mesh->mNormals[i].x;
            vertex.normal[1] = mesh->mNormals[i].y;
            vertex.normal[2] = mesh->mNormals[i].z;
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords[0] = mesh->mTextureCoords[0][i].x;
            vertex.texCoords[1] = mesh->mTextureCoords[0][i].y;
        } else {
            vertex.texCoords[0] = 0.0f;
            vertex.texCoords[1] = 0.0f;
        }

        vertices.push_back(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Process material
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // Diffuse maps
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material,
            aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        // Specular maps
        std::vector<Texture> specularMaps = loadMaterialTextures(material,
            aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

        // Normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material,
            aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

        // Height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material,
            aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    }

    Mesh resultMesh;
    resultMesh.vertices = vertices;
    resultMesh.indices = indices;
    resultMesh.textures = textures;
    resultMesh.setupMesh();

    return resultMesh;
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat,
                                                aiTextureType type,
                                                const std::string& typeName) {
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString material_texture_path;
        mat->GetTexture(type, i, &material_texture_path);

        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (std::strcmp(textures_loaded[j].path.C_Str(), material_texture_path.C_Str()) == 0) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }

        if (!skip) {
            Texture texture;
            texture.id = TextureFromFile(material_texture_path.C_Str(), directory);
            texture.type = typeName;
            texture.path = material_texture_path;
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }

    return textures;
}

GLuint Model::TextureFromFile(const char* path, const std::string& directory) {
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Texture failed to load at path: " << path << std::endl;

    }

    return textureID;
}