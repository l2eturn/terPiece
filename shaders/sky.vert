#version 410 core
// Sky dome (ShaderMaterial in world.js / medieval.js): gradient by the height of the sphere direction
layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vP;

void main() {
    vP = normalize(aPos);
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
