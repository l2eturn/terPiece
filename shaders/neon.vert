#version 410 core
// Neon tubes of the old Christmas room (vertexshaderCandle in index.html)
layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
