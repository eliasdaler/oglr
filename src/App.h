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

inline constexpr int LIGHT_TYPE_DIRECTIONAL = 0;
inline constexpr int LIGHT_TYPE_POINT = 1;
inline constexpr int LIGHT_TYPE_SPOT = 2;

struct Light {
    int type{0};
    glm::vec4 color;
    float intensity{0.f};
    float range{0.f}; // point light only
    glm::vec2 scaleOffset; // spot light only
};

struct GPULightData {
    glm::vec3 position;
    float intensity;
    glm::vec3 dir; // directional only
    float range;
    glm::vec3 color;
    int type;
    glm::vec2 scaleOffset; // spot light only
    glm::vec2 padding;
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
    void renderShadowMap();
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
    std::uint32_t depthOnlyShader{};
    std::uint32_t solidColorShader{};
    std::uint32_t postFXShader{};
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
    float timeToSpawnNewObject{1000.25f};
    std::vector<std::size_t> randomSpawnMeshes;
    std::vector<std::size_t> randomSpawnTextures;

    float pointLightRotateAngle{0.f};
    float pointLightRotateRadius{1.f};
    glm::vec3 pointLightRotateOrigin{};

    struct CameraData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 cameraPos;
    };

    struct LightData {
        glm::vec3 ambientColor;
        float ambientIntensity;

        GPULightData sunLight;
        GPULightData pointLight;
        GPULightData spotLight;

        glm::mat4 spotLightSpaceTM;
    };

    struct PerObjectData {
        glm::mat4 model;
        glm::vec4 props; // x - alpha, yzw - unused
    };
    GPUBuffer sceneDataBuffer;

    int uboAlignment{4};

    gfx::BumpAllocator sceneData;
    std::size_t MAX_CAMERAS_IN_UBO = 8;
    std::vector<std::size_t> cameraDataUboOffsets; // 0 - main camera, 1 etc. - other cameras
    std::size_t lightDataUboOffset;

    glm::vec3 ambientColor;
    float ambientIntensity;

    glm::vec3 sunLightDir;
    Light sunLight;

    glm::vec3 pointLightPosition;
    Light pointLight;

    glm::vec3 spotLightPosition;
    glm::vec3 spotLightDir;
    Light spotLight;

    Camera spotLightCamera;
    bool spotLightCulled{false};
    AABB spotLightAABB;

    std::vector<DrawInfo> drawList;
    std::vector<DrawInfo> opaqueDrawList;
    std::vector<DrawInfo> transparentDrawList;

    std::vector<DrawInfo> shadowMapOpaqueDrawList;

    gfx::GlobalState frameStartState;
    gfx::GlobalState opaqueDrawState;
    gfx::GlobalState transparentDrawState;
    gfx::GlobalState postFXDrawState;
    gfx::GlobalState wireframesDrawState;

    std::uint32_t mainDrawFBO;
    std::uint32_t mainDrawColorTexture;
    std::uint32_t mainDrawDepthTexture;

    std::uint32_t shadowMapFBO;
    std::uint32_t shadowMapDepthTexture;
    int shadowMapSize{2048};

    DebugRenderer debugRenderer;
};
