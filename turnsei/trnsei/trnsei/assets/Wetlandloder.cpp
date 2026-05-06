//assimpｓｗblender空の読み込み
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <iostream>

// main ではなく、呼び出し可能な関数名（LoadWetlandなど）に変える
bool LoadWetland(const std::string& filePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, 0);

    if (!scene) {
        return false;
    }
    std::cout << "湿地帯の読み込みに成功しました！" << std::endl;
    return true;
}