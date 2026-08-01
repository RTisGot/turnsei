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

GLFWwindow* window = nullptr;
/**
 * @brief ウィンドウモードとフルスクリーンを切り替えるトグル処理
 * OS固有のウィンドウ配置情報を保持し、切り替え時に復元させる設計。
 */
static void UpdateFullscreenToggle(GLFWwindow* targetWindow)
{
    static bool keyWasDown = false;
    static bool fullscreen = false;

    //windowの位置と配置の保存
    static int windowedX = 100;
    static int windowedY = 100;
    static int windowedWidth = SCR_Width;
    static int windowedHeight = SCR_Height;

    //
    bool toggleKeyDown =
        glfwGetKey(targetWindow, GLFW_KEY_F11) == GLFW_PRESS ||
        (glfwGetKey(targetWindow, GLFW_KEY_LEFT_ALT) == GLFW_PRESS &&
            glfwGetKey(targetWindow, GLFW_KEY_ENTER) == GLFW_PRESS);

    if (toggleKeyDown && !keyWasDown) {
        fullscreen = !fullscreen;

        // 現在のウィンドウ位置とサイズを保存
        if (fullscreen) {
            //
            glfwGetWindowPos(targetWindow, &windowedX, &windowedY);
            glfwGetWindowSize(targetWindow, &windowedWidth, &windowedHeight);

            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
            if (monitor && mode) {
                glfwSetWindowMonitor(
                    targetWindow,
                    monitor,
                    0,
                    0,
                    mode->width,
                    mode->height,
                    mode->refreshRate
                );
            }
        }
        else {
            glfwSetWindowMonitor(
                targetWindow,
                nullptr,
                windowedX,
                windowedY,
                windowedWidth,
                windowedHeight,
                GLFW_DONT_CARE
            );
        }
    }

    keyWasDown = toggleKeyDown;
}

/**
 * @brief 日本語フォントをロードする関数
 */
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
        if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, 20.0f, &cfg, io.Fonts->GetGlyphRangesJapanese()))
        {
            io.FontDefault = f;
            return;
        }
    }
    io.Fonts->AddFontDefault();
}

//
void setupStageOne(CombatSystem& combatSystem) {
    Character* player = new Character{ "Player", 120, 25, 12, 15, 10, 150, 120, 1 };
    combatSystem.addParticipant(player);
    //combatSystem.resetBattle();
    combatSystem.displayTurnOrder();
}

int main()
{
  
    if (!glfwInit()) return -1;
    srand((unsigned int)time(NULL));

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(SCR_Width, SCR_Height, u8"TIDEGLASS - 潮鏡都市", NULL, NULL);
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
        UpdateFullscreenToggle(window);
        processInput(window, combatSystem);

        int framebufferWidth = 1;
        int framebufferHeight = 1;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) continue;
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        glClearColor(0.52f, 0.72f, 0.92f, 1.0f);
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
