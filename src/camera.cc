//
// Created by forna on 07.02.2026.
//

#include "../include/camera.h"
#include <cmath>
#include <algorithm>
// ==================== CONSTRUCTEUR ====================
Camera::Camera()
    : position_{0.0f, 0.0f, 3.0f},
      front_{0.0f, 0.0f, -1.0f},
      up_{0.0f, 1.0f, 0.0f},
      right_{1.0f, 0.0f, 0.0f},
      worldUp_{0.0f, 1.0f, 0.0f},
      yaw_(-90.0f),
      pitch_(0.0f),
      screenWidth_(800),
      screenHeight_(600),
      fov_(45.0f),
      nearPlane_(0.1f),
      farPlane_(500.0f),
      movementSpeed_(DEFAULT_MOVE_SPEED),
      mouseSensitivity_(DEFAULT_MOUSE_SENSITIVITY),
      lastMouseX_(400.0f),
      lastMouseY_(300.0f),
      firstMouse_(true),
      mouseLookEnabled_(true),
      orbitModeEnabled_(false),
      orbitTarget_{0.0f, 0.0f, 0.0f},
      orbitRadius_(20.0f),
      minPitch_(MIN_PITCH),
      maxPitch_(MAX_PITCH) {
    keyState_.fill(false);
}

// ==================== INITIALISATION ====================
void Camera::initialize(int screenWidth, int screenHeight,
                        float fov, float nearPlane, float farPlane) {
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;
    fov_ = fov;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    lastMouseX_ = screenWidth_ / 2.0f;
    lastMouseY_ = screenHeight_ / 2.0f;
}

// ==================== MISES À JOUR ====================
void Camera::update(float deltaTime) {
    if (!orbitModeEnabled_) {
        processKeyboardMovement(deltaTime);
    } else {
        updateOrbitMode();
    }
}

void Camera::handleKeyInput(int key, bool pressed) {
    if (key >= 0 && key < static_cast<int>(keyState_.size())) {
        keyState_[key] = pressed;
    }
}


void Camera::handleMouseMovement(float xPos, float yPos) {
    if (firstMouse_) {
        lastMouseX_ = xPos;
        lastMouseY_ = yPos;
        firstMouse_ = false;
        return;
    }

    if (!mouseLookEnabled_) {
        return;
    }

    float xOffset = xPos - lastMouseX_;
    float yOffset = lastMouseY_ - yPos; // Y inversé
    lastMouseX_ = xPos;
    lastMouseY_ = yPos;

    xOffset *= mouseSensitivity_;
    yOffset *= mouseSensitivity_;

    yaw_ += xOffset;
    pitch_ += yOffset;

    // Contraindre le pitch
    if (pitch_ > maxPitch_) {
        pitch_ = maxPitch_;
    }
    if (pitch_ < minPitch_) {
        pitch_ = minPitch_;
    }

    updateVectors();
}

void Camera::setMouseLookEnabled(bool enabled) {
    mouseLookEnabled_ = enabled;
    firstMouse_ = true;
}

// ==================== SETTERS ====================
void Camera::setPitch(float pitch) {
    pitch_ = pitch;
    if (pitch_ > maxPitch_) pitch_ = maxPitch_;
    if (pitch_ < minPitch_) pitch_ = minPitch_;
    updateVectors();
}

// ==================== MATRICES ====================
void Camera::getViewMatrix(float* outView) const {
    core::Vec3F center = position_ + front_;
    lookAtMatrix(outView, position_, center, up_);
}

void Camera::getProjectionMatrix(float* outProj) const {
    perspectiveMatrix(outProj, fov_, getAspectRatio(), nearPlane_, farPlane_);
}

