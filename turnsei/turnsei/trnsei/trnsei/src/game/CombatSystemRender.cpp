#include <glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "Character.h"
#include "CombatSystem.h"
#include "Field.h"
#include "SimpleMap.h"
#include "../../assets/ImportedModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    GLuint g_battleShader = 0;
    GLuint g_battleCubeVao = 0;
    GLuint g_battleCubeVbo = 0;
    GLuint g_battleCubeEbo = 0;
    GLuint g_battleFloorVao = 0;
    GLuint g_battleFloorVbo = 0;
    GLuint g_battleShadowVao = 0;
    GLuint g_battleShadowVbo = 0;
    ImportedModel g_playerModel;
    ImportedModel g_enemyModels[5];
    bool g_playerModelLoadAttempted = false;
    bool g_enemyModelLoadAttempted = false;
    double g_previousBattleAnimationTime = 0.0;

    // 任意アセットが見つからなくても例外にはせず、代替形状で戦闘を継続する。
    // 制作途中のビルドでもゲーム進行を止めないための方針である。
    bool FileExists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }


    // Enemy1～Enemy5を初回だけ読み込む。欠番はスロット0を使用し、
    // 描画ループ内でファイル探索を繰り返さない。
    void LoadBattleEnemyModels()
    {
        if (g_enemyModelLoadAttempted) return;
        g_enemyModelLoadAttempted = true;

        // Visual Studio起動と実行ファイル直接起動で作業ディレクトリが異なるため、
        // 両方のResource候補を順番に探索する。
        const char* extensions[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };
        const char* names[] = { "Enemy" };

        for (int modelIndex = 0; modelIndex < 5; ++modelIndex) {
            std::string numberedName = "Enemy" + std::to_string(modelIndex + 1);
            for (const char* root : roots) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + numberedName + extension;
                    if (FileExists(path) && g_enemyModels[modelIndex].load(path)) {
                        break;
                    }
                }
                if (g_enemyModels[modelIndex].isLoaded()) break;
            }
        }

        if (!g_enemyModels[0].isLoaded()) {
            for (const char* root : roots) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + "Enemy" + extension;
                    if (FileExists(path) && g_enemyModels[0].load(path)) break;
                }
                if (g_enemyModels[0].isLoaded()) break;
            }
        }
    }

    // 修正済みCharacter_Gameplayを優先して読み込み、その60ボーンへ
    // 歩行、戦闘待機、攻撃の外部クリップを追加する。
    void LoadBattlePlayerModel()
    {
        if (g_playerModelLoadAttempted) return;
        g_playerModelLoadAttempted = true;

        const char* extensions[] = { ".fbx", ".obj", ".gltf", ".glb" };
        const char* roots[] = { "Resource/", "../trnsei/Resource/" };
        const char* names[] = { "Character_Gameplay", "Character" };

        for (const char* root : roots) {
            for (const char* name : names) {
                for (const char* extension : extensions) {
                    std::string path = std::string(root) + name + extension;
                    if (FileExists(path) && g_playerModel.load(path)) {
                        if (std::string(name) == "Character_Gameplay") {
                            const std::string walkJson =
                                std::string(root) + "Character_Walk.json";
                            const std::string idleJson =
                                std::string(root) + "Character_CombatIdle.json";
                            const std::string attackJson =
                                std::string(root) + "Character_Attack.json";
                            if (FileExists(walkJson))
                                g_playerModel.loadAnimationJson(walkJson);
                            if (FileExists(idleJson))
                                g_playerModel.loadAnimationJson(idleJson);
                            if (FileExists(attackJson))
                                g_playerModel.loadAnimationJson(attackJson);
                            if (!g_playerModel.playAnimationByName("Combat_Idle")) {
                                g_playerModel.playAnimationByName("Walk");
                                g_playerModel.updateAnimation(0.11f);
                            }
                        }
                        return;
                    }
                }
            }
        }
    }

    // 戦闘ではフィールドと異なる構図・補助光を使用するため、
    // 専用シェーダーをこのレンダラー内で管理する。
    GLuint CompileBattleShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }


    void InitBattleRenderer()
    {
        if (g_battleShader != 0) return;

        const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoords;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
out vec2 vTexCoords;
void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vColor = aColor;
    vWorldPos = world.xyz;
    vTexCoords = aTexCoords;
    gl_Position = projection * view * world;
}
)";
        const char* fragmentSource = R"(
