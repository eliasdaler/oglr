#pragma once

#include <cstdint>

#include <SDL2/SDL.h>

#include "Camera.h"

#include <random>

struct Transform {
    glm::vec3 position{};
    glm::quat heading{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};

    const glm::mat4 asMatrix() const
    {
        static const auto I = glm::mat4{1.f};
        auto transformMatrix = glm::translate(I, position);
        if (heading != glm::identity<glm::quat>()) {
            transformMatrix *= glm::mat4_cast(heading);
        }
        transformMatrix = glm::scale(transformMatrix, scale);
        return transformMatrix;
    }
};

struct GPUVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
};

struct GPUMesh {
    std::uint32_t vertexBuffer{};
    std::uint32_t indexBuffer{};
    std::uint32_t numIndices{};
};

class App {
public:
    void start();

private:
    void init();
    void cleanup();
    void run();
    void update(float dt);
    void uploadSceneData();
    void render();

    void handleFreeCameraControls(float dt);

    void generateRandomObject();
    void spawnCube(const glm::vec3& pos);

    SDL_Window* window{nullptr};
    SDL_GLContext glContext{nullptr};

    bool isRunning{false};
    bool frameLimit{true};
    float frameTime{0.f};
    float avgFPS{0.f};

    std::random_device randomDevice;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist{1, 10};
    std::uniform_real_distribution<float> dist2{-10.f, 10.f};

    std::uint32_t shaderProgram{};
    std::uint32_t vao{}; // empty vao

    std::vector<GPUMesh> meshes;
    std::vector<std::uint32_t> textures;

    struct ObjectData {
        Transform transform;
        std::size_t meshIdx{}; // index into "meshes" array
        std::size_t textureIdx{}; // index into "textures" array
    };
    std::vector<ObjectData> objects;

    Camera camera;

    float timer{0.f};
    float timeToSpawnNewCube{200.0f};

    struct GlobalSceneData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 cameraPos;
        glm::vec4 sunlightColorAndIntensity;
        glm::vec4 sunlightDirAndUnused;
        glm::vec4 ambientColorAndIntensity;
    };
    struct PerObjectData {
        glm::mat4 model;
    };
    std::uint32_t sceneDataBuffer{};
    std::uint32_t allocatedBufferSize{0};

    int uboAlignment{4};
    int globalSceneDataSize{};
    int perObjectDataElementSize{};
    std::vector<std::uint8_t> sceneData;

    glm::vec3 sunlightColor;
    float sunlightIntensity;
    glm::vec3 sunlightDir;

    glm::vec3 ambientColor;
    float ambientIntensity;

    glm::vec3 cameraVelocity;
};
