#include "Field.h"
#include "player.h"
#include "../../assets/Wetlandloder.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>


Player player;
Shader* fieldShader;
extern GLFWwindow* window;
float delta = 0.0f;

void FieldInit() {
    std::cout << "[DEBUG 1] FieldInit started" << std::endl;
    // 1. シェーダーの初期化
    if (fieldShader == nullptr) {
        fieldShader = new Shader("Shader.vert", "Shader.frag");
    }

    //  モデルの読み込み
    bool success = LoadWetland("../trnsei/Resource/21644_autosave.obj");

    if (!success) {
        std::cerr << "湿地帯モデルの読み込みに失敗しました！" << std::endl;
    }
    else
    {
		std::cout << "湿地帯モデルの読み込みに成功しました！" << std::endl;
    }
    fieldShader->use();
}

void FieldUpdate() {
    std::cout << "DEBUG: Drawing Index Count =" << indexCount << std::endl;
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float deltaTime = getDeltaTime(); // 経過時間を取得
   
    // 1. プレイヤーを動かす
    player.Update(deltaTime, window);


    // カメラ行列
    glm::mat4 view = glm::lookAt(glm::vec3(0, 500, 500), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    // 画面の比率
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    //シェーダー
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