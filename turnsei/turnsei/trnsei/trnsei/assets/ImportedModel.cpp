#include <glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "ImportedModel.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace
{
    std::string DirectoryOf(const std::string& path)//ファイルパスを受け取る
    {
        size_t slash = path.find_last_of("/\\"); //　/ または \が最後にあるか探す
        return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);//見つからなかった場合//見つかった場合
    }

    bool FileExistsLocal(const std::string& path)
    {
        FILE* file = nullptr;              //ポインタを作る
        fopen_s(&file, path.c_str(), "rb");//fileを開く//読み込み専用
        if (!file) return false;
        fclose(file);                      //fileを閉じる
        return true;
    }

    std::string ResolveTexturePath(const std::string& modelDirectory, const aiString& texturePath)
    {
        std::string path = texturePath.C_Str();
        if (path.empty() || path[0] == '*') return path;
        for (char& c : path) {
            if (c == '\\') c = '/';
        }
        if (FileExistsLocal(path)) return path;
        std::string joined = modelDirectory + path;
        if (FileExistsLocal(joined)) return joined;
        size_t slash = path.find_last_of('/');
        if (slash != std::string::npos) {
            std::string fileNameOnly = modelDirectory + path.substr(slash + 1);
            if (FileExistsLocal(fileNameOnly)) return fileNameOnly;
        }
        return joined;
    }

    GLuint CreateTextureFromPixels(unsigned char* pixels, int width, int height, int channels)
    {
        if (!pixels || width <= 0 || height <= 0) return 0;

        GLenum sourceFormat = channels == 4 ? GL_RGBA : GL_RGB;
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, sourceFormat, width, height, 0, sourceFormat, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }

    GLuint LoadTextureFile(const std::string& path)
    {
        stbi_set_flip_vertically_on_load(false);
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
        GLuint texture = CreateTextureFromPixels(pixels, width, height, channels);
        stbi_image_free(pixels);
        if (texture == 0) {
            std::cerr << "Could not load texture: " << path << std::endl;
        }
        return texture;
    }

    GLuint LoadEmbeddedTexture(const aiScene* scene, const aiString& texturePath)
    {
        std::string path = texturePath.C_Str();
        if (path.empty() || path[0] != '*') return 0;

        int textureIndex = std::atoi(path.c_str() + 1);
        if (textureIndex < 0 || static_cast<unsigned int>(textureIndex) >= scene->mNumTextures) return 0;

        const aiTexture* texture = scene->mTextures[textureIndex];
        if (!texture) return 0;

        if (texture->mHeight == 0) {
            int width = 0;
            int height = 0;
            int channels = 0;
            unsigned char* pixels = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(texture->pcData),
                static_cast<int>(texture->mWidth),
                &width,
                &height,
                &channels,
                0
            );
            GLuint textureId = CreateTextureFromPixels(pixels, width, height, channels);
            stbi_image_free(pixels);
            return textureId;
        }

        return CreateTextureFromPixels(
            reinterpret_cast<unsigned char*>(texture->pcData),
            static_cast<int>(texture->mWidth),
            static_cast<int>(texture->mHeight),
            4
        );
    }

    glm::vec3 ReadMaterialColor(const aiScene* scene, const aiMesh* mesh)
    {
        glm::vec3 color(0.85f, 0.82f, 0.76f);
        if (!scene->HasMaterials() || mesh->mMaterialIndex >= scene->mNumMaterials) {
            return color;
        }

        aiColor4D materialColor;
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &materialColor) ||
            AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &materialColor)) {
            color = glm::vec3(materialColor.r, materialColor.g, materialColor.b);
        }
        return glm::clamp(color, glm::vec3(0.02f), glm::vec3(1.0f));
    }

    glm::mat4 ToGlm(const aiMatrix4x4& value)
    {
        glm::mat4 result(1.0f);
        result[0][0] = value.a1; result[1][0] = value.a2; result[2][0] = value.a3; result[3][0] = value.a4;
        result[0][1] = value.b1; result[1][1] = value.b2; result[2][1] = value.b3; result[3][1] = value.b4;
        result[0][2] = value.c1; result[1][2] = value.c2; result[2][2] = value.c3; result[3][2] = value.c4;
        result[0][3] = value.d1; result[1][3] = value.d2; result[2][3] = value.d3; result[3][3] = value.d4;
        return result;
    }

    std::string ToLowerCopy(const std::string& text)
    {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return result;
    }

    GLuint ReadMaterialTexture(const aiScene* scene, const aiMesh* mesh, const std::string& modelDirectory)
    {
        if (!scene->HasMaterials() || mesh->mMaterialIndex >= scene->mNumMaterials) {
            return 0;
        }

        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturePath;
        if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) != AI_SUCCESS &&
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS) {
            return 0;
        }

        if (texturePath.length > 0 && texturePath.C_Str()[0] == '*') {
            return LoadEmbeddedTexture(scene, texturePath);
        }

        return LoadTextureFile(ResolveTexturePath(modelDirectory, texturePath));
    }
}

