#include "AIDialogue.h"

#include <thread>

namespace
{
    std::string BuildFallbackResponse(
        const std::string& playerInput,
        const CharacterPersonality& personality,
        const DialogueContext& context)
    {
        std::string response = personality.name.empty() ? "Adventurer" : personality.name;
        response += ": ";

        if (context.inBattle) {
            if (context.isPlayerTurn) {
                response += "Stay sharp. Pick your move and we can turn this around.";
            }
            else {
                response += "Brace yourself. The enemy is about to move.";
            }
        }
        else if (playerInput.empty()) {
            response += "こんにちは。何か知りたいことがあれば話しかけてください。";
        }
        else if (personality.name == "CPU Guide") {
            response += "「";
            response += playerInput;
            response += "」ですね。今は3Dマップ上の案内役として、近くの敵や進む方向、ゲームの操作について答えられます。";
        }
        else {
            response += "「";
            response += playerInput;
            response += "」ですね。もう少し詳しく聞かせてください。";
        }

        if (!personality.catchphrase.empty()) {
            response += " ";
            response += personality.catchphrase;
        }

        return response;
    }
}

std::string CallLocalDialogue(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context)
{
    return BuildFallbackResponse(playerInput, personality, context);
}

void CallLocalDialogueAsync(
    const std::string& playerInput,
    const CharacterPersonality& personality,
    const DialogueContext& context,
    DialogueCallback callback)
{
    std::thread([playerInput, personality, context, callback]() {
        if (callback) {
            callback(CallLocalDialogue(playerInput, personality, context));
        }
    }).detach();
}

CharacterPersonality GetPersonalityForCharacter(const std::string& characterName)
{
    if (characterName == "Hero" || characterName == "Yuki" || characterName == "Player") {
        return { characterName, "warrior", "casual", "brave and straightforward", "Let's go!" };
    }
    if (characterName == "Mia" || characterName == "Aria") {
        return { characterName, "mage", "polite", "calm and thoughtful", "I see..." };
    }
    if (characterName == "Rex" || characterName == "Garth") {
        return { characterName, "warrior", "gruff", "quiet and dependable", "..." };
    }

    return { characterName, "adventurer", "casual", "flexible and friendly", "" };
}
