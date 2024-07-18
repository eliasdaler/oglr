#pragma once

#include <cstdint>

#include <SDL2/SDL.h>

#include "Camera.h"
#include "DebugRenderer.h"
#include "GPUMesh.h"
#include "GraphicsUtil.h"
#include "Light.h"
#include "Transform.h"

#include <random>

struct ObjectData {
    Transform transform;
    std::size_t meshIdx{}; // index into "meshes" array
    std::size_t textureIdx{}; // index into "textures" array
    float alpha{1.f};
    AABB worldAABB; // aabb in world space
};

// keep in sync with basic_shader_uniforms.glsl
inline constexpr std::size_t SHADOW_MAP_ARRAY_LAYERS = 64;

inline constexpr std::size_t MAX_LIGHTS_PER_TILE{16};

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
    void initScene();
    void cleanup();
    void run();
    void update(float dt);
    void render();

    void generateDrawList();
    void generateShadowMapDrawList();
    void uploadSceneData();
    void renderSpotLightShadowMap(const CPULightData& lightData);
    void renderPointLightShadowMap(const CPULightData& lightData);
    void renderSceneObjects(const std::vector<DrawInfo>& drawList);
    void renderDebugObjects();
    void renderWireframes(const std::vector<DrawInfo>& drawList);

    void handleFreeCameraControls(float dt);

    void addSpotLight(
        const glm::vec3& pos,
        const glm::vec3& dir,
        const Light& light,
        bool castShadow);

    void generateRandomObject();
    void spawnObject(
        const glm::vec3& pos,
        std::size_t meshIdx,
        std::size_t textureIdx,
        float alpha);

    Frustum getFrustum() const;

    SDL_Window* window{nullptr};
    SDL_GLContext glContext{nullptr};
    int uboAlignment{4};

    bool isRunning{false};
    bool frameLimit{true};
    float frameTime{0.f};
    float avgFPS{0.f};

    std::random_device randomDevice;
    std::mt19937 rng;

    std::uint32_t worldShader{};
    std::uint32_t depthOnlyShader{};
    std::uint32_t solidColorShader{};
    std::uint32_t postFXShader{};
    std::uint32_t vao{}; // empty vao

    std::vector<GPUMesh> meshes;
    std::vector<std::uint32_t> textures;

    std::size_t cubeMeshIdx;
    std::size_t planeMeshIdx;
    std::size_t startMeshIdx;

    std::vector<ObjectData> objects;

    Camera camera;
    Camera testCamera;
    bool useTestCameraForCulling{false};
    bool drawAABBs{false};
    bool drawWireframes{false};

    float timer{0.f};
    float timeToSpawnNewObject{1000.5f};
    std::vector<std::size_t> randomSpawnMeshes;
    std::vector<std::size_t> randomSpawnTextures;

    struct UBOCameraData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 cameraPos;
    };
    std::size_t MAX_CAMERAS_IN_UBO = 128;

    struct UBOGlobalData {
        glm::vec4 screenSizeAndUnused;

        glm::vec3 ambientColor;
        float ambientIntensity;

        GPULightData sunLight;

        std::array<glm::mat4, MAX_SHADOW_CASTING_LIGHTS> lightSpaceTMs; // spot light viewProj
    };

    struct UBOPerObjectData {
        glm::mat4 model;
        glm::vec4 props; // x - object alpha, yzw - unused
    };
    GPUBuffer sceneDataBuffer;

    GPUBuffer lightsBuffer{};
    std::vector<GPULightData> lightGPUDataToUpload;

    gfx::BumpAllocator sceneData;
    std::size_t mainCameraUboOffset;
    std::size_t lightDataUboOffset;

    glm::vec3 ambientColor;
    float ambientIntensity;

    glm::vec3 sunLightDir;
    Light sunLight;

    std::vector<CPULightData> lights;
    std::array<Camera, 6> pointLightShadowMapCameras;

    std::vector<DrawInfo> drawList;
    std::vector<DrawInfo> opaqueDrawList;
    std::vector<DrawInfo> transparentDrawList;

    std::array<std::vector<DrawInfo>, SHADOW_MAP_ARRAY_LAYERS> shadowMapOpaqueDrawLists;

    gfx::GlobalState frameStartState;
    gfx::GlobalState shadowPassDrawState;
    gfx::GlobalState depthPrePassDrawState;
    gfx::GlobalState opaqueDrawState;
    gfx::GlobalState transparentDrawState;
    gfx::GlobalState postFXDrawState;
    gfx::GlobalState wireframesDrawState;

    std::uint32_t mainDrawFBO;
    std::uint32_t mainDrawColorTexture;
    std::uint32_t mainDrawDepthTexture;

    std::uint32_t shadowMapFBO;
    std::uint32_t shadowMapDepthTexture;
    int shadowMapSize{1024};

    const float tileSize{64.f};
    std::vector<std::array<int, MAX_LIGHTS_PER_TILE>> lightsPerTile;
    int debugTileIdx{0};
    GPUBuffer lightsPerTileBuffer;

    DebugRenderer debugRenderer;

    float normCoordX;
    float normCoordY;
};
