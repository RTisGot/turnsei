//タイトル画面に描画する内容

#include "Title.h"
#include "imgui.h"
#include "Scene.h"
#include <iostream>

//extern int g_Scene;
extern bool g_StoryNeedsLoad;

void TitleUpdate()
{
	ImGui::SetNextWindowPos(ImVec2(0, 0)); // 
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	//ImGui
	ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	//
	if (ImGui::Button("Click to Start", ImVec2(200, 100))) {
		std::cout << "Game Start" << std::endl;
		currentScene = Scene::StoryEvent;
		g_StoryNeedsLoad = true;
		if (currentScene == Scene::StoryEvent) {
			std::cout << "Scene change success!" << std::endl;
		}
	}

	if (ImGui::Button("Battle Demo", ImVec2(200, 100)))
	{
		currentScene = Scene::Battle;
	}

	if (ImGui::Button("Setting", ImVec2(200, 100)))
	{
		std::cout << "Setting" << std::endl;

	}

	if (ImGui::Button("Exit", ImVec2(200, 100)))
	{
		std::cout << "Exit" << std::endl;
		exit(0);
	}

	ImGui::End();
}
