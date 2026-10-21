#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec3 vImageSign;
in float vFade;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;    // direction toward the light
uniform vec3 uColor;       // per-object colour, authored in sRGB
uniform float uBrightness; // tunnel copy brightness (1.0 in cube mode)
uniform float uHueShift;   // subtle per-copy neon hue offset

uniform float uAmbient;
uniform float uDiffuse;
uniform float uEmissive;
uniform float uRimStrength;
uniform float uRimPower;

uniform float uFadeStart;
uniform float uFadeEnd;
uniform int uDistanceFade; // 1 = tunnel distance fade, 0 = cube mode

const float kSrgbExponent = 2.2;

vec3 toLinear(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(kSrgbExponent));
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Lambert uses the unflipped geometry normal and the real light.
    float lambert = max(dot(N, L), 0.0);

    // Rim uses the mirrored normal and the view ray to the image position, so
    // the glow blooms on the side that faces the viewer in the reflected copy.
    vec3 rimNormal = normalize(vImageSign * N);
    float rim = pow(1.0 - max(dot(rimNormal, V), 0.0), uRimPower);

    // Colours are authored in sRGB; all lighting happens in linear space.
    vec3 base = toLinear(uColor);
    vec3 tint = toLinear(uHueShift >= 0.0 ? vec3(0.30, 0.85, 1.00)
                                          : vec3(1.00, 0.35, 0.80));
    base = mix(base, tint, min(abs(uHueShift), 0.25));

    vec3 color = base * (uAmbient + uDiffuse * lambert);
    color += base * uEmissive;
    color += base * rim * uRimStrength; // can exceed 1.0: HDR bloom source

    float fade = 1.0;
    if (uDistanceFade == 1) {
        float distanceToCamera = length(uCameraPos - vWorldPos);
        fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, distanceToCamera);
    }

    color *= uBrightness * vFade * fade;
    FragColor = vec4(color, 1.0);
}
