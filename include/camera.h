//
// Created by forna on 07.02.2026.
//

#ifndef CAMERA_H
#define CAMERA_H
#include <array>
#include <cmath>
#include "maths/vec3.h"

class Camera {
public:
    // ==================== CONSTRUCTEURS =================
    Camera();
    ~Camera()= default;
    Camera(const Camera&)=delete;
    Camera& operator=(const Camera&) = delete;

    // ==================== INITIALISATION ================
    void initialize(int screenWidth, int screenHeight, float fov=45.0f, float nearPlane=0.1f, float farPlane= 500.0f);

    // ==================== MISES À JOUR ===============
    void update(float deltaTime);
    void handleKeyInput(int key, bool pressed);
    void handleMouseMovement(float xPos, float yPos);

    // ==================== CONTRÔLES ====================
    void setMouseLookEnabled(bool enabled);
    [[nodiscard]] bool idMouseLookEnabled()const {return mouseLookEnabled_;}

    // ==================== MATRICES (FORMAT TABLEAU) ====================
    void getViewMatrix(float* outView) const;
    void getProjectionMatrix(float* outProj) const;
    static void lookAtMatrix(float* outMatrix, const core::Vec3F& eye, const core::Vec3F& center,const core::Vec3F& up);
    static void perspectiveMatrix(float* outMatrix, float fov, float aspect, float near, float far);

    // ==================== GETTERS ====================
    [[nodiscard]] const core::Vec3F& getPosition() const { return position_; }
    [[nodiscard]] const core::Vec3F& getFront() const { return front_; }
    [[nodiscard]] const core::Vec3F& getUp() const { return up_; }
    [[nodiscard]] const core::Vec3F& getRight() const { return right_; }

    [[nodiscard]] float getYaw() const { return yaw_; }
    [[nodiscard]] float getPitch() const { return pitch_; }
    [[nodiscard]] float getFOV() const { return fov_; }
    [[nodiscard]] float getAspectRatio() const;

    [[nodiscard]] int getScreenWidth() const { return screenWidth_; }
    [[nodiscard]] int getScreenHeight() const { return screenHeight_; }

    // ==================== SETTERS ====================
    void setPosition(const core::Vec3F& pos) { position_ = pos; }
    void setPosition(float x, float y, float z) { position_ = core::Vec3F{x, y, z}; }

    void setYaw(float yaw) { yaw_ = yaw; updateVectors(); }
    void setPitch(float pitch);
    void setFOV(float fov) { fov_ = fov; }

    void setMovementSpeed(float speed) { movementSpeed_ = speed; }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }

    // ==================== MODE ORBITE ====================
    void setOrbitMode(const core::Vec3F& target, float radius);
    void disableOrbitMode();
    [[nodiscard]] bool isOrbitModeEnable()const {return orbitModeEnabled_;}

private:
    // ==================== PROPRIÉTÉS GÉOMÉTRIQUES ====================
    core::Vec3F position_;
    core::Vec3F front_;
    core::Vec3F up_;
    core::Vec3F right_;
    core::Vec3F worldUp_;
    // ==================== ANGLES D'EULER (degrés) ====================
    float yaw_;
    float pitch_;

    // ==================== PARAMÈTRES D'ÉCRAN ====================
    int screenWidth_;
    int screenHeight_;
    float fov_;
    float nearPlane_;
    float farPlane_;

    // ==================== CONTRÔLES ====================
    float movementSpeed_;
    float mouseSensitivity_;
    float lastMouseX_;
    float lastMouseY_;
    bool firstMouse_;
    bool mouseLookEnabled_;

    // ==================== ÉTAT DU CLAVIER ====================
    std::array<bool,512> keyState_;

    // ==================== MODE ORBITE ====================
    bool orbitModeEnabled_;
    core::Vec3F orbitTarget_;
    float orbitRadius_;
    float minPitch_;
    float maxPitch_;

    // ==================== MÉTHODES PRIVÉES ====================
    void updateVectors();
    void processKeyboardMovement(float deltaTime);
    void updateOrbitMode();
    // ==================== CONSTANTES ====================
    static constexpr float MIN_PITCH = -89.0f;
    static constexpr float MAX_PITCH = 89.0f;
    static constexpr float DEFAULT_MOVE_SPEED = 3.5f;
    static constexpr float DEFAULT_MOUSE_SENSITIVITY = 0.10f;
    static constexpr float PI = 3.14159265359f;

};



#endif //CAMERA_H
