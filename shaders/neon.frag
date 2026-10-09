#version 410 core
// fragmentshaderCandle: loops from the base colour to the final colour
uniform float time;
uniform float colorSpeed;
uniform float delay;
uniform vec3 baseColor;
uniform vec3 finalColor;

out vec4 fragColor;

void main() {
    float animatedTime = mod(time - delay, colorSpeed);
    float mixFactor = animatedTime / colorSpeed;
    fragColor = vec4(mix(baseColor, finalColor, mixFactor), 1.0);
}
