// Lattice.h
// Neon wireframe of the 3-axis image lattice used by cube mode. Each edge
// segment is owned by the cell at its minimum corner, so it is emitted exactly
// once and cannot z-fight with a neighbour's copy.
#pragma once

#include "GLWrappers.h"

#include <filesystem>

namespace ir {

class Lattice {
public:
    void init(const std::filesystem::path& shaderDir);

    // Rebuilds the instance buffer only when the reflection depth changes.
    void setDepth(int depth);

    void draw(const glm::mat4& viewProj, const glm::vec3& cameraPos, float time);

private:
    void rebuild();

    Program program_;
    VertexArray vao_;
    Buffer vertexBuffer_{GL_ARRAY_BUFFER};
    Buffer indexBuffer_{GL_ELEMENT_ARRAY_BUFFER};
    Buffer instanceBuffer_{GL_ARRAY_BUFFER};
    GLsizei indexCount_ = 0;
    GLsizei instanceCount_ = 0;
    int depth_ = -1;
};

} // namespace ir
