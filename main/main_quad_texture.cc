#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

class QuadTextureFinal : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "QuadTextureFinal::Begin()" << std::endl;
        initGL();

        // Charger une texture
        loadTexture("data/textures/brickwall.jpg", textureID, "Brick");

        if (textureID == 0) {
            createFallbackTexture();
            textureID = fallbackTextureID;
        }

        std::cout << "\n=== CONTROLES ===" << std::endl;
        std::cout << "Rotation: " << rotationSpeed << "°/sec" << std::endl;
        std::cout << "Texture Repeat: " << textureRepeat << " (1.0 = une copie)" << std::endl;
        std::cout << "=================\n" << std::endl;
    }

    void Update(float dt) override {
        time += dt;

        // Animation de rotation
        rotationAngle += rotationSpeed * dt;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;

        // Animation de défilement optionnelle
        if (enablePanning) {
            textureOffsetX = std::fmod(textureOffsetX + panSpeedX * dt, 1.0f);
            textureOffsetY = std::fmod(textureOffsetY + panSpeedY * dt, 1.0f);
        }

        // Affichage périodique des infos
        static float infoTimer = 0.0f;
        infoTimer += dt;
        if (infoTimer >= 2.0f) {
            infoTimer = 0.0f;
            std::cout << "Angle: " << rotationAngle << "° | "
                      << "Time: " << time << "s | "
                      << "TexRepeat: " << textureRepeat << std::endl;
        }
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "QuadTextureFinal::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        glUseProgram(shaderProgram);

        // Passer TOUS les uniforms
        if (timeUniformLoc != -1) {
            glUniform1f(timeUniformLoc, time);
        }

        if (rotationUniformLoc != -1) {
            glUniform1f(rotationUniformLoc, rotationAngle);
        }

        if (textureRepeatUniformLoc != -1) {
            glUniform1f(textureRepeatUniformLoc, textureRepeat);
        }

        if (textureOffsetUniformLoc != -1) {
            glUniform2f(textureOffsetUniformLoc, textureOffsetX, textureOffsetY);
        }

        if (pulseAmountUniformLoc != -1) {
            glUniform1f(pulseAmountUniformLoc, pulseAmount);
        }

        // Activer et binder la texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);

        if (textureUniformLoc != -1) {
            glUniform1i(textureUniformLoc, 0);
        }

        // Rendu
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
    GLuint textureID = 0;
    GLuint fallbackTextureID = 0;

    GLsizei indicesCount = 0;

    // Variables de contrôle
    float time = 0.0f;
    float rotationAngle = 0.0f;
    float rotationSpeed = 45.0f;  // degrés par seconde
    float textureRepeat = 1.0f;   // 1.0 = une copie sur tout le quad
    float textureOffsetX = 0.0f;
    float textureOffsetY = 0.0f;
    float panSpeedX = 0.1f;
    float panSpeedY = 0.05f;
    float pulseAmount = 0.0f;     // Effet de pulsation
    bool enablePanning = false;   // Désactiver le défilement si on veut juste la rotation

    // Locations des uniforms
    GLint timeUniformLoc = -1;
    GLint rotationUniformLoc = -1;
    GLint textureRepeatUniformLoc = -1;
    GLint textureOffsetUniformLoc = -1;
    GLint textureUniformLoc = -1;
    GLint pulseAmountUniformLoc = -1;

    struct Vertex {
        float position[2];  // x, y
        float texCoord[2];  // u, v (base 0-1)
    };

    void initGL() {
        std::cout << "Initializing Quad with Texture + Rotation..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // DONNÉES DES VERTICES - Texture couvrant tout le quad
        std::vector<Vertex> vertices = {
            // Position        // TexCoord (0.0 à 1.0 = toute la texture)
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

        // Coordonnées de texture - location 1
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                             sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoord));
        glEnableVertexAttribArray(1);

        // Déliaison
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Configuration OpenGL
        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        std::cout << "Geometry initialized: 1 texture covering entire quad" << std::endl;
    }

    void loadTexture(const std::string& filepath, GLuint& outTextureID, const std::string& name) {
        std::cout << "Loading texture: " << name << std::endl;

        int width, height, channels;
        //stbi_set_flip_vertically_on_load(true);

        unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);

        if (!data) {
            std::cout << "  [WARNING] Failed to load: " << stbi_failure_reason() << std::endl;
            outTextureID = 0;
            return;
        }

        std::cout << "  Size: " << width << "x" << height << ", Channels: " << channels << std::endl;

        // Créer la texture
        glGenTextures(1, &outTextureID);
        glBindTexture(GL_TEXTURE_2D, outTextureID);

        // Déterminer le format
        GLenum format = GL_RGB;
        if (channels == 1) format = GL_RED;
        else if (channels == 2) format = GL_RG;
        else if (channels == 3) format = GL_RGB;
        else if (channels == 4) format = GL_RGBA;

        // Charger les données
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);

        // Configuration IMPORTANTE pour une belle texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Générer mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);

        // Libérer
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "  Texture loaded! ID: " << outTextureID << std::endl;
    }

    void createFallbackTexture() {
        std::cout << "Creating procedural texture..." << std::endl;

        const int width = 512;
        const int height = 512;

        std::vector<unsigned char> textureData(width * height * 4);

        // Créer une texture procédurale intéressante
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 4;

                float u = (float)x / width;
                float v = (float)y / height;

                // Dégradé radial
                float dx = u - 0.5f;
                float dy = v - 0.5f;
                float dist = std::sqrt(dx*dx + dy*dy) * 2.0f;

                // Couleurs basées sur la position et la distance
                unsigned char r = static_cast<unsigned char>((0.5f + 0.5f * std::sin(u * 10.0f)) * 200);
                unsigned char g = static_cast<unsigned char>((0.5f + 0.5f * std::cos(v * 10.0f)) * 200);
                unsigned char b = static_cast<unsigned char>((1.0f - dist) * 255);

                textureData[index + 0] = r;
                textureData[index + 1] = g;
                textureData[index + 2] = b;
                textureData[index + 3] = 255;

                // Ajouter un motif
                if (((x / 64) + (y / 64)) % 2 == 0) {
                    textureData[index + 0] = static_cast<unsigned char>(textureData[index + 0] * 0.7f);
                    textureData[index + 1] = static_cast<unsigned char>(textureData[index + 1] * 0.7f);
                }
            }
        }

        glGenTextures(1, &fallbackTextureID);
        glBindTexture(GL_TEXTURE_2D, fallbackTextureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "Procedural texture created! ID: " << fallbackTextureID << std::endl;
    }

    void cleanup() {
        if (textureID != 0) glDeleteTextures(1, &textureID);
        if (fallbackTextureID != 0) glDeleteTextures(1, &fallbackTextureID);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (EBO != 0) glDeleteBuffers(1, &EBO);
        if (shaderProgram != 0) glDeleteProgram(shaderProgram);
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
        // Vertex Shader - Rotation + contrôle texture
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec2 aTexCoord;

            out vec2 vTexCoord;

            uniform float uRotation;       // Angle de rotation en degrés
            uniform float uTextureRepeat;  // Répétition de texture
            uniform vec2 uTextureOffset;   // Décalage de texture
            uniform float uPulseAmount;    // Effet de pulsation

            void main() {
                // Appliquer la rotation
                float angle = radians(uRotation);
                float cosA = cos(angle);
                float sinA = sin(angle);

                vec2 pos = aPosition;

                // Rotation autour du centre (0,0)
                pos = vec2(
                    pos.x * cosA - pos.y * sinA,
                    pos.x * sinA + pos.y * cosA
                );

                // Effet de pulsation léger (optionnel)
                pos *= (1.0 + uPulseAmount * 0.1);

                gl_Position = vec4(pos, 0.0, 1.0);

                // Coordonnées de texture avec répétition et décalage
                vTexCoord = aTexCoord * uTextureRepeat + uTextureOffset;
            }
        )";

        // Fragment Shader - Effets visuels
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec2 vTexCoord;
            layout(location = 0) out vec4 outColor;

            uniform sampler2D uTexture;
            uniform float uTime;
            uniform float uRotation;

            void main() {
                // Échantillonner la texture
                vec4 texColor = texture(uTexture, vTexCoord);

                // Effet de rotation subtil dans la couleur
                float rotationEffect = sin(radians(uRotation) * 2.0) * 0.1;
                texColor.r += rotationEffect;
                texColor.g -= rotationEffect * 0.5;

                // Effet de vignette (assombrissement des bords)
                vec2 centeredUV = vTexCoord * 2.0 - 1.0;
                float vignette = 1.0 - dot(centeredUV, centeredUV) * 0.2;
                vignette = clamp(vignette, 0.7, 1.0);

                // Appliquer la vignette seulement si on voit les bords
                // (quand textureRepeat est proche de 1.0)
                float repeatFactor = 1.0; // Vous pourriez passer ça en uniform
                if (repeatFactor < 1.5) {
                    texColor.rgb *= vignette;
                }

                // Bordure noire subtile
                float border = 0.01;
                float edge = smoothstep(0.0, border, vTexCoord.x) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.x) *
                           smoothstep(0.0, border, vTexCoord.y) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.y);

                texColor.rgb *= mix(0.5, 1.0, edge);

                outColor = texColor;
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
            throw std::runtime_error(std::string("Erreur linkage:\n") + std::string(infoLog));
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Récupérer les locations
        timeUniformLoc = glGetUniformLocation(program, "uTime");
        rotationUniformLoc = glGetUniformLocation(program, "uRotation");
        textureRepeatUniformLoc = glGetUniformLocation(program, "uTextureRepeat");
        textureOffsetUniformLoc = glGetUniformLocation(program, "uTextureOffset");
        textureUniformLoc = glGetUniformLocation(program, "uTexture");
        pulseAmountUniformLoc = glGetUniformLocation(program, "uPulseAmount");

        std::cout << "Shader avec rotation et texture créé!" << std::endl;

        return program;
    }
};

std::unique_ptr<QuadTextureFinal> g_quadTextureFinal;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "QUAD FINAL: Texture + Rotation" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Une texture couvre tout le quad" << std::endl;
        std::cout << "Le quad tourne sur lui-même" << std::endl;
        std::cout << "========================================" << std::endl;

        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Quad Final - Texture + Rotation";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        g_quadTextureFinal = std::make_unique<QuadTextureFinal>();

        common::SystemObserverSubject::AddObserver(g_quadTextureFinal.get());
        common::DrawObserverSubject::AddObserver(g_quadTextureFinal.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(g_quadTextureFinal.get());
        common::DrawObserverSubject::RemoveObserver(g_quadTextureFinal.get());
        g_quadTextureFinal.reset();

        std::cout << "\n=== FIN ===\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nERREUR: " << e.what() << std::endl;

        if (g_quadTextureFinal) {
            common::SystemObserverSubject::RemoveObserver(g_quadTextureFinal.get());
            common::DrawObserverSubject::RemoveObserver(g_quadTextureFinal.get());
            g_quadTextureFinal.reset();
        }
        return -1;
    }

    return 0;
}