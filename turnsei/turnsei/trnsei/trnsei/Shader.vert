#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Normal;
out float MapMotion;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float uTime;
uniform float uMapAnimationStrength;

void main()
{
    TexCoords = aTexCoords;
    vec3 animatedPos = aPos;
    float lowSurface = 1.0 - smoothstep(0.05, 0.65, aPos.y);
    float tallSurface = smoothstep(0.20, 1.20, aPos.y);
    float ripple = sin(aPos.x * 2.4 + uTime * 1.7) * cos(aPos.z * 2.1 - uTime * 1.35);
    float sway = sin(uTime * 1.25 + aPos.x * 0.75 + aPos.z * 0.55);

    animatedPos.y += ripple * lowSurface * 0.035 * uMapAnimationStrength;
    animatedPos.xz += vec2(sway, cos(uTime * 1.05 + aPos.z * 0.65)) * tallSurface * 0.025 * uMapAnimationStrength;
    MapMotion = max(lowSurface * abs(ripple), tallSurface * abs(sway));

    vec4 world = model * vec4(animatedPos, 1.0);
    WorldPos = world.xyz;
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * world;
}
