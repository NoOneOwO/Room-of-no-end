// Camera.cpp
#include "Camera.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace ir {

Camera::Camera()
    : position_(constants::kStartCameraX, constants::kStartCameraY,
               constants::kStartCameraZ),
      yawDeg_(constants::kStartYawDeg),
      pitchDeg_(constants::kStartPitchDeg) {
    clampToRoom();
}

void Camera::addMouseDelta(double deltaX, double deltaY) {
    yawDeg_ += static_cast<float>(deltaX) * constants::kMouseSensitivityDeg;
    pitchDeg_ -= static_cast<float>(deltaY) * constants::kMouseSensitivityDeg;
    pitchDeg_ = std::clamp(pitchDeg_, -constants::kMaxPitchDeg, constants::kMaxPitchDeg);

    // Keep yaw in a friendly range without changing the direction.
    if (yawDeg_ > 180.0f) {
        yawDeg_ -= 360.0f;
    } else if (yawDeg_ < -180.0f) {
        yawDeg_ += 360.0f;
    }
}

void Camera::move(const glm::vec3& localMove, float deltaTime) {
    const float yawRad = glm::radians(yawDeg_);
    // Horizontal basis: yaw -90 deg points toward -Z.
    const glm::vec3 forwardFlat(std::cos(yawRad), 0.0f, std::sin(yawRad));
    const glm::vec3 right(-std::sin(yawRad), 0.0f, std::cos(yawRad));

    glm::vec3 offset = right * localMove.x + forwardFlat * localMove.z;
    if (glm::length(offset) > 1.0f) {
        offset = glm::normalize(offset);
    }

    position_ += offset * constants::kMoveSpeed * deltaTime;
    clampToRoom();
}

void Camera::clampToRoom() {
    const float halfWidth = constants::kRoomWidth * 0.5f;
    const float halfDepth = constants::kMirrorSpacing * 0.5f;
    const float margin = constants::kCameraMargin;

    const float minX = -halfWidth + margin;
    const float maxX = halfWidth - margin;
    const float minY = constants::kFloorY + margin;
    const float maxY = constants::kFloorY + constants::kRoomHeight - margin;
    const float minZ = -halfDepth + margin;
    const float maxZ = halfDepth - margin;

    position_.x = std::clamp(position_.x, minX, maxX);
    position_.y = std::clamp(position_.y, minY, maxY);
    position_.z = std::clamp(position_.z, minZ, maxZ);
}

void Camera::setPose(const glm::vec3& position, const glm::vec3& target) {
    position_ = position;
    const glm::vec3 direction = target - position;
    const float length = glm::length(direction);
    if (length > constants::kAxisEpsilon) {
        const glm::vec3 unit = direction / length;
        yawDeg_ = glm::degrees(std::atan2(unit.z, unit.x));
        pitchDeg_ = glm::degrees(std::asin(std::clamp(unit.y, -1.0f, 1.0f)));
        pitchDeg_ = std::clamp(pitchDeg_, -constants::kMaxPitchDeg,
                               constants::kMaxPitchDeg);
    }
    clampToRoom();
}

glm::vec3 Camera::forward() const {
    const float yawRad = glm::radians(yawDeg_);
    const float pitchRad = glm::radians(pitchDeg_);
    const float cosPitch = std::cos(pitchRad);
    return glm::vec3(cosPitch * std::cos(yawRad), std::sin(pitchRad),
                     cosPitch * std::sin(yawRad));
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position_, position_ + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    const float safeAspect = aspect > 0.0f ? aspect : 1.0f;
    return glm::perspective(glm::radians(constants::kFovDegrees), safeAspect,
                            constants::kNearPlane, constants::kFarPlane);
}

} // namespace ir
