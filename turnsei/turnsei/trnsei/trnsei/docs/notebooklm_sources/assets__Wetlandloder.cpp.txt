#include <glew.h>
#include <GLFW/glfw3.h>

#include "Wetlandloder.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
    };

    GLuint wetlandVao = 0;
    GLuint wetlandVbo = 0;
    GLuint wetlandEbo = 0;
    float wetlandAnimationTime = 0.0f;
}

unsigned int indexCount = 0;

bool LoadWetland(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices
    );

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
        std::cerr << "Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        unsigned int vertexOffset = static_cast<unsigned int>(vertices.size());

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex vertex{};
            vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            boundsMin = glm::min(boundsMin, vertex.position);
            boundsMax = glm::max(boundsMax, vertex.position);
            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            if (mesh->mTextureCoords[0]) {
                vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(vertexOffset + face.mIndices[j]);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        std::cerr << "Wetland model has no drawable mesh data." << std::endl;
        return false;
    }

    glm::vec3 size = boundsMax - boundsMin;
    float horizontalSize = std::max(size.x, size.z);
    float scale = horizontalSize > 0.0001f ? 18.0f / horizontalSize : 1.0f;
    glm::vec3 center(
        (boundsMin.x + boundsMax.x) * 0.5f,
        boundsMin.y,
        (boundsMin.z + boundsMax.z) * 0.5f
    );
    for (Vertex& vertex : vertices) {
        vertex.position = (vertex.position - center) * scale;
    }

    indexCount = static_cast<unsigned int>(indices.size());
    if (wetlandVao == 0) glGenVertexArrays(1, &wetlandVao);
    if (wetlandVbo == 0) glGenBuffers(1, &wetlandVbo);
    if (wetlandEbo == 0) glGenBuffers(1, &wetlandEbo);

    glBindVertexArray(wetlandVao);
    glBindBuffer(GL_ARRAY_BUFFER, wetlandVbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wetlandEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoords)));
    glBindVertexArray(0);

    std::cout << "Wetland loaded: " << filePath
        << " (" << scene->mNumMeshes << " meshes, "
        << indexCount << " indices)" << std::endl;
    return true;
}

void UpdateWetlandAnimation(float deltaSeconds)
{
    wetlandAnimationTime += std::max(0.0f, deltaSeconds);
    if (wetlandAnimationTime > 10000.0f) {
        wetlandAnimationTime = std::fmod(wetlandAnimationTime, 10000.0f);
    }
}

void DrawWetland(Shader& shader, glm::mat4 view, glm::mat4 projection)
{
    if (wetlandVao == 0 || indexCount == 0) return;

    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("model", glm::mat4(1.0f));
    shader.setMat4("view", view);
    shader.setVec3("baseColor", glm::vec3(0.22f, 0.48f, 0.28f));
    shader.setVec3("lightDir", glm::vec3(-0.35f, 0.85f, 0.30f));
    glUniform1f(glGetUniformLocation(shader.ID, "uTime"), wetlandAnimationTime);
    glUniform1f(glGetUniformLocation(shader.ID, "uMapAnimationStrength"), 1.0f);

    glBindVertexArray(wetlandVao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
