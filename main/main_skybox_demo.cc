//
// Created by forna on 02.01.2026.
//
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <imgui.h>
#include <SDL3/SDL.h>

#include "model_loader.h"
#include "engine/engine.h"
#include "engine/window.h"
#include "engine/renderer.h"
#include "engine/system.h"
#include "third_party/gl_include.h"

#include "stb_image.h"

class SkyboxRenderer : public common::DrawInterface, public common::SystemInterface {
public:
    void Begin() override {
        std::cout << "SkyboxRenderer::Begin() - Initialisation skybox" << std::endl;
        // Utilisation de GetWindow() qui est déjà typé SDL_Window*
        SDL_Window *window = common::GetWindow();
        SetMouseLook(true);
        // if (window != nullptr) {
        //     SDL_SetWindowRelativeMouseMode(window, true);
        // } else {
        //     // Gérer l'erreur ou logger un avertissement
        //     std::cerr << "Erreur : Impossible de récupérer la fenêtre SDL." << std::endl;
        // }

        initGL();
        // --- load model ---
        model_ = std::make_unique<Model>("data/model/nanosuit2/nanosuit.obj");
        core::Vec3F c = model_->GetCenter();
        float offsetY = -model_->GetMinY();
        target_[0] = c.x;
        target_[1] = c.y+offsetY;
        target_[2] = c.z;

        //orbitRadius_ = model_->GetBoundingRadius() * 2.5f;
        orbitRadius_ = std::max(orbitRadius_, 5.0f);
        std::cout << "Center: " << target_[0] << "," << target_[1] << "," << target_[2]
          << "  Radius: " << orbitRadius_ << std::endl;
        // optionnel: angle de départ pour bien voir
        yaw_=90.0f;
        pitch_ = 10.0f;
        createSkyboxRessources();
    }

    void Update(float dt) override {
        time_ += dt;

        // --- clavier ---
        const bool *keys = SDL_GetKeyboardState(nullptr);

        // ---- TOGGLE TAB (anti-rebond) ----
        bool tabDown = keys[SDL_SCANCODE_TAB];
        if (tabDown && !tabWasDown_) {
            SetMouseLook(!mouseLookEnabled_);
        }
        tabWasDown_ = tabDown;

        // ---- si la caméra est désactivée, on ne bouge pas ----
        if (!mouseLookEnabled_) return;



        // --- souris (relative) ---
        float mdx = 0.0f, mdy = 0.0f;
        SDL_GetRelativeMouseState(&mdx, &mdy);

        mdx *= mouseSensitivity_;
        mdy *= mouseSensitivity_;

        yaw_ += mdx;
        pitch_ -= mdy;

        if (pitch_ > maxPitch_) pitch_ = maxPitch_;
        if (pitch_ < minPitch_) pitch_ = minPitch_;


    }

    void FixedUpdate() override {
    }

    void End() override {
        SetMouseLook(false);
        cleanup();
    }

    void PreDraw() override {
    }

