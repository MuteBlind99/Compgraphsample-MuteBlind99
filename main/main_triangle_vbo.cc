#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>

#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class TriangleVBO : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "TriangleVBO::Begin()" << std::endl;
        initGL();
    }

    void Update(float dt) override {}

    void FixedUpdate() override {}

    void End() override {
        std::cout << "TriangleVBO::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;

    struct Vertex {
        float position[2];
        float color[3];
    };

    void initGL() {
        std::cout << "Initializing OpenGL with VBO..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // Données des vertices
        std::vector<Vertex> vertices = {
            // Position X, Y    Couleur R, G, B
            { { 0.0f,  0.5f},   {1.0f, 0.0f, 0.0f} }, // Haut, Rouge
            { { 0.5f, -0.5f},   {0.0f, 1.0f, 0.0f} }, // Droite, Vert
            { {-0.5f, -0.5f},   {0.0f, 0.0f, 1.0f} }  // Gauche, Bleu
        };

        // 1. Création du VAO
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // 2. Création du VBO
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(),
                     GL_STATIC_DRAW);

        // 3. Configuration des attributs
        // ATTENTION: CORRECTION ICI !

        // Attribut de position (location 0) - offset 0
        glVertexAttribPointer(0,                         // location 0
                              2,                         // 2 composants (x, y)
                              GL_FLOAT,                  // type
                              GL_FALSE,                  // normalisé ?
                              sizeof(Vertex),            // stride
                              (void*)offsetof(Vertex, position));  // offset CORRECT

        glEnableVertexAttribArray(0);

        // Attribut de couleur (location 1) - offset après position (8 bytes)
        glVertexAttribPointer(1,                         // location 1
                              3,                         // 3 composants (r, g, b)
                              GL_FLOAT,                  // type
                              GL_FALSE,                  // normalisé ?
                              sizeof(Vertex),            // stride
                              (void*)offsetof(Vertex, color));  // offset CORRECT

        glEnableVertexAttribArray(1);

        // 4. Déliaison
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Configuration OpenGL
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        std::cout << "VAO: " << VAO << ", VBO: " << VBO << std::endl;
        std::cout << "OpenGL with VBO initialized successfully" << std::endl;
    }

    void cleanup() {
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (shaderProgram != 0) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
    }

    // CORRECTION: Retourne GLuint, pas GLint
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
            std::string errorMsg = "Erreur de compilation du shader " + shaderType + ":\n" + infoLog;
            throw std::runtime_error(errorMsg);
        }

        return shader;  // Retourne GLuint
    }

    GLuint createShaderProgram() {
        // Vertex Shader
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec3 aColor;

            out vec3 fragColor;

            void main() {
                gl_Position = vec4(aPosition, 0.0, 1.0);
                fragColor = aColor;
            }
        )";

        // Fragment Shader
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 fragColor;
            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(fragColor, 1.0);
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
            throw std::runtime_error(std::string("Erreur de liaison du programme shader:\n") + infoLog);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        std::cout << "Shader program created successfully" << std::endl;
        return program;
    }
};

// Variable globale
std::unique_ptr<TriangleVBO> g_triangleVBO;

int main() {
    try {
        std::cout << "Starting Triangle with VBO application..." << std::endl;

        // Configuration
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Triangle avec VAO et VBO - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création
        g_triangleVBO = std::make_unique<TriangleVBO>();

        // Enregistrement
        common::SystemObserverSubject::AddObserver(g_triangleVBO.get());
        common::DrawObserverSubject::AddObserver(g_triangleVBO.get());

        // Lancement
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_triangleVBO.get());
        common::DrawObserverSubject::RemoveObserver(g_triangleVBO.get());
        g_triangleVBO.reset();

        std::cout << "Application terminated successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << std::endl;

        if (g_triangleVBO) {
            common::SystemObserverSubject::RemoveObserver(g_triangleVBO.get());
            common::DrawObserverSubject::RemoveObserver(g_triangleVBO.get());
            g_triangleVBO.reset();
        }
        return -1;
    }

    return 0;
}