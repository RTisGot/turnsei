#define GLEW_STATIC
#include <glew.h>
#include <iostream>
#include <fstream>
#include <ctime>

#include "src/game/Scene.h"
#include "src/game/Character.h"
#include "src/game/CombatSystem.h"
#include "src/game/Field.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

const unsigned int SCR_Width = 1280, SCR_Height = 720;

bool LoadWetland(const std::string& filePath);

GLFWwindow* window = nullptr;

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
    combatSystem.resetBattle();

    combatSystem.displayTurnOrder();
}

int main()
{
    if (!glfwInit()) return -1;
    srand((unsigned int)time(NULL));

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SCR_Width, SCR_Height, u8"turnsei - 行動順", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
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
        glfwPollEvents();
        processInput(window, combatSystem);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        FieldInit();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        MainUpdate(combatSystem, window);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
