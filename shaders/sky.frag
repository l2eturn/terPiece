#version 410 core
in vec3 vP;

uniform vec3 uTop;
uniform vec3 uMid;
uniform vec3 uHor;
// smoothstep edges: horizon->mid (x, y), mid->top (z, w)
uniform vec4 uStops;

out vec4 fragColor;

void main() {
    float h = vP.y;
    vec3 c = mix(uHor, uMid, smoothstep(uStops.x, uStops.y, h));
    c = mix(c, uTop, smoothstep(uStops.z, uStops.w, h));
    fragColor = vec4(c, 1.0);
}
