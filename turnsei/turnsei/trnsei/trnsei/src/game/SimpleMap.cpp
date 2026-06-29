#include "SimpleMap.h"
#include "ActorRenderer.h"

#include <glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace
{
    GLuint mapGroundVao = 0, mapGroundVbo = 0;
    GLuint mapPropVao = 0, mapPropVbo = 0, mapPropEbo = 0;
    int mapPropIndexCount = 0;
    int groundIndexCount = 0;
    bool initialized = false;

    struct MapProp {
        glm::vec3 position;
        glm::vec3 scale;
        glm::vec3 color;
    };
    std::vector<MapProp> mapProps;

    float PathDistance(float x, float z)
    {
        float d1 = std::abs(x);
        float curve1Z = std::sin(x * 0.06f) * 8.0f;
        float d2 = std::abs(z - curve1Z);
        float d3 = std::abs(z - 7.0f);
        float d4 = std::abs(x);
        float cross = std::min(d3, d4);
        return std::min({ d1, d2, cross });
    }

    float RawTerrainHeight(float x, float z)
    {
        float hills = std::sin(x * 0.05f) * std::cos(z * 0.04f) * 2.5f
            + std::sin(x * 0.12f + 1.3f) * std::cos(z * 0.08f - 0.7f) * 1.2f
            + std::sin(x * 0.03f - 0.5f) * std::sin(z * 0.025f + 1.0f) * 3.5f;

        float dist = std::sqrt(x * x + z * z);
        float centerFlat = 1.0f - std::min(dist / 12.0f, 1.0f);
        centerFlat = centerFlat * centerFlat;
        hills *= (1.0f - centerFlat * 0.9f);

        float pd = PathDistance(x, z);
        float pathBlend = 1.0f - std::min(pd / 2.8f, 1.0f);
        pathBlend = pathBlend * pathBlend * (3.0f - 2.0f * pathBlend);
        hills *= (1.0f - pathBlend * 0.9f);

        return hills;
    }
}

float GetTerrainHeight(float x, float z)
{
    return RawTerrainHeight(x, z);
}

