/*
 * このプロジェクトでは nlohmann/json を使用しています。
 * nlohmann/json is licensed under the MIT License.
 * Copyright (c) 2013-2022 Niels Lohmann
 * https://github.com/nlohmann/json
 */

#include "StoryEvent.h"
#include "AIDialogue.h"
#include "Scene.h"
#include "../../imgui/imgui.h"
#include "../../json.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>
#include <vector>


int g_Scene = 0;
bool g_StoryNeedsLoad = false;
static size_t g_currentIndex = 0;
using json = nlohmann::json;
char g_playerName[32] = "プレイヤー"; // 入力用バッファ
std::string g_CurrentEventID = "Intro";
extern bool g_isNamingPhase = false;   // 名前入力中かどうか


struct Message {
	std::string name;   //json側の名前
	std::string text;  //         文字列
};

static std::vector<Message> g_messages;

struct StoryAIDialogueState {
    bool isOpen = false;
    char inputBuf[512] = {};
    std::string speaker;
    std::string displayResponse;
    bool waiting = false;
    std::mutex mutex;
    std::string pendingResponse;
    std::atomic<bool> hasNewResponse{ false };
};

static StoryAIDialogueState g_storyAi;

static void RenderStoryAIDialogue(const std::string& speakerName)
{
    if (speakerName.empty() || speakerName == "SYSTEM_NAMING") return;

    if (g_storyAi.speaker != speakerName) {
        g_storyAi.speaker = speakerName;
        g_storyAi.displayResponse.clear();
        g_storyAi.inputBuf[0] = '\0';
        g_storyAi.waiting = false;
        g_storyAi.hasNewResponse.store(false);
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("AI Talk")) {
        g_storyAi.isOpen = !g_storyAi.isOpen;
    }

    if (!g_storyAi.isOpen) return;

    if (g_storyAi.hasNewResponse.load()) {
        std::lock_guard<std::mutex> lock(g_storyAi.mutex);
        g_storyAi.displayResponse = g_storyAi.pendingResponse;
        g_storyAi.waiting = false;
        g_storyAi.hasNewResponse.store(false);
    }

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    float dlgW = std::min(500.0f, screenSize.x * 0.48f);
    float dlgH = 260.0f;
    ImGui::SetNextWindowPos(ImVec2(screenSize.x - dlgW - 30.0f, 80.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dlgW, dlgH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.08f, 0.14f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.55f, 0.80f, 0.70f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    bool dlgOpen = true;
    if (ImGui::Begin("##StoryAIDialogue", &dlgOpen,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImGui::TextColored(ImVec4(0.55f, 0.88f, 1.0f, 1.0f), "[ %s ]", speakerName.c_str());
        ImGui::SameLine(dlgW - 64.0f);
        if (ImGui::SmallButton("Close")) {
            dlgOpen = false;
            g_storyAi.isOpen = false;
        }
        ImGui::Separator();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.04f, 0.08f, 0.60f));
        ImGui::BeginChild("##StoryAIResponse", ImVec2(0, 120.0f), true);
        if (g_storyAi.waiting) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Thinking...");
        }
        else if (!g_storyAi.displayResponse.empty()) {
            ImGui::TextWrapped("%s", g_storyAi.displayResponse.c_str());
        }
        else {
            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.55f, 1.0f), "Talk to this character.");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SetNextItemWidth(dlgW - 112.0f);
        bool enterPressed = ImGui::InputText("##StoryAIInput", g_storyAi.inputBuf, sizeof(g_storyAi.inputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        bool canSend = !g_storyAi.waiting && g_storyAi.inputBuf[0] != '\0';
        if (!canSend) ImGui::BeginDisabled();
        bool sendClicked = ImGui::Button("Send", ImVec2(70.0f, 0.0f));
        if (!canSend) ImGui::EndDisabled();

        if (canSend && (sendClicked || enterPressed)) {
            std::string input = g_storyAi.inputBuf;
            g_storyAi.inputBuf[0] = '\0';
            g_storyAi.waiting = true;
            g_storyAi.displayResponse.clear();

            DialogueContext ctx;
            ctx.inBattle = false;
            CharacterPersonality persona = GetPersonalityForCharacter(speakerName);
            CallLocalDialogueAsync(input, persona, ctx, [](const std::string& resp) {
                std::lock_guard<std::mutex> lock(g_storyAi.mutex);
                g_storyAi.pendingResponse = resp;
                g_storyAi.hasNewResponse.store(true);
            });
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    if (!dlgOpen) g_storyAi.isOpen = false;
}

void StoryEvent() {
    if (g_StoryNeedsLoad) {
        LoadStoryData("Intro");
        g_currentIndex = 0;       //セリフの最初を一行目にコピー  
        g_StoryNeedsLoad = false; // ロード完了！
    }

    
    // メッセージウィンドウを表示（UpdateStoryを呼ぶ）
    UpdateStory();
}

//セリフデータをメモリにコピー
void LoadStoryData(std::string storyID)
{
    //ファイル読み込み用
    std::ifstream file("data.json");
    if (!file) {
        std::cout << "[ERROR] Could not open data.json!" << std::endl;
        return;
    }
    std::cout << "[SUCCESS] data.json opened!" << std::endl;
    json data;
    file >> data; // データを流し込む

    g_messages.clear();
    // JSONの配列をループで回して構造体に詰め込む
    for (const auto& event : data["events"]) {
        if (event["id"] == storyID) {
            for(const auto& item : event["messages"]){
                Message msg;
                msg.name = item["name"].get<std::string>(); // 型を明示して取得
                msg.text = item["text"].get<std::string>();
                g_messages.push_back(msg);
            }
            break;
        }
       
    }
}

void UpdateStory()
{
    //messageが空,または
    if (g_messages.empty() || g_currentIndex >= g_messages.size()) {
        currentScene = Scene::Field;
        return;
    }
    const auto& msg = g_messages[g_currentIndex];

    // --- 名前入力の判定 ---
    if (msg.name == "SYSTEM_NAMING" && !g_isNamingPhase) {
        g_isNamingPhase = true;
    }

    if (g_isNamingPhase) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        // 名前入力は少し目立つウィンドウにするためフラグを調整
        ImGui::Begin("Name Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("名前を入力してください");
        ImGui::InputText("##name", g_playerName, IM_ARRAYSIZE(g_playerName));

        if (ImGui::Button("決定", ImVec2(120, 0))) {
            if (strlen(g_playerName) > 0) {
                g_isNamingPhase = false;
                g_currentIndex++;
            }
        }
        ImGui::End();
        return; 
    }

    // --- スタイル設定 ---
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    float windowWidth = screenSize.x * 0.8f;
    float windowHeight = 150.0f;
    ImVec2 pos = ImVec2((screenSize.x - windowWidth) * 0.5f, screenSize.y - windowHeight - 50.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

    // --- セリフウィンドウの表示 ---
    ImGui::Begin("DialogueWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    // キャラクター名表示のカスタマイズ
    // 名前が "PLAYER" なら入力した名前に置き換える処理を追加
    std::string displayName = msg.name;
    if (displayName == "PLAYER") displayName = g_playerName;

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[ %s ]", displayName.c_str());
    RenderStoryAIDialogue(displayName);
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    ImGui::TextWrapped("%s", msg.text.c_str());

    // クリック待ちアイコン
    float alpha = (sinf((float)ImGui::GetTime() * 5.0f) + 1.0f) * 0.5f;
    ImGui::SetCursorPos(ImVec2(windowWidth - 40, windowHeight - 30));
    ImGui::TextColored(ImVec4(1, 1, 1, alpha), "▼");

    // クリック判定
    if (!g_storyAi.isOpen && ImGui::IsMouseClicked(0)) {
        if (g_currentIndex + 1 < g_messages.size()) {
            g_currentIndex++;
        }
        else {
            // イベント終了後の処理
            if (g_CurrentEventID == "Intro") {
                currentScene = Scene::Field; // 序盤が終わったらフィールドへ
                return;
            }
            else if (g_CurrentEventID == "Ending") {
                currentScene = Scene::Title; // エンディングならタイトルへ
            }

            else {
                currentScene = Scene::Field; // 基本はフィールドに戻る
            }
        }
    }

    ImGui::End();

    // 必ずPopしてスタイルを元に戻す
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}
void DrawStory()
{

}
