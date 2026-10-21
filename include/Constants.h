// Constants.h
// Every tunable value of the Infinity Room lives here so that the rest of the
// code contains no unexplained literals.
#pragma once

#include <algorithm>
#include <cstddef>

#include <glm/glm.hpp>

namespace ir {
namespace constants {

// ---------------------------------------------------------------- window / GL
constexpr const char* kWindowTitle = "Infinity Room";
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kMsaaSamples = 4;
constexpr double kTitleUpdateInterval = 0.25; // seconds between title updates
constexpr float kClearR = 0.0f;
constexpr float kClearG = 0.0f;
constexpr float kClearB = 0.0f;
constexpr float kClearA = 1.0f;
constexpr double kMaxFrameDelta = 0.1; // clamp dt after a stall / window drag

// ---------------------------------------------------------------------- room
// Room is 12 (x) x 6 (y) x 10 (z), centred in x/z, floor at y = 0.
constexpr float kRoomWidth = 12.0f;   // X extent
constexpr float kRoomHeight = 6.0f;   // Y extent
constexpr float kRoomDepth = 10.0f;   // Z extent, also the mirror distance W
constexpr float kFloorY = 0.0f;
constexpr float kMirrorSpacing = kRoomDepth; // W in the method of images

// The corridor mesh is built once and spans z in [-halfLength, +halfLength].
// 9.5 * W covers copies k = -9..9 and their boundary planes.
constexpr float kCorridorHalfLength = 9.5f * kMirrorSpacing;

// Grid cell size must divide both room width (12) and mirror spacing (10) so
// that the pattern is symmetric across every mirror plane.
constexpr float kGridCellSize = 2.0f;
constexpr int kGridMajorEvery = 5; // a brighter line every 5 cells (10 units)
constexpr float kGridLineSoftness = 1.0f;

constexpr glm::vec3 kFloorColor(0.030f, 0.038f, 0.052f);
constexpr glm::vec3 kCeilingColor(0.018f, 0.022f, 0.032f);
constexpr glm::vec3 kWallColor(0.026f, 0.032f, 0.046f);
constexpr glm::vec3 kGridColor(0.10f, 0.62f, 0.86f);
constexpr float kGridMinorStrength = 0.38f;
constexpr float kGridMajorStrength = 1.00f;
constexpr float kRoomAmbientScale = 0.70f;

// ------------------------------------------------------------------ mirrors
// Mirrors fill the full 12 x 6 end walls. The frame is a thin emissive border
// inset into the opening; the glass pane fills the remaining interior.
constexpr float kFrameThickness = 0.09f;
constexpr glm::vec3 kFrameColor(0.30f, 0.98f, 1.00f);
constexpr float kFrameEmissive = 3.5f; // HDR, feeds the bloom pass
constexpr glm::vec3 kCapColor(0.012f, 0.018f, 0.030f);
constexpr float kCapGradientLow = 0.50f; // bottom of the cap is darker
constexpr float kCapGradientHigh = 1.00f;
constexpr glm::vec3 kGlassTint(0.20f, 0.85f, 1.00f);
constexpr float kGlassTintAlpha = 0.05f;
constexpr float kGlassFresnelAlpha = 0.09f;
constexpr float kGlassFresnelPower = 4.0f;

// --------------------------------------------------------------- reflections
constexpr int kDefaultReflectionDepth = 5;
// Digits 1..9 select depths 1..9; F toggles between 0 and the saved depth.
constexpr int kMinReflectionDepth = 0;
constexpr int kMaxReflectionDepth = 9;
constexpr float kCopyFalloff = 0.82f;   // brightness multiplier per copy
constexpr float kCopyHueShift = 0.020f; // subtle neon hue rotation per copy
constexpr float kFadeStartFraction = 0.55f; // fraction of tunnel length
constexpr float kFadeEndFraction = 1.05f;
// Never fade inside the real room, otherwise shallow depths (N = 0) turn the
// actual walls and mirrors black. The room diagonal is about 16.7 units.
constexpr float kMinFadeStart = 18.0f;
constexpr float kMinFadeEnd = 40.0f;

// Distance-fade window for a given reflection depth. Shared by the corridor,
// cubes and mirrors so the whole tunnel dissolves consistently.
struct FadeRange {
    float start = kMinFadeStart;
    float end = kMinFadeEnd;
};

inline FadeRange fadeRangeForDepth(int reflectionDepth) {
    const float tunnelLength =
        (static_cast<float>(reflectionDepth) + 0.5f) * kMirrorSpacing;
    return FadeRange{std::max(tunnelLength * kFadeStartFraction, kMinFadeStart),
                     std::max(tunnelLength * kFadeEndFraction, kMinFadeEnd)};
}

// -------------------------------------------------------------------- camera
constexpr float kFovDegrees = 70.0f;
constexpr float kNearPlane = 0.05f;
constexpr float kFarPlane = 300.0f;
constexpr float kCameraMargin = 0.3f; // keeps the camera inside the real room
constexpr float kMoveSpeed = 3.0f;    // units / second
constexpr float kMouseSensitivityDeg = 0.09f; // degrees per pixel
constexpr float kMaxPitchDeg = 89.0f;
// Slightly off-axis start view so the cube's own reflections are visible
// beside it instead of being hidden exactly behind it.
constexpr float kStartYawDeg = -104.0f;
constexpr float kStartPitchDeg = -2.0f;
constexpr float kStartCameraX = 2.0f;
constexpr float kStartCameraY = 1.7f;
constexpr float kStartCameraZ = 3.6f;

// ------------------------------------------------------------------- objects
constexpr std::size_t kMaxObjects = 256;
constexpr float kSpawnDistance = 3.0f;
constexpr float kSpawnClearance = 0.1f; // extra margin from walls / floor / ceiling
constexpr float kUnitCubeHalfExtent = 0.5f;
constexpr float kMinObjectScale = 0.4f;
constexpr float kMaxObjectScale = 1.0f;
constexpr float kMinRotationSpeed = -1.4f; // radians / second
constexpr float kMaxRotationSpeed = 1.4f;
constexpr float kInitialCubeScale = 0.9f;
constexpr float kInitialCubeSpin = 0.70f; // radians / second
constexpr float kAxisEpsilon = 1e-5f;       // degenerate rotation axis guard
constexpr float kTwoPi = 6.28318530718f;   // full turn in radians
constexpr float kSpawnSpinDeadzone = 0.15f; // near-zero spins get a nudge
constexpr float kSpawnSpinFallback = 0.35f; // radians / second
constexpr float kSpawnSaturationMin = 0.65f;
constexpr float kSpawnSaturationMax = 0.95f;
constexpr float kSpawnValueMin = 0.80f;
constexpr float kSpawnValueMax = 1.00f;
constexpr float kInitialCubeY = 1.5f;
constexpr glm::vec3 kInitialCubeAxis(0.55f, 1.0f, 0.25f);
constexpr glm::vec3 kInitialCubeColor(0.20f, 0.92f, 1.00f);

// Lighting (view-independent, flipped in Z for mirrored copies).
constexpr glm::vec3 kLightDirection(0.35f, 0.85f, 0.40f);
constexpr float kAmbientStrength = 0.18f;
constexpr float kDiffuseStrength = 0.72f;
constexpr float kEmissiveStrength = 0.28f;
constexpr float kRimStrength = 0.80f;
constexpr float kRimPower = 2.6f;

// ----------------------------------------------------------------- cube mode
constexpr int kCubeDefaultReflectionDepth = 3;
constexpr int kCubeMaxReflectionDepth = 5;
constexpr float kCubeFalloff = 0.85f; // per copy, exponent is |i|+|j|+|k|
constexpr float kCubeRimStrength = 1.5f; // HDR rim glow in the lattice
constexpr glm::vec3 kCubeCellCenter(0.0f, kRoomHeight * 0.5f, 0.0f);

// Neon lattice edges: each cell owns the three segments that start at its
// minimum corner (X, Y and Z), so no segment is emitted twice.
constexpr float kCubeEdgeThickness = 0.04f;
constexpr float kCubeEdgeEmissive = 4.0f;
constexpr float kCubeEdgeHueSpeed = 0.02f;    // hue turns / second
constexpr float kCubeEdgeAxisHue = 0.07f;     // per-axis colour offset
constexpr float kCubeEdgeSaturation = 0.80f;

// -------------------------------------------------------------------- bloom
constexpr int kSceneMsaaSamples = 4; // offscreen scene target, not the window
constexpr bool kBloomDefaultEnabled = true;
constexpr float kBloomDefaultIntensity = 0.60f;
constexpr float kBloomIntensityStep = 0.10f;
constexpr float kBloomIntensityMin = 0.0f;
constexpr float kBloomIntensityMax = 2.0f;
constexpr float kBloomThreshold = 1.0f;
constexpr float kBloomSoftKnee = 0.50f;
constexpr int kBloomIterations = 5;
constexpr int kBloomDownsample = 2;
constexpr float kSrgbExponent = 2.2f;

// --------------------------------------------------------------------- demo
constexpr int kDemoMaxObjects = 12; // total cubes in the scene, initial included
constexpr float kDemoSpawnInterval = 0.4f;
constexpr float kDemoDepthInterval = 8.0f;
constexpr glm::vec3 kDemoCameraCenter(0.0f, kRoomHeight * 0.5f, 0.0f);
constexpr glm::vec3 kDemoCameraAmplitude(4.2f, 1.5f, 3.4f);
constexpr glm::vec3 kDemoCameraFrequency(0.21f, 0.13f, 0.17f);
constexpr glm::vec3 kDemoCameraPhase(0.0f, 1.7f, 3.1f);
constexpr glm::vec3 kDemoTargetAmplitude(2.5f, 0.9f, 2.5f);
constexpr glm::vec3 kDemoTargetFrequency(0.17f, 0.23f, 0.19f);
constexpr glm::vec3 kDemoTargetPhase(1.1f, 0.4f, 2.2f);
constexpr float kDemoSpawnRadiusMin = 2.0f;
constexpr float kDemoSpawnRadiusMax = 4.0f;
constexpr float kDemoSpawnEyeMin = 1.1f;
constexpr float kDemoSpawnEyeMax = 4.4f;

// ------------------------------------------------------------------- capture
constexpr int kScreenshotFrames = 3;
constexpr double kScreenshotDeltaTime = 1.0 / 60.0;
constexpr double kRecordFps = 60.0;
constexpr double kRecordDefaultSeconds = 12.0;
constexpr double kRecordMaxSeconds = 600.0;
// Start the animation clock a little ahead so the three captured frames show
// the cube at an interesting angle rather than nearly axis-aligned.
constexpr float kScreenshotStartAnimTime = 0.9f;

} // namespace constants
} // namespace ir
