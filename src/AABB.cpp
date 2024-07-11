#include "AABB.h"
#include "CPUMesh.h"

namespace util
{
AABB calculateMeshAABB(const CPUMesh& mesh)
{
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : mesh.vertices) {
        minX = std::min(minX, v.position.x);
        maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
        minZ = std::min(minZ, v.position.z);
        maxZ = std::max(maxZ, v.position.z);
    }

    return AABB{
        .min = glm::vec3{minX, minY, minZ},
        .max = glm::vec3{maxX, maxY, maxZ},
    };
}

}
