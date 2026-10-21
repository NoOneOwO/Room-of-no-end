#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uThreshold;
uniform float uSoftKnee;

void main() {
    vec3 color = texture(uScene, vUV).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Soft-knee threshold (COD-style): smoothly ramp values just above the
    // threshold instead of clipping them at a hard edge.
    float knee = uThreshold * uSoftKnee + 1e-5;
    float soft = clamp(brightness - uThreshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(soft, brightness - uThreshold) / max(brightness, 1e-4);

    FragColor = vec4(color * contribution, 1.0);
}
