// Room.cpp
#include "Room.h"
#include "Constants.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>

namespace ir {
namespace {

void addQuad(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices,
             const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
             const glm::vec3& p3, const glm::vec3& normal, const glm::vec3& color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back(Vertex{p0, normal, color});
    vertices.push_back(Vertex{p1, normal, color});
    vertices.push_back(Vertex{p2, normal, color});
    vertices.push_back(Vertex{p3, normal, color});

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

} // namespace

void Room::init(const std::filesystem::path& shaderDir) {
    program_.build(shaderDir / "basic.vert", shaderDir / "room.frag");

    const float halfWidth = constants::kRoomWidth * 0.5f;
    const float floorY = constants::kFloorY;
    const float ceilingY = constants::kFloorY + constants::kRoomHeight;
    const float zNear = -constants::kCorridorHalfLength;
    const float zFar = constants::kCorridorHalfLength;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(16);
    indices.reserve(24);

    // Floor (normal up).
    addQuad(vertices, indices,
            {-halfWidth, floorY, zFar}, {halfWidth, floorY, zFar},
            {halfWidth, floorY, zNear}, {-halfWidth, floorY, zNear},
            {0.0f, 1.0f, 0.0f}, constants::kFloorColor);
    // Ceiling (normal down).
    addQuad(vertices, indices,
            {-halfWidth, ceilingY, zNear}, {halfWidth, ceilingY, zNear},
            {halfWidth, ceilingY, zFar}, {-halfWidth, ceilingY, zFar},
            {0.0f, -1.0f, 0.0f}, constants::kCeilingColor);
    // Left wall (x = -W/2, normal points into the room).
    addQuad(vertices, indices,
            {-halfWidth, floorY, zNear}, {-halfWidth, floorY, zFar},
            {-halfWidth, ceilingY, zFar}, {-halfWidth, ceilingY, zNear},
            {1.0f, 0.0f, 0.0f}, constants::kWallColor);
    // Right wall (x = +W/2, normal points into the room).
    addQuad(vertices, indices,
            {halfWidth, floorY, zFar}, {halfWidth, floorY, zNear},
            {halfWidth, ceilingY, zNear}, {halfWidth, ceilingY, zFar},
            {-1.0f, 0.0f, 0.0f}, constants::kWallColor);

    mesh_.upload(vertices, indices);
}

void Room::draw(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                int maxCopy) const {
    const constants::FadeRange fade = constants::fadeRangeForDepth(maxCopy);

    program_.use();
    program_.setMat4("uViewProj", viewProj);
    program_.setMat4("uModel", glm::mat4(1.0f));
    program_.setMat3("uNormalMatrix", glm::mat3(1.0f));
    program_.setVec3("uCameraPos", cameraPos);
    program_.setVec3("uGridColor", constants::kGridColor);
    program_.setFloat("uW", constants::kMirrorSpacing);
    program_.setFloat("uFalloff", constants::kCopyFalloff);
    program_.setFloat("uCellSize", constants::kGridCellSize);
    program_.setInt("uMajorEvery", constants::kGridMajorEvery);
    program_.setInt("uMaxCopy", maxCopy);
    program_.setFloat("uGridMinorStrength", constants::kGridMinorStrength);
    program_.setFloat("uGridMajorStrength", constants::kGridMajorStrength);
    program_.setFloat("uFadeStart", fade.start);
    program_.setFloat("uFadeEnd", fade.end);
    program_.setFloat("uHueShiftPerCopy", constants::kCopyHueShift);
    program_.setFloat("uAmbientScale", constants::kRoomAmbientScale);

    // The corridor is a single open mesh that must be visible from inside; the
    // winding is not meaningful for culling here.
    glDisable(GL_CULL_FACE);
    mesh_.draw();
    glEnable(GL_CULL_FACE);
}

} // namespace ir
