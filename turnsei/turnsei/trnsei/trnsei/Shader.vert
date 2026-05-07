#version 330 core
layout (location = 0) in vec3 aPos;       // 座標
layout (location = 1) in vec3 aNormal;    // 法線
layout (location = 2) in vec2 aTexCoords; // UV座標

out vec2 TexCoords;

uniform mat4 model;      // モデル行列
uniform mat4 view;       // ビュー行列（カメラ）
uniform mat4 projection; // 投影行列

void main()
{
    TexCoords = aTexCoords;
    // 3D空間上の位置を計算
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}