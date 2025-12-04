//
// Created by forna on 26.11.2025.
//
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>

#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class TriangleEmptyVAO :
    public common::DrawInterface,
    public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "TriangleEmptyVAO::Begin() called" << std::endl;

        // Initialisation OpenGL
        initGL();
    }

    void Update(float dt) override {
        // Logique de mise à jour si nécessaire
    }

    void FixedUpdate() override {
        // Logique de mise à jour fixe si nécessaire
    }

    void End() override {
        std::cout << "TriangleEmptyVAO::End() called" << std::endl;
        // Nettoyage
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (shaderProgram != 0) {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
    }

    // Méthodes DrawInterface
    void PreDraw() override {}

    void Draw() override {
        // Rendu du triangle
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;

    void initGL() {
        std::cout << "Initializing OpenGL..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // Création du VAO vide
        glGenVertexArrays(1, &VAO);

        // Configuration OpenGL
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

        std::cout << "OpenGL initialized successfully" << std::endl;
        std::cout << "Shader program: " << shaderProgram << std::endl;
        std::cout << "VAO: " << VAO << std::endl;
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
            throw std::runtime_error("Erreur de compilation du shader " + shaderType + ":\n" + infoLog);
        }

        return shader;
    }

    GLuint createShaderProgram() {
        // Shaders intégrés directement dans le code
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            out vec3 fragColor;

            void main() {
                // Calcul direct basé sur gl_VertexID
                int vertexId = gl_VertexID;

                float x = 0.0;
                float y = 0.0;
                vec3 color = vec3(0.0);

                if (vertexId == 0) {
                    x = 0.0; y = 0.5;
                    color = vec3(1.0, 0.0, 0.0); // Rouge
                } else if (vertexId == 1) {
                    x = 0.5; y = -0.5;
                    color = vec3(0.0, 1.0, 0.0); // Vert
                } else if (vertexId == 2) {
                    x = -0.5; y = -0.5;
                    color = vec3(0.0, 0.0, 1.0); // Bleu
                }

                gl_Position = vec4(x, y, 0.0, 1.0);
                fragColor = color;
            }
        )";

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
            throw std::runtime_error("Erreur de liaison du programme shader:\n" + std::string(infoLog));
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        std::cout << "Shader program created successfully" << std::endl;
        return program;
    }
};

// Variable globale pour garantir la durée de vie
std::unique_ptr<TriangleEmptyVAO> g_triangleApp;

int main() {
    try {
        std::cout << "Starting TriangleEmptyVAO application..." << std::endl;

        // Configuration de la fenêtre
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Triangle avec VAO vide - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création de l'application
        g_triangleApp = std::make_unique<TriangleEmptyVAO>();

        // Enregistrement dans l'engine
        common::SystemObserverSubject::AddObserver(g_triangleApp.get());
        common::DrawObserverSubject::AddObserver(g_triangleApp.get());

        // Lancement de l'engine
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_triangleApp.get());
        common::DrawObserverSubject::RemoveObserver(g_triangleApp.get());
        g_triangleApp.reset();

        std::cout << "Application terminated successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << std::endl;

        // Nettoyage en cas d'erreur
        if (g_triangleApp) {
            common::SystemObserverSubject::RemoveObserver(g_triangleApp.get());
            common::DrawObserverSubject::RemoveObserver(g_triangleApp.get());
            g_triangleApp.reset();
        }
        return -1;
    }

    return 0;
}

