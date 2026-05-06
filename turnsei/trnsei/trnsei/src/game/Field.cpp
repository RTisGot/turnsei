#include "Field.h"
#include "player.h"
#include <iostream>
#include <glfw3.h>
// どこか（Field.cppなど）でPlayerクラスのインスタンスを持っている前提
Player player;
extern GLFWwindow* window;
void FieldUpdate() {
    float deltaTime = getDeltaTime(); // 経過時間を取得

    // 1. プレイヤーを動かす
    player.Update(deltaTime, window);

    // 2. 湿地帯を描画する
    wetlandModel.Draw(fieldShader);

    // 3. プレイヤー（立方体）を描画する
    player.Draw(fieldShader);

    // 4. カメラをプレイヤーの背後に持ってくる（3人称視点）
    UpdateThirdPersonCamera(camera, player);
}