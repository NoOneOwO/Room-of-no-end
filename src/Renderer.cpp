// Renderer.cpp
#include "Renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ir {
namespace {

struct CubeInstance {
    glm::vec3 offset; // (i*Wx, j*Wy, k*Wz)
    glm::vec3 sign;   // ((-1)^i, (-1)^j, (-1)^k)
    float fade;       // 0.85^(|i|+|j|+|k|)
};

constexpr float kGoldenAngle = 2.39996323f;

std::vector<Vertex> cubeVertices() {
    const glm::vec3 color(1.0f);
    const glm::vec3 p000(-0.5f, -0.5f, -0.5f);
    const glm::vec3 p001(-0.5f, -0.5f, 0.5f);
    const glm::vec3 p010(-0.5f, 0.5f, -0.5f);
    const glm::vec3 p011(-0.5f, 0.5f, 0.5f);
    const glm::vec3 p100(0.5f, -0.5f, -0.5f);
    const glm::vec3 p101(0.5f, -0.5f, 0.5f);
    const glm::vec3 p110(0.5f, 0.5f, -0.5f);
    const glm::vec3 p111(0.5f, 0.5f, 0.5f);

    // Each face is wound counter-clockwise when seen from outside, so the
    // default GL_CCW front-face rule culls the interior.
    return {
        // +X
        Vertex{p101, {1, 0, 0}, color}, Vertex{p100, {1, 0, 0}, color},
        Vertex{p110, {1, 0, 0}, color}, Vertex{p111, {1, 0, 0}, color},
        // -X
        Vertex{p000, {-1, 0, 0}, color}, Vertex{p001, {-1, 0, 0}, color},
        Vertex{p011, {-1, 0, 0}, color}, Vertex{p010, {-1, 0, 0}, color},
        // +Y
        Vertex{p011, {0, 1, 0}, color}, Vertex{p111, {0, 1, 0}, color},
        Vertex{p110, {0, 1, 0}, color}, Vertex{p010, {0, 1, 0}, color},
        // -Y
        Vertex{p000, {0, -1, 0}, color}, Vertex{p100, {0, -1, 0}, color},
        Vertex{p101, {0, -1, 0}, color}, Vertex{p001, {0, -1, 0}, color},
        // +Z
        Vertex{p001, {0, 0, 1}, color}, Vertex{p101, {0, 0, 1}, color},
        Vertex{p111, {0, 0, 1}, color}, Vertex{p011, {0, 0, 1}, color},
        // -Z
        Vertex{p100, {0, 0, -1}, color}, Vertex{p000, {0, 0, -1}, color},
        Vertex{p010, {0, 0, -1}, color}, Vertex{p110, {0, 0, -1}, color},
    };
}

std::vector<std::uint32_t> cubeIndices() {
    std::vector<std::uint32_t> indices;
    indices.reserve(36);
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    return indices;
}

void bindInstanceAttributes(GLuint vao, const Buffer& buffer) {
    const auto stride = static_cast<GLsizei>(sizeof(CubeInstance));
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.id());

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(CubeInstance, offset)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(CubeInstance, sign)));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(CubeInstance, fade)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace

Renderer::Renderer(const std::filesystem::path& shaderDir) : shaderDir_(shaderDir) {
    objectProgram_.build(shaderDir_ / "basic.vert", shaderDir_ / "basic.frag");
    cubeMesh_.upload(cubeVertices(), cubeIndices());
    room_.init(shaderDir_);
    mirror_.init(shaderDir_);
    lattice_.init(shaderDir_);
    post_.init(shaderDir_);
    post_.resize(width_, height_, constants::kSceneMsaaSamples);

    objects_.push_back(Object::makeInitial());
}

void Renderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    post_.resize(width_, height_, constants::kSceneMsaaSamples);
}

int Renderer::maxReflectionDepth() const {
    return mode_ == SceneMode::Cube ? constants::kCubeMaxReflectionDepth
                                    : constants::kMaxReflectionDepth;
}

