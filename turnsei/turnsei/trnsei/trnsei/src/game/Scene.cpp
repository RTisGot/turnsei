#include <glew.h>
#include "Scene.h"
#include "Title.h"
#include "StoryEvent.h"
#include "CombatSystem.h"
#include "Field.h"
#include "Player.h"
#include "imgui.h"
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>



Player g_Player;

// Assimpで読み込んだシーンデータを保持するポインタ
const aiScene* g_WetlandScene = nullptr;
Assimp::Importer g_Importer;
//シーン遷移を管理する

//現在のシーンを保持する変数(Title)
Scene currentScene = Scene::Title;

void SceneUpdate(Scene nextScene)
{
	currentScene = nextScene;//次の画面へ遷移
}

static void ResultUpdate(int screenWidth, int screenHeight)
{
	glDisable(GL_DEPTH_TEST);
	glClearColor(0.015f, 0.018f, 0.030f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	drawList->AddRectFilled(
		ImVec2(0.0f, 0.0f),
		ImVec2((float)screenWidth, (float)screenHeight),
		IM_COL32(4, 8, 20, 255)
	);
	drawList->AddRectFilled(
		ImVec2(0.0f, (float)screenHeight * 0.58f),
		ImVec2((float)screenWidth, (float)screenHeight),
		IM_COL32(18, 30, 45, 255)
	);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBackground;
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2((float)screenWidth, (float)screenHeight), ImGuiCond_Always);
	if (ImGui::Begin("##GameClear", nullptr, flags)) {
		ImGui::SetCursorPosY((float)screenHeight * 0.34f);
		const char* title = "GAME CLEAR";
		float titleWidth = ImGui::CalcTextSize(title).x;
		ImGui::SetCursorPosX(((float)screenWidth - titleWidth) * 0.5f);
		ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.72f, 1.0f), "%s", title);

		ImGui::Spacing();
		const char* message = "All enemies defeated";
		float messageWidth = ImGui::CalcTextSize(message).x;
		ImGui::SetCursorPosX(((float)screenWidth - messageWidth) * 0.5f);
		ImGui::TextColored(ImVec4(0.92f, 0.95f, 1.0f, 1.0f), "%s", message);
	}
	ImGui::End();
}

void MainUpdate(CombatSystem& combatSystem, GLFWwindow* window) {
	int screenWidth = 1280;
	int screenHeight = 720;
	if (window) {
		glfwGetWindowSize(window, &screenWidth, &screenHeight);
	}

	switch (currentScene) {
	case Scene::Title:
		TitleUpdate();
		break;
	case Scene::StoryEvent:
		StoryEvent();
		break;
	case Scene::Field:
		FieldUpdate(combatSystem);
		break;
	case Scene::Battle:
		combatSystem.renderUI(screenWidth, screenHeight);
		break;
	case Scene::Result:
		ResultUpdate(screenWidth, screenHeight);
		break;
	}
}
