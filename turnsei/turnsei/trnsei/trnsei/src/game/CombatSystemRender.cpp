#include <glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "Character.h"
#include "CombatSystem.h"
#include "../../assets/ImportedModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    GLuint g_battleShader = 0;
    GLuint g_battleCubeVao = 0;
    GLuint g_battleCubeVbo = 0;
    GLuint g_battleCubeEbo = 0;
    GLuint g_battleFloorVao = 0;
    GLuint g_battleFloorVbo = 0;
    GLuint g_battleShadowVao = 0;
    GLuint g_battleShadowVbo = 0;
    ImportedModel g_playerModel;
    ImportedModel g_enemyModels[5];
    bool g_playerModelLoadAttempted = false;
    bool g_enemyModelLoadAttempted = false;
    double g_previousBattleAnimationTime = 0.0;

    //fileが存在するかどうか
    bool FileExists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    void LoadBattleEnemyModels()
    {
        if (g_enemyModelLoadAttempted) return;
        g_enemyModelLoadAttempted = true;

        const char* extensions[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };

        for (int modelIndex = 0; modelIndex < 5; ++modelIndex) {
            std::string numberedName = "Enemy" + std::to_string(modelIndex + 1);
            for (const char* root : roots) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + numberedName + extension;
                    if (FileExists(path) && g_enemyModels[modelIndex].load(path)) {
                        break;
                    }
                }
                if (g_enemyModels[modelIndex].isLoaded()) break;
            }
        }

        if (!g_enemyModels[0].isLoaded()) {
            for (const char* root : roots) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + "Enemy" + extension;
                    if (FileExists(path) && g_enemyModels[0].load(path)) break;
                }
                if (g_enemyModels[0].isLoaded()) break;
            }
        }
    }

    void LoadBattlePlayerModel()
    {
        if (g_playerModelLoadAttempted) return;
        g_playerModelLoadAttempted = true;

        const char* extensions[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };
        const char* names[] = { "Character", "Player", "Hero" };

        for (const char* root : roots) {
            for (const char* name : names) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + name + extension;
                    if (FileExists(path) && g_playerModel.load(path)) return;
                }
            }
        }
    }

    //シェーダーを作成コンパイルする
    GLuint CompileBattleShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

    void InitBattleRenderer()
    {
        if (g_battleShader != 0) return;

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
out vec3 vColor;
out vec3 vWorldPos;
out vec2 vTexCoords;
void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vColor = aColor;
    vWorldPos = world.xyz;
    vTexCoords = aTexCoords;
    gl_Position = projection * view * world;
}
)";
        const char* fragmentSource = R"(
