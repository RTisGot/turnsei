#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>
#include <iostream>
#include <fstream>


#include "src/game/Scene.h"
#include "src/game/Character.h"
#include "src/game/CombatSystem.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

//ウィンドウのサイズ
const unsigned int SCR_Width = 1280, SCR_Height = 720;

//プロトタイプ宣言
bool LoadWetland(const std::string& filePath);

//実数
GLFWwindow* window = nullptr;

//フォントのロード（日本語対応）
static void LoadJapaneseFont(ImGuiIO& io)
{
	ImFontConfig cfg;                                                                                                         
	cfg.FontNo = 0;
	const char* candidates[] = {
		"C:\\Windows\\Fonts\\YuGothR.ttc",
		"C:\\Windows\\Fonts\\YuGothM.ttc",
		"C:\\Windows\\Fonts\\meiryo.ttc",
		"C:\\Windows\\Fonts\\msgothic.ttc",
	};
	for (const char* path : candidates)
	{
		std::ifstream test(path, std::ios::binary);
		if (!test.good()) continue;
		test.close();
		if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, 36.0f, &cfg, io.Fonts->GetGlyphRangesJapanese()))
		{
			io.FontDefault = f;
			return;
		}
	}
	io.Fonts->AddFontDefault();
}

void setupStageOne(CombatSystem& combatSystem) {
	Character* player = new Character{ "Player", 120, 25, 12, 15, 10, 150, 120, 1 };
	Character* enemy1 = new Character{ "Enemy A", 90, 18, 8, 10, 5, 130, 90, 0 };
	Character* enemy2 = new Character{ "Enemy B", 100, 20, 10, 12, 8, 140, 100, 0 };

	combatSystem.addParticipant(player);
	combatSystem.addParticipant(enemy1);
	combatSystem.addParticipant(enemy2);

	combatSystem.displayTurnOrder();
}

//------------------------------------------------------- 
//                          main
//----------------------------------------------------
int main()
{
	if (!glfwInit()) return -1;      //glfwの初期化に失敗した場合は終了
	srand((unsigned int)time(NULL)); // 実行のたびに乱数を変える

	// ----OpenGLのバージョンとプロファイルを指定---
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(SCR_Width, SCR_Height, u8"turnsei - 行動順", NULL, NULL);
	if (!window) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();                      //すべてのリソースを解放してプログラムを終了
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		std::cout << "Failed to initialize GLEW" << std::endl;
		return -1;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	LoadJapaneseFont(io);
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	CombatSystem combatSystem;
	setupStageOne(combatSystem);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();        // イベントの処理
		processInput(window, combatSystem);   //入力処理

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f); // 背景色の設定
		glClear(GL_COLOR_BUFFER_BIT);         // バッファのクリア

		MainUpdate(combatSystem);                         //シーンのアップデート

		//combatSystem.renderUI(SCR_Width, SCR_Height);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window); // バッファの入れ替え
		//glfwPollEvents();        // イベントの処理
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
