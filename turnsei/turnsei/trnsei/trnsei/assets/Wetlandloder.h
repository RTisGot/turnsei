#pragma once
#include <glm/glm.hpp>
#include"Shader.h"
void DrawWetland(Shader& shader, glm::mat4, glm::mat4);

bool LoadWetland(const std::string& filePath);
extern unsigned int indexCount;