#version 330 core
uniform vec3 color;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int outlineMode;
uniform int useVertexColor;
uniform int useTexture;
uniform float opacity;
uniform sampler2D diffuseTexture;
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
in vec2 vTexCoords;
out vec4 FragColor;
void main()
{
    if (outlineMode == 1) {
        FragColor = vec4(0.010, 0.013, 0.028, opacity);
        return;
    }
    if (outlineMode == 2) {
        FragColor = vec4(0.006, 0.007, 0.014, opacity);
        return;
    }

    vec3 viewDir = normalize(cameraPos - vWorldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) {
        n = -n;
    }
    vec3 l = normalize(viewDir * 0.35 + normalize(lightDir) * 0.65);
    vec3 fill = normalize(vec3(0.55, 0.35, 0.75));
    vec3 halfDir = normalize(l + viewDir);
    float ndl = max(dot(n, l), 0.0);
    float fillLight = max(dot(n, fill), 0.0);
    float hemi = max(n.y, 0.0);
    float lit = max(max(ndl, hemi * 0.58), fillLight * 0.28);
    float band = 0.44;
    if (lit > 0.88) band = 1.26;
    else if (lit > 0.62) band = 1.00;
    else if (lit > 0.32) band = 0.68;

    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.05);
    float fresnelLine = smoothstep(0.32, 0.82, rim);
    float backRim = smoothstep(0.20, 0.78, pow(1.0 - max(dot(n, normalize(vec3(-0.55, 0.15, -0.82))), 0.0), 2.6));
    float spec = pow(max(dot(n, halfDir), 0.0), 96.0) * smoothstep(0.56, 0.82, lit);
    float glint = pow(max(dot(n, normalize(viewDir + vec3(-0.6, 1.0, 0.25))), 0.0), 180.0);
    vec3 baseColor = useVertexColor == 1 ? vColor : color;
    if (useTexture == 1) {
        baseColor *= texture(diffuseTexture, vTexCoords).rgb;
    }
    baseColor = pow(baseColor, vec3(0.92));

    if (abs(n.y) > 0.94 && abs(vWorldPos.y) < 0.08) {
        float gridA = smoothstep(0.020, 0.000, abs(fract(vWorldPos.x * 0.18) - 0.5));
        float gridB = smoothstep(0.020, 0.000, abs(fract(vWorldPos.z * 0.18) - 0.5));
        float centerGlow = 1.0 - smoothstep(0.0, 13.0, length(vWorldPos.xz - vec2(0.0, 1.8)));
        vec3 floorBase = mix(vec3(0.035, 0.050, 0.082), vec3(0.10, 0.16, 0.24), centerGlow);
        vec3 floorLine = vec3(0.18, 0.36, 0.58) * (gridA + gridB) * 0.16;
        vec3 floorColor = floorBase + floorLine + vec3(0.05, 0.10, 0.18) * fresnelLine;
        FragColor = vec4(floorColor, opacity);
        return;
    }

    vec3 shadowTint = vec3(0.13, 0.16, 0.30);
    vec3 midTint = vec3(0.72, 0.82, 1.00);
    vec3 warmLight = vec3(1.18, 1.08, 0.94);
    vec3 shaded = mix(baseColor * shadowTint, baseColor * midTint, min(band, 0.92));
    shaded = mix(shaded, baseColor * warmLight, smoothstep(0.92, 1.24, band));
    shaded += vec3(0.35, 0.68, 1.00) * fresnelLine * 0.46;
    shaded += vec3(0.82, 0.46, 1.00) * backRim * 0.18;
    shaded += vec3(1.00, 0.92, 0.70) * spec * 0.58;
    shaded += vec3(0.90, 0.96, 1.00) * glint * 0.22;
    float depthFog = smoothstep(8.0, 24.0, length(cameraPos - vWorldPos));
    shaded = mix(shaded, vec3(0.055, 0.075, 0.12), depthFog * 0.18);
    shaded = mix(vec3(dot(shaded, vec3(0.299, 0.587, 0.114))), shaded, 1.20);
    shaded = pow(max(shaded, vec3(0.0)), vec3(0.96));
    FragColor = vec4(shaded, opacity);
}
)";

        GLuint vertexShader = CompileBattleShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileBattleShader(GL_FRAGMENT_SHADER, fragmentSource);
        g_battleShader = glCreateProgram();
        glAttachShader(g_battleShader, vertexShader);
        glAttachShader(g_battleShader, fragmentShader);
        glLinkProgram(g_battleShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        //立方体の頂点データ
        const float cubeVertices[] = {
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
        const unsigned int cubeIndices[] = {
            0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7,
            8, 9, 10, 10, 11, 8, 12, 15, 14, 14, 13, 12,
            16, 17, 18, 18, 19, 16, 20, 23, 22, 22, 21, 20
        };
        glGenVertexArrays(1, &g_battleCubeVao);
        glGenBuffers(1, &g_battleCubeVbo);
        glGenBuffers(1, &g_battleCubeEbo);
        glBindVertexArray(g_battleCubeVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleCubeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_battleCubeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        const float floorVertices[] = {
            -12.0f, 0.0f, -8.0f, 0.0f, 1.0f, 0.0f,
             12.0f, 0.0f, -8.0f, 0.0f, 1.0f, 0.0f,
            -12.0f, 0.0f,  8.0f, 0.0f, 1.0f, 0.0f,
             12.0f, 0.0f,  8.0f, 0.0f, 1.0f, 0.0f
        };
        glGenVertexArrays(1, &g_battleFloorVao);
        glGenBuffers(1, &g_battleFloorVbo);
        glBindVertexArray(g_battleFloorVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleFloorVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
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
        glGenVertexArrays(1, &g_battleShadowVao);
        glGenBuffers(1, &g_battleShadowVbo);
        glBindVertexArray(g_battleShadowVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleShadowVbo);
        glBufferData(GL_ARRAY_BUFFER, shadowVertices.size() * sizeof(float), shadowVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);

        LoadBattlePlayerModel();
        LoadBattleEnemyModels();
    }

    ImVec2 WorldToScreen(const glm::vec3& worldPosition, const glm::mat4& viewProjection, int width, int height)
    {
        glm::vec4 clip = viewProjection * glm::vec4(worldPosition, 1.0f);
        if (clip.w <= 0.0f) return ImVec2(-1000.0f, -1000.0f);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        return ImVec2(
            (ndc.x * 0.5f + 0.5f) * width,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * height
        );
    }

    float EaseOutBack(float value)
    {
        value = std::max(0.0f, std::min(value, 1.0f));
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(value - 1.0f, 3.0f) + c1 * std::pow(value - 1.0f, 2.0f);
    }
}

void CombatSystem::renderBattleScene(Character* activeChar, int screenWidth, int screenHeight)
{
    InitBattleRenderer();
    double currentAnimationTime = ImGui::GetTime();
    float animationDelta = g_previousBattleAnimationTime > 0.0
        ? static_cast<float>(currentAnimationTime - g_previousBattleAnimationTime)
        : 0.0f;
    g_previousBattleAnimationTime = currentAnimationTime;
    g_playerModel.updateAnimation(animationDelta);
    for (ImportedModel& enemyModel : g_enemyModels) {
        enemyModel.updateAnimation(animationDelta);
    }

    glm::vec3 cameraPosition(0.0f, 6.0f, 14.2f);
    glm::mat4 view = glm::lookAt(
        cameraPosition,
        glm::vec3(0.0f, 1.55f, 0.2f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(glm::radians(43.0f), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
    glm::mat4 viewProjection = projection * view;

    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.030f, 0.040f, 0.070f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_battleShader);
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(g_battleShader, "lightDir"), -0.42f, 0.88f, 0.28f);
    glUniform3fv(glGetUniformLocation(g_battleShader, "cameraPos"), 1, glm::value_ptr(cameraPosition));
    glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 0);
    glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
    glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
    glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), 1.0f);

    glm::mat4 floorModel(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(floorModel));
    glUniform3f(glGetUniformLocation(g_battleShader, "color"), 0.12f, 0.18f, 0.28f);
    glBindVertexArray(g_battleFloorVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    std::vector<std::pair<Character*, glm::vec3>> actorPositions;
    std::vector<std::pair<Character*, glm::vec3>> enemyPositions;
    int enemyIndex = 0;
    int enemyCount = 0;
    for (auto* character : participants) {
        if (character && character->isAlly == 0 && character->currentHp > 0) ++enemyCount;
    }

    for (auto* character : participants) {
        if (!character || character->currentHp <= 0) continue;

        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 color;
        if (character->isAlly == 1) {
            position = glm::vec3(-1.45f, 0.0f, 9.4f);
            scale = g_playerModel.isLoaded()
                ? glm::vec3(4.45f, 4.9f, 4.45f)
                : glm::vec3(2.6f, 5.1f, 2.6f);
            color = g_playerModel.isLoaded()
                ? glm::vec3(0.92f, 0.76f, 0.58f)
                : glm::vec3(0.18f, 0.55f, 0.88f);
        }
        else {
            float spacing = 2.15f;
            float x = (enemyIndex - (enemyCount - 1) * 0.5f) * spacing;
            float depth = -3.75f + std::abs(enemyIndex - (enemyCount - 1) * 0.5f) * 0.18f;
            position = glm::vec3(x + 1.25f, 0.0f, depth);
            scale = glm::vec3(2.7f, 3.0f, 2.7f);
            color = markedTarget == character ? glm::vec3(1.0f, 0.76f, 0.12f) : glm::vec3(0.82f, 0.20f, 0.18f);
            enemyPositions.push_back(std::make_pair(character, position));
        }
        actorPositions.push_back(std::make_pair(character, position));

        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);
        glm::mat4 outlineModel(1.0f);
        outlineModel = glm::translate(outlineModel, position);
        outlineModel = glm::scale(outlineModel, scale * 1.055f);
        glUniform3fv(glGetUniformLocation(g_battleShader, "color"), 1, glm::value_ptr(color));

        glm::mat4 shadowModel(1.0f);
        shadowModel = glm::translate(shadowModel, glm::vec3(position.x, 0.0f, position.z));
        shadowModel = glm::scale(shadowModel, glm::vec3(scale.x * 0.56f, 1.0f, scale.z * 0.42f));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(shadowModel));
        glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 2);
        glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
        glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), character->isAlly == 1 ? 0.34f : 0.28f);
        glBindVertexArray(g_battleShadowVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 42);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), 1.0f);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(outlineModel));
        glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 1);
        glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
        if (character->isAlly == 0) {
            int modelIndex = enemyIndex < 5 ? enemyIndex : 0;
            ImportedModel& importedModel = g_enemyModels[modelIndex].isLoaded()
                ? g_enemyModels[modelIndex]
                : g_enemyModels[0];
            if (importedModel.isLoaded()) {
                importedModel.draw();
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
        }
        else {
            if (g_playerModel.isLoaded()) {
                g_playerModel.draw();
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
        }

        glDisable(GL_CULL_FACE);
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 0);
        if (character->isAlly == 0) {
            int modelIndex = enemyIndex < 5 ? enemyIndex : 0;
            ImportedModel& importedModel = g_enemyModels[modelIndex].isLoaded()
                ? g_enemyModels[modelIndex]
                : g_enemyModels[0];
            if (importedModel.isLoaded()) {
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 1);
                importedModel.draw();
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
            ++enemyIndex;
        }
        else {
            if (g_playerModel.isLoaded()) {
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 1);
                g_playerModel.draw();
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
        }
    }
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);

    bool canSelect = activeChar && activeChar->isAlly == 1 && battleState == BattleState::InProgress;
    for (size_t i = 0; i < enemyPositions.size(); ++i) {
        Character* enemy = enemyPositions[i].first;
        glm::vec3 position = enemyPositions[i].second;
        ImVec2 feet = WorldToScreen(position, viewProjection, screenWidth, screenHeight);
        ImVec2 head = WorldToScreen(position + glm::vec3(0.0f, 2.7f, 0.0f), viewProjection, screenWidth, screenHeight);
        float height = std::max(90.0f, feet.y - head.y);
        float width = height * 0.65f;
        ImVec2 minPos(feet.x - width * 0.5f, head.y);

        ImGui::SetNextWindowPos(minPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        std::string hitWindow = "##Enemy3DHit" + std::to_string(i);
        if (ImGui::Begin(hitWindow.c_str(), nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if (canSelect && ImGui::InvisibleButton("##EnemyModel", ImVec2(width, height))) {
                markedTarget = markedTarget == enemy ? nullptr : enemy;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (markedTarget == enemy) {
            ImVec2 center(feet.x, head.y - 22.0f);
            drawList->AddCircle(center, 15.0f, IM_COL32(255, 205, 45, 255), 24, 3.0f);
            drawList->AddLine(ImVec2(center.x - 23.0f, center.y), ImVec2(center.x - 8.0f, center.y), IM_COL32(255, 205, 45, 255), 3.0f);
            drawList->AddLine(ImVec2(center.x + 8.0f, center.y), ImVec2(center.x + 23.0f, center.y), IM_COL32(255, 205, 45, 255), 3.0f);
        }
        float hpRatio = enemy->hp > 0 ? static_cast<float>(enemy->currentHp) / static_cast<float>(enemy->hp) : 0.0f;
        hpRatio = std::max(0.0f, std::min(hpRatio, 1.0f));
        ImVec2 barMin(head.x - 58.0f, head.y - 62.0f);
        ImVec2 barMax(head.x + 58.0f, head.y - 48.0f);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(24, 24, 28, 230), 3.0f);
        drawList->AddRectFilled(
            ImVec2(barMin.x + 2.0f, barMin.y + 2.0f),
            ImVec2(barMin.x + 2.0f + (barMax.x - barMin.x - 4.0f) * hpRatio, barMax.y - 2.0f),
            hpRatio > 0.35f ? IM_COL32(65, 210, 90, 255) : IM_COL32(235, 65, 55, 255),
            2.0f
        );
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 180), 3.0f);

        std::string hpText = enemy->name + "  " + std::to_string(enemy->currentHp) + "/" + std::to_string(enemy->hp);
        ImVec2 textSize = ImGui::CalcTextSize(hpText.c_str());
        drawList->AddText(
            ImVec2(head.x - textSize.x * 0.5f, barMin.y - textSize.y - 3.0f),
            IM_COL32(255, 255, 255, 255),
            hpText.c_str()
        );
    }

    double now = ImGui::GetTime();
    const double popupLifetime = 0.92;
    damagePopups.erase(
        std::remove_if(damagePopups.begin(), damagePopups.end(), [now, popupLifetime](const DamagePopup& popup) {
            return now - popup.startTime > popupLifetime || !popup.target;
        }),
        damagePopups.end()
    );

    ImDrawList* popupDrawList = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    for (const DamagePopup& popup : damagePopups) {
        auto actorIt = std::find_if(actorPositions.begin(), actorPositions.end(), [&popup](const std::pair<Character*, glm::vec3>& actor) {
            return actor.first == popup.target;
        });
        if (actorIt == actorPositions.end()) continue;

        float t = static_cast<float>((now - popup.startTime) / popupLifetime);
        t = std::max(0.0f, std::min(t, 1.0f));
        float rise = 72.0f * t;
        float punch = EaseOutBack(std::min(t * 2.2f, 1.0f));
        int alpha = static_cast<int>((1.0f - std::max(0.0f, (t - 0.58f) / 0.42f)) * 255.0f);
        alpha = std::max(0, std::min(alpha, 255));

        glm::vec3 popupWorld = actorIt->second + glm::vec3(0.0f, popup.target->isAlly == 1 ? 4.7f : 3.25f, 0.0f);
        ImVec2 anchor = WorldToScreen(popupWorld, viewProjection, screenWidth, screenHeight);
        if (anchor.x < -999.0f) continue;

        std::string text = popup.isCritical
            ? "CRIT " + std::to_string(popup.amount)
            : std::to_string(popup.amount);
        float fontSize = ImGui::GetFontSize() * (popup.isCritical ? 1.58f : 1.34f) * (0.82f + 0.18f * punch);
        ImVec2 popupTextSize = font->CalcTextSizeA(fontSize, 10000.0f, 0.0f, text.c_str());
        ImVec2 pos(anchor.x + popup.xOffset - popupTextSize.x * 0.5f, anchor.y - rise - popupTextSize.y * 0.5f);
        ImU32 shadowColor = IM_COL32(12, 14, 20, alpha);
        ImU32 fillColor = popup.isCritical
            ? IM_COL32(255, 218, 72, alpha)
            : IM_COL32(255, 245, 232, alpha);

        popupDrawList->AddText(font, fontSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f), shadowColor, text.c_str());
        popupDrawList->AddText(font, fontSize, ImVec2(pos.x - 1.0f, pos.y), shadowColor, text.c_str());
        popupDrawList->AddText(font, fontSize, pos, fillColor, text.c_str());
    }
}

