// PostProcess.cpp
#include "PostProcess.h"
#include "Constants.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace ir {

void PostProcess::init(const std::filesystem::path& shaderDir) {
    brightProgram_.build(shaderDir / "fullscreen.vert", shaderDir / "bright.frag");
    blurProgram_.build(shaderDir / "fullscreen.vert", shaderDir / "blur.frag");
    compositeProgram_.build(shaderDir / "fullscreen.vert", shaderDir / "composite.frag");
}

void PostProcess::resize(int width, int height, int requestedSamples) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == width_ && height == height_ && width_ > 0) {
        return;
    }

    destroyTargets();
    width_ = width;
    height_ = height;
    createTargets(requestedSamples);
}

void PostProcess::createTargets(int requestedSamples) {
    const int bloomWidth = std::max(1, width_ / constants::kBloomDownsample);
    const int bloomHeight = std::max(1, height_ / constants::kBloomDownsample);
    bloomWidth_ = bloomWidth;
    bloomHeight_ = bloomHeight;

    // --- Single-sample scene target: RGBA16F colour + depth for the no-MSAA
    //     fallback and the resolve destination when MSAA is available.
    sceneTexture_.create(width_, height_, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_LINEAR);
    sceneDepth_.create(width_, height_, GL_DEPTH_COMPONENT24, 1);

    sceneFbo_.create();
    sceneFbo_.bind();
    sceneFbo_.attachTexture(GL_COLOR_ATTACHMENT0, sceneTexture_);
    sceneFbo_.attachRenderbuffer(GL_DEPTH_ATTACHMENT, sceneDepth_);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (!sceneFbo_.complete()) {
        throw std::runtime_error("HDR scene framebuffer is incomplete");
    }

    // --- Half-resolution bloom ping-pong targets.
    bloomTextureA_.create(bloomWidth, bloomHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                          GL_LINEAR);
    bloomTextureB_.create(bloomWidth, bloomHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT,
                          GL_LINEAR);
    bloomFboA_.create();
    bloomFboA_.bind();
    bloomFboA_.attachTexture(GL_COLOR_ATTACHMENT0, bloomTextureA_);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (!bloomFboA_.complete()) {
        throw std::runtime_error("Bloom framebuffer A is incomplete");
    }
    bloomFboB_.create();
    bloomFboB_.bind();
    bloomFboB_.attachTexture(GL_COLOR_ATTACHMENT0, bloomTextureB_);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (!bloomFboB_.complete()) {
        throw std::runtime_error("Bloom framebuffer B is incomplete");
    }

    // Start both bloom textures at black so sampling them with bloom disabled
    // can never pick up uninitialised half-floats.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    bloomFboA_.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    bloomFboB_.bind();
    glClear(GL_COLOR_BUFFER_BIT);

    // --- Optional 4x MSAA scene target (renderbuffers: it is only ever blitted).
    samples_ = 1;
    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const int wantSamples =
        requestedSamples > 1 ? std::min(requestedSamples, maxSamples) : 1;

    if (wantSamples > 1) {
        msaaColor_.create(width_, height_, GL_RGBA16F, wantSamples);
        msaaDepth_.create(width_, height_, GL_DEPTH_COMPONENT24, wantSamples);
        msaaFbo_.create();
        msaaFbo_.bind();
        msaaFbo_.attachRenderbuffer(GL_COLOR_ATTACHMENT0, msaaColor_);
        msaaFbo_.attachRenderbuffer(GL_DEPTH_ATTACHMENT, msaaDepth_);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        if (msaaFbo_.complete()) {
            samples_ = wantSamples;
        } else {
            std::cerr << "Warning: " << wantSamples
                      << "x MSAA framebuffer is incomplete; falling back to no MSAA\n";
            msaaFbo_.destroy();
            msaaColor_.destroy();
            msaaDepth_.destroy();
            samples_ = 1;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!logged_) {
        std::cout << "Post: RGBA16F scene " << width_ << "x" << height_
                  << ", bloom " << bloomWidth_ << "x" << bloomHeight_ << ", scene MSAA "
                  << samples_ << "x\n";
        logged_ = true;
    }
}

void PostProcess::destroyTargets() {
    msaaFbo_.destroy();
    sceneFbo_.destroy();
    bloomFboA_.destroy();
    bloomFboB_.destroy();

    msaaColor_.destroy();
    msaaDepth_.destroy();
    sceneDepth_.destroy();

    sceneTexture_.destroy();
    bloomTextureA_.destroy();
    bloomTextureB_.destroy();
}

void PostProcess::beginScene() {
    if (width_ <= 0 || height_ <= 0) {
        throw std::runtime_error("PostProcess has no render target");
    }

    const Framebuffer& sceneTarget = msaaActive() ? msaaFbo_ : sceneFbo_;
    sceneTarget.bind();
    glViewport(0, 0, width_, height_);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void PostProcess::endScene(bool bloomEnabled, float bloomIntensity) {
    if (msaaActive()) {
        // Resolve the multisampled scene into the single-sample colour texture.
        msaaFbo_.bind(GL_READ_FRAMEBUFFER);
        sceneFbo_.bind(GL_DRAW_FRAMEBUFFER);
        glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    const bool useBloom = bloomEnabled && bloomIntensity > 0.0f;

    if (useBloom) {
        // Bright pass at half resolution.
        bloomFboA_.bind();
        glViewport(0, 0, bloomWidth_, bloomHeight_);
        brightProgram_.use();
        sceneTexture_.bind(GL_TEXTURE0);
        brightProgram_.setInt("uScene", 0);
        brightProgram_.setFloat("uThreshold", constants::kBloomThreshold);
        brightProgram_.setFloat("uSoftKnee", constants::kBloomSoftKnee);
        drawFullscreen();

        // Five separable 9-tap Gaussian ping-pong iterations.
        for (int iteration = 0; iteration < constants::kBloomIterations; ++iteration) {
            bloomFboB_.bind();
            glViewport(0, 0, bloomWidth_, bloomHeight_);
            blurProgram_.use();
            bloomTextureA_.bind(GL_TEXTURE0);
            blurProgram_.setInt("uTexture", 0);
            blurProgram_.setVec2("uDirection",
                                 glm::vec2(1.0f / static_cast<float>(bloomWidth_), 0.0f));
            drawFullscreen();

            bloomFboA_.bind();
            glViewport(0, 0, bloomWidth_, bloomHeight_);
            blurProgram_.use();
            bloomTextureB_.bind(GL_TEXTURE0);
            blurProgram_.setInt("uTexture", 0);
            blurProgram_.setVec2("uDirection",
                                 glm::vec2(0.0f, 1.0f / static_cast<float>(bloomHeight_)));
            drawFullscreen();
        }
    }

    // Composite to the default framebuffer: scene + bloom -> ACES -> gamma 2.2.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    compositeProgram_.use();
    sceneTexture_.bind(GL_TEXTURE0);
    bloomTextureA_.bind(GL_TEXTURE1);
    compositeProgram_.setInt("uScene", 0);
    compositeProgram_.setInt("uBloom", 1);
    compositeProgram_.setFloat("uBloomIntensity",
                               useBloom ? bloomIntensity : 0.0f);
    drawFullscreen();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void PostProcess::drawFullscreen() const {
    fullscreenVao_.bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

} // namespace ir
