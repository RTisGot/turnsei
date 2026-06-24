#pragma once

#include <glew.h>
#include <string>

class ImportedModel
{
public:
    bool load(const std::string& filePath);
    void draw() const;
    bool isLoaded() const { return vao != 0 && indexCount > 0; }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};
