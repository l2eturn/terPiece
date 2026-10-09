#version 410 core
// Screen-space quad for the HUD layers (rect in pixels, y-down)
uniform vec4 uRect;
uniform vec2 uScreen;
uniform vec4 uUv;

out vec2 vUv;

void main() {
    vec2 c = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1);
    vec2 p = uRect.xy + c * uRect.zw;
    vUv = mix(uUv.xy, uUv.zw, c);
    gl_Position = vec4(p.x / uScreen.x * 2.0 - 1.0, 1.0 - p.y / uScreen.y * 2.0, 0.0, 1.0);
}
