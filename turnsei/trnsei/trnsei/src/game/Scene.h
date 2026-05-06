#pragma once
#include <string>
class CombatSystem;
// シーンの種類を定義する列挙型
enum class Scene {
	StoryEvent, //ストーリー
	Title, //タイトル画面
	Field, //3D探索
	Battle,//戦闘画面
	Gacha, //ガチャ画面
	Result //戦闘後のリザルト画面
};

//次の画面へ遷移させる関数
//nextScene 次の画面
void SceneUpdate(Scene nextScene);

//Title画面から次の画面へ遷移させるための関数
void MainUpdate(CombatSystem& combatSystem);

//シーンの列挙型によるステート管理
extern Scene currentScene;
	
struct StoryTrigger {
	glm::vec3 position; // イベントが起きる座標
	float radius;       // 判定の広さ
	bool isUsed;        // 一度だけ発生させるためのフラグ
};

