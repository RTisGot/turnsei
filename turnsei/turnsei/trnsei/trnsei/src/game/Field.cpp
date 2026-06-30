#include "Field.h"
#include "ActorRenderer.h"
#include "CombatSystem.h"
#include "FieldEnemy.h"
#include "Player.h"
#include "Scene.h"
#include "SimpleMap.h"
#include "../../assets/ImportedModel.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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
    ImportedModel playerModel;
    bool playerModelLoadAttempted = false;
    float cameraYaw = 0.0f;
    float cameraPitch = glm::radians(32.0f);
    float cameraDistance = 20.0f;
    bool encounterTransitionActive = false;
    float encounterTransitionTime = 0.0f;
    const float encounterTransitionDuration = 0.82f;
    glm::vec3 encounterTargetPosition(0.0f);
    FieldEnemy fieldEnemies[MAX_FIELD_ENEMIES];
    WalkAnimState playerWalkAnim;
    WalkAnimState enemyWalkAnim[MAX_FIELD_ENEMIES];

    bool FileExists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    float Saturate(float v) { return std::max(0.0f, std::min(v, 1.0f)); }
    float SmoothStep(float v) { v = Saturate(v); return v * v * (3.0f - 2.0f * v); }

    void LoadPlayerModel()
    {
        if (playerModelLoadAttempted) return;
        playerModelLoadAttempted = true;

        const char* exts[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };
        const char* names[] = { "Character", "Player", "Hero" };
        for (const char* root : roots)
            for (const char* name : names)
                for (const char* ext : exts) {
                    std::string path = std::string(root) + name + ext;
                    if (FileExists(path) && playerModel.load(path)) return;
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

    void DrawEncounterOverlay(int width, int height, float progress)
    {
        if (!encounterTransitionActive) return;
        float eased = SmoothStep(progress);
        float flash = std::max(0.0f, 1.0f - progress * 5.0f);
        float fade = SmoothStep((progress - 0.42f) / 0.58f);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled({0,0}, {(float)width,(float)height}, IM_COL32(255,246,210,(int)(flash*92)));
        dl->AddRectFilled({0,0}, {(float)width,(float)height}, IM_COL32(4,8,14,(int)(fade*236)));
        float lineAlpha = Saturate(1.0f - std::abs(eased - 0.55f) * 2.0f) * 145.0f;
        float cy = (float)height * 0.5f;
        dl->AddLine({(float)width*0.18f, cy}, {(float)width*0.82f, cy}, IM_COL32(255,214,124,(int)lineAlpha), 2.0f);
    }

    bool AllFieldEnemiesDefeated()
    {
        for (int i = 0; i < MAX_FIELD_ENEMIES; ++i) {
            if (fieldEnemies[i].active) return false;
        }
        return true;
    }

    bool StartEncounterIfNeeded(CombatSystem& cs)
    {
        if (encounterTransitionActive) return false;
        for (int i = 0; i < MAX_FIELD_ENEMIES; ++i) {
            if (!fieldEnemies[i].active) continue;
            glm::vec2 pxz(player.position.x, player.position.z);
            glm::vec2 exz(fieldEnemies[i].position.x, fieldEnemies[i].position.z);
            if (glm::distance(pxz, exz) <= 2.0f) {
                fieldEnemies[i].active = false;
                encounterTransitionActive = true;
                encounterTransitionTime = 0.0f;
                encounterTargetPosition = fieldEnemies[i].position;
                return true;
            }
        }
        return false;
    }
}

void FieldInit()
{
    static bool done = false;
    if (done) return;
    done = true;

    player.position = glm::vec3(0.0f, 0.0f, 7.0f);
    player.rotationY = 0.0f;
    InitActorRenderer();
    InitSimpleMap();
    InitFieldEnemies(fieldEnemies);
    LoadPlayerModel();
}

void FieldUpdate(CombatSystem& combatSystem)
{
    float currentTime = (float)glfwGetTime();
    deltaTime = previousFrameTime > 0.0f ? currentTime - previousFrameTime : 0.0f;
    previousFrameTime = currentTime;

    if (combatSystem.consumeBattleVictory() && AllFieldEnemiesDefeated()) {
        currentScene = Scene::Result;
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (encounterTransitionActive) {
        encounterTransitionTime += deltaTime;
    }
    else {
        player.Update(deltaTime, window, cameraYaw);
        player.position.y = GetTerrainHeight(player.position.x, player.position.z);
        UpdateCameraInput();
        UpdateFieldEnemies(fieldEnemies, deltaTime);
        for (int i = 0; i < MAX_FIELD_ENEMIES; ++i)
            if (fieldEnemies[i].active)
                fieldEnemies[i].position.y = GetTerrainHeight(fieldEnemies[i].position.x, fieldEnemies[i].position.z);
    }
    StartEncounterIfNeeded(combatSystem);

    int fbW = 1280, fbH = 720;
    if (window) glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW <= 0 || fbH <= 0) return;
    glViewport(0, 0, fbW, fbH);

    float encProgress = encounterTransitionActive ? Saturate(encounterTransitionTime / encounterTransitionDuration) : 0.0f;
    float encEase = SmoothStep(encProgress);
    glm::vec3 camTarget = player.position + glm::vec3(0, 1, 0);
    if (encounterTransitionActive)
        camTarget = glm::mix(camTarget, encounterTargetPosition + glm::vec3(0,1.15f,0), encEase * 0.55f);

    float camDist = encounterTransitionActive ? glm::mix(cameraDistance, 7.4f, encEase) : cameraDistance;
    glm::vec3 camOff(camDist*std::cos(cameraPitch)*std::sin(cameraYaw),
                     camDist*std::sin(cameraPitch),
                     camDist*std::cos(cameraPitch)*std::cos(cameraYaw));
    glm::vec3 camPos = camTarget + camOff;
    if (encounterTransitionActive) {
        float shake = (1.0f - encEase) * 0.18f;
        camPos.x += std::sin(encounterTransitionTime * 68.0f) * shake;
        camPos.y += std::cos(encounterTransitionTime * 53.0f) * shake * 0.55f;
    }
    glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0,1,0));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)fbW/(float)fbH, 0.1f, 500.0f);

    DrawSimpleMap(view, proj, camPos);

    bool moving = player.isMoving();
    bool hasPlayerWalkClip = false;
    bool advancePlayerModelAnimation = false;
    if (moving) {
        hasPlayerWalkClip =
            playerModel.playAnimationByName("walk") ||
            playerModel.playAnimationByName("run") ||
            playerModel.playAnimationByName("locomotion") ||
            playerModel.playAnimationByName("move") ||
            playerModel.playAnimationByName("mixamo") ||
            playerModel.playAnimationByName("take");
        advancePlayerModelAnimation = hasPlayerWalkClip;
    }
    else {
        if (playerModel.playAnimationByName("idle") ||
            playerModel.playAnimationByName("stand")) {
            advancePlayerModelAnimation = true;
        }
        else {
            playerModel.resetAnimationPose();
        }
    }
    UpdateWalkAnimation(playerWalkAnim, deltaTime, moving && !hasPlayerWalkClip);
    if (advancePlayerModelAnimation) {
        playerModel.updateAnimation(deltaTime);
    }

    glm::vec3 pScale = playerModel.isLoaded() ? glm::vec3(2.9f,3.0f,2.9f) : glm::vec3(1.1f,2.4f,1.1f);
    glm::vec3 pColor = playerModel.isLoaded() ? glm::vec3(0.92f,0.76f,0.58f) : glm::vec3(0.12f,0.55f,1.0f);
    DrawActor(player.position, pScale, pColor, view, proj, camPos, player.rotationY, &playerModel, moving, &playerWalkAnim);

    for (int i = 0; i < MAX_FIELD_ENEMIES; ++i) {
        if (!fieldEnemies[i].active) continue;
        UpdateWalkAnimation(enemyWalkAnim[i], deltaTime, fieldEnemies[i].moving);
        DrawActor(fieldEnemies[i].position, glm::vec3(1.35f,2.1f,1.35f), glm::vec3(0.9f,0.12f,0.1f),
                  view, proj, camPos, fieldEnemies[i].rotationY, nullptr, fieldEnemies[i].moving, &enemyWalkAnim[i]);
    }
    glBindVertexArray(0);

    DrawEncounterOverlay(fbW, fbH, encProgress);
    if (encounterTransitionActive && encounterTransitionTime >= encounterTransitionDuration) {
        encounterTransitionActive = false;
        combatSystem.resetBattle();
        currentScene = Scene::Battle;
    }
}

float getDeltaTime() { return deltaTime; }