bool ImportedModel::load(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
        std::cerr << "Could not load model '" << filePath << "': "
            << importer.GetErrorString() << std::endl;
        return false;
    }

    std::vector<unsigned int> indices;
    std::vector<DrawPart> newDrawParts;
    std::map<std::string, int> boneLookup;
    bool sceneHasAnimation = scene->mNumAnimations > 0;
    sourceVertices.clear();
    gpuVertices.clear();
    bones.clear();
    nodes.clear();
    animations.clear();
    animated = false;
    activeAnimationIndex = 0;
    animationTimeSeconds = 0.0f;
    boundsMin = glm::vec3(std::numeric_limits<float>::max());
    boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    std::string modelDirectory = DirectoryOf(filePath);

    std::function<int(const aiNode*)> copyNode = [&](const aiNode* sourceNode) -> int {
        Node node{};
        node.name = sourceNode->mName.C_Str();
        node.transform = ToGlm(sourceNode->mTransformation);
        int index = static_cast<int>(nodes.size());
        nodes.push_back(node);
        for (unsigned int i = 0; i < sourceNode->mNumChildren; ++i) {
            int childIndex = copyNode(sourceNode->mChildren[i]);
            nodes[index].children.push_back(childIndex);
        }
        return index;
    };
    copyNode(scene->mRootNode);

    std::map<unsigned int, glm::mat4> meshGlobalTransforms;
    std::function<void(const aiNode*, const glm::mat4&)> findMeshTransforms =
        [&](const aiNode* node, const glm::mat4& parentGlobal) {
            glm::mat4 globalTransform = parentGlobal * ToGlm(node->mTransformation);
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                meshGlobalTransforms[node->mMeshes[i]] = globalTransform;
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                findMeshTransforms(node->mChildren[i], globalTransform);
            }
        };
    findMeshTransforms(scene->mRootNode, glm::mat4(1.0f));

    if (!meshGlobalTransforms.empty()) {
        worldTransform = meshGlobalTransforms.begin()->second;
    }
    else {
        worldTransform = ToGlm(scene->mRootNode->mTransformation);
    }
    worldTransformInverse = glm::inverse(worldTransform);
    worldNormalMatrix = glm::mat3(worldTransform);

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];

        unsigned int vertexOffset = static_cast<unsigned int>(sourceVertices.size());
        unsigned int indexOffset = static_cast<unsigned int>(indices.size());
        glm::vec3 materialColor = ReadMaterialColor(scene, mesh);
        GLuint textureId = ReadMaterialTexture(scene, mesh, modelDirectory);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            VertexSource vertex{};
            vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            if (mesh->HasNormals()) {
                vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            }
            else {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            vertex.color = materialColor;
            if (mesh->HasVertexColors(0)) {
                const aiColor4D& vertexColor = mesh->mColors[0][i];
                vertex.color = glm::vec3(vertexColor.r, vertexColor.g, vertexColor.b);
            }
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            sourceVertices.push_back(vertex);
            glm::vec3 worldPos = glm::vec3(worldTransform * glm::vec4(vertex.position, 1.0f));
            boundsMin = glm::min(boundsMin, worldPos);
            boundsMax = glm::max(boundsMax, worldPos);
        }

        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
            const aiBone* assimpBone = mesh->mBones[boneIndex];
            std::string boneName = assimpBone->mName.C_Str();
            int importedBoneIndex = 0;
            auto foundBone = boneLookup.find(boneName);
            if (foundBone == boneLookup.end()) {
                Bone bone{};
                bone.name = boneName;
                bone.offset = ToGlm(assimpBone->mOffsetMatrix);
                importedBoneIndex = static_cast<int>(bones.size());
                bones.push_back(bone);
                boneLookup[boneName] = importedBoneIndex;
            }
            else {
                importedBoneIndex = foundBone->second;
            }

            for (unsigned int weightIndex = 0; weightIndex < assimpBone->mNumWeights; ++weightIndex) {
                const aiVertexWeight& weight = assimpBone->mWeights[weightIndex];
                unsigned int sourceIndex = vertexOffset + weight.mVertexId;
                if (sourceIndex >= sourceVertices.size()) continue;

                VertexSource& vertex = sourceVertices[sourceIndex];
                int slot = -1;
                for (int i = 0; i < 4; ++i) {
                    if (vertex.boneWeights[i] <= 0.0f) {
                        slot = i;
                        break;
                    }
                }
                if (slot < 0) {
                    slot = 0;
                    for (int i = 1; i < 4; ++i) {
                        if (vertex.boneWeights[i] < vertex.boneWeights[slot]) slot = i;
                    }
                    if (vertex.boneWeights[slot] >= weight.mWeight) continue;
                }
                vertex.boneIds[slot] = importedBoneIndex;
                vertex.boneWeights[slot] = weight.mWeight;
            }
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(vertexOffset + face.mIndices[j]);
            }
        }

        DrawPart part{};
        part.indexCount = static_cast<GLsizei>(indices.size() - indexOffset);
        part.indexOffset = static_cast<GLsizei>(indexOffset);
        part.textureId = textureId;
        newDrawParts.push_back(part);
    }

    if (sourceVertices.empty() || indices.empty()) return false;

    for (VertexSource& vertex : sourceVertices) {
        float totalWeight = 0.0f;
        for (int i = 0; i < 4; ++i) totalWeight += vertex.boneWeights[i];
        if (totalWeight > 0.0001f) {
            for (int i = 0; i < 4; ++i) vertex.boneWeights[i] /= totalWeight;
        }
    }

    for (unsigned int animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex) {
        const aiAnimation* assimpAnimation = scene->mAnimations[animationIndex];
        if (!assimpAnimation) continue;

        AnimationClip clip{};
        clip.name = assimpAnimation->mName.length > 0
            ? assimpAnimation->mName.C_Str()
            : "Animation" + std::to_string(animationIndex);
        clip.duration = assimpAnimation->mDuration;
        clip.ticksPerSecond = assimpAnimation->mTicksPerSecond > 0.0 ? assimpAnimation->mTicksPerSecond : 25.0;
        for (unsigned int channelIndex = 0; channelIndex < assimpAnimation->mNumChannels; ++channelIndex) {
            const aiNodeAnim* assimpChannel = assimpAnimation->mChannels[channelIndex];
            Channel channel{};
            channel.nodeName = assimpChannel->mNodeName.C_Str();

            for (unsigned int i = 0; i < assimpChannel->mNumPositionKeys; ++i) {
                const aiVectorKey& key = assimpChannel->mPositionKeys[i];
                channel.positions.push_back({ key.mTime, glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            for (unsigned int i = 0; i < assimpChannel->mNumRotationKeys; ++i) {
                const aiQuatKey& key = assimpChannel->mRotationKeys[i];
                channel.rotations.push_back({ key.mTime, glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            for (unsigned int i = 0; i < assimpChannel->mNumScalingKeys; ++i) {
                const aiVectorKey& key = assimpChannel->mScalingKeys[i];
                channel.scales.push_back({ key.mTime, glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
            }
            clip.channels.push_back(channel);
        }
        animations.push_back(clip);
    }

    animated = !animations.empty() && !bones.empty();

    glm::vec3 size = boundsMax - boundsMin;
    float largestDimension = std::max(size.x, std::max(size.y, size.z));
    if (largestDimension <= 0.0001f) return false;

    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    if (ebo == 0) glGenBuffers(1, &ebo);

    indexCount = static_cast<GLsizei>(indices.size());
    drawParts = newDrawParts;
    gpuVertices.resize(sourceVertices.size());

    if (animated) {
        normalizationCenter = glm::vec3(0.0f);
        normalizationScale = 1.0f;
        for (size_t i = 0; i < sourceVertices.size(); ++i) {
            gpuVertices[i].position = sourceVertices[i].position;
            gpuVertices[i].normal = glm::normalize(sourceVertices[i].normal);
            gpuVertices[i].color = sourceVertices[i].color;
            gpuVertices[i].texCoords = sourceVertices[i].texCoords;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, gpuVertices.size() * sizeof(VertexGpu), gpuVertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, position)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, normal)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, color)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, texCoords)));
        glEnableVertexAttribArray(3);
        glBindVertexArray(0);

        updateAnimation(0.0f);

        glm::vec3 skinnedMin(std::numeric_limits<float>::max());
        glm::vec3 skinnedMax(std::numeric_limits<float>::lowest());
        for (const auto& v : gpuVertices) {
            skinnedMin = glm::min(skinnedMin, v.position);
            skinnedMax = glm::max(skinnedMax, v.position);
        }

        glm::vec3 skinnedSize = skinnedMax - skinnedMin;
        normalizationScale = std::max(skinnedSize.x, std::max(skinnedSize.y, skinnedSize.z));
        if (normalizationScale <= 0.0001f) return false;
        normalizationCenter = glm::vec3(
            (skinnedMin.x + skinnedMax.x) * 0.5f,
            skinnedMin.y,
            (skinnedMin.z + skinnedMax.z) * 0.5f
        );

        for (auto& v : gpuVertices) {
            v.position = (v.position - normalizationCenter) / normalizationScale;
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gpuVertices.size() * sizeof(VertexGpu), gpuVertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    else {
        normalizationCenter = glm::vec3(
            (boundsMin.x + boundsMax.x) * 0.5f,
            boundsMin.y,
            (boundsMin.z + boundsMax.z) * 0.5f
        );
        normalizationScale = largestDimension;
        for (size_t i = 0; i < sourceVertices.size(); ++i) {
            glm::vec3 worldPos = glm::vec3(worldTransform * glm::vec4(sourceVertices[i].position, 1.0f));
            gpuVertices[i].position = (worldPos - normalizationCenter) / normalizationScale;
            gpuVertices[i].normal = glm::normalize(worldNormalMatrix * sourceVertices[i].normal);
            gpuVertices[i].color = sourceVertices[i].color;
            gpuVertices[i].texCoords = sourceVertices[i].texCoords;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, gpuVertices.size() * sizeof(VertexGpu), gpuVertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, position)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, normal)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, color)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VertexGpu), reinterpret_cast<void*>(offsetof(VertexGpu, texCoords)));
        glEnableVertexAttribArray(3);
        glBindVertexArray(0);
    }

    std::cout << "Loaded Blender model: " << filePath
        << " (" << scene->mNumMeshes << " meshes"
        << (animated ? ", animated clips: " + std::to_string(animations.size()) : "")
        << ")" << std::endl;
    for (size_t i = 0; i < animations.size(); ++i) {
        std::cout << "  Animation[" << i << "]: \"" << animations[i].name
            << "\" duration=" << animations[i].duration << std::endl;
    }
    return true;
}

