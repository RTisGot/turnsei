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

static void UpdateFullscreenToggle(GLFWwindow* targetWindow)
{
    static bool keyWasDown = false;
    static bool fullscreen = false;
    static int windowedX = 100;
    static int windowedY = 100;
    static int windowedWidth = SCR_Width;
    static int windowedHeight = SCR_Height;

    bool toggleKeyDown =
        glfwGetKey(targetWindow, GLFW_KEY_F11) == GLFW_PRESS ||
        (glfwGetKey(targetWindow, GLFW_KEY_LEFT_ALT) == GLFW_PRESS &&
            glfwGetKey(targetWindow, GLFW_KEY_ENTER) == GLFW_PRESS);

    if (toggleKeyDown && !keyWasDown) {
        fullscreen = !fullscreen;

        if (fullscreen) {
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
    Character* enemy3 = new Character{ "Enemy C", 85, 22, 7, 14, 10, 145, 85, 0 };
    Character* enemy4 = new Character{ "Enemy D", 110, 17, 13, 8, 4, 125, 110, 0 };
    Character* enemy5 = new Character{ "Enemy E", 95, 19, 9, 11, 7, 135, 95, 0 };

    combatSystem.addParticipant(player);
    combatSystem.addParticipant(enemy1);
    combatSystem.addParticipant(enemy2);
    combatSystem.addParticipant(enemy3);
    combatSystem.addParticipant(enemy4);
    combatSystem.addParticipant(enemy5);
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
        UpdateFullscreenToggle(window);
        processInput(window, combatSystem);

        int framebufferWidth = 1;
        int framebufferHeight = 1;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) continue;
        glViewport(0, 0, framebufferWidth, framebufferHeight);

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
