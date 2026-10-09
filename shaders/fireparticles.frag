#version 410 core
in vec4 vColor;
in vec2 vAngle;

uniform sampler2D uMap;

out vec4 fragColor;

void main() {
    // gl_PointCoord is bottom-up in desktop GL; flip to match WebGL
    vec2 pc = vec2(gl_PointCoord.x, 1.0 - gl_PointCoord.y);
    vec2 coords = (pc - 0.5) * mat2(vAngle.x, vAngle.y, -vAngle.y, vAngle.x) + 0.5;
    fragColor = texture(uMap, coords) * vColor;
}
