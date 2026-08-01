#include "FieldEnemy.h"
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

void InitFieldEnemies(FieldEnemy* enemies)
{
    glm::vec3 spawns[] = {
        glm::vec3(-12.0f, 0.0f, -10.0f),
        glm::vec3(8.0f, 0.0f, -14.0f),
        glm::vec3(-6.0f, 0.0f, -20.0f),
        glm::vec3(15.0f, 0.0f, -5.0f),
    };
    for (int i = 0; i < MAX_FIELD_ENEMIES; ++i) {
        enemies[i].position = spawns[i];
        enemies[i].targetPos = spawns[i];
        enemies[i].wanderTimer = (float)(rand() % 300) / 100.0f;
        enemies[i].wanderInterval = 2.5f + (rand() % 200) / 100.0f;
        enemies[i].speed = 1.2f + (rand() % 100) / 100.0f;
        enemies[i].active = true;
    }
}

void UpdateFieldEnemies(FieldEnemy* enemies, float dt)
{
    for (int i = 0; i < MAX_FIELD_ENEMIES; ++i) {
        FieldEnemy& e = enemies[i];
        if (!e.active) continue;

        e.wanderTimer -= dt;
        if (e.wanderTimer <= 0.0f) {
            float angle = (float)(rand() % 628) / 100.0f;
            float dist = 3.0f + (float)(rand() % 500) / 100.0f;
            e.targetPos = e.position + glm::vec3(std::cos(angle) * dist, 0.0f, std::sin(angle) * dist);
            e.wanderTimer = e.wanderInterval + (float)(rand() % 200) / 100.0f;
        }

        glm::vec3 toTarget = e.targetPos - e.position;
        toTarget.y = 0.0f;
        float remainDist = glm::length(toTarget);
        if (remainDist > 0.3f) {
            glm::vec3 dir = toTarget / remainDist;
            e.position += dir * std::min(e.speed * dt, remainDist);
            e.rotationY = glm::degrees(std::atan2(dir.x, dir.z));
            e.moving = true;
        }
        else {
            e.moving = false;
        }
    }
}
