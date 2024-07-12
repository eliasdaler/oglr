#pragma once

#include <cstdint>

#include <SDL2/SDL.h>

#include "Camera.h"
#include "DebugRenderer.h"
#include "GPUMesh.h"
#include "GraphicsUtil.h"
#include "Transform.h"

#include <random>

struct ObjectData {
    Transform transform;
    std::size_t meshIdx{}; // index into "meshes" array
    std::size_t textureIdx{}; // index into "textures" array
    float alpha{1.f};
    AABB worldAABB; // aabb in world space
};

struct DrawInfo {
    std::size_t objectIdx;
    std::size_t uboOffset;
    float distToCamera;
};

struct Frustum;

class App {
public:
    void start();

private:
    void init();
    void cleanup();
    void run();
    void update(float dt);
    void render();

    void generateDrawList();
    void uploadSceneData();
    void renderSceneObjects(const std::vector<DrawInfo>& drawList);
    void renderDebugObjects();
    void renderWireframes(const std::vector<DrawInfo>& drawList);

    void handleFreeCameraControls(float dt);

    void generateRandomObject();
    void spawnObject(
        const glm::vec3& pos,
        std::size_t meshIdx,
        std::size_t textureIdx,
        float alpha);

    Frustum getFrustum() const;

    SDL_Window* window{nullptr};
    SDL_GLContext glContext{nullptr};

    bool isRunning{false};
    bool frameLimit{true};
    float frameTime{0.f};
    float avgFPS{0.f};

    std::random_device randomDevice;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist{1, 10};

    std::uint32_t worldShader{};
    std::uint32_t solidColorShader{};
    std::uint32_t vao{}; // empty vao

    std::vector<GPUMesh> meshes;
    std::vector<std::uint32_t> textures;

    std::size_t cubeMeshIdx;
    std::size_t planeMeshIdx;

    std::vector<ObjectData> objects;

    Camera camera;
    Camera testCamera;
    bool useTestCameraForCulling{false};
    bool drawAABBs{false};
    bool drawWireframes{false};

    float timer{0.f};
    float timeToSpawnNewObject{200.5f};
    std::vector<std::size_t> randomSpawnMeshes;
    std::vector<std::size_t> randomSpawnTextures;

    float pointLightRotateAngle{0.f};
    float pointLightRotateRadius{1.f};
    glm::vec3 pointLightRotateOrigin{};

    struct GlobalSceneData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 cameraPos;
        glm::vec4 sunlightColorAndIntensity;
        glm::vec4 sunlightDirAndUnused;
        glm::vec4 ambientColorAndIntensity;

        glm::vec4 pointLightPosAndRange;
        glm::vec4 pointLightColorAndIntensity;

        glm::vec4 spotLightPosAndRange;
        glm::vec4 spotLightColorAndIntensity;
        glm::vec4 spotLightScaleOffsetAndUnused;
        glm::vec4 spotLightDirAndUnused;
    };
    struct PerObjectData {
        glm::mat4 model;
        glm::vec4 props; // x - alpha, yzw - unused
    };
    GPUBuffer sceneDataBuffer;

    int uboAlignment{4};

    gfx::BumpAllocator sceneData;
    std::size_t sceneDataUboOffset;

    glm::vec4 sunlightColor;
    float sunlightIntensity;
    glm::vec3 sunlightDir;

    glm::vec3 ambientColor;
    float ambientIntensity;

    glm::vec3 pointLightPosition;
    float pointLightRange;
    glm::vec4 pointLightColor;
    float pointLightIntensity;

    glm::vec3 spotLightPosition;
    float spotLightRange;
    glm::vec4 spotLightColor;
    float spotLightIntensity;
    glm::vec2 spotLightScaleOffset;
    glm::vec3 spotLightDir;

    glm::vec3 cameraVelocity;

    std::vector<DrawInfo> drawList;
    std::vector<DrawInfo> opaqueDrawList;
    std::vector<DrawInfo> transparentDrawList;

    gfx::GlobalState frameStartState;
    gfx::GlobalState opaqueDrawState;
    gfx::GlobalState transparentDrawState;
    gfx::GlobalState wireframesDrawState;

    DebugRenderer debugRenderer;
};
