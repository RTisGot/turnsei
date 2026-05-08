// Fragment Shader
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D diffuseMap; // Blenderで保存した色画像
uniform sampler2D normalMap;  // 法線画像（凹凸）
uniform sampler2D roughnessMap; // 粗さ画像（濡れ具合）

void main() {
    // 色を取得
    vec3 color = texture(diffuseMap, TexCoords).rgb;
    
    //濡れ具合（Roughness）で反射を計算
    float gloss = 1.0 - texture(roughnessMap, TexCoords).r; 
    
    
    FragColor = vec4(color, 1.0); 
}