#pragma once

#include <glfw3.h>
#include <vector>
//#include "Character.h"

struct Character;//キャラクター構造体の前方宣言
class CombatSystem
{
	
public:
	bool isVisible = true;                                        //可視化
	std::vector<Character*>participants;                          //参加者リスト
	void toggleVisibility();                                     // 可視化の切り替え
	void displayTurnOrder();                                     //行動順の表示
	void renderUI(int screenWidth, int screenHeight);           //UIの描画
	void addParticipant(Character* character);                  //参加者の追加関数
	void executeSkill(Character* attacker, Character* target);
};

//skill構造体
struct Skill {
	std::string name;//スキル名
	int power;       //スキルの威力
	int cost;        //スキルのコスト
};
void processInput(GLFWwindow* window, CombatSystem& combatSystem);//入力処理関数