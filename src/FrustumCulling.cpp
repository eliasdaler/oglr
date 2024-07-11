#include "FrustumCulling.h"

#include "Camera.h"

#include <cstdio>

namespace util
{
std::array<glm::vec3, 8> calculateFrustumCornersWorldSpace(const Camera& camera)
{
    bool usesInverseDepth = false;
    bool isClipSpaceYDown = false;

    const auto nearDepth = usesInverseDepth ? 1.0f : 0.f;
    const auto farDepth = usesInverseDepth ? 0.0f : 1.f;
    const auto bottomY = isClipSpaceYDown ? 1.f : -1.f;
    const auto topY = isClipSpaceYDown ? -1.f : 1.f;
    const std::array<glm::vec3, 8> cornersNDC = {
        // near plane
        glm::vec3{-1.f, bottomY, nearDepth},
        glm::vec3{-1.f, topY, nearDepth},
        glm::vec3{1.f, topY, nearDepth},
        glm::vec3{1.f, bottomY, nearDepth},
        // far plane
        glm::vec3{-1.f, bottomY, farDepth},
        glm::vec3{-1.f, topY, farDepth},
        glm::vec3{1.f, topY, farDepth},
        glm::vec3{1.f, bottomY, farDepth},
    };

    const auto inv = glm::inverse(camera.getViewProj());
    std::array<glm::vec3, 8> corners{};
    for (int i = 0; i < 8; ++i) {
        auto corner = inv * glm::vec4(cornersNDC[i], 1.f);
        corner /= corner.w;
        corners[i] = glm::vec3{corner};
    }
    return corners;
}

Frustum createFrustumFromCamera(const Camera& camera)
{
    const auto m = camera.getViewProj();

    Frustum frustum;

    // NOTE: if clipNearZ == 0, then farFace = {{m[0][2], m[1][2], m[2][2]}, m[3][2]}
    frustum.nearFace =
        {glm::vec3{m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]}, m[3][3] + m[3][2]};

    frustum.farFace =
        {glm::vec3{m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]}, m[3][3] - m[3][2]};

    frustum.leftFace =
        {glm::vec3{m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]}, m[3][3] + m[3][0]};
    frustum.rightFace =
        {glm::vec3{m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]}, m[3][3] - m[3][0]};

    frustum.bottomFace =
        {glm::vec3{m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]}, m[3][3] + m[3][2]};
    frustum.topFace =
        {glm::vec3{m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]}, m[3][3] - m[3][2]};

    return frustum;
}

bool isInFrustum(const Frustum& frustum, const AABB& aabb)
{
    // auto c = (aabb.max + aabb.min) / 2.f;
    // auto r = (aabb.max.z - aabb.min.z) / 2.f;
    // return isInFrustum(frustum, Sphere{.center = c, .radius = r});

    bool ret = true;
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.getPlane(i);

        // Nearest point
        glm::vec3 p;
        p.x = plane.n.x >= 0 ? aabb.min.x : aabb.max.x;
        p.y = plane.n.y >= 0 ? aabb.min.y : aabb.max.y;
        p.z = plane.n.z >= 0 ? aabb.min.z : aabb.max.z;
        if (plane.getSignedDistanceToPlane(p) > 0) {
            return false;
        }

        // Farthest point
        glm::vec3 f;
        f.x = plane.n.x >= 0 ? aabb.max.x : aabb.min.x;
        f.y = plane.n.y >= 0 ? aabb.max.y : aabb.min.y;
        f.z = plane.n.z >= 0 ? aabb.max.z : aabb.min.z;
        if (plane.getSignedDistanceToPlane(f) > 0) {
            ret = true;
        }
    }
    return ret;
}

bool isInFrustum(const Frustum& frustum, const Sphere& s)
{
    bool res = true;
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.getPlane(i);
        const auto dist = plane.getSignedDistanceToPlane(s.center);
        if (dist < -s.radius) {
            return false;
        } else if (dist > -s.radius) {
            res = true;
        }
    }
    return res;
}

}
