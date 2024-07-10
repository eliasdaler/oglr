#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct CPUVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
};

struct CPUMesh {
    std::vector<CPUVertex> vertices;
    std::vector<std::uint32_t> indices;
};
