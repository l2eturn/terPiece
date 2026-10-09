#version 410 core
// Fireplace particles of the old Christmas room (home.js getParticleSystem)
layout(location = 0) in vec3 aPos;
layout(location = 8) in float size;
layout(location = 9) in float angle;
layout(location = 10) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec2 uResolution;

out vec4 vColor;
out vec2 vAngle;

void main() {
    vec4 mv = uView * uModel * vec4(aPos, 1.0);
    gl_Position = uProj * mv;
    // pointMultiplier = innerHeight / (2 * tan(30 deg))
    float pointMultiplier = uResolution.y / (2.0 * tan(radians(30.0)));
    gl_PointSize = size * pointMultiplier / gl_Position.w;
    vAngle = vec2(cos(angle), sin(angle));
    vColor = aColor;
}
