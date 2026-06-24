#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

void main()
{
    vec3 groundColor = vec3(0.18, 0.42, 0.24);
    float variation = 0.92 + 0.08 * sin(TexCoords.x * 18.0) * cos(TexCoords.y * 18.0);
    FragColor = vec4(groundColor * variation, 1.0);
}
