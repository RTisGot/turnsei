#define GLEW_STATIC
#include <glew.h>

#include "ImportedModel.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

bool ImportedModel::load(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_PreTransformVertices |
        aiProcess_GenSmoothNormals
    );

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
        std::cerr << "Could not load model '" << filePath << "': "
            << importer.GetErrorString() << std::endl;
        return false;
    }

    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(std::numeric_limits<float>::lowest());

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        unsigned int vertexOffset = static_cast<unsigned int>(vertices.size());

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            glm::vec3 position(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            vertices.push_back(position);
            boundsMin = glm::min(boundsMin, position);
            boundsMax = glm::max(boundsMax, position);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(vertexOffset + face.mIndices[j]);
            }
        }
    }

    if (vertices.empty() || indices.empty()) return false;

    glm::vec3 size = boundsMax - boundsMin;
    float largestDimension = std::max(size.x, std::max(size.y, size.z));
    if (largestDimension <= 0.0001f) return false;

    glm::vec3 center(
        (boundsMin.x + boundsMax.x) * 0.5f,
        boundsMin.y,
        (boundsMin.z + boundsMax.z) * 0.5f
    );
    for (glm::vec3& vertex : vertices) {
        vertex = (vertex - center) / largestDimension;
    }

    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    if (ebo == 0) glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    indexCount = static_cast<GLsizei>(indices.size());
    std::cout << "Loaded Blender model: " << filePath
        << " (" << scene->mNumMeshes << " meshes)" << std::endl;
    return true;
}

void ImportedModel::draw() const
{
    if (!isLoaded()) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}