int Renderer::reflectionDepth() const {
    return mode_ == SceneMode::Cube ? cubeDepth_ : tunnelDepth_;
}

void Renderer::setMode(SceneMode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    cubeInstancesDirty_ = true;
}

void Renderer::toggleMode() {
    setMode(mode_ == SceneMode::Tunnel ? SceneMode::Cube : SceneMode::Tunnel);
}

void Renderer::setReflectionDepth(int depth) {
    const int maxDepth = maxReflectionDepth();
    const int clamped = std::clamp(depth, 0, maxDepth);

    if (mode_ == SceneMode::Cube) {
        if (cubeDepth_ == clamped) {
            return;
        }
        cubeDepth_ = clamped;
        if (clamped > 0) {
            cubeSavedDepth_ = clamped;
        }
        cubeInstancesDirty_ = true;
    } else {
        if (tunnelDepth_ == clamped) {
            return;
        }
        tunnelDepth_ = clamped;
        if (clamped > 0) {
            tunnelSavedDepth_ = clamped;
        }
    }
}

void Renderer::toggleReflections() {
    if (reflectionDepth() > 0) {
        setReflectionDepth(0);
    } else {
        setReflectionDepth(mode_ == SceneMode::Cube ? cubeSavedDepth_
                                                    : tunnelSavedDepth_);
    }
}

void Renderer::setBloomIntensity(float intensity) {
    bloomIntensity_ = std::clamp(intensity, constants::kBloomIntensityMin,
                                 constants::kBloomIntensityMax);
}

void Renderer::adjustBloomIntensity(float delta) {
    setBloomIntensity(bloomIntensity_ + delta);
}

void Renderer::setDemoEnabled(bool enabled, float nowSeconds) {
    demoEnabled_ = enabled;
    if (enabled) {
        demoNextSpawn_ = nowSeconds + constants::kDemoSpawnInterval;
        demoNextDepth_ = nowSeconds + constants::kDemoDepthInterval;
        demoIndex_ = 0;
    }
}

void Renderer::toggleDemo(float nowSeconds) {
    setDemoEnabled(!demoEnabled_, nowSeconds);
}

void Renderer::spawnAt(const Camera& camera) {
    if (objects_.size() >= constants::kMaxObjects) {
        return; // cap reached: spawns are ignored
    }
    addClamped(Object::makeSpawn(camera.position() +
                                 camera.forward() * constants::kSpawnDistance));
}

void Renderer::addClamped(const Object& source) {
    if (objects_.size() >= constants::kMaxObjects) {
        return;
    }

    Object object = source;
    const float halfExtent = constants::kUnitCubeHalfExtent * object.scale;
    const float margin = halfExtent + constants::kSpawnClearance;
    const float halfWidth = constants::kRoomWidth * 0.5f;
    const float halfDepth = constants::kMirrorSpacing * 0.5f;
    const float minY = constants::kFloorY + margin;
    const float maxY = constants::kFloorY + constants::kRoomHeight - margin;

    object.position.x =
        std::clamp(object.position.x, -halfWidth + margin, halfWidth - margin);
    object.position.y = std::clamp(object.position.y, minY, maxY);
    object.position.z =
        std::clamp(object.position.z, -halfDepth + margin, halfDepth - margin);

    objects_.push_back(object);
}

void Renderer::randomizeColors() {
    for (Object& object : objects_) {
        object.color = randomNeonColor();
    }
}

void Renderer::clearSpawned() {
    if (!objects_.empty()) {
        objects_.resize(1); // keep the initial cube
    }
}

