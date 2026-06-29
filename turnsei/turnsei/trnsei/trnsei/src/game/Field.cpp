#include "Field.h"
#include "CombatSystem.h"
#include "Player.h"
#include "Scene.h"
#include "../../assets/ImportedModel.h"
#include "../../assets/Wetlandloder.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <string>
#include <vector>

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
    GLuint actorShadowVao = 0;
    GLuint actorShadowVbo = 0;
    ImportedModel playerModel;
    bool playerModelLoadAttempted = false;
    float cameraYaw = 0.0f;
    float cameraPitch = glm::radians(32.0f);
    float cameraDistance = 20.0f;
    bool enemyActive[] = { true, true, true, true, true };
    bool encounterTransitionActive = false;
    float encounterTransitionTime = 0.0f;
    const float encounterTransitionDuration = 0.82f;
    glm::vec3 encounterTargetPosition(0.0f);

    const glm::vec3 enemyPositions[] = {
        glm::vec3(-7.0f, 0.0f, -6.0f),
        glm::vec3(0.0f, 0.0f, -8.0f),
        glm::vec3(7.0f, 0.0f, -6.0f),
        glm::vec3(-7.0f, 0.0f, 0.0f),
        glm::vec3(7.0f, 0.0f, 0.0f)
    };

    bool FileExists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    float Saturate(float value)
    {
        return std::max(0.0f, std::min(value, 1.0f));
    }

    float SmoothStep(float value)
    {
        value = Saturate(value);
        return value * value * (3.0f - 2.0f * value);
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
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoords;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vNormal;
out vec3 vWorldPos;
out vec3 vColor;
out vec2 vTexCoords;
void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vColor = aColor;
    vTexCoords = aTexCoords;
    gl_Position = projection * view * world;
}
)";
        const char* fragmentSource = R"(
        #version 330 core
        uniform vec3 actorColor;
        uniform vec3 lightDir;
        uniform vec3 cameraPos;
        uniform int outlineMode;
        uniform int useVertexColor;
        uniform int useTexture;
        uniform float opacity;
        uniform sampler2D diffuseTexture;
        in vec3 vNormal;
        in vec3 vWorldPos;
        in vec3 vColor;
        in vec2 vTexCoords;
        out vec4 FragColor;
        void main()
{
    if (outlineMode == 1) {
        FragColor = vec4(0.020, 0.024, 0.038, opacity);
        return;
    }
    if (outlineMode == 2) {
        FragColor = vec4(0.012, 0.014, 0.024, opacity);
        return;
    }

    vec3 viewDir = normalize(cameraPos - vWorldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) {
        n = -n;
    }
    vec3 l = normalize(vec3(-0.28, 0.92, 0.24));
    vec3 halfDir = normalize(l + viewDir);
    float ndl = max(dot(n, l), 0.0);
    float hemi = max(n.y, 0.0);
    float lit = max(ndl, hemi * 0.58);
    float band = 0.48;
    if (lit > 0.86) band = 1.18;
    else if (lit > 0.58) band = 0.94;
    else if (lit > 0.30) band = 0.66;

    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.35);
    float fresnelLine = smoothstep(0.42, 0.86, rim);
    float spec = pow(max(dot(n, halfDir), 0.0), 72.0) * smoothstep(0.58, 0.82, lit);
    vec3 baseColor = useVertexColor == 1 ? vColor : actorColor;
    if (useTexture == 1) {
        baseColor *= texture(diffuseTexture, vTexCoords).rgb;
    }
    baseColor = pow(baseColor, vec3(0.92));
    vec3 shadowTint = vec3(0.18, 0.22, 0.34);
    vec3 warmLight = vec3(1.08, 1.02, 0.94);
    vec3 color = mix(baseColor * shadowTint, baseColor * warmLight, band);
    color += vec3(0.30, 0.55, 0.82) * fresnelLine * 0.34;
    color += vec3(1.00, 0.92, 0.72) * spec * 0.42;
    color = mix(vec3(dot(color, vec3(0.299, 0.587, 0.114))), color, 1.14);
    FragColor = vec4(color, opacity);
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
            -0.5f, 0.0f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, 0.0f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, 1.0f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, 1.0f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, 0.0f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, 0.0f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, 1.0f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, 1.0f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, 0.0f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 0.0f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f, -0.5f, -1.0f,  0.0f,  0.0f,
             0.5f, 0.0f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 0.0f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 1.0f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 1.0f, -0.5f,  1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f, 1.0f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f, 1.0f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f, 1.0f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f, 0.0f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, 0.0f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, 0.0f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, 0.0f,  0.5f,  0.0f, -1.0f,  0.0f
        };
        const unsigned int indices[] = {
            0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7,
            8, 9, 10, 10, 11, 8, 12, 15, 14, 14, 13, 12,
            16, 17, 18, 18, 19, 16, 20, 23, 22, 22, 21, 20
        };

        glGenVertexArrays(1, &actorVao);
        glGenBuffers(1, &actorVbo);
        glGenBuffers(1, &actorEbo);
        glBindVertexArray(actorVao);
        glBindBuffer(GL_ARRAY_BUFFER, actorVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, actorEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        std::vector<float> shadowVertices;
        const int shadowSegments = 40;
        shadowVertices.insert(shadowVertices.end(), { 0.0f, 0.025f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f });
        for (int i = 0; i <= shadowSegments; ++i) {
            float angle = (float)i / (float)shadowSegments * glm::two_pi<float>();
            shadowVertices.insert(shadowVertices.end(), {
                std::cos(angle), 0.025f, std::sin(angle),
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f
            });
        }
        glGenVertexArrays(1, &actorShadowVao);
        glGenBuffers(1, &actorShadowVbo);
        glBindVertexArray(actorShadowVao);
        glBindBuffer(GL_ARRAY_BUFFER, actorShadowVbo);
        glBufferData(GL_ARRAY_BUFFER, shadowVertices.size() * sizeof(float), shadowVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

    void LoadPlayerModel()
    {
        if (playerModelLoadAttempted) return;
        playerModelLoadAttempted = true;

        const char* extensions[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };
        const char* names[] = { "Character", "Player", "Hero" };

        for (const char* root : roots) {
            for (const char* name : names) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + name + extension;
                    if (FileExists(path) && playerModel.load(path)) return;
                }
            }
        }
    }

    void DrawActor(
        const glm::vec3& position,
        const glm::vec3& scale,
        const glm::vec3& color,
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& cameraPosition,
        float rotationY,
        const ImportedModel* importedModel = nullptr)
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, scale);

        glUseProgram(actorShader);
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(actorShader, "actorColor"), 1, glm::value_ptr(color));
        glUniform3f(glGetUniformLocation(actorShader, "lightDir"), -0.25f, 0.92f, 0.18f);
        glUniform3fv(glGetUniformLocation(actorShader, "cameraPos"), 1, glm::value_ptr(cameraPosition));
        glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(actorShader, "useTexture"), 0);
        glUniform1f(glGetUniformLocation(actorShader, "opacity"), 1.0f);
        glBindVertexArray(actorVao);

        glm::mat4 shadowModel(1.0f);
        shadowModel = glm::translate(shadowModel, glm::vec3(position.x, -0.01f, position.z));
        shadowModel = glm::scale(shadowModel, glm::vec3(scale.x * 0.66f, 1.0f, scale.z * 0.50f));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(shadowModel));
        glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 2);
        glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(actorShader, "useTexture"), 0);
        glUniform1f(glGetUniformLocation(actorShader, "opacity"), importedModel && importedModel->isLoaded() ? 0.24f : 0.25f);
        glBindVertexArray(actorShadowVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 42);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glUniform1f(glGetUniformLocation(actorShader, "opacity"), 1.0f);
        glBindVertexArray(actorVao);

        bool drawModelOutline = !(importedModel && importedModel->isLoaded());
        if (drawModelOutline) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glm::mat4 outlineModel(1.0f);
            outlineModel = glm::translate(outlineModel, position);
            outlineModel = glm::rotate(outlineModel, glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
            outlineModel = glm::scale(outlineModel, scale * 1.07f);
            glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(outlineModel));
            glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 1);
            glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
            glUniform1i(glGetUniformLocation(actorShader, "useTexture"), 0);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            glDisable(GL_CULL_FACE);
        }

        glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 0);
        if (importedModel && importedModel->isLoaded()) {
            glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 1);
            importedModel->draw();
            glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
        }
        else {
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }
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

    void DrawEncounterTransitionOverlay(int width, int height, float progress)
    {
        if (!encounterTransitionActive) return;

        float eased = SmoothStep(progress);
        float flash = std::max(0.0f, 1.0f - progress * 5.0f);
        float fade = SmoothStep((progress - 0.42f) / 0.58f);

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        drawList->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            ImVec2((float)width, (float)height),
            IM_COL32(255, 246, 210, (int)(flash * 92.0f))
        );
        drawList->AddRectFilled(
            ImVec2(0.0f, 0.0f),
            ImVec2((float)width, (float)height),
            IM_COL32(4, 8, 14, (int)(fade * 236.0f))
        );

        float lineAlpha = (1.0f - std::abs(eased - 0.55f) * 2.0f);
        lineAlpha = Saturate(lineAlpha) * 145.0f;
        float centerY = (float)height * 0.5f;
        drawList->AddLine(
            ImVec2((float)width * 0.18f, centerY),
            ImVec2((float)width * 0.82f, centerY),
            IM_COL32(255, 214, 124, (int)lineAlpha),
            2.0f
        );
    }

    bool StartEncounterIfNeeded(CombatSystem& combatSystem)
    {
        if (encounterTransitionActive) return false;

        const float encounterDistance = 1.65f;
        for (size_t i = 0; i < sizeof(enemyPositions) / sizeof(enemyPositions[0]); ++i) {
            if (!enemyActive[i]) continue;

            glm::vec2 playerXZ(player.position.x, player.position.z);
            glm::vec2 enemyXZ(enemyPositions[i].x, enemyPositions[i].z);
            if (glm::distance(playerXZ, enemyXZ) <= encounterDistance) {
                enemyActive[i] = false;
                encounterTransitionActive = true;
                encounterTransitionTime = 0.0f;
                encounterTargetPosition = enemyPositions[i];
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

    player.position = glm::vec3(0.0f, 0.0f, 7.0f);
    player.rotationY = 0.0f;
    InitActorRenderer();
    LoadPlayerModel();
}

void FieldUpdate(CombatSystem& combatSystem)
{
    float currentTime = static_cast<float>(glfwGetTime());
    deltaTime = previousFrameTime > 0.0f ? currentTime - previousFrameTime : 0.0f;
    previousFrameTime = currentTime;

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (encounterTransitionActive) {
        encounterTransitionTime += deltaTime;
    }
    else {
        player.Update(deltaTime, window);
        UpdateCameraInput();
    }

    StartEncounterIfNeeded(combatSystem);

    int framebufferWidth = 1280;
    int framebufferHeight = 720;
    if (window) {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    }
    if (framebufferWidth <= 0 || framebufferHeight <= 0) return;
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    float encounterProgress = encounterTransitionActive
        ? Saturate(encounterTransitionTime / encounterTransitionDuration)
        : 0.0f;
    float encounterEase = SmoothStep(encounterProgress);
    glm::vec3 cameraTarget = player.position + glm::vec3(0.0f, 1.0f, 0.0f);
    if (encounterTransitionActive) {
        cameraTarget = glm::mix(
            cameraTarget,
            encounterTargetPosition + glm::vec3(0.0f, 1.15f, 0.0f),
            encounterEase * 0.55f
        );
    }

    float activeCameraDistance = encounterTransitionActive
        ? glm::mix(cameraDistance, 7.4f, encounterEase)
        : cameraDistance;
    glm::vec3 cameraOffset(
        activeCameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw),
        activeCameraDistance * std::sin(cameraPitch),
        activeCameraDistance * std::cos(cameraPitch) * std::cos(cameraYaw)
    );
    glm::vec3 cameraPosition = cameraTarget + cameraOffset;
    if (encounterTransitionActive) {
        float shake = (1.0f - encounterEase) * 0.18f;
        cameraPosition.x += std::sin(encounterTransitionTime * 68.0f) * shake;
        cameraPosition.y += std::cos(encounterTransitionTime * 53.0f) * shake * 0.55f;
    }
    glm::mat4 view = glm::lookAt(
        cameraPosition,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
        0.1f,
        500.0f
    );

    UpdateWetlandAnimation(deltaTime);
    if (fieldShader && fieldShader->ID != 0) {
        DrawWetland(*fieldShader, view, projection);
    }

    if (player.isMoving()) {
        if (!playerModel.playAnimationByName("walk") &&
            !playerModel.playAnimationByName("run") &&
            !playerModel.playAnimationByName("locomotion")) {
            playerModel.playAnimationByIndex(0);
        }
    }
    else {
        if (!playerModel.playAnimationByName("idle") &&
            !playerModel.playAnimationByName("stand")) {
            playerModel.playAnimationByIndex(0);
        }
    }
    playerModel.updateAnimation(deltaTime);
    glm::vec3 playerScale = playerModel.isLoaded()
        ? glm::vec3(2.9f, 3.0f, 2.9f)
        : glm::vec3(1.1f, 2.4f, 1.1f);
    glm::vec3 playerColor = playerModel.isLoaded()
        ? glm::vec3(0.92f, 0.76f, 0.58f)
        : glm::vec3(0.12f, 0.55f, 1.0f);
    DrawActor(player.position, playerScale, playerColor, view, projection, cameraPosition, player.rotationY, &playerModel);
    for (size_t i = 0; i < sizeof(enemyPositions) / sizeof(enemyPositions[0]); ++i) {
        if (enemyActive[i]) {
            DrawActor(enemyPositions[i], glm::vec3(1.35f, 2.1f, 1.35f), glm::vec3(0.9f, 0.12f, 0.1f), view, projection, cameraPosition, 0.0f);
        }
    }
    glBindVertexArray(0);

    DrawEncounterTransitionOverlay(framebufferWidth, framebufferHeight, encounterProgress);

    if (encounterTransitionActive && encounterTransitionTime >= encounterTransitionDuration) {
        encounterTransitionActive = false;
        combatSystem.resetBattle();
        currentScene = Scene::Battle;
    }
}

float getDeltaTime()
{
    return deltaTime;
}
