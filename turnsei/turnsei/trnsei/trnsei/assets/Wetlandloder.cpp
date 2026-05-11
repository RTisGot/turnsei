//assimpｓｗblender空の読み込み

#include<glew.h>
#include<GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include "Wetlandloder.h"

class Shader;
struct Vertex {
    // 座標 (x, y, z)
    glm::vec3 Position;
    // 法線 (向き: ライティング計算に必要)
    glm::vec3 Normal;
    // テクスチャ座標 (u, v)
    glm::vec2 TexCoords;
};

// 描画に必要なID
unsigned int VAO, VBO, EBO;
unsigned int indexCount= 0;

// main ではなく、呼び出し可能な関数名（LoadWetlandなど）に変える
bool LoadWetland(const std::string& filePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate |      // ポリゴンを三角形に強制
        aiProcess_FlipUVs |          // UVをOpenGL用に反転
        aiProcess_CalcTangentSpace   // 法線マップ用計算
    );

    if (!scene || !scene->mRootNode) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    //湿地帯を取得
    aiMesh* mesh = scene->mMeshes[0];
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // 1. 頂点データを抽出
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        if (mesh->mTextureCoords[0]) {
            vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        vertices.push_back(vertex);
    }

    // 2. インデックス（三角形のつなぎ順）を抽出
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }
    indexCount = static_cast<unsigned int>(indices.size());
    std::cout << "[CHECK] indices vector size: " << indices.size() << std::endl;

    // グローバル変数の indexCount に代入
    indexCount = static_cast<unsigned int>(indices.size());

    std::cout << "[CHECK] global indexCount is now: " << indexCount << std::endl;
    return true;

    // 3. OpenGLのバッファ（VAO/VBO/EBO）を作成
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // 頂点属性の設定（座標、法線、UV）
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
    return true;
}

void DrawWetland(Shader& shader, glm::mat4 view, glm::mat4 projection) {
    shader.use(); // glUseProgram

    // 1. 各種行列をシェーダーに転送
    glm::mat4 model = glm::mat4(1.0f); // 湿地帯は原点に配置
    shader.setMat4("projection", projection);
    shader.setMat4("model", model);
    shader.setMat4("view", view);
   

    // 2. 描画
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}