bool ImportedModel::loadAnimationsFrom(const std::string& filePath, const std::string& clipName)
{
    if (bones.empty()) {
        std::cerr << "loadAnimationsFrom: base model has no bones, load mesh first\n";
        return false;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
    );
    if (!scene || scene->mNumAnimations == 0) {
        std::cerr << "loadAnimationsFrom: no animations in " << filePath << std::endl;
        return false;
    }

    int added = 0;
    for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai) {
        const aiAnimation* a = scene->mAnimations[ai];
        if (!a) continue;

        AnimationClip clip{};
        // clipNameが指定されていればその名前を使う、なければファイル名から生成
        if (!clipName.empty()) {
            clip.name = ai == 0 ? clipName : clipName + std::to_string(ai);
        } else {
            clip.name = a->mName.length > 0 ? a->mName.C_Str()
                                             : "Anim_" + std::to_string(animations.size() + ai);
        }
        clip.duration = a->mDuration;
        clip.ticksPerSecond = a->mTicksPerSecond > 0.0 ? a->mTicksPerSecond : 25.0;

        for (unsigned int ci = 0; ci < a->mNumChannels; ++ci) {
            const aiNodeAnim* ch = a->mChannels[ci];
            Channel channel{};
            channel.nodeName = ch->mNodeName.C_Str();
            for (unsigned int i = 0; i < ch->mNumPositionKeys; ++i) {
                const aiVectorKey& k = ch->mPositionKeys[i];
                channel.positions.push_back({ k.mTime, glm::vec3(k.mValue.x, k.mValue.y, k.mValue.z) });
            }
            for (unsigned int i = 0; i < ch->mNumRotationKeys; ++i) {
                const aiQuatKey& k = ch->mRotationKeys[i];
                channel.rotations.push_back({ k.mTime, glm::quat(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z) });
            }
            for (unsigned int i = 0; i < ch->mNumScalingKeys; ++i) {
                const aiVectorKey& k = ch->mScalingKeys[i];
                channel.scales.push_back({ k.mTime, glm::vec3(k.mValue.x, k.mValue.y, k.mValue.z) });
            }
            clip.channels.push_back(channel);
        }
        animations.push_back(clip);
        ++added;
        std::cout << "  Loaded animation \"" << clip.name << "\" from " << filePath
                  << " (duration=" << clip.duration << ")\n";
    }

    animated = !animations.empty() && !bones.empty();
    return added > 0;
}

