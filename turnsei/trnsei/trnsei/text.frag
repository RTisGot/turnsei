#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;     // フォントのアルファマップ
uniform vec3 textColor;     // 文字の色（プログラムから指定）

void main() {    
    // 赤色チャネル(r)をアルファ値として取得
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}