#version 330 core
uniform vec3 color;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int outlineMode;
uniform int useVertexColor;
uniform int useTexture;
uniform float opacity;
uniform float materialRoughness;
uniform float materialMetallic;
uniform vec3 materialEmission;
uniform sampler2D diffuseTexture;
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorldPos;
in vec2 vTexCoords;
out vec4 FragColor;
vec3 ACESFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) /
                 (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
void main()
{
    if (outlineMode == 1) {
        FragColor = vec4(0.010, 0.013, 0.028, opacity);
        return;
    }
    if (outlineMode == 2) {
        FragColor = vec4(0.006, 0.007, 0.014, opacity);
        return;
    }

    vec3 viewDir = normalize(cameraPos - vWorldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) {
        n = -n;
    }
    vec3 l = normalize(viewDir * 0.35 + normalize(lightDir) * 0.65);
    vec3 fill = normalize(vec3(0.55, 0.35, 0.75));
    vec3 halfDir = normalize(l + viewDir);
    float ndl = max(dot(n, l), 0.0);
    float fillLight = max(dot(n, fill), 0.0);
    float hemi = max(n.y, 0.0);
    float lit = max(max(ndl, hemi * 0.54), fillLight * 0.30);
    float softBand = smoothstep(0.24, 0.42, lit) * 0.32
                   + smoothstep(0.52, 0.70, lit) * 0.28
                   + smoothstep(0.82, 0.94, lit) * 0.24;

    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.05);
    float fresnelLine = smoothstep(0.32, 0.82, rim);
    float backRim = smoothstep(0.20, 0.78, pow(1.0 - max(dot(n, normalize(vec3(-0.55, 0.15, -0.82))), 0.0), 2.6));
    float rough = clamp(materialRoughness, 0.05, 1.0);
    float metal = clamp(materialMetallic, 0.0, 1.0);
    float specPower = mix(220.0, 18.0, rough * rough);
    float spec = pow(max(dot(n, halfDir), 0.0), specPower)
               * smoothstep(0.44, 0.76, lit);
    float glint = pow(max(dot(n, normalize(viewDir + vec3(-0.6, 1.0, 0.25))), 0.0), 180.0);
    vec3 baseColor = useVertexColor == 1 ? vColor : color;
    if (useTexture == 1) {
        baseColor *= texture(diffuseTexture, vTexCoords).rgb;
    }
    baseColor = pow(baseColor, vec3(0.92));

    if (abs(n.y) > 0.94 && abs(vWorldPos.y) < 0.08) {
        float gridA = smoothstep(0.020, 0.000, abs(fract(vWorldPos.x * 0.18) - 0.5));
        float gridB = smoothstep(0.020, 0.000, abs(fract(vWorldPos.z * 0.18) - 0.5));
        float centerGlow = 1.0 - smoothstep(0.0, 13.0, length(vWorldPos.xz - vec2(0.0, 1.8)));
        vec3 floorBase = mix(vec3(0.035, 0.050, 0.082), vec3(0.10, 0.16, 0.24), centerGlow);
        vec3 floorLine = vec3(0.18, 0.36, 0.58) * (gridA + gridB) * 0.16;
        vec3 floorColor = floorBase + floorLine + vec3(0.05, 0.10, 0.18) * fresnelLine;
        FragColor = vec4(floorColor, opacity);
        return;
    }

    vec3 shadowTint = vec3(0.20, 0.27, 0.43);
    vec3 midTint = vec3(0.76, 0.86, 1.00);
    vec3 warmLight = vec3(1.16, 1.08, 0.96);
    vec3 shaded = baseColor * mix(shadowTint, midTint, softBand);
    shaded = mix(shaded, baseColor * warmLight,
                 smoothstep(0.70, 0.96, lit) * 0.72);
    shaded += vec3(0.26, 0.62, 0.96) * fresnelLine * 0.38;
    shaded += vec3(0.72, 0.42, 0.92) * backRim * 0.13;
    vec3 specTint = mix(vec3(1.00, 0.91, 0.76), baseColor, metal);
    shaded += specTint * spec * mix(0.34, 0.82, metal);
    shaded += vec3(0.90, 0.96, 1.00) * glint * 0.22;
    shaded += materialEmission * 2.4;
    float depthFog = smoothstep(8.0, 24.0, length(cameraPos - vWorldPos));
    shaded = mix(shaded, vec3(0.055, 0.075, 0.12), depthFog * 0.18);
    shaded = mix(vec3(dot(shaded, vec3(0.299, 0.587, 0.114))), shaded, 1.20);
    shaded = ACESFilm(max(shaded, vec3(0.0)) * 1.08);
    shaded = pow(shaded, vec3(1.0 / 2.2));
    shaded = max(shaded, pow(baseColor * 0.18, vec3(1.0 / 2.2)));
    FragColor = vec4(shaded, opacity);
}
)";

        GLuint vertexShader = CompileBattleShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileBattleShader(GL_FRAGMENT_SHADER, fragmentSource);
        g_battleShader = glCreateProgram();
        glAttachShader(g_battleShader, vertexShader);
        glAttachShader(g_battleShader, fragmentShader);
        glLinkProgram(g_battleShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // インポートモデルがない場合に備え、確認用の代替立方体を生成する。
        const float cubeVertices[] = {
            -0.5f, 0.0f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, 0.0f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, 1.0f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, 1.0f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, 0.0f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, 0.0f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, 1.0f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, 1.0f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, 0.0f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 0.0f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f, -0.5f, -1.0f,  0.0f,  0.0f,
             0.5f, 0.0f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 0.0f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 1.0f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, 1.0f, -0.5f,  1.0f,  0.0f,  0.0f,
            -0.5f, 1.0f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f, 1.0f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f, 1.0f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f, 1.0f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f, 0.0f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, 0.0f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, 0.0f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, 0.0f,  0.5f,  0.0f, -1.0f,  0.0f
        };
        const unsigned int cubeIndices[] = {
            0, 1, 2, 2, 3, 0, 4, 6, 5, 6, 4, 7,
            8, 9, 10, 10, 11, 8, 12, 15, 14, 14, 13, 12,
            16, 17, 18, 18, 19, 16, 20, 23, 22, 22, 21, 20
        };
        glGenVertexArrays(1, &g_battleCubeVao);
        glGenBuffers(1, &g_battleCubeVbo);
        glGenBuffers(1, &g_battleCubeEbo);
        glBindVertexArray(g_battleCubeVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleCubeVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_battleCubeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        const float floorVertices[] = {
            -12.0f, 0.0f, -8.0f, 0.0f, 1.0f, 0.0f,
             12.0f, 0.0f, -8.0f, 0.0f, 1.0f, 0.0f,
            -12.0f, 0.0f,  8.0f, 0.0f, 1.0f, 0.0f,
             12.0f, 0.0f,  8.0f, 0.0f, 1.0f, 0.0f
        };
        glGenVertexArrays(1, &g_battleFloorVao);
        glGenBuffers(1, &g_battleFloorVbo);
        glBindVertexArray(g_battleFloorVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleFloorVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        std::vector<float> shadowVertices;
        const int shadowSegments = 40;
        shadowVertices.insert(shadowVertices.end(), { 0.0f, 0.025f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f });
        for (int i = 0; i <= shadowSegments; ++i) {
            float angle = (float)i / (float)shadowSegments * glm::two_pi<float>();
            shadowVertices.insert(shadowVertices.end(), {
                std::cos(angle), 0.025f, std::sin(angle),
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f
            });
        }
        glGenVertexArrays(1, &g_battleShadowVao);
        glGenBuffers(1, &g_battleShadowVbo);
        glBindVertexArray(g_battleShadowVao);
        glBindBuffer(GL_ARRAY_BUFFER, g_battleShadowVbo);
        glBufferData(GL_ARRAY_BUFFER, shadowVertices.size() * sizeof(float), shadowVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);

        LoadBattlePlayerModel();
        LoadBattleEnemyModels();
    }

    ImVec2 WorldToScreen(const glm::vec3& worldPosition, const glm::mat4& viewProjection, int width, int height)
    {
        glm::vec4 clip = viewProjection * glm::vec4(worldPosition, 1.0f);
        if (clip.w <= 0.0f) return ImVec2(-1000.0f, -1000.0f);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        return ImVec2(
            (ndc.x * 0.5f + 0.5f) * width,
            (1.0f - (ndc.y * 0.5f + 0.5f)) * height
        );
    }

    float EaseOutBack(float value)
    {
        value = std::max(0.0f, std::min(value, 1.0f));
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(value - 1.0f, 3.0f) + c1 * std::pow(value - 1.0f, 2.0f);
    }

    // 戦闘用の簡易剣を既存の立方体メッシュから組み立てる。
    // 専用モデルの読込失敗で攻撃自体が見えなくなることを避けるため、
    // 刃・鍔・柄を同じ軽量な描画経路で描く。
    void DrawSwordPart(const glm::mat4& transform, const glm::vec3& color,
        float metallic, float roughness, const glm::vec3& emission = glm::vec3(0.0f))
    {
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1,
            GL_FALSE, glm::value_ptr(transform));
        glUniform3fv(glGetUniformLocation(g_battleShader, "color"), 1,
            glm::value_ptr(color));
        glUniform1f(glGetUniformLocation(g_battleShader, "materialMetallic"), metallic);
        glUniform1f(glGetUniformLocation(g_battleShader, "materialRoughness"), roughness);
        glUniform3fv(glGetUniformLocation(g_battleShader, "materialEmission"), 1,
            glm::value_ptr(emission));
        glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
        glBindVertexArray(g_battleCubeVao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    // 右手付近を支点にし、身体の攻撃クリップと同じ時間で剣を振る。
    // 11Fのヒット時に刃が敵方向へ最も深く入るため、見た目と判定がずれない。
    void DrawBattleSword(const glm::vec3& actorPosition, float attackTime, bool attacking)
    {
        float slash = 0.0f;
        float thrust = 0.0f;
        if (attacking) {
            constexpr float windupEnd = 8.0f / 30.0f;
            constexpr float hitTime = 11.0f / 30.0f;
            constexpr float recoveryEnd = 43.0f / 30.0f;
            if (attackTime < windupEnd) {
                float t = std::max(0.0f, attackTime / windupEnd);
                slash = glm::mix(0.0f, -1.05f, t * t * (3.0f - 2.0f * t));
            }
            else if (attackTime < hitTime) {
                float t = (attackTime - windupEnd) / (hitTime - windupEnd);
                t = t * t * (3.0f - 2.0f * t);
                slash = glm::mix(-1.05f, 1.22f, t);
                thrust = glm::mix(0.0f, -0.72f, t);
            }
            else {
                float t = (attackTime - hitTime) / (recoveryEnd - hitTime);
                t = std::max(0.0f, std::min(t, 1.0f));
                t = t * t * (3.0f - 2.0f * t);
                slash = glm::mix(1.22f, 0.0f, t);
                thrust = glm::mix(-0.72f, 0.0f, t);
            }
        }

        glm::mat4 sword(1.0f);
        sword = glm::translate(sword, actorPosition + glm::vec3(0.72f, 2.02f, -0.04f + thrust));
        sword = glm::rotate(sword, glm::radians(-13.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        sword = glm::rotate(sword, glm::radians(-18.0f) + slash, glm::vec3(0.0f, 0.0f, 1.0f));

        // 各パーツは柄元を共通原点にする。刃の中心を上へずらすことで
        // 回転しても鍔から離れず、一本の剣として追従する。
        glm::mat4 blade = glm::translate(sword, glm::vec3(0.0f, 0.78f, 0.0f));
        blade = glm::scale(blade, glm::vec3(0.105f, 1.42f, 0.055f));
        DrawSwordPart(blade, glm::vec3(0.66f, 0.78f, 0.88f), 0.88f, 0.20f,
            attacking ? glm::vec3(0.025f, 0.055f, 0.08f) : glm::vec3(0.0f));

        glm::mat4 guard = glm::translate(sword, glm::vec3(0.0f, 0.04f, 0.0f));
        guard = glm::scale(guard, glm::vec3(0.62f, 0.105f, 0.13f));
        DrawSwordPart(guard, glm::vec3(0.30f, 0.20f, 0.48f), 0.72f, 0.28f);

        glm::mat4 grip = glm::translate(sword, glm::vec3(0.0f, -0.34f, 0.0f));
        grip = glm::scale(grip, glm::vec3(0.14f, 0.43f, 0.14f));
        DrawSwordPart(grip, glm::vec3(0.075f, 0.055f, 0.10f), 0.18f, 0.72f);

        // 後続のキャラクター描画へ武器用PBR値を漏らさない。
        glUniform1f(glGetUniformLocation(g_battleShader, "materialMetallic"), 0.0f);
        glUniform1f(glGetUniformLocation(g_battleShader, "materialRoughness"), 0.62f);
        glUniform3f(glGetUniformLocation(g_battleShader, "materialEmission"), 0.0f, 0.0f, 0.0f);
    }
}

void CombatSystem::renderBattleScene(Character* activeChar, int screenWidth, int screenHeight)
{
    InitBattleRenderer();
    double currentAnimationTime = ImGui::GetTime();
    float animationDelta = g_previousBattleAnimationTime > 0.0
        ? static_cast<float>(currentAnimationTime - g_previousBattleAnimationTime)
        : 0.0f;
    g_previousBattleAnimationTime = currentAnimationTime;

    // Blender側で定義した攻撃イベントを秒へ変換する。
    // 11Fでダメージを確定し、43Fを越えたら待機へ戻す。
    constexpr float attackHitTime = 11.0f / 30.0f;
    constexpr float attackDuration = 43.0f / 30.0f;
    if (playerCommandAnimating) {
        // 攻撃中も毎フレーム姿勢を更新し、経過時間をヒット判定と共有する。
        g_playerModel.updateAnimation(animationDelta);
        playerCommandAnimationTime += animationDelta;

        // ヒットフレームを初めて跨いだ瞬間だけゲーム上の攻撃を確定する。
        // フレーム落ちで11Fを飛び越えても、一度だけ実行される。
        if (!playerCommandHitApplied &&
            playerCommandAnimationTime >= attackHitTime) {
            playerCommandHitApplied = true;
            executeCommand(queuedPlayerAttacker, queuedPlayerTarget);
        }
        if (playerCommandAnimationTime >= attackDuration) {
            // 攻撃に保持していた参照を破棄し、次のコマンドを受付可能にする。
            playerCommandAnimating = false;
            playerCommandHitApplied = false;
            playerCommandAnimationTime = 0.0f;
            queuedPlayerAttacker = nullptr;
            queuedPlayerTarget = nullptr;
            g_playerModel.playAnimationByName("Combat_Idle");
        }
    }
    else {
        // コマンド待機中はCombat_Idleをループ再生する。
        g_playerModel.updateAnimation(animationDelta);
    }
    for (ImportedModel& enemyModel : g_enemyModels) {
        enemyModel.updateAnimation(animationDelta);
    }

    std::vector<Character*> fixedEnemyOrder;
    for (auto* character : participants) {
        if (character && character->isAlly == 0) {
            fixedEnemyOrder.push_back(character);
        }
    }
    std::sort(fixedEnemyOrder.begin(), fixedEnemyOrder.end(), [](const Character* a, const Character* b) {
        return a->name < b->name;
    });
    const int enemyCount = static_cast<int>(fixedEnemyOrder.size());

    // プレイヤー越しに敵陣を見せる戦闘用構図を作る。
    // ターゲット選択中はUI表示だけを変え、カメラ構図は変化させない。
    const float cameraDrift = std::sin((float)currentAnimationTime * 0.32f);
    // 味方ターンと敵ターンで同じカメラを維持する。
    // activeCharが切り替わっても攻撃のたびに視点を反転させない。
    glm::vec3 cameraPosition(-8.1f + cameraDrift * 0.22f, 3.95f, 15.0f);
    glm::vec3 cameraTarget(0.57f, 1.48f, -0.9f);
    glm::mat4 view = glm::lookAt(
        cameraPosition,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 projection = glm::perspective(glm::radians(40.0f), (float)screenWidth / (float)screenHeight, 0.1f, 100.0f);
    glm::mat4 viewProjection = projection * view;

    glViewport(0, 0, screenWidth, screenHeight);
    glEnable(GL_DEPTH_TEST);
    // 戦闘専用の背景色を設定する。明るい水色では輪郭が背景へ埋もれるため、
    // 青緑の暗部を基準にしてキャラクターのシルエットを読みやすくする。
    glClearColor(0.035f, 0.075f, 0.095f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // フィールド描画を再利用し、遭遇地点の建物・水面・照明を戦闘背景へ残す。
    {
        glm::vec3 worldOrigin = GetBattleWorldOrigin();
        // マップ全体を動かさず、遭遇地点の座標をカメラへ加算する。
        glm::vec3 bgCamPos = worldOrigin + cameraPosition;
        glm::vec3 bgCamTarget = worldOrigin + cameraTarget;
        glm::mat4 bgView = glm::lookAt(bgCamPos, bgCamTarget, glm::vec3(0,1,0));
        DrawSimpleMap(bgView, projection, bgCamPos);
        // 背景色は保持したまま深度だけを消去する。これにより遠景が
        // 手前へ配置した戦闘キャラクターを隠すことを防ぐ。
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    glUseProgram(g_battleShader);
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(glGetUniformLocation(g_battleShader, "lightDir"), -0.42f, 0.88f, 0.28f);
    glUniform3fv(glGetUniformLocation(g_battleShader, "cameraPos"), 1, glm::value_ptr(cameraPosition));
    glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 0);
    glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
    glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
    glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), 1.0f);
    glUniform1f(glGetUniformLocation(g_battleShader, "materialRoughness"), 0.62f);
    glUniform1f(glGetUniformLocation(g_battleShader, "materialMetallic"), 0.0f);
    glUniform3f(glGetUniformLocation(g_battleShader, "materialEmission"), 0.0f, 0.0f, 0.0f);

    std::vector<std::pair<Character*, glm::vec3>> actorPositions;
    std::vector<std::pair<Character*, glm::vec3>> enemyPositions;

    for (auto* character : participants) {
        if (!character || character->currentHp <= 0) continue;

        int enemyIndex = 0;
        if (character->isAlly == 0) {
            auto fixedSlot = std::find(fixedEnemyOrder.begin(), fixedEnemyOrder.end(), character);
            enemyIndex = fixedSlot != fixedEnemyOrder.end()
                ? static_cast<int>(std::distance(fixedEnemyOrder.begin(), fixedSlot))
                : 0;
        }

        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 color;
        if (character->isAlly == 1) {
            position = glm::vec3(-2.45f, 0.0f, 8.0f);
            if (playerCommandAnimating && character == queuedPlayerAttacker) {
                // カメラを動かさず、キャラクターの位置だけで攻撃を演出する。
                // 予備動作ではわずかに後退し、ヒット直前に高速で踏み込み、
                // フォロースルーでは踏み込みより長い時間を使って定位置へ戻る。
                float offsetZ = 0.0f;
                const float anticipationEnd = 8.0f / 30.0f;
                if (playerCommandAnimationTime < anticipationEnd) {
                    const float t = playerCommandAnimationTime / anticipationEnd;
                    offsetZ = 0.16f * std::sin(t * glm::pi<float>());
                }
                else if (playerCommandAnimationTime < attackHitTime) {
                    float t = (playerCommandAnimationTime - anticipationEnd) /
                              (attackHitTime - anticipationEnd);
                    t = t * t * (3.0f - 2.0f * t);
                    offsetZ = glm::mix(0.0f, -1.25f, t);
                }
                else {
                    float t = (playerCommandAnimationTime - attackHitTime) /
                              (attackDuration - attackHitTime);
                    t = std::max(0.0f, std::min(t, 1.0f));
                    t = t * t * (3.0f - 2.0f * t);
                    offsetZ = glm::mix(-1.25f, 0.0f, t);
                }
                position.z += offsetZ;
            }
            scale = g_playerModel.isLoaded()
                ? glm::vec3(4.65f, 5.1f, 4.65f)
                : glm::vec3(2.7f, 5.25f, 2.7f);
            color = g_playerModel.isLoaded()
                ? glm::vec3(0.92f, 0.76f, 0.58f)
                : glm::vec3(0.18f, 0.55f, 0.88f);
        }
        else {
            float spacing = 2.65f;
            float x = (enemyIndex - (enemyCount - 1) * 0.5f) * spacing;
            float side = enemyIndex - (enemyCount - 1) * 0.5f;
            float depth = -3.55f + side * 0.34f;
            position = glm::vec3(x + 1.35f, 0.0f, depth);
            float focusScale = markedTarget == character ? 1.08f : 1.0f;
            scale = glm::vec3(2.75f, 3.08f, 2.75f) * focusScale;
            color = markedTarget == character ? glm::vec3(1.0f, 0.76f, 0.12f) : glm::vec3(0.82f, 0.20f, 0.18f);
            enemyPositions.push_back(std::make_pair(character, position));
        }
        actorPositions.push_back(std::make_pair(character, position));

        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::scale(model, scale);
        glm::mat4 outlineModel(1.0f);
        outlineModel = glm::translate(outlineModel, position);
        outlineModel = glm::scale(outlineModel, scale * 1.055f);
        glUniform3fv(glGetUniformLocation(g_battleShader, "color"), 1, glm::value_ptr(color));

        glm::mat4 shadowModel(1.0f);
        shadowModel = glm::translate(shadowModel, glm::vec3(position.x, position.y, position.z));
        shadowModel = glm::scale(shadowModel, glm::vec3(scale.x * 0.56f, 1.0f, scale.z * 0.42f));
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(shadowModel));
        glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 2);
        glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
        glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
        glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), character->isAlly == 1 ? 0.34f : 0.28f);
        glBindVertexArray(g_battleShadowVao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 42);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glUniform1f(glGetUniformLocation(g_battleShader, "opacity"), 1.0f);

        bool hasImportedModel = false;
        if (character->isAlly == 0) {
            int modelIndex = enemyIndex < 5 ? enemyIndex : 0;
            ImportedModel& importedModel = g_enemyModels[modelIndex].isLoaded()
                ? g_enemyModels[modelIndex]
                : g_enemyModels[0];
            hasImportedModel = importedModel.isLoaded();
        }
        else {
            hasImportedModel = g_playerModel.isLoaded();
        }

        // 背面拡張アウトラインは、面の向きが揃った閉じた形状でのみ安定する。
        // 髪や服には両面・開放面があり、黒い複製形状が発生するため適用しない。
        // このパスは閉じた代替立方体だけに限定する。
        if (!hasImportedModel) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(outlineModel));
            glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 1);
            glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
            glUniform1i(glGetUniformLocation(g_battleShader, "useTexture"), 0);
            if (character->isAlly == 0) {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
            glDisable(GL_CULL_FACE);
        }

        glUniformMatrix4fv(glGetUniformLocation(g_battleShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform1i(glGetUniformLocation(g_battleShader, "outlineMode"), 0);
        if (character->isAlly == 0) {
            int modelIndex = enemyIndex < 5 ? enemyIndex : 0;
            ImportedModel& importedModel = g_enemyModels[modelIndex].isLoaded()
                ? g_enemyModels[modelIndex]
                : g_enemyModels[0];
            if (importedModel.isLoaded()) {
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 1);
                importedModel.draw();
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }
        }
        else {
            if (g_playerModel.isLoaded()) {
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 1);
                g_playerModel.draw();
                glUniform1i(glGetUniformLocation(g_battleShader, "useVertexColor"), 0);
            }
            else {
                glBindVertexArray(g_battleCubeVao);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
            }

            // 簡易剣はプレイヤーの描画直後に重ね、攻撃中だけでなく待機中も
            // 右手側に保持する。これにより抜刀前後で突然出現・消失しない。
            DrawBattleSword(position, playerCommandAnimationTime,
                playerCommandAnimating && character == queuedPlayerAttacker);
        }
    }
    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);

    bool canSelect = activeChar && activeChar->isAlly == 1 && battleState == BattleState::InProgress;
    for (size_t i = 0; i < enemyPositions.size(); ++i) {
        Character* enemy = enemyPositions[i].first;
        glm::vec3 position = enemyPositions[i].second;
        ImVec2 feet = WorldToScreen(position, viewProjection, screenWidth, screenHeight);
        ImVec2 head = WorldToScreen(position + glm::vec3(0.0f, 2.7f, 0.0f), viewProjection, screenWidth, screenHeight);
        float height = std::max(90.0f, feet.y - head.y);
        float width = height * 0.65f;
        ImVec2 minPos(feet.x - width * 0.5f, head.y);

        ImGui::SetNextWindowPos(minPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        std::string hitWindow = "##Enemy3DHit" + std::to_string(i);
        if (ImGui::Begin(hitWindow.c_str(), nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if (canSelect && ImGui::InvisibleButton("##EnemyModel", ImVec2(width, height))) {
                markedTarget = markedTarget == enemy ? nullptr : enemy;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (markedTarget == enemy) {
            ImVec2 center(feet.x, head.y - 22.0f);
            drawList->AddCircle(center, 15.0f, IM_COL32(255, 205, 45, 255), 24, 3.0f);
            drawList->AddLine(ImVec2(center.x - 23.0f, center.y), ImVec2(center.x - 8.0f, center.y), IM_COL32(255, 205, 45, 255), 3.0f);
            drawList->AddLine(ImVec2(center.x + 8.0f, center.y), ImVec2(center.x + 23.0f, center.y), IM_COL32(255, 205, 45, 255), 3.0f);
        }
        float hpRatio = enemy->hp > 0 ? static_cast<float>(enemy->currentHp) / static_cast<float>(enemy->hp) : 0.0f;
        hpRatio = std::max(0.0f, std::min(hpRatio, 1.0f));
        ImVec2 barMin(head.x - 58.0f, head.y - 62.0f);
        ImVec2 barMax(head.x + 58.0f, head.y - 48.0f);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(24, 24, 28, 230), 3.0f);
        drawList->AddRectFilled(
            ImVec2(barMin.x + 2.0f, barMin.y + 2.0f),
            ImVec2(barMin.x + 2.0f + (barMax.x - barMin.x - 4.0f) * hpRatio, barMax.y - 2.0f),
            hpRatio > 0.35f ? IM_COL32(65, 210, 90, 255) : IM_COL32(235, 65, 55, 255),
            2.0f
        );
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 180), 3.0f);

        if (enemy->maxTideguard > 0) {
            float guardRatio = (float)enemy->tideguard / (float)enemy->maxTideguard;
            ImVec2 guardMin(barMin.x, barMax.y + 4.0f);
            ImVec2 guardMax(barMax.x, barMax.y + 10.0f);
            drawList->AddRectFilled(guardMin, guardMax, IM_COL32(12, 30, 38, 225), 2.0f);
            drawList->AddRectFilled(guardMin,
                ImVec2(guardMin.x + (guardMax.x - guardMin.x) * guardRatio, guardMax.y),
                enemy->isBroken ? IM_COL32(255, 224, 151, 255) : IM_COL32(72, 220, 215, 255), 2.0f);
            ImVec2 labelPos(barMin.x, barMin.y - 22.0f);
            std::string label = enemy->isBroken
                ? std::string(u8"潮防崩壊  /  ") + enemy->affinity
                : enemy->name + "  [ " + enemy->affinity + " ]";
            drawList->AddText(ImGui::GetFont(), 15.0f, labelPos,
                enemy->isBroken ? IM_COL32(255, 226, 160, 255) : IM_COL32(208, 243, 244, 245),
                label.c_str());
        }

    }

    double now = ImGui::GetTime();
    const double popupLifetime = 0.92;
    damagePopups.erase(
        std::remove_if(damagePopups.begin(), damagePopups.end(), [now, popupLifetime](const DamagePopup& popup) {
            return now - popup.startTime > popupLifetime || !popup.target;
        }),
        damagePopups.end()
    );

    ImDrawList* popupDrawList = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    for (const DamagePopup& popup : damagePopups) {
        auto actorIt = std::find_if(actorPositions.begin(), actorPositions.end(), [&popup](const std::pair<Character*, glm::vec3>& actor) {
            return actor.first == popup.target;
        });
        if (actorIt == actorPositions.end()) continue;

        float t = static_cast<float>((now - popup.startTime) / popupLifetime);
        t = std::max(0.0f, std::min(t, 1.0f));
        float rise = 72.0f * t;
        float punch = EaseOutBack(std::min(t * 2.2f, 1.0f));
        int alpha = static_cast<int>((1.0f - std::max(0.0f, (t - 0.58f) / 0.42f)) * 255.0f);
        alpha = std::max(0, std::min(alpha, 255));

        glm::vec3 popupWorld = actorIt->second + glm::vec3(0.0f, popup.target->isAlly == 1 ? 4.7f : 3.25f, 0.0f);
        ImVec2 anchor = WorldToScreen(popupWorld, viewProjection, screenWidth, screenHeight);
        if (anchor.x < -999.0f) continue;

        std::string text = popup.isCritical
            ? "CRIT " + std::to_string(popup.amount)
            : std::to_string(popup.amount);
        float fontSize = ImGui::GetFontSize() * (popup.isCritical ? 1.58f : 1.34f) * (0.82f + 0.18f * punch);
        ImVec2 popupTextSize = font->CalcTextSizeA(fontSize, 10000.0f, 0.0f, text.c_str());
        ImVec2 pos(anchor.x + popup.xOffset - popupTextSize.x * 0.5f, anchor.y - rise - popupTextSize.y * 0.5f);
        ImU32 shadowColor = IM_COL32(12, 14, 20, alpha);
        ImU32 fillColor = popup.isCritical
            ? IM_COL32(255, 218, 72, alpha)
            : IM_COL32(255, 245, 232, alpha);

        popupDrawList->AddText(font, fontSize, ImVec2(pos.x + 2.0f, pos.y + 2.0f), shadowColor, text.c_str());
        popupDrawList->AddText(font, fontSize, ImVec2(pos.x - 1.0f, pos.y), shadowColor, text.c_str());
        popupDrawList->AddText(font, fontSize, pos, fillColor, text.c_str());
    }
}

