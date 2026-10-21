// Object.h
// A single rotating neon cube. Time is supplied by the caller so that pausing
// does not cause a jump when resumed.
#pragma once

#include <glm/glm.hpp>

namespace ir {

struct Object {
    glm::vec3 position{0.0f};
    glm::vec3 rotationAxis{0.0f, 1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float scale = 1.0f;
    float rotationSpeed = 0.0f; // radians / second

    // Rotation angle = rotationSpeed * animTime.
    glm::mat4 modelMatrix(float animTime) const;

    static Object makeInitial();
    // Spawn colour, scale, axis and speed are randomised; position is chosen
    // and clamped by the caller.
    static Object makeSpawn(const glm::vec3& position);
    // Deterministic variant used by --demo / T demo mode. The index seeds the
    // colour, scale, axis and spin so a capture is reproducible frame for frame.
    static Object makeDemoSpawn(const glm::vec3& position, int index);
};

// Random neon colour (high-saturation HSV), used by spawning and by the
// "randomise all colours" action.
glm::vec3 randomNeonColor();

} // namespace ir
