#version 410 core
// THREE.Sprite: a camera-facing quad sized by the object's world scale (sprite_vert in r151)
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec2 uMapOffset;
uniform vec2 uMapRepeat;

out vec2 vUV;
out float vFogDepth;

void main() {
    vec4 mv = uView * uModel * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 scale = vec2(length(uModel[0].xyz), length(uModel[1].xyz));
    mv.xy += aPos.xy * scale;
    vUV = aUV * uMapRepeat + uMapOffset;
    vFogDepth = -mv.z;
    gl_Position = uProj * mv;
}
