#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;

// Narkowicz ACES fit, applied in linear HDR space.
vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    vec3 bloom = texture(uBloom, vUV).rgb;
    vec3 hdr = scene + bloom * uBloomIntensity;

    vec3 mapped = acesTonemap(hdr);
    mapped = pow(mapped, vec3(1.0 / 2.2)); // linear -> sRGB-ish display
    FragColor = vec4(mapped, 1.0);
}
