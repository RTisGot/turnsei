#include <glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "Character.h"
#include "CombatSystem.h"
#include "Scene.h"
#include "../../assets/ImportedModel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>

// --------------------------------------------------
// ターン順表示
// --------------------------------------------------
void CombatSystem::displayTurnOrder()
{
    sortTurnOrder();

    //全員を順番に並べる
    for (size_t i = 0; i < participants.size(); i++)
    {
        if (!participants[i] || participants[i]->currentHp <= 0) continue;//キャラが存在しないか、HPがなかったら返す
        std::cout << (i + 1) << u8": " << participants[i]->name << std::endl;
    }
}
//----------------------------------------------------
//表示を切り替える
void CombatSystem::toggleVisibility()
{
    isVisible = !isVisible;
}

//
//コマンド切り替え
//
static const char* GetCommandName(BattleCommand command)
{
    switch (command) {
    case BattleCommand::BasicAttack: return "Basic";//基本攻撃
    case BattleCommand::Skill: return "Skill";      //スキル
    case BattleCommand::Ultimate: return "Ultimate";//必殺
    default: return "None";
    }
}

//
//戦闘画面か分ける
//
void CombatSystem::renderUI(int screenWidth, int screenHeight)
{
    if (!isVisible || participants.empty()) return;

    sortTurnOrder();
    checkBattleState();

    Character* activeChar = getActiveCharacter();//現在行動中のキャラクターのポインタを取得
    if (battleState != BattleState::InProgress) {//戦闘が終わった後の場合
        if (!battleEndQueued) {                  
            battleEndQueued = true;              //戦闘処理を開始
            battleEndStartTime = ImGui::GetTime();//
            battleEndResult = battleState;
            pendingCommand = BattleCommand::None;
            enemyActionQueued = false;
        }

        renderBattleScene(activeChar, screenWidth, screenHeight);//戦闘シーンの描画
        renderBattleEndOverlay(screenWidth, screenHeight);
        if (ImGui::GetTime() - battleEndStartTime >= 2.4) {
            returnToFieldAfterBattle();
        }
        return;
    }
    if (markedTarget && markedTarget->currentHp <= 0) markedTarget = nullptr;
    if (!activeChar || battleState != BattleState::InProgress) {
        pendingCommand = BattleCommand::None;
    }

    renderBattleScene(activeChar, screenWidth, screenHeight);
    renderBattleCards(activeChar, (float)screenWidth, 18.f, 18.f, 210.0f, 48.0f, 5.f);
    renderActionMenu(activeChar, screenWidth, screenHeight);
    renderBattleLogWindow(screenWidth, screenHeight);
}
static bool pKeyWasPressed = false;

//キーボード入力でゲームの処理を行う関数
void processInput(GLFWwindow* window, CombatSystem& combatSystem) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        //ウィンドウを閉じる処理
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
//戦闘に参加するキャラクターを追加する
void CombatSystem::addParticipant(Character* character) {
    if (character != nullptr) {
        participants.push_back(character);
    }
}
//----------
//スキル攻撃の処理
//----
void CombatSystem::executeSkill(Character* attacker, Character* target)
{
    if (!attacker || !target || battleState != BattleState::InProgress) return;

    //攻撃力から対象の防御力を引く
    int damage = attacker->power - (target->defense / 2);
    if (damage < 1) damage = 1;            //ダメージが1より小さければ1にする


    bool isCritical = (rand() % 100) < attacker->critical;//クリティカルが発生する割合をランダムに
    
    if (isCritical) {
        damage = damage * attacker->criticalDamage / 100;//クリティカルが出たときのダメージ計算
    }

    if (target->isGuarding) {
        damage /= 2;
        if (damage < 1) damage = 1;
        target->isGuarding = false;
    }

    target->currentHp -= damage;//現在のHPからdamage分を引く
    if (target->currentHp < 0) target->currentHp = 0;

    DamagePopup popup;
    popup.target = target;         //ダメージを与えた相手
    popup.amount = damage;         //ダメージを計算を保存
    popup.isCritical = isCritical;
    popup.startTime = ImGui::GetTime();
    popup.xOffset = static_cast<float>((rand() % 41) - 20);
    damagePopups.push_back(popup);

    std::string log = attacker->name + " attacked " + target->name + " for " + std::to_string(damage) + " damage";
    if (isCritical) log += " (critical)";
    addLog(log);

    std::cout << log << std::endl;
    if (target->currentHp == 0) {
        addLog(target->name + " defeated");
        std::cout << target->name << " defeated" << std::endl;
    }

    advanceTurn(attacker);
    checkBattleState();
}

