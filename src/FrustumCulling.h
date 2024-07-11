#pragma once

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "AABB.h"

class Camera;

struct Sphere {
    glm::vec3 center;
    float radius;
};

struct Frustum {
    struct Plane {
        Plane() = default;

        // the plane equation is ai+bj+ck+d=0, where (i,j,k) is basis
        Plane(float a, float b, float c, float d)
        {
            const auto mag = glm::length(glm::vec3{a, b, c});
            this->n = glm::vec3{a, b, c} / mag;
            this->d = d / mag;
        }

        Plane(glm::vec3 p, glm::vec3 n) : n(glm::normalize(n)), d(-glm::dot(p, glm::normalize(n)))
        {}

        glm::vec3 n{0.f, 1.f, 0.f};
        float d{0.f};

        float getSignedDistanceToPlane(const glm::vec3& point) const
        {
            return glm::dot(n, point) + d;
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

    // Note: normals point inward
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
bool isInFrustum(const Frustum& frustum, const Sphere& s);
}
