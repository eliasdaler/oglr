#pragma once

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "AABB.h"

class Camera;

struct Frustum {
    struct Plane {
        Plane() = default;
        Plane(const glm::vec3& p1, const glm::vec3& norm) :
            normal(glm::normalize(norm)), distance(glm::dot(normal, p1))
        {}

        glm::vec3 normal{0.f, 1.f, 0.f};

        // distance from the origin to the nearest point in the plane
        float distance{0.f};

        float getSignedDistanceToPlane(const glm::vec3& point) const
        {
            return glm::dot(normal, point) - distance;
        }
    };

    const Plane& getPlane(int i) const
    {
        switch (i) {
        case 0:
            return farFace;
        case 1:
            return nearFace;
        case 2:
            return leftFace;
        case 3:
            return rightFace;
        case 4:
            return topFace;
        case 5:
            return bottomFace;
        default:
            assert(false);
            return nearFace;
        }
    }

    Plane farFace;
    Plane nearFace;

    Plane leftFace;
    Plane rightFace;

    Plane topFace;
    Plane bottomFace;
};

namespace util
{
std::array<glm::vec3, 8> calculateFrustumCornersWorldSpace(const Camera& camera);
Frustum createFrustumFromCamera(const Camera& camera);
bool isInFrustum(const Frustum& frustum, const AABB& aabb);
}
