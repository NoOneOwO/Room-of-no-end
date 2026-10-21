#version 330 core

out vec2 vUV;

void main() {
    // One oversized triangle: (0,0), (2,0), (0,2) in UV space. No vertex buffer
    // is needed, gl_VertexID alone generates it.
    vec2 uv = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
