#version 410 core
// Grade pass from postfx.js: slight warm tint + vignette, then linear -> sRGB (LinearTosRGB in r151).
// The scene texture is sampled NEAREST, so the low-res target upscales into crisp pixels.
in vec2 vUv;

uniform sampler2D uTex;
uniform float uVignette;
uniform float uWarm;

out vec4 fragColor;

vec3 linearToSRGB(vec3 v) {
    vec3 hi = pow(max(v, vec3(0.0)), vec3(0.41666)) * 1.055 - vec3(0.055);
    vec3 lo = v * 12.92;
    return mix(hi, lo, vec3(lessThanEqual(v, vec3(0.0031308))));
}

void main() {
    vec4 c = texture(uTex, vUv);
    c.rgb *= vec3(1.0 + uWarm, 1.0, 1.0 - uWarm);
    vec2 q = vUv - 0.5;
    c.rgb *= 1.0 - dot(q, q) * uVignette * 2.0;
    fragColor = vec4(linearToSRGB(c.rgb), 1.0);
}
