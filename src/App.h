#pragma once

#include <cstdint>

#include <SDL2/SDL.h>

#include "Camera.h"

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

class App {
public:
    void start();

private:
    void init();
    void cleanup();
    void run();
    void update(float dt);
    void render();

    SDL_Window* window{nullptr};
    SDL_GLContext glContext{nullptr};

    bool isRunning{false};
    bool frameLimit{true};
    float frameTime{0.f};
    float avgFPS{0.f};

    std::uint32_t shaderProgram{};
    std::uint32_t vbo{};
    std::uint32_t vao{};
    std::uint32_t ebo{};
    std::uint32_t texture{};

    Transform cubeTransform;

    Camera camera;
};
