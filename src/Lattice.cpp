// Lattice.cpp
#include "Lattice.h"
#include "Constants.h"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ir {
namespace {

struct EdgeInstance {
    glm::vec3 center;
    glm::vec3 extent; // full edge length along the box axes
    float fade;
    float axis;
};

// A unit cube in [-0.5, 0.5]. Culling is disabled for the edges, so only
// positions matter and winding is irrelevant.
const float kUnitCube[24] = {
    -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
};

const std::uint32_t kUnitCubeIndices[36] = {
    0, 1, 2, 0, 2, 3, // -Z
    4, 6, 5, 4, 7, 6, // +Z
    0, 4, 5, 0, 5, 1, // -Y
    3, 2, 6, 3, 6, 7, // +Y
    0, 3, 7, 0, 7, 4, // -X
    1, 5, 6, 1, 6, 2, // +X
};

} // namespace

void Lattice::init(const std::filesystem::path& shaderDir) {
    program_.build(shaderDir / "edge.vert", shaderDir / "edge.frag");

    vao_.bind();
    vertexBuffer_.upload(kUnitCube, sizeof(kUnitCube));
    indexBuffer_.upload(kUnitCubeIndices, sizeof(kUnitCubeIndices));
    indexCount_ = 36;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    // Per-instance attributes: centre, extent, fade and axis id. The unit cube
    // spans [-0.5, 0.5], so multiplying by a full extent gives that extent.
    instanceBuffer_.bind();
    const auto stride = static_cast<GLsizei>(sizeof(EdgeInstance));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(EdgeInstance, center)));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(EdgeInstance, extent)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(EdgeInstance, fade)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(offsetof(EdgeInstance, axis)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    instanceCount_ = 0;
}

void Lattice::setDepth(int depth) {
    if (depth == depth_) {
        return;
    }
    depth_ = depth;
    rebuild();
}

void Lattice::rebuild() {
    const int n = depth_;
    std::vector<EdgeInstance> instances;
    const int side = 2 * n + 1;
    instances.reserve(static_cast<std::size_t>(3 * side * side * side));

    const float wx = constants::kRoomWidth;
    const float wy = constants::kRoomHeight;
    const float wz = constants::kMirrorSpacing;
    const float thickness = constants::kCubeEdgeThickness;

    // The real cell spans x in [-Wx/2, +Wx/2], y in [0, Wy], z in [-Wz/2, +Wz/2].
    // Cell (i,j,k) therefore has its minimum corner at
    //     (i*Wx - Wx/2, j*Wy, k*Wz - Wz/2).
    // Three segments leave that corner along +X, +Y and +Z. Every lattice edge
    // is owned by exactly one such corner, which is what removes duplicates.
    for (int i = -n; i <= n; ++i) {
        for (int j = -n; j <= n; ++j) {
            for (int k = -n; k <= n; ++k) {
                const float fade = std::pow(constants::kCubeFalloff,
                                            static_cast<float>(std::abs(i) +
                                                               std::abs(j) +
                                                               std::abs(k)));
                const float baseX = static_cast<float>(i) * wx - wx * 0.5f;
                const float baseY = static_cast<float>(j) * wy;
                const float baseZ = static_cast<float>(k) * wz - wz * 0.5f;

                // X edge at the min-Y / min-Z corner.
                instances.push_back(EdgeInstance{
                    glm::vec3(baseX + wx * 0.5f, baseY, baseZ),
                    glm::vec3(wx, thickness, thickness), fade, 0.0f});
                // Y edge at the min-X / min-Z corner.
                instances.push_back(EdgeInstance{
                    glm::vec3(baseX, baseY + wy * 0.5f, baseZ),
                    glm::vec3(thickness, wy, thickness), fade, 1.0f});
                // Z edge at the min-X / min-Y corner.
                instances.push_back(EdgeInstance{
                    glm::vec3(baseX, baseY, baseZ + wz * 0.5f),
                    glm::vec3(thickness, thickness, wz), fade, 2.0f});
            }
        }
    }

    instanceBuffer_.upload(instances.data(), instances.size() * sizeof(EdgeInstance));
    instanceCount_ = static_cast<GLsizei>(instances.size());
}

void Lattice::draw(const glm::mat4& viewProj, const glm::vec3& cameraPos, float time) {
    (void)cameraPos; // edges are emissive; kept for signature symmetry
    if (instanceCount_ == 0) {
        return;
    }

    program_.use();
    program_.setMat4("uViewProj", viewProj);
    program_.setFloat("uTime", time);
    program_.setFloat("uEmissive", constants::kCubeEdgeEmissive);
    program_.setFloat("uHueSpeed", constants::kCubeEdgeHueSpeed);
    program_.setFloat("uAxisHue", constants::kCubeEdgeAxisHue);
    program_.setFloat("uSaturation", constants::kCubeEdgeSaturation);

    vao_.bind();
    glDrawElementsInstanced(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr,
                            instanceCount_);
    glBindVertexArray(0);
}

} // namespace ir
