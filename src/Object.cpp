// Object.cpp
#include "Object.h"
#include "Constants.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

namespace ir {
namespace {

std::mt19937& rng() {
    static std::mt19937 engine(std::random_device{}());
    return engine;
}

float randomFloat(float minValue, float maxValue) {
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(rng());
}

glm::vec3 randomUnitVector() {
    // Uniform direction on the sphere.
    const float z = randomFloat(-1.0f, 1.0f);
    const float azimuth = randomFloat(0.0f, constants::kTwoPi);
    const float radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return glm::vec3(radius * std::cos(azimuth), radius * std::sin(azimuth), z);
}

glm::vec3 hsvToRgb(float hue, float saturation, float value) {
    const float scaled = hue * 6.0f;
    const int sector = static_cast<int>(std::floor(scaled)) % 6;
    const float fraction = scaled - std::floor(scaled);
    const float p = value * (1.0f - saturation);
    const float q = value * (1.0f - saturation * fraction);
    const float t = value * (1.0f - saturation * (1.0f - fraction));

    switch (sector) {
    case 0: return glm::vec3(value, t, p);
    case 1: return glm::vec3(q, value, p);
    case 2: return glm::vec3(p, value, t);
    case 3: return glm::vec3(p, q, value);
    case 4: return glm::vec3(t, p, value);
    default: return glm::vec3(value, p, q);
    }
}

} // namespace

glm::vec3 randomNeonColor() {
    // HSV with high saturation and value, then converted to RGB: every cube
    // reads as a neon object instead of a muddy random colour.
    return hsvToRgb(randomFloat(0.0f, 1.0f),
                    randomFloat(constants::kSpawnSaturationMin,
                                constants::kSpawnSaturationMax),
                    randomFloat(constants::kSpawnValueMin, constants::kSpawnValueMax));
}

glm::mat4 Object::modelMatrix(float animTime) const {
    const float angle = rotationSpeed * animTime;
    const glm::vec3 axis = glm::length(rotationAxis) > constants::kAxisEpsilon
                               ? glm::normalize(rotationAxis)
                               : glm::vec3(0.0f, 1.0f, 0.0f);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, angle, axis);
    model = glm::scale(model, glm::vec3(scale));
    return model;
}

Object Object::makeInitial() {
    Object object;
    object.position = glm::vec3(0.0f, constants::kInitialCubeY, 0.0f);
    object.rotationAxis = glm::normalize(constants::kInitialCubeAxis);
    object.color = constants::kInitialCubeColor;
    object.scale = constants::kInitialCubeScale;
    object.rotationSpeed = constants::kInitialCubeSpin;
    return object;
}

Object Object::makeSpawn(const glm::vec3& position) {
    Object object;
    object.position = position;
    object.rotationAxis = randomUnitVector();
    object.color = randomNeonColor();
    object.scale = randomFloat(constants::kMinObjectScale, constants::kMaxObjectScale);
    object.rotationSpeed =
        randomFloat(constants::kMinRotationSpeed, constants::kMaxRotationSpeed);

    // Avoid an almost-static cube: a tiny spin still reads as "alive".
    if (std::abs(object.rotationSpeed) < constants::kSpawnSpinDeadzone) {
        object.rotationSpeed = object.rotationSpeed < 0.0f
                                   ? -constants::kSpawnSpinFallback
                                   : constants::kSpawnSpinFallback;
    }
    return object;
}

Object Object::makeDemoSpawn(const glm::vec3& position, int index) {
    // Small deterministic integer hash: identical on every run and platform.
    std::uint32_t hash = static_cast<std::uint32_t>(index) * 747796405u + 2891336453u;
    const auto next = [&hash]() {
        hash ^= hash >> 16;
        hash *= 0x7feb352du;
        hash ^= hash >> 15;
        hash *= 0x846ca68bu;
        hash ^= hash >> 16;
        return static_cast<float>(hash & 0x00FFFFFFu) / 16777215.0f;
    };

    Object object;
    object.position = position;

    // Golden-ratio hue walk keeps successive demo cubes visually distinct.
    const float hue = std::fmod(static_cast<float>(index) * 0.61803398875f, 1.0f);
    object.color = hsvToRgb(hue, 0.80f, 1.0f);
    object.scale = constants::kMinObjectScale +
                   (constants::kMaxObjectScale - constants::kMinObjectScale) * next();

    glm::vec3 axis(next() * 2.0f - 1.0f, next() * 2.0f - 1.0f,
                   next() * 2.0f - 1.0f);
    if (glm::length(axis) < constants::kAxisEpsilon) {
        axis = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    object.rotationAxis = glm::normalize(axis);
    object.rotationSpeed = (0.35f + 0.80f * next()) * (next() < 0.5f ? -1.0f : 1.0f);
    return object;
}

} // namespace ir
