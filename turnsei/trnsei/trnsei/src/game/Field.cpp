#include "Field.h"
#include "player.h"
#include <iostream>
// どこか（Field.cppなど）でPlayerクラスのインスタンスを持っている前提
Player player;
extern GLFWwindow* window;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
void FieldUpdate() {
    float deltaTime = getDeltaTime(); // 経過時間を取得

    // 1. プレイヤーを動かす
    player.Update(deltaTime, window);
}

float getDeltaTime() {
    return deltaTime;
}