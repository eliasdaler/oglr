#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "GPUBuffer.h"
#include "GraphicsUtil.h"

struct AABB;
class Camera;

class DebugRenderer {
public:
    void init();
    void cleanup();

    void beginDrawing(); // should be called in update before adding new commands

    void addLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    void addLine(
        const glm::vec3& from,
        const glm::vec3& to,
        const glm::vec4& fromColor,
        const glm::vec4& toColor);
    void addQuadLines(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        const glm::vec3& d,
        const glm::vec4& color);
    void addAABBLines(const AABB& aabb, const glm::vec4& color);
    void addFrustumLines(const Camera& camera);

    void render(const Camera& camera);

private:
    struct LineVertex {
        glm::vec3 pos;
        float unused;
        glm::vec4 color;
    };
    std::vector<LineVertex> lines;
    GPUBuffer linesBuffer{};
    const int MAX_LINES = 10000;
    std::uint32_t linesShader{};

    gfx::GlobalState linesDrawState;
};
