#version 410 core
// MeshBasic / MeshLambert / MeshStandard (no env map) with three.js r151 legacy-light maths:
// hemisphere + one directional light (PCF soft shadow) + point lights, linear fog. Output stays linear;
// the post pass does the sRGB encode.
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vColor;
in float vFogDepth;
in vec4 vShadowCoord;
in float vBlend;

// 0 = basic, 1 = lambert, 2 = standard
uniform int uType;
uniform vec3 uColor;
uniform float uOpacity;
uniform sampler2D uMap;
uniform int uHasMap;
uniform sampler2D uMap2;
uniform int uHasMap2;
uniform float uAlphaTest;
uniform int uVertexColors;
uniform vec3 uEmissive;
uniform sampler2D uEmissiveMap;
uniform int uHasEmissiveMap;
uniform float uRoughness;
uniform float uMetalness;
// 0 = front, 1 = back, 2 = double
uniform int uSide;

uniform vec3 uHemiSky;
uniform vec3 uHemiGround;
uniform vec3 uSunColor;
uniform vec3 uSunDir;
uniform int uReceiveShadow;
uniform sampler2DShadow uShadowMap;
uniform float uShadowBias;
uniform float uShadowTexel;

#define MAX_LIGHTS 16
uniform int uNumLights;
uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightDist[MAX_LIGHTS];
uniform float uLightDecay[MAX_LIGHTS];

uniform vec3 uCamPos;
uniform int uFog;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform int uClip;
uniform vec4 uClipPlane;

out vec4 fragColor;

const float PI = 3.141592653589793;

// getDistanceAttenuation() for legacy lights
float distanceAttenuation(float d, float cutoff, float decay) {
    if (cutoff > 0.0 && decay > 0.0) return pow(clamp(-d / cutoff + 1.0, 0.0, 1.0), decay);
    return 1.0;
}

// PCF soft: 3x3 hardware-filtered compares
float shadowFactor() {
    vec3 c = vShadowCoord.xyz / vShadowCoord.w;
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0 || c.z > 1.0) return 1.0;
    c.z += uShadowBias;
    float s = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x) s += texture(uShadowMap, vec3(c.xy + vec2(x, y) * uShadowTexel, c.z));
    return s / 9.0;
}

// GGX specular (BRDF_GGX in three.js)
vec3 brdfGGX(vec3 l, vec3 v, vec3 n, vec3 f0, float roughness) {
    float alpha = roughness * roughness;
    vec3 h = normalize(l + v);
    float dotNL = clamp(dot(n, l), 0.0, 1.0);
    float dotNV = clamp(dot(n, v), 0.0, 1.0);
    float dotNH = clamp(dot(n, h), 0.0, 1.0);
    float dotVH = clamp(dot(v, h), 0.0, 1.0);
    float fresnel = exp2((-5.55473 * dotVH - 6.98316) * dotVH);
    vec3 F = f0 * (1.0 - fresnel) + fresnel;
    float a2 = alpha * alpha;
    float gv = dotNL * sqrt(a2 + (1.0 - a2) * dotNV * dotNV);
    float gl = dotNV * sqrt(a2 + (1.0 - a2) * dotNL * dotNL);
    float V = 0.5 / max(gv + gl, 1e-6);
    float denom = dotNH * dotNH * (a2 - 1.0) + 1.0;
    float D = (1.0 / PI) * a2 / (denom * denom);
    return F * (V * D);
}

void main() {
    if (uClip == 1 && dot(uClipPlane.xyz, vWorld) + uClipPlane.w < 0.0) discard;

    vec4 diffuse = vec4(uColor, uOpacity);
    if (uHasMap == 1) {
        vec4 t = texture(uMap, vUV);
        if (uHasMap2 == 1) t = mix(t, texture(uMap2, vUV), clamp(vBlend, 0.0, 1.0));
        diffuse *= t;
    }
    if (uVertexColors == 1) diffuse.rgb *= vColor;
    if (diffuse.a < uAlphaTest) discard;

    vec3 outgoing;
    if (uType == 0) {
        outgoing = diffuse.rgb;
    } else {
        vec3 n = normalize(vNormal);
        if (uSide == 1) n = -n;
        if (uSide == 2 && !gl_FrontFacing) n = -n;
        vec3 v = normalize(uCamPos - vWorld);
        vec3 albedo = uType == 2 ? diffuse.rgb * (1.0 - uMetalness) : diffuse.rgb;
        vec3 f0 = mix(vec3(0.04), diffuse.rgb, uMetalness);
        float rough = clamp(uRoughness, 0.0525, 1.0);

        // hemisphere light = indirect diffuse
        vec3 irradiance = mix(uHemiGround, uHemiSky, 0.5 * n.y + 0.5);
        vec3 direct = vec3(0.0);
        vec3 spec = vec3(0.0);

        // directional light (moon / sun)
        float sh = uReceiveShadow == 1 ? shadowFactor() : 1.0;
        float dotNL = max(dot(n, uSunDir), 0.0);
        vec3 sunIrr = uSunColor * dotNL * sh;
        direct += sunIrr;
        if (uType == 2) spec += sunIrr * brdfGGX(uSunDir, v, n, f0, rough);

        for (int i = 0; i < uNumLights; ++i) {
            vec3 lv = uLightPos[i] - vWorld;
            float d = length(lv);
            vec3 l = lv / max(d, 1e-5);
            vec3 c = uLightColor[i] * distanceAttenuation(d, uLightDist[i], uLightDecay[i]);
            vec3 irr = c * max(dot(n, l), 0.0);
            direct += irr;
            if (uType == 2) spec += irr * brdfGGX(l, v, n, f0, rough);
        }

        vec3 emissive = uEmissive;
        if (uHasEmissiveMap == 1) emissive *= texture(uEmissiveMap, vUV).rgb;
        outgoing = albedo * (direct + irradiance) + spec + emissive;
    }

    if (uFog == 1) outgoing = mix(outgoing, uFogColor, smoothstep(uFogNear, uFogFar, vFogDepth));
    fragColor = vec4(outgoing, diffuse.a);
}