void InitSimpleMap()
{
    if (initialized) return;
    initialized = true;

    const float groundSize = 60.0f;
    const int gridRes = 60;

    std::vector<float> groundVerts;
    for (int z = 0; z <= gridRes; ++z) {
        for (int x = 0; x <= gridRes; ++x) {
            float fx = ((float)x / gridRes - 0.5f) * groundSize * 2.0f;
            float fz = ((float)z / gridRes - 0.5f) * groundSize * 2.0f;
            float height = RawTerrainHeight(fx, fz);

            float pd = PathDistance(fx, fz);
            float pathBlend = 1.0f - std::min(pd / 2.8f, 1.0f);
            pathBlend = pathBlend * pathBlend * (3.0f - 2.0f * pathBlend);

            float heightFactor = std::max(0.0f, std::min(height / 4.0f, 1.0f));
            glm::vec3 lowGrass(0.22f, 0.40f, 0.14f);
            glm::vec3 highGrass(0.35f, 0.55f, 0.22f);
            glm::vec3 grassColor = glm::mix(lowGrass, highGrass, heightFactor);
            glm::vec3 pathColor(0.55f, 0.48f, 0.35f);
            glm::vec3 edgeColor(0.38f, 0.44f, 0.25f);

            glm::vec3 color;
            if (pathBlend > 0.6f)
                color = glm::mix(edgeColor, pathColor, (pathBlend - 0.6f) / 0.4f);
            else if (pathBlend > 0.1f)
                color = glm::mix(grassColor, edgeColor, (pathBlend - 0.1f) / 0.5f);
            else {
                float noise = std::sin(fx * 0.5f + fz * 0.3f) * 0.03f;
                color = grassColor + glm::vec3(noise, noise * 0.5f, -noise);
            }

            groundVerts.insert(groundVerts.end(), {
                fx, height, fz, 0.0f, 1.0f, 0.0f, color.r, color.g, color.b
            });
        }
    }

    std::vector<unsigned int> groundIdx;
    for (int z = 0; z < gridRes; ++z) {
        for (int x = 0; x < gridRes; ++x) {
            unsigned int tl = z * (gridRes + 1) + x;
            unsigned int tr = tl + 1;
            unsigned int bl = (z + 1) * (gridRes + 1) + x;
            unsigned int br = bl + 1;
            groundIdx.insert(groundIdx.end(), { tl, bl, tr, tr, bl, br });
        }
    }
    groundIndexCount = (int)groundIdx.size();

    GLuint groundEbo = 0;
    glGenVertexArrays(1, &mapGroundVao);
    glGenBuffers(1, &mapGroundVbo);
    glGenBuffers(1, &groundEbo);
    glBindVertexArray(mapGroundVao);
    glBindBuffer(GL_ARRAY_BUFFER, mapGroundVbo);
    glBufferData(GL_ARRAY_BUFFER, groundVerts.size() * sizeof(float), groundVerts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, groundIdx.size() * sizeof(unsigned int), groundIdx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    const float cubeV[] = {
        -0.5f,0,-0.5f, 0,0,-1, 0.5f,0,-0.5f, 0,0,-1, 0.5f,1,-0.5f, 0,0,-1, -0.5f,1,-0.5f, 0,0,-1,
        -0.5f,0,0.5f, 0,0,1, 0.5f,0,0.5f, 0,0,1, 0.5f,1,0.5f, 0,0,1, -0.5f,1,0.5f, 0,0,1,
        -0.5f,0,-0.5f, -1,0,0, -0.5f,0,0.5f, -1,0,0, -0.5f,1,0.5f, -1,0,0, -0.5f,1,-0.5f, -1,0,0,
        0.5f,0,-0.5f, 1,0,0, 0.5f,0,0.5f, 1,0,0, 0.5f,1,0.5f, 1,0,0, 0.5f,1,-0.5f, 1,0,0,
        -0.5f,1,-0.5f, 0,1,0, 0.5f,1,-0.5f, 0,1,0, 0.5f,1,0.5f, 0,1,0, -0.5f,1,0.5f, 0,1,0,
    };
    const unsigned int cubeI[] = {
        0,1,2,2,3,0, 4,6,5,6,4,7, 8,9,10,10,11,8,
        12,15,14,14,13,12, 16,17,18,18,19,16
    };
    glGenVertexArrays(1, &mapPropVao);
    glGenBuffers(1, &mapPropVbo);
    glGenBuffers(1, &mapPropEbo);
    glBindVertexArray(mapPropVao);
    glBindBuffer(GL_ARRAY_BUFFER, mapPropVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeV), cubeV, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mapPropEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeI), cubeI, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    mapPropIndexCount = 30;

    srand(42);

    auto addProp = [](float x, float z, glm::vec3 s, glm::vec3 c) {
        mapProps.push_back({ glm::vec3(x, RawTerrainHeight(x, z), z), s, c });
    };

    for (int i = 0; i < 25; ++i) {
        float x = ((rand() % 1000) / 1000.0f - 0.5f) * 90.0f;
        float z = ((rand() % 1000) / 1000.0f - 0.5f) * 90.0f;
        if (std::sqrt(x*x + z*z) < 5.0f || PathDistance(x, z) < 4.5f) continue;
        float h = 3.0f + (rand() % 100) / 50.0f;
        float groundY = RawTerrainHeight(x, z);
        addProp(x, z, glm::vec3(0.5f, h, 0.5f), glm::vec3(0.35f, 0.25f, 0.12f));
        float r = 1.8f + (rand() % 100) / 80.0f;
        mapProps.push_back({ glm::vec3(x, groundY + h - 0.5f, z), glm::vec3(r, r*0.9f, r),
            glm::vec3(0.15f + (rand()%20)/100.0f, 0.42f + (rand()%20)/100.0f, 0.10f) });
    }
    for (int i = 0; i < 15; ++i) {
        float x = ((rand() % 1000) / 1000.0f - 0.5f) * 80.0f;
        float z = ((rand() % 1000) / 1000.0f - 0.5f) * 80.0f;
        if (std::sqrt(x*x + z*z) < 4.0f || PathDistance(x, z) < 3.5f) continue;
        float s = 0.6f + (rand() % 100) / 80.0f;
        addProp(x, z, glm::vec3(s, s*0.5f, s*0.8f), glm::vec3(0.42f, 0.40f, 0.38f));
    }
    for (int i = 0; i < 30; ++i) {
        float x = ((rand() % 1000) / 1000.0f - 0.5f) * 85.0f;
        float z = ((rand() % 1000) / 1000.0f - 0.5f) * 85.0f;
        if (PathDistance(x, z) < 3.0f) continue;
        float s = 0.4f + (rand() % 60) / 100.0f;
        addProp(x, z, glm::vec3(s, s*0.6f, s),
            glm::vec3(0.20f + (rand()%10)/100.0f, 0.38f + (rand()%15)/100.0f, 0.14f));
    }

    struct RuinDef { float x,z,w,h,d; };
    RuinDef ruins[] = {
        {-18,-15,4,5,3}, {-16,-15,1,3.5f,3}, {-20,-13,3.5f,2,0.6f},
        {20,-18,5,4.5f,4}, {23,-17,1.2f,6,1.2f}, {20,-14.5f,4.5f,1.5f,0.5f},
        {-10,-28,6,3,5}, {-7.5f,-27,0.8f,4.5f,0.8f}, {-13,-27,0.8f,2.5f,0.8f},
        {12,15,3.5f,3.5f,3}, {14.5f,16,1,5.5f,1},
    };
    for (const auto& r : ruins) {
        glm::vec3 c = r.h > 4.0f ? glm::vec3(0.48f,0.44f,0.38f)
            : (r.h < 2.0f ? glm::vec3(0.46f,0.42f,0.36f) : glm::vec3(0.52f,0.48f,0.42f));
        addProp(r.x, r.z, glm::vec3(r.w, r.h, r.d), c);
    }

    struct FenceDef { float x,z; int count; float angle; };
    FenceDef fences[] = { {-5,-22,5,0}, {10,-10,4,1.2f}, {-25,5,3,0.5f} };
    for (const auto& f : fences) {
        for (int i = 0; i < f.count; ++i) {
            float off = (float)i * 1.8f;
            float px = f.x + std::cos(f.angle) * off;
            float pz = f.z + std::sin(f.angle) * off;
            addProp(px, pz, glm::vec3(0.15f, 1.2f + (rand()%40)/100.0f, 0.15f), glm::vec3(0.38f,0.30f,0.18f));
            if (i < f.count - 1) {
                float rx = f.x + std::cos(f.angle)*(off+0.9f);
                float rz = f.z + std::sin(f.angle)*(off+0.9f);
                mapProps.push_back({ glm::vec3(rx, RawTerrainHeight(rx, rz) + 0.6f, rz),
                    glm::vec3(1.6f,0.1f,0.1f), glm::vec3(0.35f,0.28f,0.16f) });
            }
        }
    }

    for (int i = 0; i < 8; ++i) {
        float x = ((rand() % 1000) / 1000.0f - 0.5f) * 70.0f;
        float z = ((rand() % 1000) / 1000.0f - 0.5f) * 70.0f;
        if (PathDistance(x, z) < 3.5f || std::sqrt(x*x + z*z) < 6.0f) continue;
        float s = 0.7f + (rand() % 50) / 100.0f;
        addProp(x, z, glm::vec3(s, s, s), glm::vec3(0.45f,0.35f,0.20f));
    }

    srand((unsigned int)time(NULL));
}

void DrawSimpleMap(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos)
{
    GLuint shader = GetActorShader();
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shader, "cameraPos"), 1, glm::value_ptr(cameraPos));
    glUniform3f(glGetUniformLocation(shader, "lightDir"), -0.25f, 0.92f, 0.18f);
    glUniform1i(glGetUniformLocation(shader, "outlineMode"), 0);
    glUniform1i(glGetUniformLocation(shader, "useTexture"), 0);
    glUniform1f(glGetUniformLocation(shader, "opacity"), 1.0f);

    glm::mat4 groundModel(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(groundModel));
    glUniform1i(glGetUniformLocation(shader, "useVertexColor"), 1);
    glBindVertexArray(mapGroundVao);
    glDrawElements(GL_TRIANGLES, groundIndexCount, GL_UNSIGNED_INT, nullptr);

    glUniform1i(glGetUniformLocation(shader, "useVertexColor"), 0);
    glBindVertexArray(mapPropVao);
    for (const MapProp& prop : mapProps) {
        glm::mat4 model(1.0f);
        model = glm::translate(model, prop.position);
        model = glm::scale(model, prop.scale);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(shader, "actorColor"), 1, glm::value_ptr(prop.color));
        glDrawElements(GL_TRIANGLES, mapPropIndexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}
