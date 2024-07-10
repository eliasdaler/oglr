#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

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