void CombatSystem::renderBattleCards(Character* activeChar, float screenWidth, float marginX, float marginY, float cardWidth, float cardHeight, float spacingY)
{
    float currentY = marginY;
    int orderNumber = 1;

    for (size_t i = 0; i < participants.size(); i++)
    {
        Character* c = participants[i];
        if (!c || c->currentHp <= 0) continue;

        bool isMarkedTarget = markedTarget == c;

        ImGui::SetNextWindowPos(ImVec2(marginX, currentY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

        ImVec4 bgColor = (c->isAlly == 1)
            ? ImVec4(0.06f, 0.14f, 0.22f, 0.90f)
            : ImVec4(0.20f, 0.07f, 0.08f, 0.90f);
        ImVec4 borderColor = (c == activeChar)
            ? ImVec4(0.95f, 0.80f, 0.32f, 1.0f)
            : ImVec4(0.55f, 0.60f, 0.66f, 0.55f);
        if (isMarkedTarget) borderColor = ImVec4(0.2f, 0.85f, 1.0f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, isMarkedTarget ? 4.0f : 2.0f);

        std::string windowName = "##BattleCard" + std::to_string(i);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            ImGui::TextColored(
                c == activeChar ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f) : ImVec4(0.7f, 0.74f, 0.8f, 1.0f),
                "%02d", orderNumber
            );
            ImGui::SameLine();
            ImGui::Text("%s", c->name.c_str());

            if (isMarkedTarget) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "LOCK");
            }
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        currentY += cardHeight + spacingY;
        ++orderNumber;
    }
}

