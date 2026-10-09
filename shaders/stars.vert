#version 410 core
// Twinkling star points (world.js)
layout(location = 0) in vec3 aPos;
layout(location = 8) in float seed;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;

out float vA;

void main() {
    vA = 0.4 + 0.6 * abs(sin(uTime * (0.5 + seed * 2.0) + seed * 50.0));
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
    gl_PointSize = 1.5 + seed * 2.5;
}
