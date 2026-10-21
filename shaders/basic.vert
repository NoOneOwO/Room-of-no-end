#version 330 core

// Shared vertex shader for cubes, the corridor and the mirrors. Cube mode adds
// per-instance copy data (offset, sign, fade) and applies the 3-axis image
// transform after the object's own model matrix.
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec3 aOffset;  // (i*Wx, j*Wy, k*Wz)
layout (location = 4) in vec3 aSign;    // ((-1)^i, (-1)^j, (-1)^k)
layout (location = 5) in float aFade;   // 0.85^(|i|+|j|+|k|)

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;
uniform int uCubeMode;
uniform vec3 uCellCenter;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec3 vImageSign;
out float vFade;

void main() {
    vec4 modelPos = uModel * vec4(aPosition, 1.0);
    vec3 worldPos = modelPos.xyz;
    vec3 imageSign = vec3(1.0);
    float fade = 1.0;

    if (uCubeMode == 1) {
        // 3-axis method of images. Every copy is the real cell reflected about
        // the lattice planes, so a point p maps to
        //     c + offset + sign * (p - c)
        // with c the real cell centre, offset = (i*Wx, j*Wy, k*Wz) and
        // sign = ((-1)^i, (-1)^j, (-1)^k).
        worldPos = uCellCenter + aOffset + aSign * (modelPos.xyz - uCellCenter);
        imageSign = aSign;
        fade = aFade;
    }

    vWorldPos = worldPos;
    // This is the object's real (unflipped) world normal: Lambert shading stays
    // attached to the geometry instead of being mirrored per copy.
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    vImageSign = imageSign;
    vFade = fade;
    gl_Position = uViewProj * vec4(worldPos, 1.0);
}
