#version 410 core
in vec2 vUv;

uniform sampler2D uTex;
uniform int uHasTex;
uniform vec4 uColor;
// corner radius in pixels (rounded minimap), 0 = square
uniform float uRadius;
uniform vec4 uRect;

out vec4 fragColor;

void main() {
    vec4 c = uColor;
    if (uHasTex == 1) c *= texture(uTex, vUv);
    if (uRadius > 0.0) {
        vec2 local = (gl_FragCoord.xy);
        // gl_FragCoord is bottom-up; rounded corners are symmetric so use uv-space distance instead
        vec2 px = vUv * uRect.zw;
        vec2 q = abs(px - uRect.zw * 0.5) - (uRect.zw * 0.5 - uRadius);
        float d = length(max(q, 0.0)) - uRadius;
        c.a *= clamp(0.5 - d, 0.0, 1.0);
    }
    fragColor = c;
}
