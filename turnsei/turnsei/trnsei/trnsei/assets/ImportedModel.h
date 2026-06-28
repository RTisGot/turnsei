#pragma once

#include <glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstddef>
#include <string>
#include <vector>

class ImportedModel
{
public:
    bool load(const std::string& filePath);
    void updateAnimation(float deltaSeconds);
    bool playAnimationByName(const std::string& keyword);
    bool playAnimationByIndex(size_t index);
    void draw() const;
    bool isLoaded() const { return vao != 0 && indexCount > 0; }
    bool hasAnimation() const { return animated; }
    size_t getAnimationCount() const { return animations.size(); }

private:
    struct DrawPart
    {
        GLsizei indexCount = 0;
        GLsizei indexOffset = 0;
        GLuint textureId = 0;
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    std::vector<DrawPart> drawParts;

    struct VertexGpu
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 texCoords;
    };

    struct VertexSource
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 texCoords;
        int boneIds[4] = { -1, -1, -1, -1 };
        float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct Bone
    {
        std::string name;
        glm::mat4 offset = glm::mat4(1.0f);
        glm::mat4 finalTransform = glm::mat4(1.0f);
    };

    struct Node
    {
        std::string name;
        glm::mat4 transform = glm::mat4(1.0f);
        std::vector<int> children;
    };

    struct VecKey
    {
        double time = 0.0;
        glm::vec3 value = glm::vec3(0.0f);
    };

    struct QuatKey
    {
        double time = 0.0;
        glm::quat value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };

    struct Channel
    {
        std::string nodeName;
        std::vector<VecKey> positions;
        std::vector<QuatKey> rotations;
        std::vector<VecKey> scales;
    };

    struct AnimationClip
    {
        std::string name;
        double duration = 0.0;
        double ticksPerSecond = 25.0;
        std::vector<Channel> channels;
    };

    bool animated = false;
    size_t activeAnimationIndex = 0;
    float animationTimeSeconds = 0.0f;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    glm::vec3 normalizationCenter = glm::vec3(0.0f);
    float normalizationScale = 1.0f;
    glm::mat4 rootInverse = glm::mat4(1.0f);
    std::vector<VertexSource> sourceVertices;
    std::vector<VertexGpu> gpuVertices;
    std::vector<Bone> bones;
    std::vector<Node> nodes;
    std::vector<AnimationClip> animations;
};
