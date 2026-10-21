// Room.h
// One long corridor mesh spanning every mirror copy. The fragment shader works
// out which copy a fragment belongs to and discards anything outside [-N, N].
#pragma once

#include "GLWrappers.h"

#include <filesystem>

namespace ir {

class Room {
public:
    void init(const std::filesystem::path& shaderDir);

    void draw(const glm::mat4& viewProj, const glm::vec3& cameraPos,
              int maxCopy) const;

private:
    Program program_;
    Mesh mesh_;
};

} // namespace ir
