#version 410 core
in float vA;

uniform float uFade;

out vec4 fragColor;

void main() {
    float d = length(gl_PointCoord - 0.5);
    fragColor = vec4(1.0, 1.0, 1.0, vA * uFade * smoothstep(0.5, 0.0, d));
}
