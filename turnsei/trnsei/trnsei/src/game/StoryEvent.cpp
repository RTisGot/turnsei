/*
 * このプロジェクトでは nlohmann/json を使用しています。
 * nlohmann/json is licensed under the MIT License.
 * Copyright (c) 2013-2022 Niels Lohmann
 * https://github.com/nlohmann/json
 */

#include "StoryEvent.h"
#include "Scene.h"
#include "imgui.h"
#include "../../json.hpp"
#include <iostream>
#include <fstream>
#include <vector>


int g_Scene = 0;
bool g_StoryNeedsLoad = false;
static size_t g_currentIndex = 0;
using json = nlohmann::json;
char g_playerName[32] = "プレイヤー"; // 入力用バッファ
extern bool g_isNamingPhase = false;   // 名前入力中かどうか


struct Message {
	std::string name;   //json側の名前
	std::string text;  //         文字列
};

static std::vector<Message> g_messages;

void StoryEvent() {
    if (g_StoryNeedsLoad) {
        LoadStoryData();
        g_currentIndex = 0;       //セリフの最初を一行目にコピー  
        g_StoryNeedsLoad = false; // ロード完了！
    }

    // セリフがなかったら
    if (g_messages.empty()) {
        ImGui::Begin("Debug");
        ImGui::Text("Error: No messages loaded. Check data.json.");
        if (ImGui::Button("Back")) currentScene = Scene::Title;
        ImGui::End();
        return;
    }

    // 3. メッセージウィンドウを表示（UpdateStoryを呼ぶ）
    UpdateStory();
}
//セリフデータをメモリにコピー
void LoadStoryData()
{//ファイル読み込み用
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
    for (const auto& item : data["events"]) {
        Message msg;
        msg.name = item["name"].get<std::string>(); // 型を明示して取得
        msg.text = item["text"].get<std::string>();
        g_messages.push_back(msg);
    }
}

void UpdateStory()
{
    if (g_messages.empty()) return;
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
        return; // ここで抜ける場合はスタイル変更前なので安全！
    }

    // --- 2. スタイル設定（通常のセリフ用） ---
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    float windowWidth = screenSize.x * 0.8f;
    float windowHeight = 150.0f;
    ImVec2 pos = ImVec2((screenSize.x - windowWidth) * 0.5f, screenSize.y - windowHeight - 50.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

    // --- 3. セリフウィンドウの表示 ---
    ImGui::Begin("DialogueWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    // キャラクター名表示のカスタマイズ
    // 名前が "PLAYER" なら入力した名前に置き換える処理を追加
    std::string displayName = msg.name;
    if (displayName == "PLAYER") displayName = g_playerName;

    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[ %s ]", displayName.c_str());
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    ImGui::TextWrapped("%s", msg.text.c_str());

    // クリック待ちアイコン
    float alpha = (sinf((float)ImGui::GetTime() * 5.0f) + 1.0f) * 0.5f;
    ImGui::SetCursorPos(ImVec2(windowWidth - 40, windowHeight - 30));
    ImGui::TextColored(ImVec4(1, 1, 1, alpha), "▼");

    // クリック判定
    // IsWindowHoveredだけでなく、背景クリックでも進むようにしたい場合は調整
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        if (g_currentIndex + 1 < g_messages.size()) {
            g_currentIndex++;
        }
        else {
            currentScene = Scene::Title;
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