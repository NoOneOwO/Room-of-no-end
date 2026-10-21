#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uDirection; // one texel step along the blur axis, in UV units

void main() {
    // 9-tap Gaussian (sigma ~= 2 texels).
    const float weights[5] = float[5](0.2270270270, 0.1945945946, 0.1216216216,
                                      0.0540540541, 0.0162162162);
    vec3 result = texture(uTexture, vUV).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = uDirection * float(i);
        result += texture(uTexture, vUV + offset).rgb * weights[i];
        result += texture(uTexture, vUV - offset).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}
