#pragma once

#include <glm/common.hpp> // abs
#include <glm/vec3.hpp>

struct CPUMesh;

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 calculateSize() const { return glm::abs(max - min); }
};

namespace util
{
AABB calculateMeshAABB(const CPUMesh& mesh);
AABB calculateAABB(const std::vector<glm::vec3>& points);
}