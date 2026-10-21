// Mirror.cpp
#include "Mirror.h"
#include "Constants.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace ir {
namespace {

void addQuad(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices,
             const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
             const glm::vec3& p3, const glm::vec3& color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
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

void Mirror::init(const std::filesystem::path& shaderDir) {
    program_.build(shaderDir / "basic.vert", shaderDir / "mirror.frag");

    const float halfWidth = constants::kRoomWidth * 0.5f;
    const float bottom = constants::kFloorY;
    const float top = constants::kFloorY + constants::kRoomHeight;
    const float frame = constants::kFrameThickness;

    const float xLeft = -halfWidth;
    const float xRight = halfWidth;
    const float yBottom = bottom + frame;
    const float yTop = top - frame;

    // --- Frame: four coplanar bars forming a border around the opening.
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        addQuad(vertices, indices,
                {xLeft, yTop, 0.0f}, {xRight, yTop, 0.0f},
                {xRight, top, 0.0f}, {xLeft, top, 0.0f},
                constants::kFrameColor); // top
        addQuad(vertices, indices,
                {xLeft, bottom, 0.0f}, {xRight, bottom, 0.0f},
                {xRight, yBottom, 0.0f}, {xLeft, yBottom, 0.0f},
                constants::kFrameColor); // bottom
        addQuad(vertices, indices,
                {xLeft, yBottom, 0.0f}, {xLeft + frame, yBottom, 0.0f},
                {xLeft + frame, yTop, 0.0f}, {xLeft, yTop, 0.0f},
                constants::kFrameColor); // left
        addQuad(vertices, indices,
                {xRight - frame, yBottom, 0.0f}, {xRight, yBottom, 0.0f},
                {xRight, yTop, 0.0f}, {xRight - frame, yTop, 0.0f},
                constants::kFrameColor); // right
        frameMesh_.upload(vertices, indices);
    }

    // --- Pane: the inset opening, used for both glass and the opaque end cap.
    {
        const float innerHalfWidth = halfWidth - frame;
        const glm::vec3 tint = constants::kGlassTint;
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        addQuad(vertices, indices,
                {-innerHalfWidth, yBottom, 0.0f}, {innerHalfWidth, yBottom, 0.0f},
                {innerHalfWidth, yTop, 0.0f}, {-innerHalfWidth, yTop, 0.0f}, tint);
        paneMesh_.upload(vertices, indices);
    }
}

void Mirror::drawOpaque(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                        int maxCopy) const {
    const constants::FadeRange fade = constants::fadeRangeForDepth(maxCopy);

    program_.use();
    program_.setMat4("uViewProj", viewProj);
    program_.setMat4("uModel", glm::mat4(1.0f));
    program_.setMat3("uNormalMatrix", glm::mat3(1.0f));
    program_.setVec3("uCameraPos", cameraPos);
    program_.setFloat("uFadeStart", fade.start);
    program_.setFloat("uFadeEnd", fade.end);
    program_.setFloat("uRoomHeight", constants::kRoomHeight);
    program_.setFloat("uCapGradientLow", constants::kCapGradientLow);
    program_.setFloat("uCapGradientHigh", constants::kCapGradientHigh);
    program_.setFloat("uTintAlpha", constants::kGlassTintAlpha);
    program_.setFloat("uFresnelAlpha", constants::kGlassFresnelAlpha);
    program_.setFloat("uFresnelPower", constants::kGlassFresnelPower);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    for (int j = -maxCopy - 1; j <= maxCopy; ++j) {
        // Boundary planes sit half a room-length away from each copy centre:
        // z = (j + 0.5) * W, j = -N-1 .. N.
        const float z = (static_cast<float>(j) + 0.5f) * constants::kMirrorSpacing;
        const bool isTunnelEnd = (j == -maxCopy - 1) || (j == maxCopy);

        program_.setMat4("uModel", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z)));

        if (isTunnelEnd) {
            // With N = 0 both boundaries are tunnel ends, which yields a plain
            // room with dark end panels.
            program_.setInt("uMode", 2);
            program_.setVec3("uColor", constants::kCapColor);
            program_.setFloat("uEmissive", 1.0f);
            paneMesh_.draw();
        }

        // The neon frame is drawn exactly once per boundary, never per copy.
        program_.setInt("uMode", 1);
        program_.setVec3("uColor", constants::kFrameColor);
        program_.setFloat("uEmissive", constants::kFrameEmissive);
        frameMesh_.draw();
    }

    glEnable(GL_CULL_FACE);
}

void Mirror::drawGlass(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                       int maxCopy) const {
    const constants::FadeRange fade = constants::fadeRangeForDepth(maxCopy);

    // Interior boundaries only; the two outermost are opaque caps.
    std::vector<std::pair<float, float>> panes; // (distance, z)
    panes.reserve(static_cast<std::size_t>(std::max(0, 2 * maxCopy)));
    for (int j = -maxCopy; j <= maxCopy - 1; ++j) {
        const float z = (static_cast<float>(j) + 0.5f) * constants::kMirrorSpacing;
        panes.emplace_back(std::abs(z - cameraPos.z), z);
    }
    std::sort(panes.begin(), panes.end(),
              [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                  return a.first > b.first; // far to near
              });

    program_.use();
    program_.setMat4("uViewProj", viewProj);
    program_.setInt("uMode", 0);
    program_.setVec3("uColor", constants::kGlassTint);
    program_.setVec3("uCameraPos", cameraPos);
    program_.setFloat("uFadeStart", fade.start);
    program_.setFloat("uFadeEnd", fade.end);
    program_.setFloat("uTintAlpha", constants::kGlassTintAlpha);
    program_.setFloat("uFresnelAlpha", constants::kGlassFresnelAlpha);
    program_.setFloat("uFresnelPower", constants::kGlassFresnelPower);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    for (const auto& pane : panes) {
        program_.setMat4("uModel",
                         glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, pane.second)));
        paneMesh_.draw();
    }

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace ir