    void Draw() override {
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //matrices
        float view[16], proj[16], modelMat[16];


        float yawRad   = yaw_   * 3.1415926535f / 180.0f;
        float pitchRad = pitch_ * 3.1415926535f / 180.0f;

        // position caméra sur la sphère autour du centre
        float camX = target_[0] + orbitRadius_ * std::cos(pitchRad) * std::cos(yawRad);
        float camY = target_[1] + orbitRadius_ * std::sin(pitchRad);
        float camZ = target_[2] + orbitRadius_ * std::cos(pitchRad) * std::sin(yawRad);


        // caméra -> regarde le centre du modèle
        lookAtMatrix(view,
            camX, camY, camZ,
            target_[0], target_[1], target_[2],
            0.0f, 1.0f, 0.0f);
        perspectiveMatrix(proj, 60.0f, 800.0f / 600.0f, 0.1f, 500.0f);

        // --- Modèle ---
        if (!model_) {
            std::cout << "[Model] model_ is null\n";
        } else {
            std::cout << "[Model] drawing...\n";
        }


        identityMatrix(modelMat);
        // float s = 0.8f;      // essaye 0.1f, 0.05f, 0.01f
        // modelMat[0]  = s;      // scale X
        // modelMat[5]  = s;      // scale Y
        // modelMat[10] = s;      // scale Z
        //modelMat[13] += -model_->GetMinY();
        // direction modèle -> caméra sur le plan XZ
        float dx = camX - target_[0];
        float dz = camZ - target_[2];

        // atan2 renvoie l'angle autour de Y
        float angleY = std::atan2(dx, dz); // radians

        float c = std::cos(angleY);
        float s = std::sin(angleY);
        // rotation Y
        modelMat[0]  =  c;   modelMat[8]  =  s;
        modelMat[2]  = -s;   modelMat[10] =  c;

        // pose au sol
        modelMat[13] += -model_->GetMinY();
        glDepthFunc(GL_LESS);
        glUseProgram(modelProgram_);

        GLint loc;

        loc=glGetUniformLocation(modelProgram_,"uViewPos");
        if (loc>=0)glUniform3f(loc, camX,camY, camZ);
        loc=glGetUniformLocation(modelProgram_,"uLightPos");
        if (loc>=0)glUniform3f(loc,target_[0]+3.0f,target_[1]+5.0f,target_[2]+3.0f);
        loc = glGetUniformLocation(modelProgram_, "uLightColor");
        if (loc >= 0) glUniform3f(loc, 1.0f, 1.0f, 1.0f);
        loc = glGetUniformLocation(modelProgram_, "uMaterialShininess");
        if (loc >= 0) glUniform1f(loc, 32.0f);

        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uModel"), 1, GL_FALSE, modelMat);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uView"), 1, GL_FALSE, view);
        glUniformMatrix4fv(glGetUniformLocation(modelProgram_, "uProj"), 1, GL_FALSE, proj);
        glUniform3fv(lightPosUniformLoc, 1, lightPos);
        glUniform3f(lightColorUniformLoc, 1.0f, 1.0f, 1.0f);
        glUniform1f(materialShininessUniformLoc, 32.0f);
        model_->Draw(modelProgram_);

        //--Skybox--
        glDepthFunc(GL_LEQUAL);

        glUseProgram(skyboxProgram_);

        // matrices
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uProj"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uView"), 1, GL_FALSE, view);

        // sampler -> unité 0
        GLint locSky = glGetUniformLocation(skyboxProgram_, "uSkybox");
        glUniform1i(locSky, 0);

        //  bind cubemap
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex_);

        glBindVertexArray(skyboxVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
        glUseProgram(0);

        glDepthFunc(GL_LESS);
    }

    void PostDraw() override {
    }

