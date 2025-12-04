//
// Created by forna on 02.12.2025.
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

class QuadEBO : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "QuadEBO::Begin()" << std::endl;
        initGL();
    }

    void Update(float dt) override {
        rotationAngle += rotationSpeed * dt;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
    }

    void FixedUpdate() override{}

    void End() override {
        std::cout << "QuadEBO::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {
    }

    void Draw() override {
        glUseProgram(shaderProgram);

        // Passer l'uniform de rotation au shader
        if (rotationUniformLoc != -1) {
            glUniform1f(rotationUniformLoc, rotationAngle);
        }

        glBindVertexArray(VAO);

        // IMPORTANT: Utilisation de glDrawElements au lieu de glDrawArrays
        glDrawElements(GL_TRIANGLES,         // Mode
                       indicesCount,         // Nombre d'indices
                       GL_UNSIGNED_INT,      // Type des indices
                       (void*)0);            // Offset

        glBindVertexArray(0);
    }

    void PostDraw() override {
    }

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei indicesCount = 0;
    float rotationAngle = 0.0f;
    float rotationSpeed = 60.0f;
    GLint rotationUniformLoc = -1;

    struct Vertex {
        float position[2]; // x, y
        float color[3]; // r, g, b
    };

    void initGL() {
        std::cout << "Initializing Quad with VBO+EBO (optimized)..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // 1. DONNÉES DES VERTICES UNIQUES (4 seulement !)
        std::vector<Vertex> vertices = {
            // Seulement 4 sommets uniques au lieu de 6
            {{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}, // 0: Haut-gauche - Rouge
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, // 1: Haut-droite - Vert
            {{0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, // 2: Bas-droite  - Bleu
            {{-0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}} // 3: Bas-gauche  - Jaune
        };

        // 2. DONNÉES DES INDICES (référence aux vertices)
        std::vector<GLuint> indices = {
            0, 1, 2, // Premier triangle:  Haut-gauche → Haut-droite → Bas-droite
            0, 2, 3 // Second triangle:   Haut-gauche → Bas-droite → Bas-gauche
        };
        indicesCount = static_cast<GLsizei>(indices.size());

        // Génération des objets OpenGL
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Configuration du VAO
        glBindVertexArray(VAO);

        // VBO - Données des vertices
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(),
                     GL_STATIC_DRAW);

        // EBO - Données des indices
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(GLuint),
                     indices.data(),
                     GL_STATIC_DRAW);

        // Attribut 0: Position (x, y)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        // Attribut 1: Couleur (r, g, b)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              sizeof(Vertex),
                              (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(1);

        // Détacher le VAO (optionnel mais bonne pratique)
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void cleanup() {
        glDeleteProgram(shaderProgram);
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
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
    // Vertex Shader avec uniform de rotation
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
        glGetShaderInfoLog(program, 512, nullptr, infoLog);
        throw std::runtime_error(std::string("Erreur linkage shader:\n") + std::string(infoLog));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Récupération de la location de l'uniform
    rotationUniformLoc = glGetUniformLocation(program, "uRotation");

    return program;
}
};

// Variable globale
std::unique_ptr<QuadEBO> g_quadEBO;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Sample 4: Quad avec VBO + EBO" << std::endl;
        std::cout << "Optimisation mémoire avec indices" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Quad avec VAO, VBO et EBO - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        g_quadEBO = std::make_unique<QuadEBO>();
        common::SystemObserverSubject::AddObserver(g_quadEBO.get());
        common::DrawObserverSubject::AddObserver(g_quadEBO.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(g_quadEBO.get());
        common::DrawObserverSubject::RemoveObserver(g_quadEBO.get());
        g_quadEBO.reset();

        std::cout << "\nApplication terminée avec succès!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\nERREUR: " << e.what() << std::endl;

        if (g_quadEBO) {
            common::SystemObserverSubject::RemoveObserver(g_quadEBO.get());
            common::DrawObserverSubject::RemoveObserver(g_quadEBO.get());
            g_quadEBO.reset();
        }
        return -1;
    }
    return 0;
}