void CombatSystem::executeGuard(Character* character)
{
    if (!character || battleState != BattleState::InProgress) return;
    character->isGuarding = true;
    addLog(character->name + " guarded");
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

    if (command == BattleCommand::Skill && attacker->charge < 2) {
        addLog(attacker->name + ": not enough charge");
        return;
    }

    int originalPower = attacker->power;
    if (command == BattleCommand::Skill) {
        attacker->power += 10;
        attacker->charge -= 2;
    }
    if (command == BattleCommand::Ultimate) {
        attacker->power += 18;
        attacker->energy = 0;
    }

    if (command == BattleCommand::BasicAttack) {
        attacker->energy = std::min(attacker->energy + 20, attacker->maxEnergy);
        attacker->charge = std::min(attacker->charge + 1, attacker->maxCharge);
    }
    else if (command == BattleCommand::Skill) {
        attacker->energy = std::min(attacker->energy + 30, attacker->maxEnergy);
    }

    addLog(attacker->name + " used " + GetCommandName(command));
    executeSkill(attacker, target);

    attacker->power = originalPower;
    if (target->currentHp <= 0 && markedTarget == target) markedTarget = nullptr;
}

//戦闘勝利フラグを受け取る
bool CombatSystem::consumeBattleVictory()
{
    bool won = lastBattleVictory;
    lastBattleVictory = false;
    return won;
}

void CombatSystem::resetBattle()
{
    for (auto it = participants.begin(); it != participants.end(); ) {
        if (*it && (*it)->isAlly == 0) {
            delete *it;
            it = participants.erase(it);
        }
        else {
            ++it;
        }
    }

    const char* enemyNames[] = { "Enemy A", "Enemy B", "Enemy C" };
    int enemyHp[]      = { 80 + rand() % 10,80 + rand() % 10,80 +  rand() % 10};
    int enemyPower[]    = { 1 + rand() % 10, 1 + rand() % 10, 1 + rand() % 10};
    int enemyDefense[]  = { 2, 6, 7 };
    int enemySpeed[]    = { 8, 12, 14 };
    int enemyCrit[]     = { 5, 8, 10 };
    int enemyCritDmg[]  = { 130, 140, 145 };

    int enemyCount = 1 + rand() % 3;
    for (int i = 0; i < enemyCount; ++i) {
        Character* enemy = new Character{
            enemyNames[i], enemyHp[i], enemyPower[i], enemyDefense[i],
            enemySpeed[i], enemyCrit[i], enemyCritDmg[i], enemyHp[i], 0
        };
        addParticipant(enemy);
    }

    for (auto* c : participants) {
        if (!c) continue;
        c->currentHp = c->hp;
        c->isGuarding = false;
        c->turnGauge = c->speed;
        c->charge = 0;
        c->energy = 0;
    }
    battleState = BattleState::InProgress;
    enemyActionQueued = false;
    enemyActionTime = 0.0;
    battleEndQueued = false;
    battleEndStartTime = 0.0;
    battleEndResult = BattleState::InProgress;
    lastBattleVictory = false;
    pendingCommand = BattleCommand::None;
    markedTarget = nullptr;
    damagePopups.clear();
    battleLog.clear();
    addLog("Battle start");
    sortTurnOrder();
}

void CombatSystem::returnToFieldAfterBattle()
{
    bool won = battleEndResult == BattleState::Victory;

    for (auto it = participants.begin(); it != participants.end(); ) {
        if (*it && (*it)->isAlly == 0) {
            delete *it;
            it = participants.erase(it);
        }
        else {
            if (*it) {
                (*it)->currentHp = (*it)->hp;
                (*it)->isGuarding = false;
                (*it)->turnGauge = (*it)->speed;
                (*it)->charge = 0;
                (*it)->energy = 0;
            }
            ++it;
        }
    }

    battleState = BattleState::InProgress;
    enemyActionQueued = false;
    enemyActionTime = 0.0;
    battleEndQueued = false;
    battleEndStartTime = 0.0;
    battleEndResult = BattleState::InProgress;
    lastBattleVictory = won;
    pendingCommand = BattleCommand::None;
    markedTarget = nullptr;
    damagePopups.clear();
    battleLog.clear();
    currentPoints = 0;
    currentScene = Scene::Field;
}

//
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
