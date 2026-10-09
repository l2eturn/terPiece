#version 410 core
in vec2 vUV;

uniform sampler2D uMap;
uniform int uHasMap;
uniform float uAlphaTest;

void main() {
    // sprite characters cut their silhouette out of the shadow
    if (uHasMap == 1 && uAlphaTest > 0.0 && texture(uMap, vUV).a < uAlphaTest) discard;
}