void CombatSystem::renderActionMenu(Character* activeChar, int screenWidth, int screenHeight)
{
    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(ImVec2(18.0f, (float)screenHeight - 172.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(310.0f, 150.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.075f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##PartyHud", nullptr, hudFlags)) {
        for (auto* c : participants) {
            if (!c || c->isAlly != 1) continue;

            float hpFraction = c->hp > 0 ? (float)c->currentHp / (float)c->hp : 0.0f;
            ImGui::Text("%s", c->name.c_str());
            ImGui::SameLine(145.0f);
            ImGui::Text("%d / %d", c->currentHp, c->hp);
            ImVec4 hpColor = hpFraction < 0.25f
                ? ImVec4(0.95f, 0.22f, 0.18f, 1.0f)
                : ImVec4(0.15f, 0.78f, 0.68f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hpColor);
            ImGui::ProgressBar(hpFraction, ImVec2(-1.0f, 8.0f), "");
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::SetNextWindowPos(ImVec2((float)screenWidth - 118.0f, 18.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(100.0f, 48.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.075f, 0.84f));
    if (ImGui::Begin("##LogButton", nullptr, hudFlags)) {
        if (ImGui::Button(isBattleLogOpen ? "Close" : "Log", ImVec2(-1.0f, 28.0f))) {
            isBattleLogOpen = !isBattleLogOpen;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();

    float commandWidth = std::min(520.0f, (float)screenWidth * 0.48f);
    ImGui::SetNextWindowPos(
        ImVec2((float)screenWidth - commandWidth - 22.0f, (float)screenHeight - 158.0f),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(commandWidth, 136.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.075f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.72f, 0.62f, 0.32f, 0.60f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    if (ImGui::Begin("##CommandHud", nullptr, hudFlags)) {
        if (battleState != BattleState::InProgress) {
            ImGui::TextColored(
                battleState == BattleState::Victory ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                battleState == BattleState::Victory ? "Victory" : "Defeat"
            );
            if (ImGui::Button("Retry", ImVec2(160, 42))) {
                resetBattle();
            }
        }
        else if (activeChar && activeChar->isAlly == 1) {
            ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.42f, 1.0f), "%s", activeChar->name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled(markedTarget ? "Target: %s" : "Target: Random", markedTarget ? markedTarget->name.c_str() : "");
            ImGui::Separator();

            float buttonWidth = (commandWidth - 48.0f) / 3.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.24f, 0.30f, 1.0f));
            if (ImGui::Button("Basic\nAttack", ImVec2(buttonWidth, 62.0f))) {
                chooseCommand(BattleCommand::BasicAttack);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.42f, 0.58f, 1.0f));
            if (ImGui::Button("Combat\nSkill", ImVec2(buttonWidth, 62.0f))) {
                chooseCommand(BattleCommand::Skill);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.42f, 0.12f, 1.0f));
            if (ImGui::Button("Ultimate", ImVec2(buttonWidth, 62.0f))) {
                chooseCommand(BattleCommand::Ultimate);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }
        else if (activeChar) {
            ImGui::TextColored(ImVec4(0.95f, 0.38f, 0.34f, 1.0f), "%s", activeChar->name.c_str());
            ImGui::Text("Enemy turn...");

            if (!enemyActionQueued) {
                enemyActionQueued = true;
                enemyActionTime = ImGui::GetTime() + 0.6;
            }

            if (ImGui::GetTime() >= enemyActionTime) {
                Character* target = getRandomAliveTarget(1);
                if (target) executeSkill(activeChar, target);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void CombatSystem::renderBattleLogWindow(int screenWidth, int screenHeight)
{
    if (!isBattleLogOpen) return;

    ImGui::SetNextWindowPos(ImVec2(20.f, 80.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(420.f, 220.f), ImGuiCond_Always);
    if (ImGui::Begin("Battle Log", &isBattleLogOpen, ImGuiWindowFlags_NoResize)) {
        for (const auto& line : battleLog) {
            ImGui::BulletText("%s", line.c_str());
        }
    }
    ImGui::End();
}
