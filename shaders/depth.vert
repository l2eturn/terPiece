#version 410 core
// Shadow-map depth pass (MeshDepthMaterial / customDepthMaterial with an alpha-tested map)
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;
layout(location = 4) in mat4 aInstance;

uniform mat4 uModel;
uniform mat4 uProj;
uniform int uInstanced;
uniform vec2 uMapOffset;
uniform vec2 uMapRepeat;

out vec2 vUV;

void main() {
    vec4 local = vec4(aPos, 1.0);
    if (uInstanced == 1) local = aInstance * local;
    vUV = aUV * uMapRepeat + uMapOffset;
    gl_Position = uProj * uModel * local;
}
