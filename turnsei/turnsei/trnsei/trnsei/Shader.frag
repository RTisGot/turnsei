#version 330 core

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;
in float MapMotion;
out vec4 FragColor;

uniform vec3 baseColor;
uniform vec3 lightDir;
uniform float uTime;

vec3 ACESFilm(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) /
                 (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 n = normalize(Normal);
    vec3 l = normalize(lightDir);
    vec3 viewDir = normalize(vec3(-0.25, 0.45, 0.85));
    float ndl = max(dot(n, l), 0.0);
    float hemi = max(n.y, 0.0);
    float lit = max(ndl, hemi * 0.70);

    float band = 0.40
        + smoothstep(0.22, 0.38, lit) * 0.20
        + smoothstep(0.50, 0.68, lit) * 0.24
        + smoothstep(0.80, 0.94, lit) * 0.20;

    float inkPattern = 0.018 * sin(TexCoords.x * 34.0 + uTime * 0.65) * cos(TexCoords.y * 31.0 - uTime * 0.42);
    vec3 shadowTint = vec3(0.16, 0.25, 0.34);
    vec3 lightTint = vec3(1.02, 1.00, 0.91);
    vec3 color = mix(baseColor * shadowTint, baseColor * lightTint, band) + inkPattern;
    color += vec3(0.06, 0.17, 0.16) * MapMotion;

    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.4);
    color += vec3(0.18, 0.32, 0.42) * smoothstep(0.36, 0.86, rim);
    color = mix(vec3(dot(color, vec3(0.299, 0.587, 0.114))), color, 1.12);
    color = pow(ACESFilm(max(color, vec3(0.0)) * 1.05),
                vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
