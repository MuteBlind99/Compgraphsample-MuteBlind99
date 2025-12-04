//
// Created by forna on 03.12.2025.
//

#ifndef SHADER_H
#define SHADER_H
#include <string_view>
#include <GL/glew.h>

class Shader {
public:
   void Load(std::string_view path, GLenum type);
   GLuint shader_name_ = 0;
};
#endif //SHADER_H
