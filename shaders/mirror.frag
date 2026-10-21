#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;

out vec4 FragColor;

// 0 = glass pane, 1 = neon frame, 2 = opaque tunnel-end cap.
uniform int uMode;

uniform vec3 uCameraPos;
uniform vec3 uColor;       // authored in sRGB
uniform float uEmissive;   // HDR multiplier for the frame
uniform float uRoomHeight;
uniform float uCapGradientLow;
uniform float uCapGradientHigh;

uniform float uTintAlpha;
uniform float uFresnelAlpha;
uniform float uFresnelPower;

uniform float uFadeStart;
uniform float uFadeEnd;

const float kSrgbExponent = 2.2;

void main() {
    float distanceToCamera = length(uCameraPos - vWorldPos);
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, distanceToCamera);
    vec3 linearColor = pow(max(uColor, vec3(0.0)), vec3(kSrgbExponent));

    if (uMode == 1) {
        // Neon frame: HDR emissive bar, a bloom source.
        FragColor = vec4(linearColor * uEmissive * fade, 1.0);
    } else if (uMode == 2) {
        // Opaque dark cap at the tunnel ends, slightly lighter near the top.
        float height = clamp(vWorldPos.y / max(uRoomHeight, 1e-4), 0.0, 1.0);
        float gradient = mix(uCapGradientLow, uCapGradientHigh, height);
        FragColor = vec4(linearColor * gradient * fade, 1.0);
    } else {
        // See-through pane: ~5% cyan tint plus a faint grazing-angle sheen.
        vec3 viewDir = normalize(uCameraPos - vWorldPos);
        float facing = abs(dot(normalize(vNormal), viewDir));
        float fresnel = pow(1.0 - facing, uFresnelPower);
        float alpha = (uTintAlpha + uFresnelAlpha * fresnel) * fade;
        FragColor = vec4(linearColor, clamp(alpha, 0.0, 1.0));
    }
}