void CombatSystem::renderBattleEndOverlay(int screenWidth, int screenHeight)
{
    double elapsed = battleEndQueued ? ImGui::GetTime() - battleEndStartTime : 0.0;
    float intro = std::max(0.0f, std::min(static_cast<float>(elapsed / 0.35), 1.0f));
    float fade = std::max(0.0f, std::min(static_cast<float>((elapsed - 1.05) / 1.15), 1.0f));

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)),
        IM_COL32(0, 0, 0, static_cast<int>(60 + fade * 195.0f))
    );

    const char* resultText = battleEndResult == BattleState::Victory ? "Victory" : "Defeat";
    ImFont* font = ImGui::GetFont();
    float fontSize = 72.0f;
    ImVec2 textSize = font->CalcTextSizeA(fontSize, 10000.0f, 0.0f, resultText);
    ImVec2 textPos(
        (static_cast<float>(screenWidth) - textSize.x) * 0.5f,
        static_cast<float>(screenHeight) * 0.42f - textSize.y * 0.5f
    );
    int textAlpha = static_cast<int>(255.0f * intro);
    ImU32 shadowColor = IM_COL32(0, 0, 0, textAlpha);
    ImU32 textColor = battleEndResult == BattleState::Victory
        ? IM_COL32(120, 255, 150, textAlpha)
        : IM_COL32(255, 115, 115, textAlpha);

    drawList->AddText(font, fontSize, ImVec2(textPos.x + 4.0f, textPos.y + 4.0f), shadowColor, resultText);
    drawList->AddText(font, fontSize, textPos, textColor, resultText);
}

