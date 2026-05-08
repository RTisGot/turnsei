#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "../../assets/Shader.h"

extern Shader* fieldShader;
float getDeltaTime();
void FieldUpdate();
void FieldInit();