private:
    float time_ = 0.0f;
    float lightPos[3] = {2.0f, 3.0f, 3.0f};
    //float camPos_[3]={0.0f,1.0f,3.0f};
    float camPos_[3] = {0.0f, 0.0f, 3.0f};
    float camFront_[3] = {0.0f, 0.0f, -1.0f};
    float camUp_[3] = {0.0f, 1.0f, 0.0f};

    float yaw_ = -90.0f; // regarde vers -Z
    float pitch_ = 0.0f;

    // centre de l’orbite (le modèle)
    float target_[3] = {0.0f, 0.0f, 0.0f};

    // rayon d’orbite
    float orbitRadius_ = 20.0f;

    // limites de pitch
    float minPitch_ = -89.0f;
    float maxPitch_ = 89.0f;

    bool firstMouse_ = true;
    float lastMouseX_ = 400.0f;
    float lastMouseY_ = 300.0f;

    bool mouseLookEnabled_ = true;
    bool tabWasDown_ = false; //anti-rebond

    float mouseSensitivity_ = 0.10f;
    float moveSpeed_ = 3.5f;
    GLint modelMatrixUniformLoc = -1;
    GLint viewMatrixUniformLoc = -1;
    GLint projectionMatrixUniformLoc = -1;
    GLint viewPosUniformLoc = -1;
    GLint lightPosUniformLoc = -1;
    GLint lightColorUniformLoc = -1;
    GLint materialShininessUniformLoc = -1;
    GLint diffuseTextureUniformLoc = -1;

    GLuint skyboxVAO_ = 0;
    GLuint skyboxVBO_ = 0;
    GLuint cubemapTex_ = 0;
    GLuint skyboxProgram_ = 0;

    //--- Model ---
    std::unique_ptr<Model> model_;
    GLuint modelProgram_ = 0;

    static void identityMatrix(float *m) {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    static void normalize(float v[3]) {
        float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (len > 0.00001f) {
            v[0] /= len;
            v[1] /= len;
            v[2] /= len;
        }
    }

    static void cross(float out[3], const float a[3], const float b[3]) {
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
    }

    void SetMouseLook(bool enabled) {
        mouseLookEnabled_ = enabled;
        SDL_Window *window = common::GetWindow();

        SDL_SetWindowRelativeMouseMode(window, enabled);
        float dx, dy;
        SDL_GetRelativeMouseState(&dx, &dy);
        firstMouse_ = true;
    }

     GLuint createModelShaderProgram(){
        // Vertex Shader
        const char* vertexSource = R"(
            #version 300 es
            precision mediump float;

            layout(location = 0) in vec3 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aTexCoord;

            out vec3 FragPos;
            out vec3 Normal;
            out vec2 TexCoord;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProj;

            void main() {
                FragPos = vec3(uModel * vec4(aPosition, 1.0));
                Normal = mat3(transpose(inverse(uModel))) * aNormal;
                TexCoord = aTexCoord;

                gl_Position = uProj* uView * vec4(FragPos, 1.0);
            }
        )";

        // Fragment Shader
        const char* fragmentSource = R"(
            #version 300 es
            precision mediump float;

            in vec3 FragPos;
            in vec3 Normal;
            in vec2 TexCoord;

            out vec4 FragColor;

            uniform sampler2D texture_diffuse1;

            uniform vec3 uViewPos;
            uniform vec3 uLightPos;
            uniform vec3 uLightColor;
            uniform float uMaterialShininess;

            void main() {
                // Couleur diffuse de la texture
                vec3 diffuseColor = texture(texture_diffuse1, TexCoord).rgb;

                // Ambient
                vec3 ambient = uLightColor * diffuseColor * 0.1;

                // Diffuse
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(uLightPos - FragPos);
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = uLightColor * diff * diffuseColor;

                // Specular
                vec3 viewDir = normalize(uViewPos - FragPos);
                vec3 reflectDir = reflect(-lightDir, norm);
                float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterialShininess);
                vec3 specular = uLightColor * spec * 0.5;

                // Résultat final
                vec3 result = ambient + diffuse + specular;

                FragColor = vec4(result, 1.0);
            }
        )";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        // Vérifier les erreurs
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error(std::string("Erreur linkage shader: ") + infoLog);
        }

        // Récupérer les locations
        modelMatrixUniformLoc = glGetUniformLocation(program, "uModel");
        viewMatrixUniformLoc = glGetUniformLocation(program, "uView");
        projectionMatrixUniformLoc = glGetUniformLocation(program, "uProj");
        viewPosUniformLoc = glGetUniformLocation(program, "uViewPos");
        lightPosUniformLoc = glGetUniformLocation(program, "uLightPos");
        lightColorUniformLoc = glGetUniformLocation(program, "uLightColor");
        materialShininessUniformLoc = glGetUniformLocation(program, "uMaterialShininess");
        diffuseTextureUniformLoc = glGetUniformLocation(program, "uDiffuseTexture");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        std::cout << "✓ Shader modèle créé" << std::endl;

        return program;
}

     void initGL() {
        // Créer les shaders
        modelProgram_ = createModelShaderProgram();
        //lightShaderProgram_ = createLightShaderProgram();

        // Configuration OpenGL
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Activer le blending pour les textures transparentes
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        //glCullFace(GL_FRONT); // pendant skybox
        glEnable(GL_CULL_FACE);
        // draw
        glCullFace(GL_BACK); // après
    }

    void createSkyboxRessources() {
        // 1) géométrie cube skybox (positions uniquement)
        constexpr float skyboxVertices[] = {
            -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1,
            -1, -1, 1, -1, -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1,
            1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1,
            -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 1, -1, -1, 1,
            -1, 1, -1, 1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1, 1, -1, 1, -1,
            -1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, -1, 1, 1, -1, 1
        };
        glGenVertexArrays(1, &skyboxVAO_);
        glGenBuffers(1, &skyboxVBO_);
        glBindVertexArray(skyboxVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
        glBindVertexArray(0);

        // 2) shaders skybox (inline, comme ton demo)
        const std::string vs = R"(
            #version 300 es
            precision highp float;
            layout (location = 0) in vec3 aPos;
            out vec3 TexCoords;

            uniform mat4 uView;
            uniform mat4 uProj;

            void main() {
                TexCoords = aPos;

                // enlever la translation de la view (skybox centrée)
                mat4 viewNoTranslation = mat4(mat3(uView));
                vec4 pos = uProj * viewNoTranslation * vec4(aPos, 1.0);

                // profondeur à 1.0
                gl_Position = pos.xyww;
            }
        )";

        const std::string fs = R"(
            #version 300 es
            precision mediump float;
            precision mediump samplerCube;
            in vec3 TexCoords;
            out vec4 FragColor;

            uniform samplerCube uSkybox;

            void main() {

                FragColor = texture(uSkybox, TexCoords);
            }
        )";
        skyboxProgram_ = createProgram(vs, fs);
        glUseProgram(skyboxProgram_);
        glUniform1i(glGetUniformLocation(skyboxProgram_, "uSkybox"), 0);

        glUseProgram(0);

        // 3) chargement cubemap
        // IMPORTANT : ordre standard cubemap OpenGL
        // right, left, top, bottom, front, back
        cubemapTex_ = loadCubemap({
            "data/assets/right.jpg",
            "data/assets/left.jpg",
            "data/assets/top.jpg",
            "data/assets/bottom.jpg",
            "data/assets/front.jpg",
            "data/assets/back.jpg"
        });
    }

    static GLuint loadCubemap(const std::vector<std::string> &path) {
        // Vérification de sécurité
        if (path.size() < 6) {
            std::cerr << "[Skybox] Error: Path vector must contain 6 textures.\n";
            return 0;
        }

        GLuint texID;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

        // pour cubemap : généralement on ne flip PAS
        stbi_set_flip_vertically_on_load(false);

        int w, h, comp;
        for (int i = 0; i < 6; i++) {
            unsigned char *data = stbi_load(path[i].c_str(), &w, &h, &comp, 0);
            if (!data) {
                std::cerr << "[Skybox] FAILED to load: " << path[i]
                        << " | reason: " << stbi_failure_reason() << "\n";
            } else {
                std::cout << "[Skybox] Loaded: " << path[i]
                        << " (" << w << "x" << h << ", comp=" << comp << ")\n";
            }
            //GLenum format = (comp==4)? GL_RGBA:GL_RGB;
            GLenum format = GL_RGB;
            if (comp == 1) format = GL_RED;
            else if (comp == 3) format = GL_RGB;
            else if (comp == 4) format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format,GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        return texID;
    }

    static GLuint compileShader(GLenum type, const std::string &source) {
        GLuint shader = glCreateShader(type);
        const char *src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::string shaderType = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            throw std::runtime_error("Erreur compilation " + shaderType + ":\n" + std::string(infoLog));
        }
        return shader;
    }

    static GLuint createProgram(const std::string &vertexSrc, const std::string &fragmentSrc) {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            throw std::runtime_error("Erreur link program:\n" + std::string(infoLog));
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    static void lookAtMatrix(float *m,
                             float eyeX, float eyeY, float eyeZ,
                             float centerX, float centerY, float centerZ,
                             float upX, float upY, float upZ) {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        fx /= fl;
        fy /= fl;
        fz /= fl;

        float ux = upX, uy = upY, uz = upZ;
        float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
        ux /= ul;
        uy /= ul;
        uz /= ul;

        float rx = fy * uz - fz * uy;
        float ry = fz * ux - fx * uz;
        float rz = fx * uy - fy * ux;

        float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
        rx /= rl;
        ry /= rl;
        rz /= rl;

        float ux2 = ry * fz - rz * fy;
        float uy2 = rz * fx - rx * fz;
        float uz2 = rx * fy - ry * fx;

        // colonne-major (OpenGL)
        m[0] = rx;
        m[4] = ux2;
        m[8] = -fx;
        m[12] = 0.0f;
        m[1] = ry;
        m[5] = uy2;
        m[9] = -fy;
        m[13] = 0.0f;
        m[2] = rz;
        m[6] = uz2;
        m[10] = -fz;
        m[14] = 0.0f;
        m[3] = 0;
        m[7] = 0;
        m[11] = 0;
        m[15] = 1.0f;

        // translation
        float t[16] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -eyeX, -eyeY, -eyeZ, 1
        };

        float out[16];
        multiplyMat4(out, m, t);
        for (int i = 0; i < 16; i++) m[i] = out[i];
    }

    static void perspectiveMatrix(float *m, float fov, float aspect, float near, float far) {
        float rad = fov * 3.1415926535f / 180.0f;
        float f = 1.0f / std::tan(rad * 0.5f);

        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = f / aspect;
        m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    static void multiplyMat4(float *out, const float *a, const float *b) {
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                out[c * 4 + r] =
                        a[0 * 4 + r] * b[c * 4 + 0] +
                        a[1 * 4 + r] * b[c * 4 + 1] +
                        a[2 * 4 + r] * b[c * 4 + 2] +
                        a[3 * 4 + r] * b[c * 4 + 3];
            }
        }
    }

    void cleanup() {
        if (skyboxProgram_)
            glDeleteProgram(skyboxProgram_);
        if (cubemapTex_)glDeleteTextures(1, &cubemapTex_);
        if (skyboxVBO_)
            glDeleteBuffers(1, &skyboxVBO_);
        if (skyboxVAO_)
            glDeleteVertexArrays(1, &skyboxVAO_);
        skyboxProgram_ = 0;
        cubemapTex_ = 0;
        skyboxVAO_ = 0;
        skyboxVBO_ = 0;
    }
};

