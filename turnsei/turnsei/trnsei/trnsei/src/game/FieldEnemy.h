#pragma once
#include <glm/glm.hpp>

struct FieldEnemy {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 targetPos = glm::vec3(0.0f);
    float rotationY = 0.0f;
    float wanderTimer = 0.0f;
    float wanderInterval = 3.0f;
    float speed = 1.5f;
    bool active = true;
    bool moving = false;
};

const int MAX_FIELD_ENEMIES = 4;

void InitFieldEnemies(FieldEnemy* enemies);
void UpdateFieldEnemies(FieldEnemy* enemies, float dt);
