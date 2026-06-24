#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "../../assets/Shader.h"

class CombatSystem;

extern Shader* fieldShader;
float getDeltaTime();
void FieldUpdate(CombatSystem& combatSystem);
void FieldInit();