void Renderer::updateDemo(float animTime, Camera& camera) {
    if (!demoEnabled_) {
        return;
    }

    // Smooth Lissajous camera path entirely inside the real cell.
    const glm::vec3 pos = constants::kDemoCameraCenter +
                          glm::vec3(constants::kDemoCameraAmplitude.x *
                                        std::sin(constants::kDemoCameraFrequency.x * animTime +
                                                 constants::kDemoCameraPhase.x),
                                    constants::kDemoCameraAmplitude.y *
                                        std::sin(constants::kDemoCameraFrequency.y * animTime +
                                                 constants::kDemoCameraPhase.y),
                                    constants::kDemoCameraAmplitude.z *
                                        std::sin(constants::kDemoCameraFrequency.z * animTime +
                                                 constants::kDemoCameraPhase.z));
    const glm::vec3 target =
        constants::kDemoCameraCenter +
        glm::vec3(constants::kDemoTargetAmplitude.x *
                      std::sin(constants::kDemoTargetFrequency.x * animTime +
                               constants::kDemoTargetPhase.x),
                  constants::kDemoTargetAmplitude.y *
                      std::sin(constants::kDemoTargetFrequency.y * animTime +
                               constants::kDemoTargetPhase.y),
                  constants::kDemoTargetAmplitude.z *
                      std::sin(constants::kDemoTargetFrequency.z * animTime +
                               constants::kDemoTargetPhase.z));
    camera.setPose(pos, target);

    // Deterministic auto-spawn: one cube every 0.4 s, up to 12 total.
    while (animTime >= demoNextSpawn_ &&
           objects_.size() < static_cast<std::size_t>(constants::kDemoMaxObjects)) {
        const float radius =
            constants::kDemoSpawnRadiusMin +
            (constants::kDemoSpawnRadiusMax - constants::kDemoSpawnRadiusMin) *
                std::fmod(static_cast<float>(demoIndex_) * 0.37f, 1.0f);
        const float angle = static_cast<float>(demoIndex_) * kGoldenAngle;
        const float eye =
            constants::kDemoSpawnEyeMin +
            (constants::kDemoSpawnEyeMax - constants::kDemoSpawnEyeMin) *
                std::fmod(static_cast<float>(demoIndex_) * 0.61f, 1.0f);

        const glm::vec3 position(radius * std::cos(angle), eye,
                                 radius * std::sin(angle));
        addClamped(Object::makeDemoSpawn(position, demoIndex_));
        ++demoIndex_;
        demoNextSpawn_ += constants::kDemoSpawnInterval;
    }

    // Slowly cycle the reflection depth of the current mode.
    if (animTime >= demoNextDepth_) {
        int next = reflectionDepth() + 1;
        if (next > maxReflectionDepth() || next < 1) {
            next = 1;
        }
        setReflectionDepth(next);
        demoNextDepth_ = animTime + constants::kDemoDepthInterval;
    }
}

void Renderer::rebuildCubeInstances() {
    const int n = cubeDepth_;
    const int side = 2 * n + 1;
    const std::size_t expected =
        static_cast<std::size_t>(side) * static_cast<std::size_t>(side) *
        static_cast<std::size_t>(side);
    std::vector<CubeInstance> evenInstances;
    std::vector<CubeInstance> oddInstances;
    evenInstances.reserve(expected);
    oddInstances.reserve(expected);

    for (int i = -n; i <= n; ++i) {
        for (int j = -n; j <= n; ++j) {
            for (int k = -n; k <= n; ++k) {
                CubeInstance instance;
                instance.offset = glm::vec3(
                    static_cast<float>(i) * constants::kRoomWidth,
                    static_cast<float>(j) * constants::kRoomHeight,
                    static_cast<float>(k) * constants::kMirrorSpacing);
                instance.sign = glm::vec3((i % 2 == 0) ? 1.0f : -1.0f,
                                          (j % 2 == 0) ? 1.0f : -1.0f,
                                          (k % 2 == 0) ? 1.0f : -1.0f);
                instance.fade = std::pow(constants::kCubeFalloff,
                                         static_cast<float>(std::abs(i) +
                                                            std::abs(j) +
                                                            std::abs(k)));

                // Product of the three signs decides handedness. Even parity
                // keeps CCW front faces; odd parity needs glFrontFace(GL_CW).
                if ((i + j + k) % 2 == 0) {
                    evenInstances.push_back(instance);
                } else {
                    oddInstances.push_back(instance);
                }
            }
        }
    }

    evenInstanceBuffer_.upload(evenInstances.data(),
                               evenInstances.size() * sizeof(CubeInstance));
    oddInstanceBuffer_.upload(oddInstances.data(),
                              oddInstances.size() * sizeof(CubeInstance));
    evenInstanceCount_ = static_cast<GLsizei>(evenInstances.size());
    oddInstanceCount_ = static_cast<GLsizei>(oddInstances.size());

    lattice_.setDepth(cubeDepth_);
}

