#version 410 core
// Aurora ribbons (world.js): the plane waves in y/z over time
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;

out vec2 vUv;

void main() {
    vUv = aUV;
    vec3 p = aPos;
    p.y += sin(aUV.x * 9.0 + uTime * 0.4) * 6.0;
    p.z += sin(aUV.x * 5.0 + uTime * 0.3) * 12.0;
    gl_Position = uProj * uView * uModel * vec4(p, 1.0);
}
