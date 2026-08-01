#pragma once

#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    unsigned int ID;
    int success;

    Shader(const char* vertexPath, const char* fragmentPath);
    Shader() : ID(0), success(0) {}

    void use();

    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
};