bool ImportedModel::playAnimationByIndex(size_t index)
{
    if (!animated || index >= animations.size()) return false;
    if (activeAnimationIndex != index) {
        activeAnimationIndex = index;
        animationTimeSeconds = 0.0f;
    }
    return true;
}

std::string ImportedModel::getAnimationName(size_t index) const
{
    if (index >= animations.size()) return "";
    return animations[index].name;
}

int ImportedModel::findAnimationByKeywords(const std::vector<std::string>& keywords) const
{
    if (!animated) return -1;
    for (const auto& kw : keywords) {
        std::string lowKw;
        for (char c : kw) lowKw += (char)std::tolower((unsigned char)c);
        for (size_t i = 0; i < animations.size(); ++i) {
            std::string lowName;
            for (char c : animations[i].name) lowName += (char)std::tolower((unsigned char)c);
            if (lowName.find(lowKw) != std::string::npos) return (int)i;
        }
    }
    return -1;
}

bool ImportedModel::playAnimationByName(const std::string& keyword)
{
    if (!animated || keyword.empty()) return false;

    std::string loweredKeyword;
    for (char c : keyword) loweredKeyword += (char)std::tolower((unsigned char)c);
    for (size_t i = 0; i < animations.size(); ++i) {
        std::string lowName;
        for (char c : animations[i].name) lowName += (char)std::tolower((unsigned char)c);
        if (lowName.find(loweredKeyword) != std::string::npos) {
            return playAnimationByIndex(i);
        }
    }
    return false;
}

