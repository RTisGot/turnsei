#include "Player.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
    float NormalizeAngle(float angle)
    {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }
}

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

void Player::Draw(GLuint shaderProgram,glm::mat4 view,glm::mat4 projection) {
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

void Player::Update(float deltaTime, GLFWwindow* window, float cameraYaw) {
    if (window == nullptr) {
        moving = false;
        moveVelocity = glm::vec3(0.0f);
        return;
    }

    float moveSpeed = 5.0f;
    glm::vec3 inputDir(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputDir.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputDir.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputDir.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputDir.x += 1.0f;

    if (glm::length(inputDir) > 0.0f) {
        inputDir = glm::normalize(inputDir);
    }

    bool hasInput = glm::length(inputDir) > 0.0f;
    float currentSpeed = glm::length(moveVelocity);

    if (hasInput) {
        float camForwardX = -std::sin(cameraYaw);
        float camForwardZ = -std::cos(cameraYaw);
        float camRightX = -camForwardZ;
        float camRightZ = camForwardX;

        glm::vec3 worldDir(
            camForwardX * (-inputDir.z) + camRightX * inputDir.x,
            0.0f,
            camForwardZ * (-inputDir.z) + camRightZ * inputDir.x
        );

        if (glm::length(worldDir) > 0.001f) {
            worldDir = glm::normalize(worldDir);
        }

        float targetRotationY = glm::degrees(std::atan2(worldDir.x, worldDir.z));
        float angleDelta = NormalizeAngle(targetRotationY - rotationY);
        float maxTurn = 720.0f * deltaTime;
        if (angleDelta > maxTurn) angleDelta = maxTurn;
        if (angleDelta < -maxTurn) angleDelta = -maxTurn;
        rotationY = NormalizeAngle(rotationY + angleDelta);
    }

    float targetSpeed = hasInput ? moveSpeed : 0.0f;
    float response = hasInput ? 10.0f : 14.0f;
    float blend = 1.0f - std::exp(-response * deltaTime);
    currentSpeed += (targetSpeed - currentSpeed) * blend;

    if (currentSpeed > 0.02f) {
        float forwardAngle = glm::radians(rotationY);
        glm::vec3 moveDir(std::sin(forwardAngle), 0.0f, std::cos(forwardAngle));
        moveVelocity = moveDir * currentSpeed;
        position += moveVelocity * deltaTime;
        moving = currentSpeed > 0.12f;
    }
    else {
        moveVelocity = glm::vec3(0.0f);
        moving = false;
    }
}