void CombatSystem::renderBattleCards(Character* activeChar, float screenWidth, float marginX, float marginY, float cardWidth, float cardHeight, float spacingY)
{
    float currentY = marginY;
    int orderNumber = 1;

    for (size_t i = 0; i < participants.size(); i++)
    {
        Character* c = participants[i];
        if (!c || c->currentHp <= 0) continue;

        bool isMarkedTarget = markedTarget == c;

        ImGui::SetNextWindowPos(ImVec2(marginX, currentY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);

        ImVec4 bgColor = (c->isAlly == 1)
            ? ImVec4(0.06f, 0.14f, 0.22f, 0.90f)
            : ImVec4(0.20f, 0.07f, 0.08f, 0.90f);
        ImVec4 borderColor = (c == activeChar)
            ? ImVec4(0.95f, 0.80f, 0.32f, 1.0f)
            : ImVec4(0.55f, 0.60f, 0.66f, 0.55f);
        if (isMarkedTarget) borderColor = ImVec4(0.2f, 0.85f, 1.0f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, isMarkedTarget ? 4.0f : 2.0f);

        std::string windowName = "##BattleCard" + std::to_string(i);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin(windowName.c_str(), nullptr, flags))
        {
            ImGui::TextColored(
                c == activeChar ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f) : ImVec4(0.7f, 0.74f, 0.8f, 1.0f),
                "%02d", orderNumber
            );
            ImGui::SameLine();
            ImGui::Text("%s", c->name.c_str());

            if (isMarkedTarget) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "LOCK");
            }
        }

        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        currentY += cardHeight + spacingY;
        ++orderNumber;
    }
}

