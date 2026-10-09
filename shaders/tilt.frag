#version 410 core
// Tilt-shift blur from postfx.js (sharp band in the middle, blurred top/bottom), one direction per pass
in vec2 vUv;

uniform sampler2D uTex;
uniform vec2 uDir;
uniform vec2 uRes;
uniform float uAmount;

out vec4 fragColor;

const float FOCUS = 0.48;
const float BAND = 0.3;

void main() {
    float d = max(0.0, abs(vUv.y - FOCUS) - BAND);
    float r = clamp(d * 9.0, 0.0, 1.0) * 3.2 * uAmount;
    vec2 o = uDir / uRes * r;
    vec4 c = texture(uTex, vUv) * 0.2270270270;
    c += (texture(uTex, vUv + o * 1.3846153846) + texture(uTex, vUv - o * 1.3846153846)) * 0.3162162162;
    c += (texture(uTex, vUv + o * 3.2307692308) + texture(uTex, vUv - o * 3.2307692308)) * 0.0702702703;
    fragColor = c;
}
