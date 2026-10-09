#version 410 core
// Firework sparks (createFireworks in effects.js)
layout(location = 0) in vec3 aPos;
layout(location = 8) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
// drawing-buffer size; three.js passes innerHeight * pixelRatio * 0.4 as the point scale
uniform vec2 uResolution;

out vec4 vColor;

void main() {
    vec4 mv = uView * uModel * vec4(aPos, 1.0);
    gl_Position = uProj * mv;
    gl_PointSize = 0.9 * (uResolution.y * 0.4) / -mv.z;
    vColor = aColor;
}