void Renderer::render(const Camera& camera, float animTime, int width, int height) {
    resize(width, height);

    post_.beginScene();

    const float aspect =
        static_cast<float>(width_) / static_cast<float>(std::max(1, height_));
    const glm::mat4 viewProj =
        camera.projectionMatrix(aspect) * camera.viewMatrix();
    const glm::vec3 cameraPos = camera.position();

    if (mode_ == SceneMode::Tunnel) {
        room_.draw(viewProj, cameraPos, tunnelDepth_);
        drawObjectsTunnel(viewProj, cameraPos, animTime);
        mirror_.drawOpaque(viewProj, cameraPos, tunnelDepth_);
        mirror_.drawGlass(viewProj, cameraPos, tunnelDepth_);
    } else {
        if (cubeInstancesDirty_) {
            rebuildCubeInstances();
            cubeInstancesDirty_ = false;
        }
        glDisable(GL_CULL_FACE); // thin emissive boxes, both sides visible
        lattice_.draw(viewProj, cameraPos, animTime);
        glEnable(GL_CULL_FACE);
        drawObjectsCube(viewProj, cameraPos, animTime);
    }

    post_.endScene(bloomEnabled_, bloomIntensity_);
}

void Renderer::drawObjectsTunnel(const glm::mat4& viewProj,
                                 const glm::vec3& cameraPos, float animTime) {
    const constants::FadeRange fade =
        constants::fadeRangeForDepth(tunnelDepth_);

    // Instance attributes belong to the shared cube VAO; disable them so the
    // non-instanced tunnel draws use only the per-vertex data.
    glBindVertexArray(cubeMesh_.vaoId());
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glBindVertexArray(0);

    objectProgram_.use();
    objectProgram_.setMat4("uViewProj", viewProj);
    objectProgram_.setVec3("uCameraPos", cameraPos);
    objectProgram_.setInt("uCubeMode", 0);
    objectProgram_.setInt("uDistanceFade", 1);
    objectProgram_.setVec3("uCellCenter", constants::kCubeCellCenter);
    objectProgram_.setFloat("uAmbient", constants::kAmbientStrength);
    objectProgram_.setFloat("uDiffuse", constants::kDiffuseStrength);
    objectProgram_.setFloat("uEmissive", constants::kEmissiveStrength);
    objectProgram_.setFloat("uRimStrength", constants::kRimStrength);
    objectProgram_.setFloat("uRimPower", constants::kRimPower);
    objectProgram_.setFloat("uFadeStart", fade.start);
    objectProgram_.setFloat("uFadeEnd", fade.end);

    // Every copy k of the scene is the real scene transformed by
    //     T(0, 0, kW) * S(1, 1, (-1)^k)
    // so the image of a point is (x, y, kW + (-1)^k z). Odd copies have a
    // negative determinant, hence the glFrontFace flip below.
    for (int k = -tunnelDepth_; k <= tunnelDepth_; ++k) {
        const float sign = (k % 2 == 0) ? 1.0f : -1.0f;
        const glm::mat4 copyMatrix =
            glm::translate(glm::mat4(1.0f),
                           glm::vec3(0.0f, 0.0f,
                                     static_cast<float>(k) * constants::kMirrorSpacing)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, sign));

        // Mirroring flips Z, so the light's Z component has to flip too in
        // order for the reflected copy to be lit like a true reflection.
        const glm::vec3 lightDir = glm::normalize(
            glm::vec3(constants::kLightDirection.x, constants::kLightDirection.y,
                      constants::kLightDirection.z * sign));

        const float brightness =
            std::pow(constants::kCopyFalloff, static_cast<float>(std::abs(k)));
        const float hueShift = static_cast<float>(k) * constants::kCopyHueShift;

        objectProgram_.setVec3("uLightDir", lightDir);
        objectProgram_.setFloat("uBrightness", brightness);
        objectProgram_.setFloat("uHueShift", hueShift);

        glFrontFace(sign < 0.0f ? GL_CW : GL_CCW);

        for (const Object& object : objects_) {
            const glm::mat4 model = copyMatrix * object.modelMatrix(animTime);
            const glm::mat3 normalMatrix =
                glm::transpose(glm::inverse(glm::mat3(model)));
            objectProgram_.setMat4("uModel", model);
            objectProgram_.setMat3("uNormalMatrix", normalMatrix);
            objectProgram_.setVec3("uColor", object.color);
            cubeMesh_.draw();
        }
    }

    glFrontFace(GL_CCW);
}

