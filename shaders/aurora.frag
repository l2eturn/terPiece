#version 410 core
in vec2 vUv;

uniform float uTime;
uniform float uFade;

out vec4 fragColor;

void main() {
    float wave = 0.5 + 0.5 * sin(vUv.x * 40.0 + uTime * 1.3 + sin(vUv.x * 13.0 - uTime) * 2.0);
    float a = smoothstep(0.0, 0.25, vUv.y) * smoothstep(1.0, 0.35, vUv.y) * (0.35 + 0.65 * wave);
    a *= smoothstep(0.0, 0.15, vUv.x) * smoothstep(1.0, 0.85, vUv.x);
    vec3 c = mix(vec3(0.2, 1.0, 0.6), vec3(0.7, 0.3, 1.0), vUv.y);
    fragColor = vec4(c, a * 0.33 * uFade);
}
