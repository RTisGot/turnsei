#pragma once
#include <glm/glm.hpp>

void InitSimpleMap();
void DrawSimpleMap(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos);
float GetTerrainHeight(float x, float z);
