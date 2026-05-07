#include "Character.h"
#include "CombatSystem.h"

#include <iostream>
#include <algorithm>
#include <string>



#include "imgui.h"

// 参加者の表示関数		
void CombatSystem::displayTurnOrder()
{
	// 素早さ順に参加者を並べ替える([]で外部の変数を受け付けない)
	std::sort(participants.begin(), participants.end(), [](Character* a, Character* b) {
		return a->speed > b->speed;
	});

	//(u8 UTF-8 エンコーディング)
	std::cout << u8"----行動順（コンソール）----" << std::endl;
	for (size_t i = 0; i < participants.size(); i++)
	{
		std::cout << (i + 1) << u8"番目: " << participants[i]->name << std::endl;
	}
}

// 可視化の切り替え関数
void CombatSystem::toggleVisibility()
{
	isVisible = !isVisible;
}

void CombatSystem::renderUI(int screenWidth, int screenHeight)
{
	if (!isVisible || participants.empty()) return;             // 非表示か参加者がいない場合は描画しない

	// 毎フレーム素早さ順に並べ替え（ウィンドウ表示と一致）
	std::sort(participants.begin(), participants.end(), [](Character* a, Character* b) {
		return a->speed > b->speed;
		});

	// --- 配置の定数 ---
	const float cardWidth = 280.0f;  // カードの横幅
	const float cardHeight = 70.0f; // カードの高さ（固定すると綺麗に並びます）
	const float marginX = 20.f;      // 右端からの余白
	const float marginY = 20.f;      // 上端からの余白
	const float spacingY = 10.f;     // カード同士の隙間（ここが重要！）
	float currentY = marginY;  //キャラごとに縦に並べる開始地点

	//行動順の表示
	//味方と敵の表示分け
	for (size_t i = 0; i < participants.size(); i++)
	{
		Character* c = participants[i];  //参加キャラクターのポインタを取得
		if (c->currentHp <= 0)continue;  //HP0以下のキャラは表示しない


		ImGui::SetNextWindowPos(ImVec2((float)screenWidth - cardWidth - marginX, currentY), ImGuiCond_Always);//ウィンドウの位置を画面右上に設定
		ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);                                   //ウィンドウの幅を設定（高さは内容に合わせて自動調整）

		// 背景を半透明に設定
		ImVec4 bgColor = (c->isAlly == 1) ? ImVec4(0.1f, 0.4f, 0.1f, 0.6f) : ImVec4(0.4f, 0.1f, 0.1f, 0.6f);
		ImVec4 borderColor = (c->isAlly == 1) ? ImVec4(0.4f, 1.0f, 0.4f, 0.6f) : ImVec4(1.0f, 0.4f, 0.4f, 0.6f);

		// 背景色のスタイルを上書き
		ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
		ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

		//キャラの名前が被っても内部で識別するよう
		std::string windowName = "##Card" + std::to_string(i); // IDは隠す

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_NoMove          // 移動禁止
			| ImGuiWindowFlags_NoResize        // リサイズ禁止
			| ImGuiWindowFlags_NoTitleBar;      // タイトルバー非表示（より透過感を出す場合）

		if (ImGui::Begin(windowName.c_str(), nullptr, flags))
		{
			// 名前と順番
			if (i == 0)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), u8"★ NEXT");
				ImGui::SameLine();
			}
			ImGui::Text("%d. %s", (int)i + 1, c->name.c_str());

			ImGui::Separator();

		}

		ImGui::End();

		ImGui::PopStyleColor(2); // WindowBg, Border
		ImGui::PopStyleVar(2);  // WindowRounding, WindowBorderSize

		currentY += cardHeight + spacingY;
	}

	// participantsの先頭が現在の手番
	if (participants.empty()) return; //リストが空なら返す
	Character* activeChar = participants[0];//リストの先頭の現在のキャラクターを取得

	// --- コマンドウィンドウ（画面下部） ---
	//(ImGuiCond_Alwaysでマイフレーム強制的に適用させる)
	ImGui::SetNextWindowPos(ImVec2(16.f, (float)screenHeight - 200.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2((float)screenWidth - 32.f, 180.f), ImGuiCond_Always);

	if (ImGui::Begin(u8"コマンドメニュー", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		// 左側：味方のステータス一覧（アイコン ＋ HPバー）
		ImGui::BeginChild("AllyStatus", ImVec2((float)screenWidth * 0.4f, 0), true);
		ImGui::Text(u8"パーティステータス");
		ImGui::Separator();

		for (auto* c : participants) {
			if (c->isAlly == 1) {
				ImGui::BeginGroup(); // アイコンとバーを縦にまとめるグループ

				// 1. キャラクターのアイコン（代わりの四角形）
				ImVec2 iconSize(50, 50);
				ImGui::Button(c->name.substr(0, 1).c_str(), iconSize); // 名前の頭文字をアイコン代わりに表示

				// 2. その下のHPバー
				float hpFraction = (float)c->currentHp / (float)c->hp;
				// 残りHPに応じて色を変える（赤、黄、緑）
				ImVec4 hpColor = (hpFraction < 0.2f) ? ImVec4(1, 0, 0, 1) : (hpFraction < 0.5f) ? ImVec4(1, 1, 0, 1) : ImVec4(0, 1, 0, 1);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hpColor);
				ImGui::ProgressBar(hpFraction, ImVec2(50, 8), "");
				ImGui::PopStyleColor();

				// 3. HP数値
				ImGui::Text("%d/%d", c->currentHp, c->hp);

				ImGui::EndGroup();
				ImGui::SameLine(0, 15); // 次のキャラを横に15ピクセル空けて並べる
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// 右側：コマンド選択
		ImGui::BeginChild("ActionMenu", ImVec2(0, 0), false);
		if (activeChar->isAlly == 1) {
			ImGui::Text(u8"【 %s の行動 】", activeChar->name.c_str());
			ImGui::Separator();

			// ターゲット選択ボタンを表示
			for (auto* target : participants) {
				if (target->isAlly == 0 && target->currentHp > 0) {
					if (ImGui::Button(target->name.c_str(), ImVec2(120, 40))) {
						executeSkill(activeChar, target);
						activeChar->speed -= 100;
					}
					ImGui::SameLine();
				}
			}
		}
		else {
			{
				ImGui::Text(u8"【%s（敵）のターン】", activeChar->name.c_str()); // 敵はとりあえずボタンで行動させてみる
				std::vector<Character*>aliveAllies;//生きている味方のリスト
				for (auto* c : participants) {
					if (c && c->isAlly == 1 && c->currentHp > 0) {
						aliveAllies.push_back(c);
					}
				}
				// 2. 味方が存在すれば、ランダムに一人選んで攻撃
				if (!aliveAllies.empty()) {
					int randomIndex = rand() % aliveAllies.size(); // 簡易的な乱数
					Character* target = aliveAllies[randomIndex];

					executeSkill(activeChar, target); // 攻撃実行
					activeChar->speed -= 100;        // 行動順を後ろに下げる
				}
			}
			
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

static bool pKeyWasPressed = false;

void processInput(GLFWwindow* window, CombatSystem& combatSystem) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
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

void CombatSystem::addParticipant(Character* character) {
	if (character != nullptr) {
		participants.push_back(character);
	}
}


void CombatSystem::executeSkill(Character* attacker, Character* target)
{
	target->currentHp -= attacker->power;
	if (target->currentHp < 0) target->currentHp = 0;

	std::cout << attacker->name << u8" の攻撃！ "
		<< target->name << u8" に " << attacker->power << u8" ダメージ！" << std::endl;

	if (target->currentHp == 0) {
		std::cout << target->name << u8" を倒した！" << std::endl;
	}
}

