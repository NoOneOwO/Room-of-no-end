// Mirror.h
// Draws the boundary planes between the mirror copies: one neon frame per
// boundary, an alpha-blended glass pane per interior boundary, and an opaque
// dark cap at the two tunnel ends.
#pragma once

#include "GLWrappers.h"

#include <filesystem>

namespace ir {

class Mirror {
public:
    void init(const std::filesystem::path& shaderDir);

    // Opaque pass: frames and the two tunnel-end caps.
    void drawOpaque(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                    int maxCopy) const;

    // Transparent pass: glass panes, far to near, depth writes disabled.
    void drawGlass(const glm::mat4& viewProj, const glm::vec3& cameraPos,
                   int maxCopy) const;

private:
    Program program_;
    Mesh frameMesh_;
    Mesh paneMesh_;
};

} // namespace ir
