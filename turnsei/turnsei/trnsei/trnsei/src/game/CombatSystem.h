#pragma once
#define GLFW_INCLUDE_NONE
#include <imgui.h>
#include <functional>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

struct Character;

enum class BattleState {
    InProgress,
    Victory,
    Defeat
};

enum class BattleCommand {
    None,
    BasicAttack,
    Skill,
    Ultimate
};

class CombatSystem
{
public:
    bool isVisible = true;
    std::vector<Character*> participants;
    BattleState battleState = BattleState::InProgress;

    void toggleVisibility();
    void displayTurnOrder();
    void renderUI(int screenWidth, int screenHeight);
    void addParticipant(Character* character);
    void executeSkill(Character* attacker, Character* target);
    void executeGuard(Character* character);
    void resetBattle();
    bool consumeBattleVictory();
    void DrawStyledButton(const char* label, const char* desc, ImVec4 color, float width, float height, std::function<void()> onClick);

private:
    int currentPoints = 0;
    int maxPoints = 3;
    bool enemyActionQueued = false;
    double enemyActionTime = 0.0;
    bool battleEndQueued = false;
    double battleEndStartTime = 0.0;
    BattleState battleEndResult = BattleState::InProgress;
    bool lastBattleVictory = false;
    bool isBattleLogOpen = false;
    BattleCommand pendingCommand = BattleCommand::None;
    Character* markedTarget = nullptr;
    std::vector<std::string> battleLog;
    struct DamagePopup {
        Character* target = nullptr;
        int amount = 0;
        bool isCritical = false;
        double startTime = 0.0;
        float xOffset = 0.0f;
    };
    std::vector<DamagePopup> damagePopups;

    void sortTurnOrder();
    void advanceTurn(Character* character);
    void checkBattleState();
    Character* getActiveCharacter();
    Character* getRandomAliveTarget(int isAlly);
    void renderBattleScene(Character* activeChar, int screenWidth, int screenHeight);
    void renderBattleEndOverlay(int screenWidth, int screenHeight);
    void renderBattleCards(Character* activeChar, float screenWidth, float marginX, float marginY, float cardWidth, float cardHeight, float spacingY);
    void renderActionMenu(Character* activeChar, int screenWidth, int screenHeight);
   
    void renderBattleLogWindow(int screenWidth, int screenHeight);
    void chooseCommand(BattleCommand command);
    void executeCommand(Character* attacker, Character* target);
    void addLog(const std::string& text);
    void returnToFieldAfterBattle();
};

struct Skill {
    std::string name;
    int power;
    int cost;
};

void processInput(GLFWwindow* window, CombatSystem& combatSystem);
