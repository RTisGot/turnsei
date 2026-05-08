#include "Scene.h"
#include "Title.h"
#include "StoryEvent.h"
#include "CombatSystem.h"
#include "Field.h"
#include "Player.h"
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

void MainUpdate(CombatSystem& combatSystem, GLFWwindow* window) {
	std::cout << "MainUpdate currentScene: " << (int)currentScene << std::endl;
	switch (currentScene) {
	case Scene::Title:
		TitleUpdate();
		break;
	case Scene::StoryEvent:
		StoryEvent();
		break;
	case Scene::Field:
		FieldUpdate();
		break;
	case Scene::Battle:
		combatSystem.renderUI(1280, 780);
		break;
	}
}