void ImportedModel::resetAnimationPose()
{
    if (!animated || animations.empty()) return;
    animationTimeSeconds = 0.0f;
    updateAnimation(0.0f);
}

void ImportedModel::updateAnimation(float deltaSeconds)
{
    if (!animated || animations.empty() || sourceVertices.empty()) return;

    animationTimeSeconds += std::max(0.0f, deltaSeconds);
    if (activeAnimationIndex >= animations.size()) activeAnimationIndex = 0;
    const AnimationClip& clip = animations[activeAnimationIndex];
    if (clip.duration <= 0.0 || clip.ticksPerSecond <= 0.0) return;

    double animationTicks = std::fmod(animationTimeSeconds * clip.ticksPerSecond, clip.duration);

    auto interpolateVec = [animationTicks](const std::vector<VecKey>& keys, const glm::vec3& fallback) {
        if (keys.empty()) return fallback;
        if (keys.size() == 1) return keys[0].value;

        size_t index = keys.size() - 2;
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (animationTicks < keys[i + 1].time) {
                index = i;
                break;
            }
        }
        size_t nextIndex = std::min(index + 1, keys.size() - 1);
        double frameDelta = keys[nextIndex].time - keys[index].time;
        float factor = frameDelta > 0.0 ? static_cast<float>((animationTicks - keys[index].time) / frameDelta) : 0.0f;
        factor = std::max(0.0f, std::min(factor, 1.0f));
        return glm::mix(keys[index].value, keys[nextIndex].value, factor);
    };

    auto interpolateQuat = [animationTicks](const std::vector<QuatKey>& keys) {
        if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (keys.size() == 1) return glm::normalize(keys[0].value);

        size_t index = keys.size() - 2;
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            if (animationTicks < keys[i + 1].time) {
                index = i;
                break;
            }
        }
        size_t nextIndex = std::min(index + 1, keys.size() - 1);
        double frameDelta = keys[nextIndex].time - keys[index].time;
        float factor = frameDelta > 0.0 ? static_cast<float>((animationTicks - keys[index].time) / frameDelta) : 0.0f;
        factor = std::max(0.0f, std::min(factor, 1.0f));
        return glm::normalize(glm::slerp(keys[index].value, keys[nextIndex].value, factor));
    };

    auto findChannel = [&clip](const std::string& nodeName) -> const Channel* {
        for (const Channel& channel : clip.channels) {
            if (channel.nodeName == nodeName) return &channel;
        }
        return nullptr;
    };

    auto findBone = [this](const std::string& boneName) -> int {
        for (size_t i = 0; i < bones.size(); ++i) {
            if (bones[i].name == boneName) return static_cast<int>(i);
        }
        return -1;
    };

    std::function<void(int, const glm::mat4&)> updateNode = [&](int nodeIndex, const glm::mat4& parentTransform) {
        const Node& node = nodes[nodeIndex];
        glm::mat4 localTransform = node.transform;
        if (const Channel* channel = findChannel(node.name)) {
            glm::vec3 translation = interpolateVec(channel->positions, glm::vec3(0.0f));
            glm::quat rotation = interpolateQuat(channel->rotations);
            glm::vec3 scale = interpolateVec(channel->scales, glm::vec3(1.0f));
            localTransform =
                glm::translate(glm::mat4(1.0f), translation) *
                glm::mat4_cast(rotation) *
                glm::scale(glm::mat4(1.0f), scale);
        }

        glm::mat4 globalTransform = parentTransform * localTransform;
        int boneIndex = findBone(node.name);
        if (boneIndex >= 0) {
            bones[boneIndex].finalTransform = globalTransform * bones[boneIndex].offset;
        }

        for (int childIndex : node.children) {
            updateNode(childIndex, globalTransform);
        }
    };

    if (!nodes.empty()) {
        updateNode(0, glm::mat4(1.0f));
    }

    for (size_t vertexIndex = 0; vertexIndex < sourceVertices.size(); ++vertexIndex) {
        const VertexSource& source = sourceVertices[vertexIndex];
        glm::vec4 skinnedPosition(0.0f);
        glm::vec3 skinnedNormal(0.0f);
        float totalWeight = 0.0f;

        for (int i = 0; i < 4; ++i) {
            int boneIndex = source.boneIds[i];
            float weight = source.boneWeights[i];
            if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()) || weight <= 0.0f) continue;

            const glm::mat4& transform = bones[boneIndex].finalTransform;
            skinnedPosition += transform * glm::vec4(source.position, 1.0f) * weight;
            skinnedNormal += glm::mat3(transform) * source.normal * weight;
            totalWeight += weight;
        }

        if (totalWeight <= 0.0001f) {
            skinnedPosition = worldTransform * glm::vec4(source.position, 1.0f);
            skinnedNormal = worldNormalMatrix * source.normal;
        }

        gpuVertices[vertexIndex].position = (glm::vec3(skinnedPosition) - normalizationCenter) / normalizationScale;
        gpuVertices[vertexIndex].normal = glm::normalize(skinnedNormal);
        gpuVertices[vertexIndex].color = source.color;
        gpuVertices[vertexIndex].texCoords = source.texCoords;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, gpuVertices.size() * sizeof(VertexGpu), gpuVertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ImportedModel::draw() const
{
    if (!isLoaded()) return;
    glBindVertexArray(vao);

    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    GLint useTextureLocation = currentProgram ? glGetUniformLocation(static_cast<GLuint>(currentProgram), "useTexture") : -1;
    GLint diffuseTextureLocation = currentProgram ? glGetUniformLocation(static_cast<GLuint>(currentProgram), "diffuseTexture") : -1;
    if (diffuseTextureLocation >= 0) glUniform1i(diffuseTextureLocation, 0);

    if (drawParts.empty()) {
        if (useTextureLocation >= 0) glUniform1i(useTextureLocation, 0);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }
    else {
        for (const DrawPart& part : drawParts) {
            if (part.textureId != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, part.textureId);
                if (useTextureLocation >= 0) glUniform1i(useTextureLocation, 1);
            }
            else if (useTextureLocation >= 0) {
                glUniform1i(useTextureLocation, 0);
            }
            glDrawElements(
                GL_TRIANGLES,
                part.indexCount,
                GL_UNSIGNED_INT,
                reinterpret_cast<void*>(static_cast<size_t>(part.indexOffset) * sizeof(unsigned int))
            );
        }
    }

    if (useTextureLocation >= 0) glUniform1i(useTextureLocation, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
