#version 330 core

// Unit cube scaled into one thin lattice-edge box per instance.
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aCenter;
layout (location = 2) in vec3 aExtent;
layout (location = 3) in float aFade;
layout (location = 4) in float aAxis; // 0 = X edge, 1 = Y edge, 2 = Z edge

uniform mat4 uViewProj;

out float vFade;
out float vAxis;

void main() {
    vec3 worldPos = aCenter + aPosition * aExtent;
    vFade = aFade;
    vAxis = aAxis;
    gl_Position = uViewProj * vec4(worldPos, 1.0);
}