std::unique_ptr<SkyboxRenderer> g_skyboxRenderer;

int main() {
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "SKYBOX DEMO (CUBEMAP)" << std::endl;
        std::cout << "========================================" << std::endl;

        common::WindowConfig config;
        config.width = 800;
        config.height = 600;
        config.title = "Skybox Demo - Cubemap";
        config.renderer = common::WindowConfig::RendererType::OPENGL;
        config.fixed_dt = 1.0f / 60.0f;
        common::SetWindowConfig(config);

        g_skyboxRenderer = std::make_unique<SkyboxRenderer>();
        common::SystemObserverSubject::AddObserver(g_skyboxRenderer.get());
        common::DrawObserverSubject::AddObserver(g_skyboxRenderer.get());

        common::RunEngine();

        common::SystemObserverSubject::RemoveObserver(g_skyboxRenderer.get());
        common::DrawObserverSubject::RemoveObserver(g_skyboxRenderer.get());
        g_skyboxRenderer.reset();

        std::cout << "\n✓ Démonstration terminée avec succès!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << std::endl;

        if (g_skyboxRenderer) {
            common::SystemObserverSubject::RemoveObserver(g_skyboxRenderer.get());
            common::DrawObserverSubject::RemoveObserver(g_skyboxRenderer.get());
            g_skyboxRenderer.reset();
        }
        return -1;
    }
    return 0;
}
