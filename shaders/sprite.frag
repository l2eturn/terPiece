#version 410 core
in vec2 vUV;
in float vFogDepth;

uniform vec3 uColor;
uniform float uOpacity;
uniform sampler2D uMap;
uniform int uHasMap;
uniform float uAlphaTest;
uniform int uFog;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;

out vec4 fragColor;

void main() {
    vec4 c = vec4(uColor, uOpacity);
    if (uHasMap == 1) c *= texture(uMap, vUV);
    if (c.a < uAlphaTest) discard;
    vec3 rgb = c.rgb;
    if (uFog == 1) rgb = mix(rgb, uFogColor, smoothstep(uFogNear, uFogFar, vFogDepth));
    fragColor = vec4(rgb, c.a);
}
