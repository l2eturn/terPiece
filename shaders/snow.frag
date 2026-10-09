#version 410 core
in float vAlpha;

uniform sampler2D uMap;

out vec4 fragColor;

void main() {
    fragColor = texture(uMap, gl_PointCoord) * vec4(1.0, 1.0, 1.0, vAlpha);
}
