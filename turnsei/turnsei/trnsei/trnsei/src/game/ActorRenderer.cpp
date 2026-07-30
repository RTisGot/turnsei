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
uniform sampler2D diffuseTexture;
in vec3 vNormal;
in vec3 vWorldPos;
in vec3 vColor;
in vec2 vTexCoords;
out vec4 FragColor;
void main()
{
    if (outlineMode == 1) { FragColor = vec4(0.020, 0.024, 0.038, opacity); return; }
    if (outlineMode == 2) { FragColor = vec4(0.012, 0.014, 0.024, opacity); return; }

    vec3 viewDir = normalize(cameraPos - vWorldPos);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) n = -n;
    vec3 l = normalize(vec3(-0.28, 0.92, 0.24));
    vec3 halfDir = normalize(l + viewDir);
    float ndl = max(dot(n, l), 0.0);
    float hemi = max(n.y, 0.0);
    float lit = max(ndl, hemi * 0.58);
    float band = 0.48;
    if (lit > 0.86) band = 1.18;
    else if (lit > 0.58) band = 0.94;
    else if (lit > 0.30) band = 0.66;

    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.35);
    float fresnelLine = smoothstep(0.42, 0.86, rim);
    float spec = pow(max(dot(n, halfDir), 0.0), 72.0) * smoothstep(0.58, 0.82, lit);
    vec3 baseColor = useVertexColor == 1 ? vColor : actorColor;
    if (useTexture == 1) baseColor *= texture(diffuseTexture, vTexCoords).rgb;
    baseColor = pow(baseColor, vec3(0.92));
    vec3 shadowTint = vec3(0.18, 0.22, 0.34);
    vec3 warmLight = vec3(1.08, 1.02, 0.94);
    vec3 color = mix(baseColor * shadowTint, baseColor * warmLight, band);
    color += vec3(0.30, 0.55, 0.82) * fresnelLine * 0.34;
    color += vec3(1.00, 0.92, 0.72) * spec * 0.42;
    color = mix(vec3(dot(color, vec3(0.299, 0.587, 0.114))), color, 1.14);
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