// パーティ状態とコマンド一覧を、同じ基準座標を使うHUDレイヤーとして描画する。
void CombatSystem::renderActionMenu(Character* activeChar, int screenWidth, int screenHeight)
{
    ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoDecoration;

    ImGui::SetNextWindowPos(ImVec2(24.0f, (float)screenHeight - 266.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(480.0f, 258.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (ImGui::Begin("##PartyHud", nullptr, hudFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        int allyIndex = 0;
        for (auto* c : participants) {
            if (!c || c->isAlly != 1) continue;

            float hpFraction = c->hp > 0 ? (float)c->currentHp / (float)c->hp : 0.0f;
            float spFraction = c->maxEnergy > 0 ? (float)c->energy / (float)c->maxEnergy : 0.0f;
            hpFraction = std::max(0.0f, std::min(hpFraction, 1.0f));
            spFraction = std::max(0.0f, std::min(spFraction, 1.0f));

            const float rowY = 4.0f + allyIndex * 124.0f;
            const ImVec2 sigil(origin.x + 54.0f, origin.y + rowY + 52.0f);
            const float textX = origin.x + 104.0f;
            const float barX = origin.x + 151.0f;
            const float barWidth = 275.0f;
            const bool isActive = c == activeChar;

            // 背景の明度に左右されず文字を読めるよう、濃い半透明パネルを敷く。
            dl->AddRectFilledMultiColor(
                ImVec2(origin.x + 5.0f, origin.y + rowY),
                ImVec2(origin.x + 465.0f, origin.y + rowY + 112.0f),
                IM_COL32(3, 7, 17, 205),
                IM_COL32(7, 10, 24, 174),
                IM_COL32(5, 8, 19, 192),
                IM_COL32(2, 5, 13, 215));
            dl->AddRect(
                ImVec2(origin.x + 5.0f, origin.y + rowY),
                ImVec2(origin.x + 465.0f, origin.y + rowY + 112.0f),
                IM_COL32(168, 132, 215, isActive ? 115 : 55), 3.0f, 0, 1.0f);

            // キャラクター情報の視線誘導点として円形シジルを描画する。
            dl->AddCircleFilled(sigil, 31.0f, IM_COL32(28, 20, 47, 205), 32);
            dl->AddCircle(sigil, 30.0f, IM_COL32(199, 165, 239, isActive ? 225 : 135), 32, 1.5f);
            dl->AddCircle(sigil, 20.0f, IM_COL32(158, 126, 201, 155), 24, 1.1f);
            dl->AddLine(ImVec2(sigil.x - 38.0f, sigil.y), ImVec2(sigil.x + 38.0f, sigil.y),
                IM_COL32(183, 151, 223, 105), 1.0f);
            dl->AddLine(ImVec2(sigil.x, sigil.y - 38.0f), ImVec2(sigil.x, sigil.y + 38.0f),
                IM_COL32(183, 151, 223, 105), 1.0f);
            dl->AddTriangle(
                ImVec2(sigil.x, sigil.y - 15.0f),
                ImVec2(sigil.x + 13.0f, sigil.y + 9.0f),
                ImVec2(sigil.x - 13.0f, sigil.y + 9.0f),
                IM_COL32(218, 199, 246, 170), 1.1f);
            dl->AddCircleFilled(sigil, 3.0f,
                isActive ? IM_COL32(226, 197, 255, 245) : IM_COL32(154, 132, 185, 155), 12);

            // キャラクター名と、HP領域を分ける細い区切り線を描画する。
            const float nameFontSize = 25.0f;
            const float statFontSize = 17.0f;
            ImFont* hudFont = ImGui::GetFont();
            dl->AddText(hudFont, nameFontSize,
                ImVec2(textX + 1.0f, origin.y + rowY + 8.0f),
                IM_COL32(39, 20, 65, 220), c->name.c_str());
            dl->AddText(hudFont, nameFontSize,
                ImVec2(textX, origin.y + rowY + 7.0f),
                isActive ? IM_COL32(250, 244, 255, 255) : IM_COL32(226, 223, 237, 240),
                c->name.c_str());
            dl->AddLine(
                ImVec2(textX, origin.y + rowY + 31.0f),
                ImVec2(origin.x + 449.0f, origin.y + rowY + 31.0f),
                IM_COL32(199, 158, 241, isActive ? 175 : 90), 1.0f);

            char hpText[32];
            char spText[32];
            snprintf(hpText, sizeof(hpText), "%d / %d", c->currentHp, c->hp);
            snprintf(spText, sizeof(spText), "%d / %d", c->energy, c->maxEnergy);

            dl->AddText(hudFont, statFontSize,
                ImVec2(textX, origin.y + rowY + 38.0f), IM_COL32(246, 240, 252, 255), "HP");
            dl->AddText(hudFont, statFontSize,
                ImVec2(textX, origin.y + rowY + 76.0f), IM_COL32(213, 182, 250, 255), "SP");
            ImVec2 hpValueSize = hudFont->CalcTextSizeA(statFontSize, FLT_MAX, 0.0f, hpText);
            ImVec2 spValueSize = hudFont->CalcTextSizeA(statFontSize, FLT_MAX, 0.0f, spText);
            dl->AddText(hudFont, statFontSize,
                ImVec2(origin.x + 449.0f - hpValueSize.x, origin.y + rowY + 38.0f),
                IM_COL32(250, 246, 255, 255), hpText);
            dl->AddText(hudFont, statFontSize,
                ImVec2(origin.x + 449.0f - spValueSize.x, origin.y + rowY + 76.0f),
                IM_COL32(226, 202, 255, 245), spText);

            // HPとSPは数値を隠さない細い発光ラインとして描画する。
            const float hpY = origin.y + rowY + 60.0f;
            const float spY = origin.y + rowY + 98.0f;
            dl->AddRectFilled(ImVec2(barX, hpY), ImVec2(barX + barWidth, hpY + 6.0f),
                IM_COL32(24, 24, 39, 230), 2.0f);
            dl->AddRectFilled(ImVec2(barX, spY), ImVec2(barX + barWidth, spY + 5.0f),
                IM_COL32(23, 21, 38, 225), 2.0f);
            ImU32 hpColor = hpFraction < 0.25f
                ? IM_COL32(238, 88, 108, 245)
                : IM_COL32(217, 205, 237, 240);
            dl->AddRectFilled(ImVec2(barX, hpY), ImVec2(barX + barWidth * hpFraction, hpY + 6.0f),
                hpColor, 2.0f);
            dl->AddRectFilled(ImVec2(barX, spY), ImVec2(barX + barWidth * spFraction, spY + 5.0f),
                IM_COL32(180, 119, 241, 255), 2.0f);
            dl->AddCircleFilled(ImVec2(barX + barWidth * hpFraction, hpY + 3.0f), 3.5f, hpColor, 10);
            dl->AddCircleFilled(ImVec2(barX + barWidth * spFraction, spY + 2.5f), 3.0f,
                IM_COL32(211, 172, 255, 235), 10);

            ++allyIndex;
            if (allyIndex >= 2) break;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::SetNextWindowPos(ImVec2((float)screenWidth -110.f, 30.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(80.0f, 60.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.075f, 0.84f));
    if (ImGui::Begin("##LogButton", nullptr, hudFlags)) {
        if (ImGui::Button(isBattleLogOpen ? "Close" : "Log", ImVec2(-1.0f, 40.0f))) {
            isBattleLogOpen = !isBattleLogOpen;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();

    float commandWidth = std::min(360.0f, (float)screenWidth * 0.34f);
    ImGui::SetNextWindowPos(
        ImVec2((float)screenWidth - commandWidth - 34.0f, (float)screenHeight * 0.5f - 245.0f),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(commandWidth, 490.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    if (ImGui::Begin("##CommandHud", nullptr, hudFlags)) {
        if (battleState != BattleState::InProgress) {
            ImGui::TextColored(
                battleState == BattleState::Victory ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                battleState == BattleState::Victory ? "Victory" : "Defeat"
            );
            if (ImGui::Button("Back to Map", ImVec2(160, 42))) {
               
            }
        }

        // 味方・敵ターンでパネル位置を変えず、フェーズ切替時のHUDの跳ねを防ぐ。
        else if (activeChar && activeChar->isAlly == 1) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetWindowPos();
            const float rowHeight = 68.0f;
            const float menuTop = 62.0f;
            const float textX = 126.0f;
            const float nodeX = 88.0f;
            const ImU32 violet = IM_COL32(193, 139, 255, 255);
            const ImU32 pale = IM_COL32(239, 229, 255, 255);

            // 明るい3D背景の上でもラベルを読めるよう、柔らかい暗色面を敷く。
            const ImVec2 panelMin(origin.x + 38.0f, origin.y + 8.0f);
            const ImVec2 panelMax(origin.x + commandWidth - 5.0f, origin.y + menuTop + rowHeight * 5.0f + 8.0f);
            dl->AddRectFilledMultiColor(
                panelMin, panelMax,
                IM_COL32(2, 5, 13, 28),
                IM_COL32(3, 6, 16, 178),
                IM_COL32(3, 6, 16, 156),
                IM_COL32(2, 5, 13, 18));
            dl->AddLine(
                ImVec2(panelMax.x - 1.0f, panelMin.y + 24.0f),
                ImVec2(panelMax.x - 1.0f, panelMax.y - 24.0f),
                IM_COL32(176, 132, 225, 52), 1.0f);

            dl->AddText(ImVec2(origin.x + textX, origin.y + 22.0f),
                IM_COL32(185, 169, 208, 220),
                ("AP  " + std::to_string(currentPoints) + " / " + std::to_string(maxPoints)).c_str());
            dl->AddLine(ImVec2(origin.x + nodeX, origin.y + menuTop - 18.0f),
                ImVec2(origin.x + nodeX + 18.0f, origin.y + menuTop + rowHeight * 5.0f),
                IM_COL32(150, 103, 205, 60), 1.0f);

            // コマンド文字より奥に、低コントラストの破片と粒子を描画する。
            const float time = (float)ImGui::GetTime();
            for (int i = 0; i < 18; ++i) {
                float px = origin.x + 32.0f + fmodf((float)(i * 67), commandWidth + 100.0f);
                float py = origin.y + 18.0f + fmodf((float)(i * 43) + sinf(time * 0.7f + i) * 9.0f, 420.0f);
                float pulse = 0.45f + 0.35f * sinf(time * 1.7f + (float)i * 0.8f);
                ImU32 particleColor = IM_COL32(200, 163, 255, (int)(38.0f + 70.0f * pulse));
                dl->AddCircleFilled(ImVec2(px, py), (i % 4 == 0) ? 1.8f : 1.0f, particleColor, 8);
                if (i % 5 == 0) {
                    dl->AddLine(ImVec2(px - 8.0f, py), ImVec2(px + 8.0f, py), particleColor, 0.8f);
                    dl->AddLine(ImVec2(px, py - 8.0f), ImVec2(px, py + 8.0f), particleColor, 0.8f);
                }
            }
            dl->AddTriangleFilled(
                ImVec2(origin.x + commandWidth - 36.0f, origin.y + 78.0f),
                ImVec2(origin.x + commandWidth + 12.0f, origin.y + 54.0f),
                ImVec2(origin.x + commandWidth - 6.0f, origin.y + 112.0f),
                IM_COL32(153, 99, 215, 28));
            dl->AddTriangle(
                ImVec2(origin.x + commandWidth - 62.0f, origin.y + 328.0f),
                ImVec2(origin.x + commandWidth + 8.0f, origin.y + 294.0f),
                ImVec2(origin.x + commandWidth - 18.0f, origin.y + 375.0f),
                IM_COL32(190, 135, 247, 70), 1.0f);

            const char* labels[] = { u8"攻撃", u8"スキル", u8"アイテム", u8"防御", u8"逃走" };
            const char* subLabels[] = { "ATTACK", "SKILL", "ITEM", "GUARD", "ESCAPE" };
            const bool enabled[] = {
                true,
                currentPoints >= 1,
                false,
                true,
                false
            };

            for (int i = 0; i < 5; ++i) {
                const float y = menuTop + rowHeight * (float)i;
                ImGui::SetCursorPos(ImVec2(55.0f, y));
                ImGui::PushID(i);
                ImGui::InvisibleButton("##BattleCommand", ImVec2(commandWidth - 70.0f, 58.0f));
                const bool hovered = ImGui::IsItemHovered() && enabled[i];
                const bool clicked = ImGui::IsItemClicked() && enabled[i];
                ImGui::PopID();

                ImU32 mainColor = enabled[i]
                    ? (hovered ? IM_COL32_WHITE : IM_COL32(228, 226, 238, 235))
                    : IM_COL32(139, 136, 153, 125);
                ImU32 subColor = hovered
                    ? IM_COL32(222, 190, 255, 255)
                    : IM_COL32(155, 151, 177, enabled[i] ? 185 : 95);

                if (hovered) {
                    const ImVec2 arrow(origin.x + textX - 22.0f, origin.y + y + 23.0f);
                    dl->AddRectFilledMultiColor(
                        ImVec2(origin.x + 72.0f, arrow.y - 24.0f),
                        ImVec2(origin.x + commandWidth - 14.0f, arrow.y + 28.0f),
                        IM_COL32(102, 53, 166, 18),
                        IM_COL32(120, 64, 188, 104),
                        IM_COL32(76, 41, 130, 76),
                        IM_COL32(93, 48, 151, 14));
                    dl->AddTriangleFilled(
                        ImVec2(origin.x - commandWidth * 0.52f, arrow.y + 53.0f),
                        ImVec2(origin.x + textX + 196.0f, arrow.y - 17.0f),
                        ImVec2(origin.x + textX + 142.0f, arrow.y + 31.0f),
                        IM_COL32(127, 70, 191, 18));
                    dl->AddLine(ImVec2(origin.x - commandWidth * 0.82f, arrow.y + 42.0f),
                        ImVec2(arrow.x - 15.0f, arrow.y), IM_COL32(171, 101, 238, 150), 1.2f);
                    dl->AddLine(ImVec2(origin.x - commandWidth * 0.55f, arrow.y - 19.0f),
                        ImVec2(arrow.x - 15.0f, arrow.y), IM_COL32(211, 156, 255, 125), 1.0f);

                    // 選択中の項目だけ、右向きの発光矢印で現在位置を示す。
                    dl->AddCircleFilled(arrow, 15.0f, IM_COL32(136, 73, 204, 36), 20);
                    dl->AddTriangleFilled(
                        ImVec2(arrow.x - 12.0f, arrow.y - 9.0f),
                        ImVec2(arrow.x - 12.0f, arrow.y + 9.0f),
                        ImVec2(arrow.x + 2.0f, arrow.y),
                        IM_COL32(220, 186, 255, 225));
                    dl->AddTriangle(
                        ImVec2(arrow.x - 7.0f, arrow.y - 13.0f),
                        ImVec2(arrow.x - 7.0f, arrow.y + 13.0f),
                        ImVec2(arrow.x + 11.0f, arrow.y),
                        IM_COL32_WHITE, 1.5f);
                    dl->AddLine(
                        ImVec2(arrow.x - 20.0f, arrow.y),
                        ImVec2(arrow.x - 7.0f, arrow.y),
                        IM_COL32(224, 193, 255, 210), 2.0f);

                    dl->AddLine(ImVec2(arrow.x + 8.0f, arrow.y - 2.0f),
                        ImVec2(origin.x + commandWidth - 34.0f, arrow.y - 16.0f),
                        IM_COL32(190, 125, 248, 90), 0.8f);
                    dl->AddLine(
                        ImVec2(origin.x + textX + 8.0f, arrow.y + 27.0f),
                        ImVec2(origin.x + commandWidth - 32.0f, arrow.y + 27.0f),
                        IM_COL32(205, 155, 255, 185), 1.4f);
                }
                else {
                    dl->AddRect(ImVec2(origin.x + nodeX - 3.0f, origin.y + y + 21.0f),
                        ImVec2(origin.x + nodeX + 3.0f, origin.y + y + 27.0f),
                        IM_COL32(161, 130, 197, enabled[i] ? 100 : 38), 0.0f, 0, 1.0f);
                }

                char order[4];
                snprintf(order, sizeof(order), "%02d", i + 1);
                dl->AddText(ImVec2(origin.x + 50.0f, origin.y + y + 20.0f),
                    IM_COL32(129, 118, 149, enabled[i] ? 100 : 45), order);
                if (hovered) {
                    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.72f,
                        ImVec2(origin.x + textX + 11.5f, origin.y + y + 7.5f),
                        IM_COL32(103, 46, 166, 145), labels[i]);
                }
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.86f,
                    ImVec2(origin.x + textX + (hovered ? 10.0f : 0.0f), origin.y + y + 6.0f),
                    mainColor, labels[i]);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.65f,
                    ImVec2(origin.x + textX + 2.0f + (hovered ? 10.0f : 0.0f), origin.y + y + 38.0f),
                    subColor, subLabels[i]);

                if (!clicked) continue;
                // 攻撃の回収が終わるまで追加入力を無視し、二重実行を防ぐ。
                if (playerCommandAnimating) continue;

                // 明示選択がなければ、生存している敵から有効な対象を補完する。
                Character* target = markedTarget ? markedTarget : getRandomAliveTarget(0);
                if (i == 0) {
                    // 通常攻撃はAPを1回復し、攻撃クリップの11Fまで処理を保留する。
                    currentPoints = std::min(maxPoints, currentPoints + 1);
                    chooseCommand(BattleCommand::BasicAttack);
                    if (target && g_playerModel.playAnimationByName("Basic_Attack")) {
                        playerCommandAnimating = true;
                        playerCommandHitApplied = false;
                        playerCommandAnimationTime = 0.0f;
                        queuedPlayerAttacker = activeChar;
                        queuedPlayerTarget = target;
                    }
                    else executeCommand(activeChar, target);
                }
                else if (i == 1) {
                    // 現段階ではスキルも同じ身体攻撃を使う。専用クリップ追加時は
                    // ここで再生名とヒット時刻を差し替えられる。
                    currentPoints = std::max(0, currentPoints - 1);
                    chooseCommand(BattleCommand::Skill);
                    if (target && g_playerModel.playAnimationByName("Basic_Attack")) {
                        playerCommandAnimating = true;
                        playerCommandHitApplied = false;
                        playerCommandAnimationTime = 0.0f;
                        queuedPlayerAttacker = activeChar;
                        queuedPlayerTarget = target;
                    }
                    else executeCommand(activeChar, target);
                }
                else if (i == 3) {
                    executeGuard(activeChar);
                }
            }
        }
        else if (activeChar) {
            ImGui::TextColored(ImVec4(0.95f, 0.38f, 0.34f, 1.0f), "%s", activeChar->name.c_str());
            ImGui::Text("Enemy turn...");

            if (!enemyActionQueued) {
                enemyActionQueued = true;
                enemyActionTime = ImGui::GetTime() + 0.6;
            }

            if (ImGui::GetTime() >= enemyActionTime) {
                Character* target = getRandomAliveTarget(1);
                if (target) executeSkill(activeChar, target);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// ImGuiのPush/Popを必ず対にしながら、共通ボタンの配色と角丸を適用する。
void CombatSystem::DrawStyledButton(const char* label, const char* desc, ImVec4 color, float width, float height, std::function<void()> onClick) {
    ImVec4 hoveredColor = ImVec4(color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    if (ImGui::Button(label, ImVec2(width, height))) onClick();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", desc);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void CombatSystem::renderBattleLogWindow(int screenWidth, int screenHeight)
{
    if (!isBattleLogOpen) return;

    ImGui::SetNextWindowPos(ImVec2(950.f, 80.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300.f, 220.f), ImGuiCond_Always);
    if (ImGui::Begin("Battle Log", &isBattleLogOpen, ImGuiWindowFlags_NoResize)) {
        for (const auto& line : battleLog) {
            ImGui::BulletText("%s", line.c_str());
        }
    }
    ImGui::End();
}

