#include "Player.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


void Player::Init() {
    // ここに立方体の頂点データを定義（static等にしてファイル内に隠蔽）
  // 頂点データ (Position: 3, Color: 3)
    float vertices[] = {
        // 座標 (x, y, z)          // 色 (r, g, b)
        // 底面 (y=0)
        -0.5f,  0.0f, -0.5f,  0.4f, 0.2f, 0.1f, // 0
         0.5f,  0.0f, -0.5f,  0.4f, 0.2f, 0.1f, // 1
         0.5f,  0.0f,  0.5f,  0.4f, 0.2f, 0.1f, // 2
        -0.5f,  0.0f,  0.5f,  0.4f, 0.2f, 0.1f, // 3

        // 上面 (y=2.0 人の高さ)
        -0.5f,  2.0f, -0.5f,  0.8f, 0.4f, 0.2f, // 4
         0.5f,  2.0f, -0.5f,  0.8f, 0.4f, 0.2f, // 5
         0.5f,  2.0f,  0.5f,  0.8f, 0.4f, 0.2f, // 6
        -0.5f,  2.0f,  0.5f,  0.8f, 0.4f, 0.2f  // 7
    };

    // インデックスデータ（三角形の組み合わせ）
    unsigned int indices[] = {
        // 底面
        0, 1, 2,  2, 3, 0,
        // 前面
        3, 2, 6,  6, 7, 3,
        // 右側面
        2, 1, 5,  5, 6, 2,
        // 背面
        1, 0, 4,  4, 5, 1,
        // 左側面
        0, 3, 7,  7, 4, 0,
        // 上面
        4, 5, 6,  6, 7, 4
    };

    // OpenGLのバッファ生成（glGenVertexArraysなど）
    // ...
    this->indexCount = 36; // 立方体の全インデックス数
}

void Player::Draw(GLuint shaderProgram) {
    glUseProgram(shaderProgram);

    // プレイヤーの位置と回転を反映した行列を作る
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationY), glm::vec3(0, 1, 0));
    model = glm::scale(model, glm::vec3(1.0f, 2.0f, 1.0f)); // 身代わりなので少し縦長に

    GLuint modelLoc = glGetUniformLocation(shaderProgram, "model");
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

void Player::Update(float deltaTime, GLFWwindow* window) {
    float moveSpeed = 5.0f; // 移動速度
    glm::vec3 moveDir(0.0f);

    // WASDキーの入力判定
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir.x += 1.0f;

    // 斜め移動でも速くならないように正規化
    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
        position += moveDir * moveSpeed * deltaTime;
    }
}