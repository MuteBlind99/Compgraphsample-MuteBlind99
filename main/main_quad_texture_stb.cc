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

class QuadTextureFixed : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "QuadTextureFixed::Begin()" << std::endl;
        initGL();

        // Charger une texture
        loadTexture("data/textures/brickwall.jpg", textureID, "Brick");

        if (textureID == 0) {
            createFallbackTexture();
            textureID = fallbackTextureID;
        }
    }

    void Update(float dt) override {
        time += dt;

        rotationAngle += rotationSpeed * dt;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
        // Contrôles pour ajuster la texture
        // Augmentez/diminuez textureRepeat avec des touches si vous voulez
        static bool spacePressed = false;
        // Vous pourriez ajouter: if (keyPressed) textureRepeat += 0.1f;


        // Animation de défilement optionnelle
        textureOffsetX = std::fmod(textureOffsetX + panSpeedX * dt, 1.0f);
        textureOffsetY = std::fmod(textureOffsetY + panSpeedY * dt, 1.0f);
    }

    void FixedUpdate() override {}

    void End() override {
        std::cout << "QuadTextureFixed::End()" << std::endl;
        cleanup();
    }

    void PreDraw() override {}

    void Draw() override {
        glUseProgram(shaderProgram);

        // Passer les uniforms
        if (timeUniformLoc != -1) {
            glUniform1f(timeUniformLoc, time);
        }

        if (textureRepeatUniformLoc != -1) {
            glUniform1f(textureRepeatUniformLoc, textureRepeat);
        }

        if (textureOffsetUniformLoc != -1) {
            glUniform2f(textureOffsetUniformLoc, textureOffsetX, textureOffsetY);
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
    float rotationSpeed = 30.0f;
    float textureRepeat = 1.0f;  // 1.0 = une fois, 2.0 = deux fois, etc.
    float textureOffsetX = 0.0f;
    float textureOffsetY = 0.0f;
    float panSpeedX = 0.0f;  // Mettre à 0 pour pas de défilement
    float panSpeedY = 0.0f;

    // Locations des uniforms
    GLint timeUniformLoc = -1;
    GLint textureRepeatUniformLoc = -1;
    GLint textureOffsetUniformLoc = -1;
    GLint textureUniformLoc = -1;

    struct Vertex {
        float position[2];  // x, y
        float texCoord[2];  // u, v (base 0-1)
    };

    void initGL() {
        std::cout << "Initializing Quad with Fixed Texture Mapping..." << std::endl;

        // Création des shaders
        shaderProgram = createShaderProgram();

        // DONNÉES DES VERTICES AVEC COORDONNÉES DE BASE (0-1)
        // La texture sera étirée sur tout le quad
        std::vector<Vertex> vertices = {
            // Position        // TexCoord BASE (0.0 à 1.0)
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
        glClearColor(0.2f, 0.2f, 0.25f, 1.0f);

        std::cout << "Quad with proper texture mapping initialized!" << std::endl;
        std::cout << "Texture will cover the entire quad (UV: 0-1)" << std::endl;
    }

    void loadTexture(const std::string& filepath, GLuint& outTextureID, const std::string& name) {
        std::cout << "Loading texture: " << name << std::endl;

        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);

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

        // IMPORTANT: Configurer le wrapping et le filtrage pour couvrir tout le quad

        // Option A: STRETCH (étirer) - Par défaut, étire la texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Option B: REPEAT (répéter) - Répète si textureRepeat > 1
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Filtrage
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Générer mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);

        // Libérer
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "  Texture loaded successfully! ID: " << outTextureID << std::endl;
    }

    void createFallbackTexture() {
        std::cout << "Creating colorful gradient texture..." << std::endl;

        const int width = 512;
        const int height = 512;

        std::vector<unsigned char> textureData(width * height * 4);

        // Créer une texture dégradé colorée
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 4;

                // Dégradé rouge/vert/bleu
                float u = (float)x / width;
                float v = (float)y / height;

                textureData[index + 0] = static_cast<unsigned char>(u * 255);        // R
                textureData[index + 1] = static_cast<unsigned char>(v * 255);        // G
                textureData[index + 2] = static_cast<unsigned char>((1.0f - u) * 255); // B
                textureData[index + 3] = 255;                                       // A

                // Ajouter un motif de damier subtil
                if (((x / 32) + (y / 32)) % 2 == 0) {
                    textureData[index + 0] = textureData[index + 0] * 0.8f;
                    textureData[index + 1] = textureData[index + 1] * 0.8f;
                    textureData[index + 2] = textureData[index + 2] * 0.8f;
                }
            }
        }

        glGenTextures(1, &fallbackTextureID);
        glBindTexture(GL_TEXTURE_2D, fallbackTextureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());

        // Pour couvrir tout le quad
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::cout << "Gradient texture created! ID: " << fallbackTextureID << std::endl;
    }

    void cleanup() {
        if (textureID != 0) {
            glDeleteTextures(1, &textureID);
        }
        if (fallbackTextureID != 0) {
            glDeleteTextures(1, &fallbackTextureID);
        }
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
        }
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
        }
        if (EBO != 0) {
            glDeleteBuffers(1, &EBO);
        }
        if (shaderProgram != 0) {
            glDeleteProgram(shaderProgram);
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
        // Vertex Shader - Applique le facteur de répétition
        std::string vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec2 aPosition;
            layout(location = 1) in vec2 aTexCoord;

            out vec2 vTexCoord;

            uniform float uTextureRepeat; // Combien de fois répéter la texture
            uniform vec2 uTextureOffset;  // Décalage de texture

            void main() {
                // Position standard
                gl_Position = vec4(aPosition, 0.0, 1.0);

                // Appliquer le facteur de répétition et le décalage
                vTexCoord = aTexCoord * uTextureRepeat + uTextureOffset;
            }
        )";

        // Fragment Shader - Simple échantillonnage
        std::string fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec2 vTexCoord;
            layout(location = 0) out vec4 outColor;

            uniform sampler2D uTexture;
            uniform float uTime;

            void main() {
                // Échantillonner la texture
                vec4 texColor = texture(uTexture, vTexCoord);

                // Option: effet de pulsation subtil basé sur le temps
                float pulse = 0.9 + 0.1 * sin(uTime * 2.0);
                texColor.rgb *= pulse;

                // Option: effet de bordure
                float border = 0.02;
                float edge = smoothstep(0.0, border, vTexCoord.x) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.x) *
                           smoothstep(0.0, border, vTexCoord.y) *
                           smoothstep(1.0, 1.0 - border, vTexCoord.y);

                // Appliquer seulement si textureRepeat > 1
                if (edge < 0.99) {
                    texColor.rgb *= edge;
                }

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
        textureRepeatUniformLoc = glGetUniformLocation(program, "uTextureRepeat");
        textureOffsetUniformLoc = glGetUniformLocation(program, "uTextureOffset");
        textureUniformLoc = glGetUniformLocation(program, "uTexture");

        std::cout << "Shader program created with texture control uniforms" << std::endl;

        return program;
    }
};

std::unique_ptr<QuadTextureFixed> g_quadTextureFixed;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Quad avec Texture (Couverture complète)" << std::endl;
        std::cout << "TextureRepeat = 1.0 (une copie sur tout le quad)" << std::endl;
        std::cout << "Changez textureRepeat pour 2.0, 3.0, etc." << std::endl;
        std::cout << "========================================" << std::endl;

        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Quad - Texture Complète";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        g_quadTextureFixed = std::make_unique<QuadTextureFixed>();

        common::SystemObserverSubject::AddObserver(g_quadTextureFixed.get());
        common::DrawObserverSubject::AddObserver(g_quadTextureFixed.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(g_quadTextureFixed.get());
        common::DrawObserverSubject::RemoveObserver(g_quadTextureFixed.get());
        g_quadTextureFixed.reset();

        std::cout << "\nTerminé!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nERREUR: " << e.what() << std::endl;

        if (g_quadTextureFixed) {
            common::SystemObserverSubject::RemoveObserver(g_quadTextureFixed.get());
            common::DrawObserverSubject::RemoveObserver(g_quadTextureFixed.get());
            g_quadTextureFixed.reset();
        }
        return -1;
    }

    return 0;
}