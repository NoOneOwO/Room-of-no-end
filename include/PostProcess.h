// PostProcess.h
// HDR scene target + bloom chain. The scene is rendered into a 4x MSAA
// RGBA16F framebuffer, resolved to a single-sample RGBA16F texture, bright-
// passed and blurred at half resolution, then composited with ACES tonemapping
// and gamma 2.2 to the default framebuffer.
#pragma once

#include "GLWrappers.h"

#include <filesystem>

namespace ir {

class PostProcess {
public:
    void init(const std::filesystem::path& shaderDir);

    // Recreates every target on size change. Falls back to no MSAA if the
    // multisampled framebuffer cannot be completed.
    void resize(int width, int height, int requestedSamples);

    void beginScene();
    void endScene(bool bloomEnabled, float bloomIntensity);

    int samples() const { return samples_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool msaaActive() const { return samples_ > 1; }

private:
    void createTargets(int requestedSamples);
    void destroyTargets();
    void drawFullscreen() const;

    Program brightProgram_;
    Program blurProgram_;
    Program compositeProgram_;
    VertexArray fullscreenVao_;

    Texture sceneTexture_;
    Texture bloomTextureA_;
    Texture bloomTextureB_;

    Framebuffer sceneFbo_;
    Framebuffer bloomFboA_;
    Framebuffer bloomFboB_;
    Framebuffer msaaFbo_;

    Renderbuffer sceneDepth_;
    Renderbuffer msaaColor_;
    Renderbuffer msaaDepth_;

    int width_ = 0;
    int height_ = 0;
    int samples_ = 1;
    int bloomWidth_ = 1;
    int bloomHeight_ = 1;
    bool logged_ = false;
};

} // namespace ir
