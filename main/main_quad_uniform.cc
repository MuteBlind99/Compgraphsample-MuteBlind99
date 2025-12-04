//
// Created by forna on 02.12.2025.
//
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <cmath>

#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class QuadUniform : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "QuadUniform::Begin()" << std::endl;
        initGL();
    }

    void Update(float dt) override {
        // Mettre à jour les animations
        time += dt;

        // Animation de rotation
        rotationAngle += rotationSpeed * dt;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;

        // Animation de pulsation (0.5 à 1.0)
        pulseValue = 0.75f + 0.25f * std::sin(time * pulseSpeed);

        // Animation de couleur
        colorShift = std::fmod(colorShift + colorSpeed * dt, 1.0f);
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "QuadUniform::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        // Utiliser le programme shader
        glUseProgram(shaderProgram);

        // 1. Passer les uniforms au shader
        // Uniform de temps
        if (timeUniformLoc != -1) {
            glUniform1f(timeUniformLoc, time);
        }

        // Uniform de rotation
        if (rotationUniformLoc != -1) {
            glUniform1f(rotationUniformLoc, rotationAngle);
        }

        // Uniform de pulsation
        if (pulseUniformLoc != -1) {
            glUniform1f(pulseUniformLoc, pulseValue);
        }

        // Uniform de décalage de couleur
        if (colorShiftUniformLoc != -1) {
            glUniform1f(colorShiftUniformLoc, colorShift);
        }

        // Uniform de couleur de base
        if (baseColorUniformLoc != -1) {
            glUniform3f(baseColorUniformLoc, baseColor[0], baseColor[1], baseColor[2]);
        }

        // 2. Rendu du quad
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void PostDraw() override {}

private:
    GLuint shaderProgram = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei indicesCount = 0;

    // Variables d'animation
    float time = 0.0f;
    float rotationAngle = 0.0f;
    float rotationSpeed = 45.0f;    // degrés par seconde
    float pulseValue = 1.0f;
    float pulseSpeed = 2.0f;        // cycles par seconde
    float colorShift = 0.0f;
    float colorSpeed = 0.5f;        // changement par seconde
    float baseColor[3] = {0.2f, 0.4f, 0.8f}; // Couleur de base (bleu)

    // Locations des uniforms
    GLint timeUniformLoc = -1;
    GLint rotationUniformLoc = -1;
    GLint pulseUniformLoc = -1;
    GLint colorShiftUniformLoc = -1;
    GLint baseColorUniformLoc = -1;

    struct Vertex {
        float position[2];  // x, y
        float texCoord[2];  // u, v (pour les futurs samples avec textures)
    };

    void initGL() {
        std::cout << "Initializing Quad with Uniforms..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // Données des vertices (position + coordonnées de texture)
        std::vector<Vertex> vertices = {
            // Position        // TexCoord
            { {-0.5f,  0.5f}, {0.0f, 1.0f} }, // Haut-gauche
            { { 0.5f,  0.5f}, {1.0f, 1.0f} }, // Haut-droite
            { { 0.5f, -0.5f}, {1.0f, 0.0f} }, // Bas-droite
            { {-0.5f, -0.5f}, {0.0f, 0.0f} }  // Bas-gauche
        };

        // Indices
        std::vector<GLuint> indices = {
            0, 1, 2,  // Premier triangle
            0, 2, 3   // Second triangle
        };

        indicesCount = static_cast<GLsizei>(indices.size());

        // Création du VAO
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        // Création et configuration du VBO
        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(),
                     GL_STATIC_DRAW);

        // Création et configuration de l'EBO
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(GLuint),
                     indices.data(),
                     GL_STATIC_DRAW);

        // Configuration des attributs
        // Position - location 0
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        // Coordonnées de texture - location 1 (pour plus tard)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(1);

        // Déliaison
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0); // L'EBO reste attaché au VAO

        // Configuration OpenGL
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // Fond bleu très foncé

        std::cout << "Quad with Uniforms initialized successfully!" << std::endl;
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
        if (EBO != 0) {
            glDeleteBuffers(1, &EBO);
            EBO = 0;
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
            std::string errorMsg = "Erreur compilation " + shaderType + ":\n" + std::string(infoLog);
            throw std::runtime_error(errorMsg);
        }

        return shader;
    }

    GLuint createShaderProgram() {
        std::cout << "Creating shader program with uniforms..." << std::endl;

        // Vertex Shader - plusieurs uniforms
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec2 aTexCoord;

            out vec2 vTexCoord;

            // UNIFORMS - constants pour tous les vertices
            uniform float uTime;           // Temps écoulé
            uniform float uRotation;       // Angle de rotation
            uniform float uPulse;          // Facteur de pulsation

            void main() {
                // Passer les coordonnées de texture au fragment shader
                vTexCoord = aTexCoord;

                // Appliquer la rotation
                float angle = radians(uRotation);
                float cosA = cos(angle);
                float sinA = sin(angle);

                vec2 pos = aPosition;

                // Rotation
                pos = vec2(
                    pos.x * cosA - pos.y * sinA,
                    pos.x * sinA + pos.y * cosA
                );

                // Pulsation (scale)
                pos *= uPulse;

                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";

        // Fragment Shader - plusieurs uniforms
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec2 vTexCoord;
            layout(location = 0) out vec4 outColor;

            // UNIFORMS - constants pour tous les fragments
            uniform float uTime;           // Temps écoulé
            uniform float uColorShift;     // Décalage de couleur
            uniform vec3 uBaseColor;       // Couleur de base

            void main() {
                // Créer un motif basé sur les coordonnées de texture
                float pattern = sin(vTexCoord.x * 10.0 + uTime * 3.0) *
                               cos(vTexCoord.y * 10.0 + uTime * 2.0);

                // Ajuster le motif avec le décalage de couleur
                pattern = 0.5 + 0.5 * pattern;
                pattern += uColorShift;

                // Créer une couleur dynamique
                vec3 color = uBaseColor;

                // Ajouter des variations basées sur le temps
                color.r += 0.2 * sin(uTime + vTexCoord.x * 2.0);
                color.g += 0.2 * cos(uTime * 1.5 + vTexCoord.y * 2.0);
                color.b += 0.2 * sin(uTime * 0.7 + (vTexCoord.x + vTexCoord.y) * 1.5);

                // Ajouter le motif
                color *= (0.7 + 0.3 * pattern);

                // Ajouter un effet de bordure
                float border = 0.1;
                float edge = smoothstep(0.0, border, vTexCoord.x) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.x) *
                           smoothstep(0.0, border, vTexCoord.y) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.y);

                color *= edge;

                outColor = vec4(color, 1.0);
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

        // Récupération des locations des uniforms
        timeUniformLoc = glGetUniformLocation(program, "uTime");
        rotationUniformLoc = glGetUniformLocation(program, "uRotation");
        pulseUniformLoc = glGetUniformLocation(program, "uPulse");
        colorShiftUniformLoc = glGetUniformLocation(program, "uColorShift");
        baseColorUniformLoc = glGetUniformLocation(program, "uBaseColor");

        // Afficher les locations
        std::cout << "Uniform locations:" << std::endl;
        std::cout << "  uTime: " << timeUniformLoc << std::endl;
        std::cout << "  uRotation: " << rotationUniformLoc << std::endl;
        std::cout << "  uPulse: " << pulseUniformLoc << std::endl;
        std::cout << "  uColorShift: " << colorShiftUniformLoc << std::endl;
        std::cout << "  uBaseColor: " << baseColorUniformLoc << std::endl;

        return program;
    }
};

// Variable globale
std::unique_ptr<QuadUniform> g_quadUniform;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Sample 5: Quad avec Uniforms" << std::endl;
        std::cout << "Variables constantes modifiables depuis le CPU" << std::endl;
        std::cout << "========================================" << std::endl;

        // Configuration
        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Quad avec Uniforms - OpenGL";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        // Création
        g_quadUniform = std::make_unique<QuadUniform>();

        // Enregistrement
        common::SystemObserverSubject::AddObserver(g_quadUniform.get());
        common::DrawObserverSubject::AddObserver(g_quadUniform.get());

        // Lancement
        common::RunEngine();

        // Nettoyage
        common::SystemObserverSubject::RemoveObserver(g_quadUniform.get());
        common::DrawObserverSubject::RemoveObserver(g_quadUniform.get());
        g_quadUniform.reset();

        std::cout << "\nApplication terminée avec succès!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nERREUR: " << e.what() << std::endl;

        if (g_quadUniform) {
            common::SystemObserverSubject::RemoveObserver(g_quadUniform.get());
            common::DrawObserverSubject::RemoveObserver(g_quadUniform.get());
            g_quadUniform.reset();
        }
        return -1;
    }

    return 0;
}