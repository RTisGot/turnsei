#include "Field.h"
#include "player.h"
#include "../../assets/Wetlandloder.h"
#include "../../assets/Shader.h"
#include <iostream>

// どこか（Field.cppなど）でPlayerクラスのインスタンスを持っている前提
Player player;
extern Shader* fieldShader;
extern GLFWwindow* window;
void FieldUpdate() {
    float deltaTime = getDeltaTime(); // 経過時間を取得

   
    
    // 1. プレイヤーを動かす
    player.Update(deltaTime, window);

    // パスが合っていないと読み込みエラーになります
    fieldShader = new Shader("Shader.vert", "Shader.frag");
    fieldShader->use();

    // 2. 湿地帯を描画する
    DrawWetland(*fieldShader);

    // 3. プレイヤー（立方体）を描画する
    player.Draw(fieldShader->ID);
    

}