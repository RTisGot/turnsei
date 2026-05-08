#pragma once

//@brief
#include<string>
#include<map>
//Character構造体の定義(味方も敵も)

struct Character
{
	std::string name;   //Characterの名前
	int hp;             //CharacterのHP
	int power;          //Characterの攻撃力
	int defense;        //Characterの防御力
	int speed;          //Characterの素早さ
	int critical;       //Characterのクリティカル率
	int criticalDamage; //Characterのクリティカルダメージ倍率

	int currentHp;   //Characterの現在のHP
	int isAlly;     //Characterが味方かどうかを示すフラグ（0:敵, 1:味方）


	bool isEnemy() const;

	
};
