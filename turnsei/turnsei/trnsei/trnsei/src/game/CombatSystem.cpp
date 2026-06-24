#define GLEW_STATIC
#include <glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "Character.h"
#include "CombatSystem.h"
#include "../../assets/ImportedModel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

void CombatSystem::displayTurnOrder()
{
    sortTurnOrder();

    std::cout << u8"----行動順（コンソール）----" << std::endl;
    for (size_t i = 0; i < participants.size(); i++)
    {
        if (!participants[i] || participants[i]->currentHp <= 0) continue;
        std::cout << (i + 1) << u8"番目: " << participants[i]->name << std::endl;
    }
}

void CombatSystem::toggleVisibility()
{
    isVisible = !isVisible;
}

static const char* GetCommandName(BattleCommand command)
{
    switch (command) {
    case BattleCommand::BasicAttack: return "Basic";
    case BattleCommand::Skill: return "Skill";
    case BattleCommand::Ultimate: return "Ultimate";
    default: return "None";
    }
}

namespace
{
    GLuint g_battleShader = 0;
    GLuint g_battleCubeVao = 0;
    GLuint g_battleCubeVbo = 0;
    GLuint g_battleCubeEbo = 0;
    GLuint g_battleFloorVao = 0;
    GLuint g_battleFloorVbo = 0;
    ImportedModel g_enemyModels[5];
    bool g_enemyModelLoadAttempted = false;

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
uniform vec3 color;
out vec4 FragColor;
void main()
{
    FragColor = vec4(color, 1.0);
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

        const float cubeVertices[] = {
            -0.5f, 0.0f, -0.5f,  0.5f, 0.0f, -0.5f,
             0.5f, 1.0f, -0.5f, -0.5f, 1.0f, -0.5f,
            -0.5f, 0.0f,  0.5f,  0.5f, 0.0f,  0.5f,
             0.5f, 1.0f,  0.5f, -0.5f, 1.0f,  0.5f
        };
        const unsigned int cubeIndices[] = {
            0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
            0, 4, 7, 7, 3, 0, 1, 5, 6, 6, 2, 1,
            3, 2, 6, 6, 7, 3, 0, 1, 5, 5, 4, 0
        };
        glGenVertexArrays(1, &g_battleCubeVao);
        glGenBuffers(1, &g_battleCubeVbo);
        glGenBuffers(1, &g_battleCubeEbo);
        glBindVertexArray(g_battleCubeVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleCubeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_battleCubeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        const float floorVertices[] = {
            -12.0f, 0.0f, -8.0f, 12.0f, 0.0f, -8.0f,
            -12.0f, 0.0f,  8.0f, 12.0f, 0.0f,  8.0f
        };
        glGenVertexArrays(1, &g_battleFloorVao);
        glGenBuffers(1, &g_battleFloorVbo);
        glBindVertexArray(g_battleFloorVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleFloorVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

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
}

void CombatSystem::renderBattleScene(Character* activeChar, int screenWidth, int screenHeight)
{
    InitBattleRenderer();

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 6.5f, 12.0f),
        glm::vec3(0.0f, 1.2f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
    glm::mat4 viewProjection = projection * view;

    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.055f, 0.065f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_battleShader);
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::mat4 floorModel(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(floorModel));
    glUniform3f(glGetUniformLocation(g_battleShader, "color"), 0.12f, 0.16f, 0.21f);
    glBindVertexArray(g_battleFloorVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

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
            position = glm::vec3(-4.5f, 0.0f, 2.0f);
            scale = glm::vec3(1.1f, 2.3f, 1.1f);
            color = glm::vec3(0.18f, 0.55f, 0.88f);
        }
        else {
            float spacing = 2.15f;
            float x = (enemyIndex - (enemyCount - 1) * 0.5f) * spacing;
            float depth = -1.6f + std::abs(enemyIndex - (enemyCount - 1) * 0.5f) * 0.22f;
            position = glm::vec3(x + 1.5f, 0.0f, depth);
            scale = glm::vec3(2.5f, 2.7f, 2.5f);
            color = markedTarget == character ? glm::vec3(1.0f, 0.76f, 0.12f) : glm::vec3(0.82f, 0.20f, 0.18f);
            enemyPositions.push_back(std::make_pair(character, position));
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(g_battleShader, "color"), 1, glm::value_ptr(color));
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
            ++enemyIndex;
        }
        else {
            glBindVertexArray(g_battleCubeVao);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
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
        drawList->AddText(ImVec2(head.x - 42.0f, head.y - 52.0f), IM_COL32(255, 255, 255, 255), enemy->name.c_str());
    }
}

void CombatSystem::renderBattleCards(Character* activeChar, float screenWidth, float marginX, float marginY, float cardWidth, float cardHeight, float spacingY)
{
    float currentY = marginY;

    for (size_t i = 0; i < participants.size(); i++)
    {
        Character* c = participants[i];
        if (!c || c->currentHp <= 0) continue;

        bool isMarkedTarget = markedTarget == c;

        ImGui::SetNextWindowPos(ImVec2(screenWidth - cardWidth - marginX, currentY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

        ImVec4 bgColor = (c->isAlly == 1) ? ImVec4(0.1f, 0.34f, 0.18f, 0.72f) : ImVec4(0.42f, 0.12f, 0.12f, 0.72f);
        ImVec4 borderColor = (c == activeChar) ? ImVec4(1.0f, 0.84f, 0.2f, 1.0f) : ((c->isAlly == 1) ? ImVec4(0.4f, 1.0f, 0.5f, 0.6f) : ImVec4(1.0f, 0.45f, 0.45f, 0.6f));
        if (isMarkedTarget) borderColor = ImVec4(0.2f, 0.85f, 1.0f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, isMarkedTarget ? 4.0f : 2.0f);

        std::string windowName = "##BattleCard" + std::to_string(i);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            if (c == activeChar && battleState == BattleState::InProgress) {
                ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.0f, 1.0f), "NEXT");
                ImGui::SameLine();
            }
            ImGui::Text("%s", c->name.c_str());
            ImGui::Text("HP %d/%d  SPD %d", c->currentHp, c->hp, c->speed);

            if (isMarkedTarget) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "TARGET");
            }

        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        currentY += cardHeight + spacingY;
    }
}

