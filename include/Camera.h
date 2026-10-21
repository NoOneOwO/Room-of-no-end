// Camera.h
// First-person camera with yaw/pitch mouse look and a hard clamp that keeps it
// inside the real (k = 0) room.
#pragma once

#include <glm/glm.hpp>

namespace ir {

class Camera {
public:
    Camera();

    // Mouse look, in pixels. Pitch is clamped to +-kMaxPitchDeg.
    void addMouseDelta(double deltaX, double deltaY);

    // localMove.x = strafe right, localMove.z = forward. Normalised by the
    // caller if diagonal movement should not be faster.
    void move(const glm::vec3& localMove, float deltaTime);

    // Keeps the camera kCameraMargin away from every wall, floor and ceiling.
    void clampToRoom();

    // Used by the scripted demo camera: places the camera and aims it at a
    // target while keeping the pitch clamp and room clamp intact.
    void setPose(const glm::vec3& position, const glm::vec3& target);

    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;
    glm::vec3 position() const { return position_; }
    glm::vec3 forward() const; // unit vector including pitch
    float yawDegrees() const { return yawDeg_; }
    float pitchDegrees() const { return pitchDeg_; }

private:
    glm::vec3 position_{0.0f, 1.6f, 0.0f};
    float yawDeg_ = 0.0f;
    float pitchDeg_ = 0.0f;
};

} // namespace ir
