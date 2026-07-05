#pragma once
#include <string>
#include <functional>

// キャラクターの性格パラメータ
struct CharacterPersonality {
    std::string name;
    std::string role;        // "warrior", "mage", "healer" etc.
    std::string speechStyle; // "formal", "casual", "gruff", "cheerful"
    std::string traits;      // 自由記述の性格説明
    std::string catchphrase; // 口癖 (省略可)
};

// AIに渡すゲーム状態コンテキスト
struct DialogueContext {
    bool inBattle = false;
    int playerHp = 100;
    int playerMaxHp = 100;
    int enemyHp = 0;
    int enemyMaxHp = 100;
    std::string enemyName;
    int turnNumber = 1;
    bool isPlayerTurn = true;
};

using DialogueCallback = std::function<void(const std::string& response)>;

// 同期呼び出し (フレームがブロックされる)
std::string CallClaudeDialogue(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context
);

// 非同期呼び出し (バックグラウンドスレッドで実行し、完了時にcallbackを呼ぶ)
void CallClaudeDialogueAsync(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context,
    DialogueCallback callback
);

// キャラクター名から性格を返すヘルパー
CharacterPersonality GetPersonalityForCharacter(const std::string& characterName);
