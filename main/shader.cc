//
// Created by forna on 03.12.2025.
//
#include "shader.h"

#include <iostream>
#include <string_view>
#include <GL/glew.h>
#include <SDL3/SDL_iostream.h>
#include "shader.h"

namespace graphics {
    void Shader::Load(std::string_view path, GLenum type) {

        size_t data_size;
        auto shader_content =
            static_cast<const GLchar *>(SDL_LoadFile(path.data(), &data_size));
        if (!shader_content) {
            std::cerr << "Failed to load shader file: " << path << " with code error: " << SDL_GetError() << std::endl;
            throw std::runtime_error("Failed to load shader file");
        }
         GLuint shader_name_ = glCreateShader(type);
        glShaderSource( shader_name_, 1, &shader_content, nullptr);
        glCompileShader( shader_name_);
        GLint compile_status;
        glGetShaderiv( shader_name_, GL_COMPILE_STATUS, &compile_status);
        if (!compile_status) {
            std::string buffer{};
            buffer.resize(512);
            glGetShaderInfoLog( shader_name_, buffer.size(), nullptr, buffer.data());
            std::cerr << "Shader compilation error: " << buffer << std::endl;
            throw std::runtime_error("Failed to compile shader");
        }
    }
}