void Renderer::drawObjectsCube(const glm::mat4& viewProj,
                               const glm::vec3& cameraPos, float animTime) {
    const constants::FadeRange fade =
        constants::fadeRangeForDepth(cubeDepth_);

    objectProgram_.use();
    objectProgram_.setMat4("uViewProj", viewProj);
    objectProgram_.setVec3("uCameraPos", cameraPos);
    objectProgram_.setVec3("uCellCenter", constants::kCubeCellCenter);
    objectProgram_.setInt("uCubeMode", 1);
    objectProgram_.setInt("uDistanceFade", 0);
    objectProgram_.setVec3("uLightDir", glm::normalize(constants::kLightDirection));
    objectProgram_.setFloat("uAmbient", constants::kAmbientStrength);
    objectProgram_.setFloat("uDiffuse", constants::kDiffuseStrength);
    objectProgram_.setFloat("uEmissive", constants::kEmissiveStrength);
    objectProgram_.setFloat("uRimStrength", constants::kCubeRimStrength);
    objectProgram_.setFloat("uRimPower", constants::kRimPower);
    objectProgram_.setFloat("uFadeStart", fade.start);
    objectProgram_.setFloat("uFadeEnd", fade.end);
    objectProgram_.setFloat("uBrightness", 1.0f);
    objectProgram_.setFloat("uHueShift", 0.0f);

    // Even parity: determinant +1, normal CCW winding.
    bindInstanceAttributes(cubeMesh_.vaoId(), evenInstanceBuffer_);
    glFrontFace(GL_CCW);
    for (const Object& object : objects_) {
        const glm::mat4 model = object.modelMatrix(animTime);
        const glm::mat3 normalMatrix =
            glm::transpose(glm::inverse(glm::mat3(model)));
        objectProgram_.setMat4("uModel", model);
        objectProgram_.setMat3("uNormalMatrix", normalMatrix);
        objectProgram_.setVec3("uColor", object.color);
        cubeMesh_.drawInstanced(evenInstanceCount_);
    }

    // Odd parity: determinant -1, mirrored winding is front-facing.
    bindInstanceAttributes(cubeMesh_.vaoId(), oddInstanceBuffer_);
    glFrontFace(GL_CW);
    for (const Object& object : objects_) {
        const glm::mat4 model = object.modelMatrix(animTime);
        const glm::mat3 normalMatrix =
            glm::transpose(glm::inverse(glm::mat3(model)));
        objectProgram_.setMat4("uModel", model);
        objectProgram_.setMat3("uNormalMatrix", normalMatrix);
        objectProgram_.setVec3("uColor", object.color);
        cubeMesh_.drawInstanced(oddInstanceCount_);
    }

    glFrontFace(GL_CCW);
}

} // namespace ir
