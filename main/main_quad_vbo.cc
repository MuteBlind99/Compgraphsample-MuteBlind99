//
// Created by forna on 01.12.2025.
//
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

class QuadVBO : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "QuadVBO::Begin()" << std::endl;
        initGL();
    }

    void Update(float dt) override {
        // Animation simple : rotation du quad
        rotationAngle += rotationSpeed * dt;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "QuadVBO::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vertices pour 2 triangles
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;

    float rotationAngle = 0.0f;
    float rotationSpeed = 45.0f; // degrés par seconde

    struct Vertex {
        float position[2];  // x, y
        float color[3];     // r, g, b
    };

    void initGL() {
        std::cout << "Initializing Quad with VBO..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // Données des vertices pour un QUAD (2 triangles)
        // Un quad = 4 coins, mais 6 vertices car 2 triangles
        std::vector<Vertex> vertices = {
            // Triangle 1 (Haut-gauche → Haut-droite → Bas-droite)
            // Haut-gauche
            { {-0.5f,  0.5f},   {1.0f, 0.0f, 0.0f} }, // Rouge
            // Haut-droite
            { { 0.5f,  0.5f},   {0.0f, 1.0f, 0.0f} }, // Vert
            // Bas-droite
            { { 0.5f, -0.5f},   {0.0f, 0.0f, 1.0f} }, // Bleu

            // Triangle 2 (Haut-gauche → Bas-droite → Bas-gauche)
            // Haut-gauche (même sommet que le premier triangle)
            { {-0.5f,  0.5f},   {1.0f, 0.0f, 0.0f} }, // Rouge
            // Bas-droite (même sommet que le premier triangle)
            { { 0.5f, -0.5f},   {0.0f, 0.0f, 1.0f} }, // Bleu
            // Bas-gauche
            { {-0.5f, -0.5f},   {1.0f, 1.0f, 0.0f} }  // Jaune
        };

        // Visualisation des données
        std::cout << "Quad vertices (" << vertices.size() << "):" << std::endl;
        for (size_t i = 0; i < vertices.size(); ++i) {
            std::cout << "  Vertex " << i << ": "
                      << "pos(" << vertices[i].position[0] << ", "
                      << vertices[i].position[1] << "), "
                      << "color(" << vertices[i].color[0] << ", "
                      << vertices[i].color[1] << ", "
                      << vertices[i].color[2] << ")" << std::endl;
        }

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
        // Position - location 0
        glVertexAttribPointer(0,                    // location
                              2,                    // 2 composants (x, y)
                              GL_FLOAT,             // type
                              GL_FALSE,             // normalisé ?
                              sizeof(Vertex),       // stride
                              (void*)offsetof(Vertex, position));  // offset

        glEnableVertexAttribArray(0);

        // Couleur - location 1
        glVertexAttribPointer(1,                    // location
                              3,                    // 3 composants (r, g, b)
                              GL_FLOAT,             // type
                              GL_FALSE,             // normalisé ?
                              sizeof(Vertex),       // stride
                              (void*)offsetof(Vertex, color));     // offset

        glEnableVertexAttribArray(1);

        // 4. Déliaison
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Configuration OpenGL
        glClearColor(0.0f, 0.0f, 0.1f, 1.0f); // Fond bleu foncé
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::cout << "VAO: " << VAO << ", VBO: " << VBO << std::endl;
        std::cout << "Quad with VBO initialized successfully" << std::endl;
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

        return shader;
    }

    GLuint createShaderProgram() {
        // Vertex Shader - Ajout d'une rotation simple
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec3 aColor;

            out vec3 fragColor;

            uniform float uRotation; // Uniform pour la rotation

            void main() {
                // Matrice de rotation 2D
                float angle = radians(uRotation);
                float cosA = cos(angle);
                float sinA = sin(angle);

                // Application de la rotation
                float x = aPosition.x * cosA - aPosition.y * sinA;
                float y = aPosition.x * sinA + aPosition.y * cosA;

                gl_Position = vec4(x, y, 0.0, 1.0);
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

        // Récupération de la location de l'uniform
        rotationUniformLoc = glGetUniformLocation(program, "uRotation");

        std::cout << "Shader program created successfully" << std::endl;
        std::cout << "Rotation uniform location: " << rotationUniformLoc << std::endl;

        return program;
    }

    GLint rotationUniformLoc = -1;
};

// Variable globale
std::unique_ptr<QuadVBO> g_quadVBO;

int main() {
    try {
        std::cout << "Starting Quad with VBO application..." << std::endl;

        // Configuration
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Quad avec VAO et VBO - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création
        g_quadVBO = std::make_unique<QuadVBO>();

        // Enregistrement
        common::SystemObserverSubject::AddObserver(g_quadVBO.get());
        common::DrawObserverSubject::AddObserver(g_quadVBO.get());

        // Lancement
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_quadVBO.get());
        common::DrawObserverSubject::RemoveObserver(g_quadVBO.get());
        g_quadVBO.reset();

        std::cout << "Application terminated successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << std::endl;

        if (g_quadVBO) {
            common::SystemObserverSubject::RemoveObserver(g_quadVBO.get());
            common::DrawObserverSubject::RemoveObserver(g_quadVBO.get());
            g_quadVBO.reset();
        }
        return -1;
    }

    return 0;
}