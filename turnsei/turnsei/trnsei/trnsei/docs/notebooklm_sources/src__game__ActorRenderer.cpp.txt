#include "ActorRenderer.h"
#include "../../assets/ImportedModel.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>

namespace
{
    GLuint actorShader = 0;
    GLuint actorVao = 0;
    GLuint actorVbo = 0;
    GLuint actorEbo = 0;
    GLuint actorShadowVao = 0;
    GLuint actorShadowVbo = 0;
    WalkAnimState defaultWalkAnim;

    GLuint CompileShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }
}

GLuint GetActorShader() { return actorShader; }

void UpdateWalkAnimation(WalkAnimState& state, float deltaTime, bool isWalking)
{
    if (isWalking) state.time += deltaTime;

    float blendTarget = isWalking ? 1.0f : 0.0f;
    float blendSpeed = isWalking ? 8.0f : 6.0f;
    state.blend += (blendTarget - state.blend) * (1.0f - std::exp(-blendSpeed * deltaTime));
    if (!isWalking && state.blend < 0.01f) {
        state.blend = 0.0f;
        state.time = 0.0f;
    }
}

void InitActorRenderer()
{
    if (actorShader != 0) return;

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
out vec3 vWorldPos;
out vec3 vColor;
out vec2 vTexCoords;
void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vColor = aColor;
    vTexCoords = aTexCoords;
    gl_Position = projection * view * world;
}
)";
    const char* fragmentSource = R"(
