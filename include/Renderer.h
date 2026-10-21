// Renderer.h
// Owns the scene and draws it with the method of images: no stencil masks and
// no framebuffer recursion. Tunnel mode repeats the room along Z; cube mode
// repeats the real cell along X, Y and Z through instancing.
#pragma once

#include "Camera.h"
#include "Constants.h"
#include "GLWrappers.h"
#include "Lattice.h"
#include "Mirror.h"
#include "Object.h"
#include "PostProcess.h"
#include "Room.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace ir {

enum class SceneMode { Tunnel, Cube };

class Renderer {
public:
    explicit Renderer(const std::filesystem::path& shaderDir);

    void resize(int width, int height);
    void render(const Camera& camera, float animTime, int width, int height);

    void spawnAt(const Camera& camera);
    void randomizeColors();
    void clearSpawned();
    std::size_t objectCount() const { return objects_.size(); }

    void setMode(SceneMode mode);
    SceneMode mode() const { return mode_; }
    void toggleMode();

    void setReflectionDepth(int depth);
    int reflectionDepth() const;
    int maxReflectionDepth() const;
    void toggleReflections();

    void setBloomEnabled(bool enabled) { bloomEnabled_ = enabled; }
    bool bloomEnabled() const { return bloomEnabled_; }
    void toggleBloom() { bloomEnabled_ = !bloomEnabled_; }
    void setBloomIntensity(float intensity);
    float bloomIntensity() const { return bloomIntensity_; }
    void adjustBloomIntensity(float delta);

    void setDemoEnabled(bool enabled, float nowSeconds = 0.0f);
    bool demoEnabled() const { return demoEnabled_; }
    void toggleDemo(float nowSeconds);
    void updateDemo(float animTime, Camera& camera);

    int sceneSamples() const { return post_.samples(); }
    bool msaaActive() const { return post_.msaaActive(); }

private:
    void drawObjectsTunnel(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                           float animTime);
    void drawObjectsCube(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                         float animTime);
    void rebuildCubeInstances();
    void addClamped(const Object& object);

    std::filesystem::path shaderDir_;
    Program objectProgram_;
    Mesh cubeMesh_;
    Room room_;
    Mirror mirror_;
    Lattice lattice_;
    PostProcess post_;

    Buffer evenInstanceBuffer_{GL_ARRAY_BUFFER};
    Buffer oddInstanceBuffer_{GL_ARRAY_BUFFER};
    GLsizei evenInstanceCount_ = 0;
    GLsizei oddInstanceCount_ = 0;

    std::vector<Object> objects_;
    SceneMode mode_ = SceneMode::Tunnel;

    int tunnelDepth_ = constants::kDefaultReflectionDepth;
    int cubeDepth_ = constants::kCubeDefaultReflectionDepth;
    int tunnelSavedDepth_ = constants::kDefaultReflectionDepth;
    int cubeSavedDepth_ = constants::kCubeDefaultReflectionDepth;
    bool cubeInstancesDirty_ = true;

    bool bloomEnabled_ = constants::kBloomDefaultEnabled;
    float bloomIntensity_ = constants::kBloomDefaultIntensity;

    bool demoEnabled_ = false;
    float demoNextSpawn_ = 0.0f;
    float demoNextDepth_ = 0.0f;
    int demoIndex_ = 0;

    int width_ = constants::kWindowWidth;
    int height_ = constants::kWindowHeight;
};

} // namespace ir
