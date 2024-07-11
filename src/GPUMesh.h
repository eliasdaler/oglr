#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

#include "GPUBuffer.h"

struct GPUVertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
};

struct GPUMesh {
    GPUBuffer vertexBuffer{};
    GPUBuffer indexBuffer{};
    std::uint32_t numIndices{};
};
