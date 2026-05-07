#pragma once
#include <glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>

//Playerのクラス
class Player {
public:
    glm::vec3 position;
    float rotationY;

    // 初期化（ここで立方体のVBOなどを作る）
    void Init();
    // 描画
    void Draw(GLuint shaderProgram);

    // 毎フレームの更新（移動処理）
    void Update(float deltaTime, GLFWwindow* window);
private:
    // 将来的にFBXモデルのクラス（Model等）に差し替える部分
    GLuint vao, vbo, ebo;
    int indexCount;
};