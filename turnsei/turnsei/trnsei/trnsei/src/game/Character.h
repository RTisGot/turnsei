#pragma once

//@brief
#include<string>
#include<map>
//

struct Character
{
	std::string name;   
	int hp;             
	int power;          
	int defense;        
	int speed;          
	int critical;       
	int criticalDamage; 

	int currentHp;
	int isAlly;
	bool isGuarding = false;
	int turnGauge = 0;

	int charge = 0;
	int maxCharge = 5;
	int energy = 0;
	int maxEnergy = 100;


	bool isEnemy() const;


};