#version 330 core
uniform vec3 actorColor;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int outlineMode;
uniform int useVertexColor;
uniform int useTexture;
uniform float opacity;
uniform float materialRoughness;
uniform float materialMetallic;
uniform vec3 materialEmission;
uniform int environmentMode;
uniform int useShadowMap;
uniform mat4 lightSpaceMatrix;
uniform sampler2D shadowMap;
uniform sampler2D diffuseTexture;
in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vColor;
in vec2 vTexCoords;
out vec4 FragColor;
const float PI = 3.14159265359;
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float nh = max(dot(N, H), 0.0);
    float d = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 0.0001);
}
float GeometrySchlickGGX(float nv, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nv / max(nv * (1.0 - k) + k, 0.0001);
}
vec3 FresnelSchlick(float cosine, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosine, 5.0);
}
vec3 ACESFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) /
                 (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float SampleSunShadow(vec3 normal, vec3 sunDir) {
    if (useShadowMap == 0) return 0.0;
    vec4 lightClip = lightSpaceMatrix * vec4(vWorldPos, 1.0);
    vec3 p = lightClip.xyz / lightClip.w;
    p = p * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 0.0;
    float bias = max(0.0012 * (1.0 - dot(normal, sunDir)), 0.00022);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x)
        for (int y = -2; y <= 2; ++y)
            result += p.z - bias > texture(shadowMap, p.xy + vec2(x,y) * texel).r ? 1.0 : 0.0;
    return result / 25.0;
}
void main()
{
    if (outlineMode == 1) { FragColor = vec4(0.020, 0.024, 0.038, opacity); return; }
    if (outlineMode == 2) { FragColor = vec4(0.012, 0.014, 0.024, opacity); return; }

    vec3 viewDir = normalize(cameraPos - vWorldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) n = -n;
    vec3 l = normalize(lightDir);
    vec3 halfDir = normalize(l + viewDir);
    float ndl = max(dot(n, l), 0.0);
    float ndv = max(dot(n, viewDir), 0.001);
    float rough = clamp(materialRoughness, 0.04, 1.0);
    float metal = clamp(materialMetallic, 0.0, 1.0);
    vec3 baseColor = useVertexColor == 1 ? vColor : actorColor;
    if (useTexture == 1) baseColor *= texture(diffuseTexture, vTexCoords).rgb;
    baseColor = pow(max(baseColor, vec3(0.001)), vec3(2.2));
    if (environmentMode == 1) {
        float macro = sin(vWorldPos.x * 1.71 + sin(vWorldPos.z * 2.13)) * 0.5 + 0.5;
        float waterline = 1.0 - smoothstep(0.0, 1.8, abs(vWorldPos.y - 0.70));
        baseColor *= mix(0.90, 1.04, macro);
        baseColor = mix(baseColor, baseColor * vec3(0.42, 0.62, 0.54), waterline * rough * 0.34);
        rough = clamp(rough - waterline * 0.20, 0.045, 1.0);
    }
    vec3 f0 = mix(vec3(0.04), baseColor, metal);
    float D = DistributionGGX(n, halfDir, rough);
    float G = GeometrySchlickGGX(ndv, rough) * GeometrySchlickGGX(ndl, rough);
    vec3 F = FresnelSchlick(max(dot(halfDir, viewDir), 0.0), f0);
    vec3 specular = (D * G * F) / max(4.0 * ndv * ndl, 0.001);
    vec3 kd = (vec3(1.0) - F) * (1.0 - metal);
    float shadow = SampleSunShadow(n, l);
    // PBRの材質応答を維持したまま、拡散光の明暗境界だけを演出的に整える。
    // 材質ごとに単純な二値化を行わず、影の柔らかさを共通制御する。
    float halfLambert = dot(n, l) * 0.5 + 0.5;
    float toonKey = smoothstep(0.38, 0.58, halfLambert);
    float penumbra = smoothstep(0.08, 0.92, 1.0 - shadow);
    float shapedDiffuse = mix(ndl * 0.42, max(ndl, 0.16), toonKey) * penumbra;
    vec3 sunRadiance = vec3(4.55, 4.12, 3.62);
    vec3 warmKey = sunRadiance * mix(vec3(0.86, 0.93, 1.04),
                                    vec3(1.08, 1.01, 0.91), toonKey);
    vec3 direct = (kd * baseColor / PI * shapedDiffuse +
                   specular * ndl * (1.0 - shadow * 0.90)) * warmKey;
    float skyAmount = clamp(n.y * 0.5 + 0.5, 0.0, 1.0);
    float groundAmount = clamp(-n.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyIrradiance = mix(vec3(0.075, 0.115, 0.165),
                             vec3(0.24, 0.34, 0.40), skyAmount);
    vec3 waterBounce = vec3(0.035, 0.115, 0.125) *
                       groundAmount * (environmentMode == 1 ? 1.0 : 0.28);
    vec3 ambient = baseColor * (skyIrradiance + waterBounce) *
                   mix(0.72, 1.0, 1.0 - metal);
    vec3 color = direct + ambient;
    color += materialEmission * 2.8;
    float reflectiveSurface = 1.0 - smoothstep(0.10, 0.30, rough);
    float fresnel = pow(1.0 - max(dot(n, viewDir), 0.0), 5.0);
    vec3 skyReflection = mix(vec3(0.025, 0.075, 0.105),
                             vec3(0.30, 0.52, 0.62),
                             clamp(n.y * 0.5 + 0.5, 0.0, 1.0));
    color = mix(color, skyReflection,
                reflectiveSurface * (0.16 + 0.58 * fresnel));
    // カメラ方向の寒色リムライトで、情報量の多い背景から人物を分離する。
    float rimMask = smoothstep(0.52, 0.92,
        pow(1.0 - clamp(dot(n, viewDir), 0.0, 1.0), 1.65));
    float backLight = smoothstep(-0.15, 0.55, dot(-l, viewDir));
    color += mix(vec3(0.055, 0.15, 0.21), vec3(0.16, 0.29, 0.34),
                 skyAmount) * rimMask * (0.28 + 0.72 * backLight);
    float fog = smoothstep(32.0, 105.0, length(cameraPos - vWorldPos));
    color = mix(color, vec3(0.035, 0.090, 0.105), fog * 0.78);
    color = ACESFilm(color * 0.92);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, opacity);
}
)";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    actorShader = glCreateProgram();
    glAttachShader(actorShader, vs);
    glAttachShader(actorShader, fs);
    glLinkProgram(actorShader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    const float vertices[] = {
        -0.5f,0,-0.5f, 0,0,-1, 0.5f,0,-0.5f, 0,0,-1, 0.5f,1,-0.5f, 0,0,-1, -0.5f,1,-0.5f, 0,0,-1,
        -0.5f,0,0.5f, 0,0,1, 0.5f,0,0.5f, 0,0,1, 0.5f,1,0.5f, 0,0,1, -0.5f,1,0.5f, 0,0,1,
        -0.5f,0,-0.5f, -1,0,0, -0.5f,0,0.5f, -1,0,0, -0.5f,1,0.5f, -1,0,0, -0.5f,1,-0.5f, -1,0,0,
        0.5f,0,-0.5f, 1,0,0, 0.5f,0,0.5f, 1,0,0, 0.5f,1,0.5f, 1,0,0, 0.5f,1,-0.5f, 1,0,0,
        -0.5f,1,-0.5f, 0,1,0, 0.5f,1,-0.5f, 0,1,0, 0.5f,1,0.5f, 0,1,0, -0.5f,1,0.5f, 0,1,0,
        -0.5f,0,-0.5f, 0,-1,0, 0.5f,0,-0.5f, 0,-1,0, 0.5f,0,0.5f, 0,-1,0, -0.5f,0,0.5f, 0,-1,0,
    };
    const unsigned int indices[] = {
        0,1,2,2,3,0, 4,6,5,6,4,7, 8,9,10,10,11,8,
        12,15,14,14,13,12, 16,17,18,18,19,16, 20,23,22,22,21,20
    };
    glGenVertexArrays(1, &actorVao);
    glGenBuffers(1, &actorVbo);
    glGenBuffers(1, &actorEbo);
    glBindVertexArray(actorVao);
    glBindBuffer(GL_ARRAY_BUFFER, actorVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, actorEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    std::vector<float> shadowVerts;
    const int segs = 40;
    shadowVerts.insert(shadowVerts.end(), { 0,0.025f,0, 0,1,0, 0,0,0 });
    for (int i = 0; i <= segs; ++i) {
        float a = (float)i / segs * glm::two_pi<float>();
        shadowVerts.insert(shadowVerts.end(), { std::cos(a),0.025f,std::sin(a), 0,1,0, 0,0,0 });
    }
    glGenVertexArrays(1, &actorShadowVao);
    glGenBuffers(1, &actorShadowVbo);
    glBindVertexArray(actorShadowVao);
    glBindBuffer(GL_ARRAY_BUFFER, actorShadowVbo);
    glBufferData(GL_ARRAY_BUFFER, shadowVerts.size() * sizeof(float), shadowVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void DrawActor(
    const glm::vec3& position,
    const glm::vec3& scale,
    const glm::vec3& color,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition,
    float rotationY,
    const ImportedModel* importedModel,
    bool isWalking,
    WalkAnimState* walkAnim)
{
    WalkAnimState& anim = walkAnim ? *walkAnim : defaultWalkAnim;

    glm::mat4 model(1.0f);
    glm::vec3 drawPos = position;
    float modelFacingY = rotationY + 180.0f;

    bool proceduralWalk = isWalking && !(importedModel && importedModel->hasAnimation());
    if (proceduralWalk || anim.blend > 0.001f) {
        float bob = std::abs(std::sin(anim.time * 9.5f)) * 0.16f;
        float tilt = std::sin(anim.time * 4.75f) * 0.085f;
        float sway = std::sin(anim.time * 9.5f) * 0.050f;
        drawPos.y += bob * anim.blend;
        model = glm::translate(model, drawPos);
        model = glm::rotate(model, glm::radians(modelFacingY), glm::vec3(0,1,0));
        model = glm::rotate(model, tilt * anim.blend, glm::vec3(0,0,1));
        model = glm::rotate(model, sway * anim.blend, glm::vec3(1,0,0));
        model = glm::scale(model, scale);
    }
    else {
        model = glm::translate(model, drawPos);
        model = glm::rotate(model, glm::radians(modelFacingY), glm::vec3(0,1,0));
        model = glm::scale(model, scale);
    }

    glUseProgram(actorShader);
    glUniformMatrix4fv(glGetUniformLocation(actorShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(actorShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(actorShader, "actorColor"), 1, glm::value_ptr(color));
    glUniform3f(glGetUniformLocation(actorShader, "lightDir"), -0.25f, 0.92f, 0.18f);
    glUniform3fv(glGetUniformLocation(actorShader, "cameraPos"), 1, glm::value_ptr(cameraPosition));
    glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
    glUniform1i(glGetUniformLocation(actorShader, "useTexture"), 0);
    glUniform1f(glGetUniformLocation(actorShader, "opacity"), 1.0f);
    glUniform1f(glGetUniformLocation(actorShader, "materialRoughness"), 0.62f);
    glUniform1f(glGetUniformLocation(actorShader, "materialMetallic"), 0.0f);
    glUniform3f(glGetUniformLocation(actorShader, "materialEmission"), 0.0f, 0.0f, 0.0f);
    glUniform1i(glGetUniformLocation(actorShader, "environmentMode"), 0);

    glm::mat4 shadowModel(1.0f);
    shadowModel = glm::translate(shadowModel, glm::vec3(position.x, -0.01f, position.z));
    shadowModel = glm::scale(shadowModel, glm::vec3(scale.x * 0.66f, 1.0f, scale.z * 0.50f));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(shadowModel));
    glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 2);
    glUniform1f(glGetUniformLocation(actorShader, "opacity"), importedModel && importedModel->isLoaded() ? 0.24f : 0.25f);
    glBindVertexArray(actorShadowVao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 42);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUniform1f(glGetUniformLocation(actorShader, "opacity"), 1.0f);
    glBindVertexArray(actorVao);

    if (!(importedModel && importedModel->isLoaded())) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glm::mat4 outline(1.0f);
        outline = glm::translate(outline, position);
        outline = glm::rotate(outline, glm::radians(modelFacingY), glm::vec3(0,1,0));
        outline = glm::scale(outline, scale * 1.07f);
        glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(outline));
        glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 1);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        glDisable(GL_CULL_FACE);
    }

    glUniformMatrix4fv(glGetUniformLocation(actorShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(actorShader, "outlineMode"), 0);
    if (importedModel && importedModel->isLoaded()) {
        glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 1);
        importedModel->draw();
        glUniform1i(glGetUniformLocation(actorShader, "useVertexColor"), 0);
    }
    else {
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }
}
