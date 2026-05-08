#include "Field.h"
#include "player.h"
#include "../../assets/Wetlandloder.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// どこか（Field.cppなど）でPlayerクラスのインスタンスを持っている前提
Player player;
Shader* fieldShader;
extern GLFWwindow* window;
float delta = 0.0f;

void FieldInit() {
    if (fieldShader == nullptr) {
        fieldShader = new Shader("Shader.vert", "Shader.frag");
    }
    fieldShader->use();
}

void FieldUpdate() {
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    float deltaTime = getDeltaTime(); // 経過時間を取得
   
    // 1. プレイヤーを動かす
    player.Update(deltaTime, window);

   

    // カメラ行列（playerの視点、あるいは固定カメラ）
    glm::mat4 view = glm::lookAt(glm::vec3(0, 5, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    // 画面の比率（ウィンドウサイズに合わせて調整）
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    if (fieldShader) {
        fieldShader->use();
        // 湿地帯を描画
        DrawWetland(*fieldShader, view, projection);

        // プレイヤーを描画
        // player.Draw の中でも view と projection を設定するようにしてください
        player.Draw(fieldShader->ID, view, projection);
    }

}

float getDeltaTime() {
    // 時間を計算して返す処理
    return delta;
}