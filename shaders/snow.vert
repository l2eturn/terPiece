#version 410 core
// Snowfall (createSnowfall in effects.js): all motion computed on the GPU inside a box around the camera
layout(location = 0) in vec3 aPos;
layout(location = 8) in float rnd;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform float uTime;
uniform vec3 uCenter;
uniform vec3 uBox;
// drawing-buffer size; three.js passes innerHeight * pixelRatio * 0.4 as the point scale
uniform vec2 uResolution;

out float vAlpha;

void main() {
    vec3 p = aPos;
    p.y -= uTime * (0.8 + rnd * 0.9);
    p.x += sin(uTime * 0.7 + rnd * 40.0) * 1.2 + uTime * 0.3;
    p.z += cos(uTime * 0.5 + rnd * 30.0) * 1.0;
    p = mod(p - uCenter + uBox * 0.5, uBox) + uCenter - uBox * 0.5;
    vec4 mv = uView * uModel * vec4(p, 1.0);
    gl_Position = uProj * mv;
    gl_PointSize = (0.06 + rnd * 0.08) * (uResolution.y * 0.4) / -mv.z;
    vAlpha = smoothstep(60.0, 20.0, -mv.z);
}
