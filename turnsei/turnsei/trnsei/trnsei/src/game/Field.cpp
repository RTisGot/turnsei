#include "Field.h"
#include "CombatSystem.h"
#include "Player.h"
#include "Scene.h"
#include "../../assets/Wetlandloder.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

Player player;
Shader* fieldShader = nullptr;
extern GLFWwindow* window;

namespace
{
    float deltaTime = 0.0f;
    float previousFrameTime = 0.0f;
    GLuint actorShader = 0;
    GLuint actorVao = 0;
    GLuint actorVbo = 0;
    GLuint actorEbo = 0;
    float cameraYaw = 0.0f;
    float cameraPitch = glm::radians(32.0f);
    float cameraDistance = 20.0f;
    bool enemyActive[] = { true, true, true, true, true };

    const glm::vec3 enemyPositions[] = {
        glm::vec3(-5.0f, 0.0f, -4.0f),
        glm::vec3(0.0f, 0.0f, -6.0f),
        glm::vec3(5.0f, 0.0f, -4.0f),
        glm::vec3(-3.0f, 0.0f, 4.0f),
        glm::vec3(4.0f, 0.0f, 3.0f)
    };

    bool FileExists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    GLuint CompileActorShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

    void InitActorRenderer()
    {
        if (actorShader != 0) return;

        const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";
        const char* fragmentSource = R"(
#version 330 core
uniform vec3 actorColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(actorColor, 1.0);
}
)";

        GLuint vertexShader = CompileActorShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileActorShader(GL_FRAGMENT_SHADER, fragmentSource);
        actorShader = glCreateProgram();
        glAttachShader(actorShader, vertexShader);
        glAttachShader(actorShader, fragmentShader);
        glLinkProgram(actorShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        const float vertices[] = {
            -0.5f, 0.0f, -0.5f,  0.5f, 0.0f, -0.5f,
             0.5f, 1.0f, -0.5f, -0.5f, 1.0f, -0.5f,
            -0.5f, 0.0f,  0.5f,  0.5f, 0.0f,  0.5f,
             0.5f, 1.0f,  0.5f, -0.5f, 1.0f,  0.5f
        };
        const unsigned int indices[] = {
            0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
            0, 4, 7, 7, 3, 0, 1, 5, 6, 6, 2, 1,
            3, 2, 6, 6, 7, 3, 0, 1, 5, 5, 4, 0
        };

        glGenVertexArrays(1, &actorVao);
        glGenBuffers(1, &actorVbo);
        glGenBuffers(1, &actorEbo);
        glBindVertexArray(actorVao);
        glBindBuffer(GL_ARRAY_BUFFER, actorVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, actorEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void DrawActor(
        const glm::vec3& position,
        const glm::vec3& scale,
        const glm::vec3& color,
        const glm::mat4& view,
        const glm::mat4& projection)
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);

        glUseProgram(actorShader);
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(actorShader, "actorColor"), 1, glm::value_ptr(color));
        glBindVertexArray(actorVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    void UpdateCameraInput()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            cameraYaw -= io.MouseDelta.x * 0.006f;
            cameraPitch -= io.MouseDelta.y * 0.006f;
            cameraPitch = std::max(glm::radians(12.0f), std::min(cameraPitch, glm::radians(75.0f)));
        }

        if (!io.WantCaptureMouse && io.MouseWheel != 0.0f) {
            cameraDistance -= io.MouseWheel * 1.4f;
            cameraDistance = std::max(6.0f, std::min(cameraDistance, 35.0f));
        }
    }

    bool StartEncounterIfNeeded(CombatSystem& combatSystem)
    {
        const float encounterDistance = 1.65f;
        for (size_t i = 0; i < sizeof(enemyPositions) / sizeof(enemyPositions[0]); ++i) {
            if (!enemyActive[i]) continue;

            glm::vec2 playerXZ(player.position.x, player.position.z);
            glm::vec2 enemyXZ(enemyPositions[i].x, enemyPositions[i].z);
            if (glm::distance(playerXZ, enemyXZ) <= encounterDistance) {
                enemyActive[i] = false;
                combatSystem.resetBattle();
                currentScene = Scene::Battle;
                return true;
            }
        }
        return false;
    }
}

void FieldInit()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    std::cout << "Field initialization started." << std::endl;
    fieldShader = new Shader("Shader.vert", "Shader.frag");

    const char* modelCandidates[] = {
        "Resource/21644_autosave_fixed.fbx",
        "../trnsei/Resource/21644_autosave_fixed.fbx",
        "trnsei/Resource/21644_autosave_fixed.fbx",
        "turnsei/turnsei/trnsei/trnsei/Resource/21644_autosave_fixed.fbx",
        "Resource/21644_autosave.fbx",
        "../trnsei/Resource/21644_autosave.fbx",
        "trnsei/Resource/21644_autosave.fbx",
        "turnsei/turnsei/trnsei/trnsei/Resource/21644_autosave.fbx"
    };

    bool loaded = false;
    for (const char* path : modelCandidates) {
        if (FileExists(path) && LoadWetland(path)) {
            loaded = true;
            break;
        }
    }

    if (!loaded) {
        std::cerr << "Wetland model not found. Put 21644_autosave.fbx in Resource/." << std::endl;
    }

    player.position = glm::vec3(0.0f, 0.0f, 5.0f);
    player.rotationY = 0.0f;
    InitActorRenderer();
}

void FieldUpdate(CombatSystem& combatSystem)
{
    float currentTime = static_cast<float>(glfwGetTime());
    deltaTime = previousFrameTime > 0.0f ? currentTime - previousFrameTime : 0.0f;
    previousFrameTime = currentTime;

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    player.Update(deltaTime, window);
    UpdateCameraInput();

    if (StartEncounterIfNeeded(combatSystem)) return;

    int framebufferWidth = 1280;
    int framebufferHeight = 720;
    if (window) {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0) return;
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    glm::vec3 cameraTarget = player.position + glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 cameraOffset(
        cameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw),
        cameraDistance * std::sin(cameraPitch),
        cameraDistance * std::cos(cameraPitch) * std::cos(cameraYaw)
    );
    glm::mat4 view = glm::lookAt(
        cameraTarget + cameraOffset,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
        0.1f,
        500.0f
    );

    if (fieldShader && fieldShader->ID != 0) {
        DrawWetland(*fieldShader, view, projection);
    }

    DrawActor(player.position, glm::vec3(1.1f, 2.4f, 1.1f), glm::vec3(0.12f, 0.55f, 1.0f), view, projection);
    for (size_t i = 0; i < sizeof(enemyPositions) / sizeof(enemyPositions[0]); ++i) {
        if (enemyActive[i]) {
            DrawActor(enemyPositions[i], glm::vec3(1.35f, 2.1f, 1.35f), glm::vec3(0.9f, 0.12f, 0.1f), view, projection);
        }
    }
    glBindVertexArray(0);
}

float getDeltaTime()
{
    return deltaTime;
}
