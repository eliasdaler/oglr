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

AABB calculateAABB(const std::vector<glm::vec3>& points)
{
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : points) {
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
        minZ = std::min(minZ, v.z);
        maxZ = std::max(maxZ, v.z);
    }

    return AABB{
        .min = glm::vec3{minX, minY, minZ},
        .max = glm::vec3{maxX, maxY, maxZ},
    };
}

AABB calculateWorldAABB(const AABB& aabbLocal, const glm::mat4& tm)
{
    // Transforming Axis-Aligned Bounding Boxes from Graphics Gems.
    // + used noclip.website implementation of it
    const auto srcMin = aabbLocal.min;
    const auto srcMax = aabbLocal.max;

    // Translation can be applied directly.
    auto dstMin = glm::vec3{tm[3][0], tm[3][1], tm[3][2]};
    auto dstMax = glm::vec3{tm[3][0], tm[3][1], tm[3][2]};

    for (std::size_t i = 0; i < 3; i++) {
        for (std::size_t j = 0; j < 3; j++) {
            const auto a = tm[i][j] * srcMin[i];
            const auto b = tm[i][j] * srcMax[i];
            dstMin[j] += std::min(a, b);
            dstMax[j] += std::max(a, b);
        }
    }

    return AABB{.min = dstMin, .max = dstMax};
}

}