void Camera::lookAtMatrix(float* outMatrix,
                          const core::Vec3F& eye,
                          const core::Vec3F& center,
                          const core::Vec3F& up) {
    // Calculer les vecteurs de la base
    core::Vec3F f = center - eye;
    f = f.Normalize();

    core::Vec3F s = f.Cross(up);
    s = s.Normalize();

    core::Vec3F u = s.Cross(f);
    u = u.Normalize();


    // Initialiser la matrice identité
    for (int i = 0; i < 16; ++i) {
        outMatrix[i] = 0.0f;
    }
    outMatrix[0] = s.x;
    outMatrix[4] = s.y;
    outMatrix[8] = s.z;
    outMatrix[1] = u.x;
    outMatrix[5] = u.y;
    outMatrix[9] = u.z;
    outMatrix[2] = -f.x;
    outMatrix[6] = -f.y;
    outMatrix[10] = -f.z;
    outMatrix[15] = 1.0f;

    // Appliquer la translation
    outMatrix[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
    outMatrix[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
    outMatrix[14] = f.x * eye.x + f.y * eye.y + f.z * eye.z;
}

void Camera::perspectiveMatrix(float* outMatrix,
                               float fov,
                               float aspect,
                               float near,
                               float far) {
    // Initialiser à zéro
    for (int i = 0; i < 16; ++i) {
        outMatrix[i] = 0.0f;
    }

    float f = 1.0f / std::tan(fov * PI / 360.0f);
    float nf = 1.0f / (near - far);

    outMatrix[0] = f / aspect;
    outMatrix[5] = f;
    outMatrix[10] = (far + near) * nf;
    outMatrix[14] = 2.0f * far * near * nf;
    outMatrix[11] = -1.0f;
}

float Camera::getAspectRatio() const {
    return static_cast<float>(screenWidth_) / screenHeight_;
}

// ==================== MODE ORBITE ====================
void Camera::setOrbitMode(const core::Vec3F& target, float radius) {
    orbitTarget_ = target;
    orbitRadius_ = radius;
    orbitModeEnabled_ = true;
    updateOrbitMode();
}

void Camera::disableOrbitMode() {
    orbitModeEnabled_ = false;
}

// ==================== MÉTHODES PRIVÉES ====================
void Camera::updateVectors() {
    // Convertir les angles d'Euler en vecteurs de direction
    float yawRad = yaw_ * PI / 180.0f;
    float pitchRad = pitch_ * PI / 180.0f;

    front_.x = std::cos(yawRad) * std::cos(pitchRad);
    front_.y = std::sin(pitchRad);
    front_.z = std::sin(yawRad) * std::cos(pitchRad);
    front_ = front_.Normalize();

    // Recalculer les vecteurs right et up
    right_ = front_.Cross(worldUp_);
    right_ = right_.Normalize();

    up_ = right_.Cross(front_);
    up_ = up_.Normalize();
}


void Camera::processKeyboardMovement(float deltaTime) {
    float velocity = movementSpeed_ * deltaTime;

    // GLFW_KEY_W = 87
    if (keyState_[87]) {
        position_ = position_ + (front_ * velocity);
    }
    // GLFW_KEY_S = 83
    if (keyState_[83]) {
        position_ = position_ - (front_ * velocity);
    }
    // GLFW_KEY_A = 65
    if (keyState_[65]) {
        position_ = position_ - (right_ * velocity);
    }
    // GLFW_KEY_D = 68
    if (keyState_[68]) {
        position_ = position_ + (right_ * velocity);
    }
    // GLFW_KEY_SPACE = 32
    if (keyState_[32]) {
        position_ = position_ + (worldUp_ * velocity);
    }
    // GLFW_KEY_LEFT_CONTROL = 341
    if (keyState_[341]) {
        position_ = position_ - (worldUp_ * velocity);
    }
}


void Camera::updateOrbitMode() {
    // Position orbitale autour de la cible
    float yawRad = yaw_ * PI / 180.0f;
    float pitchRad = pitch_ * PI / 180.0f;

    float x = orbitTarget_.x + orbitRadius_ * std::cos(yawRad) * std::cos(pitchRad);
    float y = orbitTarget_.y + orbitRadius_ * std::sin(pitchRad);
    float z = orbitTarget_.z + orbitRadius_ * std::sin(yawRad) * std::cos(pitchRad);

    position_ = {x, y, z};

    // Orienter vers la cible
    front_ = (orbitTarget_ - position_).Normalize();

    right_ = front_.Cross(worldUp_);
    right_ = right_.Normalize();

    up_ = right_.Cross(front_);
    up_ = up_.Normalize();

}
