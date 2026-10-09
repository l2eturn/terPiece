#version 410 core
in vec4 vColor;

uniform sampler2D uMap;

out vec4 fragColor;

void main() {
    fragColor = texture(uMap, gl_PointCoord) * vColor;
}
