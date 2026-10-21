#version 330 core

in float vFade;
in float vAxis;

out vec4 FragColor;

uniform float uTime;
uniform float uEmissive;   // ~4.0 HDR
uniform float uHueSpeed;   // hue turns per second
uniform float uAxisHue;    // per-axis hue offset
uniform float uSaturation;

vec3 hsvToRgb(vec3 hsv) {
    vec3 p = abs(fract(hsv.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return hsv.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), hsv.y);
}

void main() {
    // Slow global hue cycle with a small offset per lattice axis.
    float hue = fract(uTime * uHueSpeed + vAxis * uAxisHue);
    vec3 color = hsvToRgb(vec3(hue, uSaturation, 1.0));
    // Neon is authored in sRGB and must enter the linear pipeline before the
    // HDR emissive multiplier (which pushes it far above 1.0 for bloom).
    color = pow(max(color, vec3(0.0)), vec3(2.2));
    color *= uEmissive * vFade;
    FragColor = vec4(color, 1.0);
}
