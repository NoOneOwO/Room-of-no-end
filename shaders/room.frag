#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uGridColor;
uniform float uW;          // mirror spacing W
uniform float uFalloff;    // 0.82
uniform float uCellSize;   // grid cell, divides W
uniform int uMajorEvery;   // brighter line every N cells
uniform int uMaxCopy;      // N
uniform float uGridMinorStrength;
uniform float uGridMajorStrength;
uniform float uFadeStart;
uniform float uFadeEnd;
uniform float uHueShiftPerCopy;
uniform float uAmbientScale;

const float kSrgbExponent = 2.2;

void main() {
    // Copy index of this fragment. Rooms repeat every W along Z, so the copy
    // index is simply the nearest multiple of W to this world-space Z.
    float k = floor(vWorldPos.z / uW + 0.5);
    if (abs(k) > float(uMaxCopy) + 0.001) {
        discard;
    }

    vec3 normal = normalize(vNormal);
    // Use the two world axes that lie in the face's plane.
    vec2 plane = abs(normal.y) > 0.5 ? vWorldPos.xz : vWorldPos.zy;

    // World-space procedural grid, anti-aliased with screen-space derivatives.
    vec2 coord = plane / uCellSize;
    vec2 derivative = max(fwidth(coord), vec2(1e-5));
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float minorLine = 1.0 - min(min(grid.x, grid.y), 1.0);

    vec2 majorCoord = coord / float(uMajorEvery);
    vec2 majorDerivative = max(fwidth(majorCoord), vec2(1e-5));
    vec2 majorGrid = abs(fract(majorCoord - 0.5) - 0.5) / majorDerivative;
    float majorLine = 1.0 - min(min(majorGrid.x, majorGrid.y), 1.0);

    // Subtle neon hue shift per copy, then convert authored sRGB colours.
    float shift = k * uHueShiftPerCopy;
    vec3 tint = shift >= 0.0 ? vec3(0.35, 0.95, 1.00) : vec3(1.00, 0.45, 0.85);
    vec3 gridColor = mix(uGridColor, tint, min(abs(shift), 0.25));
    gridColor = pow(max(gridColor, vec3(0.0)), vec3(kSrgbExponent));
    vec3 baseColor = pow(max(vColor, vec3(0.0)), vec3(kSrgbExponent));

    vec3 color = baseColor * uAmbientScale;
    color += gridColor * (minorLine * uGridMinorStrength +
                          majorLine * uGridMajorStrength);

    // Per-copy falloff and a distance fade so the corridor dissolves.
    float copyBrightness = pow(uFalloff, abs(k));
    float distanceToCamera = length(uCameraPos - vWorldPos);
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, distanceToCamera);

    color *= copyBrightness * fade;
    FragColor = vec4(color, 1.0);
}
