#version 410 core
// Shared vertex stage for Basic / Lambert / Standard meshes (three.js r151 meshlambert/meshstandard vert).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;
// InstancedMesh matrix (constant identity when not instanced)
layout(location = 4) in mat4 aInstance;
// terrain texture blend (0 when the geometry has no such attribute)
layout(location = 8) in float aBlend;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMatrix;
uniform int uInstanced;
uniform vec2 uMapOffset;
uniform vec2 uMapRepeat;
uniform mat4 uShadowMatrix;
uniform float uShadowNormalBias;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;
out float vFogDepth;
out vec4 vShadowCoord;
out float vBlend;

void main() {
    vec3 n = aNormal;
    vec4 local = vec4(aPos, 1.0);
    if (uInstanced == 1) {
        // three.js: undo the instance scale on the normal, then rotate
        mat3 m = mat3(aInstance);
        n /= vec3(dot(m[0], m[0]), dot(m[1], m[1]), dot(m[2], m[2]));
        n = m * n;
        local = aInstance * local;
    }
    vec4 world = uModel * local;
    vWorld = world.xyz;
    vNormal = normalize(uNormalMatrix * n);
    vUV = aUV * uMapRepeat + uMapOffset;
    vColor = aColor;
    vBlend = aBlend;
    vec4 mv = uView * world;
    vFogDepth = -mv.z;
    vShadowCoord = uShadowMatrix * vec4(world.xyz + vNormal * uShadowNormalBias, 1.0);
    gl_Position = uProj * mv;
}