void CombatSystem::renderActionMenu(Character* activeChar, int screenWidth, int screenHeight)
{
    ImGui::SetNextWindowPos(ImVec2(16.f, (float)screenHeight - 180.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)screenWidth - 32.f, 164.f), ImGuiCond_Always);

    if (ImGui::Begin("Command Menu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImGui::BeginChild("AllyStatus", ImVec2((float)screenWidth * 0.36f, 0), true);
        ImGui::Text("Party");
        ImGui::Separator();

        for (auto* c : participants) {
            if (c && c->isAlly == 1) {
                ImGui::BeginGroup();
                ImGui::Button(c->name.substr(0, 1).c_str(), ImVec2(56, 44));

                float hpFraction = c->hp > 0 ? (float)c->currentHp / (float)c->hp : 0.0f;
                ImVec4 hpColor = (hpFraction < 0.2f) ? ImVec4(1, 0.15f, 0.1f, 1) : (hpFraction < 0.5f) ? ImVec4(1, 0.82f, 0.1f, 1) : ImVec4(0.2f, 0.85f, 0.35f, 1);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hpColor);
                ImGui::ProgressBar(hpFraction, ImVec2(70, 9), "");
                ImGui::PopStyleColor();
                ImGui::Text("%d/%d", c->currentHp, c->hp);
                ImGui::EndGroup();
                ImGui::SameLine(0, 16);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("ActionMenu", ImVec2(0, 0), false);

        if (ImGui::Button(isBattleLogOpen ? "Hide Log" : "Log", ImVec2(92, 34))) {
            isBattleLogOpen = !isBattleLogOpen;
        }
        ImGui::Separator();

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
            ImGui::Text("%s", activeChar->name.c_str());
            ImGui::Text("Choose a technique");
            ImGui::Spacing();

            if (ImGui::Button("Basic", ImVec2(120, 42))) {
                chooseCommand(BattleCommand::BasicAttack);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::SameLine();
            if (ImGui::Button("Skill", ImVec2(120, 42))) {
                chooseCommand(BattleCommand::Skill);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::SameLine();
            if (ImGui::Button("Ultimate", ImVec2(120, 42))) {
                chooseCommand(BattleCommand::Ultimate);
                executeCommand(activeChar, markedTarget ? markedTarget : getRandomAliveTarget(0));
            }
            ImGui::Separator();
            ImGui::Text(markedTarget ? "Target: %s" : "Target: Random", markedTarget ? markedTarget->name.c_str() : "");
        }
        else if (activeChar) {
            ImGui::Text("%s", activeChar->name.c_str());
            ImGui::Separator();
            ImGui::Text("Enemy is acting...");

            if (!enemyActionQueued) {
                enemyActionQueued = true;
                enemyActionTime = ImGui::GetTime() + 0.6;
            }

            if (ImGui::GetTime() >= enemyActionTime) {
                Character* target = getRandomAliveTarget(1);
                if (target) executeSkill(activeChar, target);
            }
        }

        ImGui::EndChild();
    }
    ImGui::End();
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

void CombatSystem::renderUI(int screenWidth, int screenHeight)
{
    if (!isVisible || participants.empty()) return;

    {
        sortTurnOrder();
        checkBattleState();

        Character* activeChar = getActiveCharacter();
        if (markedTarget && markedTarget->currentHp <= 0) markedTarget = nullptr;
        if (!activeChar || battleState != BattleState::InProgress) {
            pendingCommand = BattleCommand::None;
        }

        renderBattleScene(activeChar, screenWidth, screenHeight);
        renderBattleCards(activeChar, (float)screenWidth, 20.f, 20.f, 280.0f, 68.0f, 6.f);
        renderActionMenu(activeChar, screenWidth, screenHeight);
        renderBattleLogWindow(screenWidth, screenHeight);
        return;
    }

    sortTurnOrder();
    checkBattleState();

    const float cardWidth = 280.0f;
    const float cardHeight = 76.0f;
    const float marginX = 20.f;
    const float marginY = 20.f;
    const float spacingY = 10.f;
    float currentY = marginY;

    Character* activeChar = getActiveCharacter();

    for (size_t i = 0; i < participants.size(); i++)
    {
        Character* c = participants[i];
        if (!c || c->currentHp <= 0) continue;

        ImGui::SetNextWindowPos(ImVec2((float)screenWidth - cardWidth - marginX, currentY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

        ImVec4 bgColor = (c->isAlly == 1) ? ImVec4(0.1f, 0.34f, 0.18f, 0.72f) : ImVec4(0.42f, 0.12f, 0.12f, 0.72f);
        ImVec4 borderColor = (c == activeChar) ? ImVec4(1.0f, 0.84f, 0.2f, 1.0f) : ((c->isAlly == 1) ? ImVec4(0.4f, 1.0f, 0.5f, 0.6f) : ImVec4(1.0f, 0.45f, 0.45f, 0.6f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

        std::string windowName = "##Card" + std::to_string(i);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            if (c == activeChar && battleState == BattleState::InProgress)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.0f, 1.0f), u8"NEXT");
                ImGui::SameLine();
            }
            ImGui::Text("%s", c->name.c_str());
            ImGui::Text(u8"HP %d/%d  SPD %d", c->currentHp, c->hp, c->speed);
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        currentY += cardHeight + spacingY;
    }

    ImGui::SetNextWindowPos(ImVec2(16.f, (float)screenHeight - 220.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)screenWidth - 32.f, 204.f), ImGuiCond_Always);

    if (ImGui::Begin(u8"コマンドメニュー", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImGui::BeginChild("AllyStatus", ImVec2((float)screenWidth * 0.36f, 0), true);
        ImGui::Text(u8"パーティステータス");
        ImGui::Separator();

        for (auto* c : participants) {
            if (c && c->isAlly == 1) {
                ImGui::BeginGroup();
                ImVec2 iconSize(56, 44);
                ImGui::Button(c->name.substr(0, 1).c_str(), iconSize);

                float hpFraction = c->hp > 0 ? (float)c->currentHp / (float)c->hp : 0.0f;
                ImVec4 hpColor = (hpFraction < 0.2f) ? ImVec4(1, 0.15f, 0.1f, 1) : (hpFraction < 0.5f) ? ImVec4(1, 0.82f, 0.1f, 1) : ImVec4(0.2f, 0.85f, 0.35f, 1);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hpColor);
                ImGui::ProgressBar(hpFraction, ImVec2(70, 9), "");
                ImGui::PopStyleColor();
                ImGui::Text("%d/%d", c->currentHp, c->hp);
                ImGui::EndGroup();
                ImGui::SameLine(0, 16);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("ActionMenu", ImVec2(0, 0), false);

        if (battleState != BattleState::InProgress) {
            ImGui::TextColored(
                battleState == BattleState::Victory ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                battleState == BattleState::Victory ? u8"勝利！" : u8"敗北..."
            );
            ImGui::Separator();
            if (ImGui::Button(u8"もう一度戦う", ImVec2(160, 42))) {
                resetBattle();
            }
        }
        else if (activeChar && activeChar->isAlly == 1) {
            ImGui::Text(u8"【 %s の行動 】", activeChar->name.c_str());
            ImGui::Separator();

            if (ImGui::Button(u8"防御", ImVec2(120, 40))) {
                executeGuard(activeChar);
            }
            ImGui::SameLine();

            for (auto* target : participants) {
                if (target && target->isAlly == 0 && target->currentHp > 0) {
                    if (ImGui::Button(target->name.c_str(), ImVec2(120, 40))) {
                        executeSkill(activeChar, target);
                    }
                    ImGui::SameLine();
                }
            }
        }
        else if (activeChar) {
            ImGui::Text(u8"【 %s（敵）のターン 】", activeChar->name.c_str());
            ImGui::Separator();
            ImGui::Text(u8"敵が行動を選んでいます...");

            if (!enemyActionQueued) {
                enemyActionQueued = true;
                enemyActionTime = ImGui::GetTime() + 0.6;
            }

            if (ImGui::GetTime() >= enemyActionTime) {
                Character* target = getRandomAliveTarget(1);
                if (target) executeSkill(activeChar, target);
            }
        }

        ImGui::Separator();
        ImGui::Text(u8"ログ");
        for (const auto& line : battleLog) {
            ImGui::BulletText("%s", line.c_str());
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

static bool pKeyWasPressed = false;

void processInput(GLFWwindow* window, CombatSystem& combatSystem) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyWasPressed) {
            combatSystem.toggleVisibility();
            pKeyWasPressed = true;
        }
    }
    else {
        pKeyWasPressed = false;
    }
}

void CombatSystem::addParticipant(Character* character) {
    if (character != nullptr) {
        participants.push_back(character);
    }
}

void CombatSystem::executeSkill(Character* attacker, Character* target)
{
    if (!attacker || !target || battleState != BattleState::InProgress) return;

    int damage = attacker->power - (target->defense / 2);
    if (damage < 1) damage = 1;

    bool isCritical = (rand() % 100) < attacker->critical;
    if (isCritical) {
        damage = damage * attacker->criticalDamage / 100;
    }

    if (target->isGuarding) {
        damage /= 2;
        if (damage < 1) damage = 1;
        target->isGuarding = false;
    }

    target->currentHp -= damage;
    if (target->currentHp < 0) target->currentHp = 0;

    std::string log = attacker->name + u8" の攻撃: " + target->name + u8" に " + std::to_string(damage) + u8" ダメージ";
    if (isCritical) log += u8"（クリティカル）";
    addLog(log);

    std::cout << log << std::endl;
    if (target->currentHp == 0) {
        addLog(target->name + u8" を倒した！");
        std::cout << target->name << u8" を倒した！" << std::endl;
    }

    advanceTurn(attacker);
    checkBattleState();
}

void CombatSystem::executeGuard(Character* character)
{
    if (!character || battleState != BattleState::InProgress) return;
    character->isGuarding = true;
    addLog(character->name + u8" は防御した");
    advanceTurn(character);
}

void CombatSystem::chooseCommand(BattleCommand command)
{
    pendingCommand = command;
}

void CombatSystem::executeCommand(Character* attacker, Character* target)
{
    if (!attacker || !target || pendingCommand == BattleCommand::None) return;

    BattleCommand command = pendingCommand;
    pendingCommand = BattleCommand::None;

    int originalPower = attacker->power;
    if (command == BattleCommand::Skill) attacker->power += 8;
    if (command == BattleCommand::Ultimate) attacker->power += 18;

    addLog(attacker->name + " used " + GetCommandName(command));
    executeSkill(attacker, target);

    attacker->power = originalPower;
    if (target->currentHp <= 0 && markedTarget == target) markedTarget = nullptr;
}

void CombatSystem::resetBattle()
{
    for (auto* c : participants) {
        if (!c) continue;
        c->currentHp = c->hp;
        c->isGuarding = false;
        c->turnGauge = c->speed;
    }
    battleState = BattleState::InProgress;
    enemyActionQueued = false;
    enemyActionTime = 0.0;
    pendingCommand = BattleCommand::None;
    markedTarget = nullptr;
    battleLog.clear();
    addLog(u8"戦闘開始！");
    sortTurnOrder();
}

void CombatSystem::sortTurnOrder()
{
    std::sort(participants.begin(), participants.end(), [](Character* a, Character* b) {
        if (!a) return false;
        if (!b) return true;
        if ((a->currentHp > 0) != (b->currentHp > 0)) return a->currentHp > 0;
        return a->turnGauge > b->turnGauge;
        });
}

void CombatSystem::advanceTurn(Character* character)
{
    if (!character) return;

    character->turnGauge -= 100;
    enemyActionQueued = false;
    enemyActionTime = 0.0;

    bool allSlow = true;
    for (auto* c : participants) {
        if (c && c->currentHp > 0 && c->turnGauge > 0) {
            allSlow = false;
            break;
        }
    }
    if (allSlow) {
        for (auto* c : participants) {
            if (c && c->currentHp > 0) c->turnGauge += c->speed;
        }
    }

    sortTurnOrder();
}

void CombatSystem::checkBattleState()
{
    bool hasAlly = false;
    bool hasEnemy = false;
    for (auto* c : participants) {
        if (!c || c->currentHp <= 0) continue;
        if (c->isAlly == 1) hasAlly = true;
        else hasEnemy = true;
    }

    if (!hasEnemy) battleState = BattleState::Victory;
    else if (!hasAlly) battleState = BattleState::Defeat;
}

Character* CombatSystem::getActiveCharacter()
{
    sortTurnOrder();
    for (auto* c : participants) {
        if (c && c->currentHp > 0) return c;
    }
    return nullptr;
}

Character* CombatSystem::getRandomAliveTarget(int isAlly)
{
    std::vector<Character*> targets;
    for (auto* c : participants) {
        if (c && c->isAlly == isAlly && c->currentHp > 0) {
            targets.push_back(c);
        }
    }
    if (targets.empty()) return nullptr;
    return targets[rand() % targets.size()];
}

void CombatSystem::addLog(const std::string& text)
{
    battleLog.insert(battleLog.begin(), text);
    if (battleLog.size() > 5) battleLog.pop_back();
}
