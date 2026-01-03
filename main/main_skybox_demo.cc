//
// Created by forna on 02.01.2026.
//
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <imgui.h>
#include <SDL3/SDL.h>

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
        SDL_Window* window = common::GetWindow();

        if (window != nullptr) {
            SDL_SetWindowRelativeMouseMode(window, true);
        } else {
            // Gérer l'erreur ou logger un avertissement
            std::cerr << "Erreur : Impossible de récupérer la fenêtre SDL." << std::endl;
        }


        SDL_SetWindowRelativeMouseMode(window, true);
        initGL();
        createSkyboxRessources();
    }

    void Update(float dt) override {
        time_+=dt;

        // caméra orbitale simple autour de l’origine
        // float radius = 3.0f;
        // camPos_[0]=std::cos(time_*0.3f)*radius;
        // camPos_[1] = 1.0f;
        // camPos_[2] = std::sin(time_ * 0.3f) * radius;
        // --- clavier ---
        const bool* keys = SDL_GetKeyboardState(nullptr);

        float velocity = moveSpeed_ * dt;

        if (keys[SDL_SCANCODE_W]) {
            camPos_[0] += camFront_[0] * velocity;
            camPos_[1] += camFront_[1] * velocity;
            camPos_[2] += camFront_[2] * velocity;
        }
        if (keys[SDL_SCANCODE_S]) {
            camPos_[0] -= camFront_[0] * velocity;
            camPos_[1] -= camFront_[1] * velocity;
            camPos_[2] -= camFront_[2] * velocity;
        }

        float right[3];
        cross(right, camFront_, camUp_);
        normalize(right);

        if (keys[SDL_SCANCODE_D]) {
            camPos_[0] += right[0] * velocity;
            camPos_[1] += right[1] * velocity;
            camPos_[2] += right[2] * velocity;
        }
        if (keys[SDL_SCANCODE_A]) {
            camPos_[0] -= right[0] * velocity;
            camPos_[1] -= right[1] * velocity;
            camPos_[2] -= right[2] * velocity;
        }

        // --- souris (relative) ---
        float mdx = 0.0f, mdy = 0.0f;
        SDL_GetRelativeMouseState(&mdx, &mdy);

        mdx *= mouseSensitivity_;
        mdy *= mouseSensitivity_;

        yaw_   += mdx;
        pitch_ -= mdy;

        if (pitch_ > 89.0f) pitch_ = 89.0f;
        if (pitch_ < -89.0f) pitch_ = -89.0f;

        // recalcul direction (front) depuis yaw/pitch
        float yawRad = yaw_ * 3.1415926535f / 180.0f;
        float pitchRad = pitch_ * 3.1415926535f / 180.0f;

        camFront_[0] = std::cos(yawRad) * std::cos(pitchRad);
        camFront_[1] = std::sin(pitchRad);
        camFront_[2] = std::sin(yawRad) * std::cos(pitchRad);
        normalize(camFront_);

    }

    void FixedUpdate() override{}

    void End() override{cleanup();}

    void PreDraw() override {}

    void Draw() override{
        glClearColor(0.05f,0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        //matrices
        float view[16];
        float proj[16];
        lookAtMatrix(view, camPos_[0],camPos_[1],camPos_[2], camPos_[0] + camFront_[0],
            camPos_[1] + camFront_[1], camPos_[2] + camFront_[2], camUp_[0], camUp_[1], camUp_[2]);
        perspectiveMatrix(proj, 60.0f, 800.0f / 600.0f, 0.1f, 100.0f);

        glDepthFunc(GL_LEQUAL);

        glUseProgram(skyboxProgram_);

        // 1) matrices
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uProj"), 1, GL_FALSE, proj);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram_, "uView"), 1, GL_FALSE, view);

        // 2) sampler -> unité 0 (tu peux le faire ici, ou une seule fois dans Begin())
        GLint locSky = glGetUniformLocation(skyboxProgram_, "uSkybox");
        glUniform1i(locSky, 0);

        // 3) bind la cubemap sur GL_TEXTURE0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex_);

        glBindVertexArray(skyboxVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
        glUseProgram(0);

        glDepthFunc(GL_LESS);
}

    void PostDraw() override{}

private:
    float time_=0.0f;
    //float camPos_[3]={0.0f,1.0f,3.0f};
    float camPos_[3]   = {0.0f, 0.0f, 3.0f};
    float camFront_[3] = {0.0f, 0.0f, -1.0f};
    float camUp_[3]    = {0.0f, 1.0f, 0.0f};

    float yaw_   = -90.0f;  // regarde vers -Z
    float pitch_ = 0.0f;

    bool firstMouse_ = true;
    float lastMouseX_ = 400.0f;
    float lastMouseY_ = 300.0f;

    float mouseSensitivity_ = 0.10f;
    float moveSpeed_ = 3.5f;
    GLuint skyboxVAO_ = 0;
    GLuint skyboxVBO_ = 0;
    GLuint cubemapTex_ = 0;
    GLuint skyboxProgram_ = 0;

    static void normalize(float v[3]) {
        float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        if (len > 0.00001f) { v[0]/=len; v[1]/=len; v[2]/=len; }
    }

    static void cross(float out[3], const float a[3], const float b[3]) {
        out[0] = a[1]*b[2] - a[2]*b[1];
        out[1] = a[2]*b[0] - a[0]*b[2];
        out[2] = a[0]*b[1] - a[1]*b[0];
    }
    static void initGL() {
        // glEnable(GL_DEPTH_TEST);
        // glDepthFunc(GL_LESS);
        //
        // //
        // glEnable(GL_CULL_FACE);
        // glCullFace(GL_BACK);
        // glFrontFace(GL_CCW);
        glCullFace(GL_FRONT);   // pendant skybox
        // draw
        glCullFace(GL_BACK);    // après
    }

    void createSkyboxRessources() {
        // 1) géométrie cube skybox (positions uniquement)
        constexpr float skyboxVertices[] = {
            -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
            -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
             1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
            -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
            -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
            -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1
        };
        glGenVertexArrays(1, &skyboxVAO_);
        glGenBuffers(1, &skyboxVBO_);
        glBindVertexArray(skyboxVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
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
        skyboxProgram_=createProgram(vs,fs);
        glUseProgram(skyboxProgram_);
        glUniform1i(glGetUniformLocation(skyboxProgram_,"uSkybox"),0);

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

    static GLuint loadCubemap(const std::vector<std::string>& path) {
        // Vérification de sécurité
        if (path.size() < 6) {
            std::cerr << "[Skybox] Error: Path vector must contain 6 textures.\n";
            return 0;
        }

        GLuint texID;
        glGenTextures(1,&texID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

        // pour cubemap : généralement on ne flip PAS
        stbi_set_flip_vertically_on_load(false);

        int w,h,comp;
        for (int i=0; i<6;i++) {
            unsigned char* data = stbi_load(path[i].c_str(), &w, &h, &comp, 0);
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
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i,0,format,w,h,0,format,GL_UNSIGNED_BYTE,data);
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

    static GLuint compileShader(GLenum type,const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src=source.c_str();
        glShaderSource(shader,1,&src,nullptr );
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::string shaderType=(type ==GL_VERTEX_SHADER)?"VERTEX":"FRAGMENT";
            throw std::runtime_error("Erreur compilation " + shaderType + ":\n" + std::string(infoLog));
        }
        return shader;
    }

    static GLuint createProgram(const std::string& vertexSrc, const std::string& fragmentSrc) {
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

    static void lookAtMatrix(float* m,
                             float eyeX, float eyeY, float eyeZ,
                             float centerX, float centerY, float centerZ,
                             float upX, float upY, float upZ) {
        float fx = centerX - eyeX;
        float fy = centerY - eyeY;
        float fz = centerZ - eyeZ;
        float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
        fx /= fl; fy /= fl; fz /= fl;

        float ux = upX, uy = upY, uz = upZ;
        float ul = std::sqrt(ux*ux + uy*uy + uz*uz);
        ux /= ul; uy /= ul; uz /= ul;

        float rx = fy * uz - fz * uy;
        float ry = fz * ux - fx * uz;
        float rz = fx * uy - fy * ux;

        float rl = std::sqrt(rx*rx + ry*ry + rz*rz);
        rx /= rl; ry /= rl; rz /= rl;

        float ux2 = ry * fz - rz * fy;
        float uy2 = rz * fx - rx * fz;
        float uz2 = rx * fy - ry * fx;

        // colonne-major (OpenGL)
        m[0] = rx;  m[4] = ux2; m[8]  = -fx; m[12] = 0.0f;
        m[1] = ry;  m[5] = uy2; m[9]  = -fy; m[13] = 0.0f;
        m[2] = rz;  m[6] = uz2; m[10] = -fz; m[14] = 0.0f;
        m[3] = 0;   m[7] = 0;   m[11] = 0;   m[15] = 1.0f;

        // translation
        float t[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            -eyeX, -eyeY, -eyeZ, 1
        };

        float out[16];
        multiplyMat4(out, m, t);
        for (int i=0;i<16;i++) m[i]=out[i];
    }

    static void perspectiveMatrix(float* m , float fov, float aspect, float near, float far) {
        float rad = fov * 3.1415926535f / 180.0f;
        float f = 1.0f / std::tan(rad * 0.5f);

        for (int i=0;i<16;i++) m[i]=0.0f;
        m[0] = f / aspect;
        m[5] = f;
        m[10] = (far + near) / (near - far);
        m[11] = -1.0f;
        m[14] = (2.0f * far * near) / (near - far);
    }

    static void multiplyMat4(float* out, const float* a,const float* b ) {
        for (int c = 0; c < 4; c++) {
            for (int r = 0; r < 4; r++) {
                out[c*4 + r] =
                    a[0*4 + r] * b[c*4 + 0] +
                    a[1*4 + r] * b[c*4 + 1] +
                    a[2*4 + r] * b[c*4 + 2] +
                    a[3*4 + r] * b[c*4 + 3];
            }
        }
    }

    void cleanup() {
        if (skyboxProgram_)glDeleteProgram(skyboxProgram_);
        if (cubemapTex_)glDeleteTextures(1,&cubemapTex_);
        if (skyboxVBO_)glDeleteBuffers(1,&skyboxVBO_);
        if (skyboxVAO_) glDeleteVertexArrays(1,&skyboxVAO_);
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
    } catch (const std::exception& e) {
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
