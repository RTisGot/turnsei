#pragma once
#include <glew.h>
#include <glm/glm.hpp>

class ImportedModel;

struct WalkAnimState {
    float time = 0.0f;
    float blend = 0.0f;
};

void InitActorRenderer();
GLuint GetActorShader();

void DrawActor(
    const glm::vec3& position,
    const glm::vec3& scale,
    const glm::vec3& color,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition,
    float rotationY,
    const ImportedModel* importedModel = nullptr,
    bool isWalking = false,
    WalkAnimState* walkAnim = nullptr);

void UpdateWalkAnimation(WalkAnimState& state, float deltaTime, bool isWalking);
