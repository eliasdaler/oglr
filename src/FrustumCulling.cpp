#include "FrustumCulling.h"

#include "Camera.h"

namespace util
{
std::array<glm::vec3, 8> calculateFrustumCornersWorldSpace(const glm::mat4& vp)
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

    const auto inv = glm::inverse(vp);
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
    return createFrustumFromVPMatrix(camera.getViewProj());
}

Frustum createFrustumFromVPMatrix(const glm::mat4& m)
{
    // http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
    // Need to negate everything because we're looking at -Z, not +Z
    // NOTE: if clipNearZ == 0, then farFace = {-m[0][2], -m[1][2], -m[2][2], -m[3][2]}
    Frustum frustum;

    frustum.nearFace = {
        -(m[0][3] + m[0][2]),
        -(m[1][3] + m[1][2]),
        -(m[2][3] + m[2][2]),
        -(m[3][3] + m[3][2]),
    };

    frustum.farFace = {
        -(m[0][3] - m[0][2]),
        -(m[1][3] - m[1][2]),
        -(m[2][3] - m[2][2]),
        -(m[3][3] - m[3][2]),
    };

    frustum.leftFace = {
        -(m[0][3] + m[0][0]),
        -(m[1][3] + m[1][0]),
        -(m[2][3] + m[2][0]),
        -(m[3][3] + m[3][0]),
    };
    frustum.rightFace = {
        -(m[0][3] - m[0][0]),
        -(m[1][3] - m[1][0]),
        -(m[2][3] - m[2][0]),
        -(m[3][3] - m[3][0]),
    };

    frustum.bottomFace = {
        -(m[0][3] + m[0][1]),
        -(m[1][3] + m[1][1]),
        -(m[2][3] + m[2][1]),
        -(m[3][3] + m[3][1]),
    };

    frustum.topFace = {
        -(m[0][3] - m[0][1]),
        -(m[1][3] - m[1][1]),
        -(m[2][3] - m[2][1]),
        -(m[3][3] - m[3][1]),
    };

    return frustum;
}

Frustum createSubFrustum(const glm::mat4& m, int tileX, int tileY, int numTilesX, int numTilesY)
{
    // Based on http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
    // We subdivide frustum by numTilesX on X and numTilesY on Y
    // And then figure out the planes from tileX and tileY based on similar derivations
    // NOTE: if clipNearZ == 0, then farFace = {-m[0][2], -m[1][2], -m[2][2], -m[3][2]}
    Frustum frustum;

    frustum.nearFace = {
        -(m[0][3] + m[0][2]),
        -(m[1][3] + m[1][2]),
        -(m[2][3] + m[2][2]),
        -(m[3][3] + m[3][2]),
    };

    frustum.farFace = {
        -(m[0][3] - m[0][2]),
        -(m[1][3] - m[1][2]),
        -(m[2][3] - m[2][2]),
        -(m[3][3] - m[3][2]),
    };

    const auto L = (1.f - 2.f * (float)tileX / (float)numTilesX);
    const auto R = (2.f * (float)(tileX + 1) / (float)numTilesX - 1.f);

    frustum.leftFace = {
        -(L * m[0][3] + m[0][0]),
        -(L * m[1][3] + m[1][0]),
        -(L * m[2][3] + m[2][0]),
        -(L * m[3][3] + m[3][0]),
    };

    frustum.rightFace = {
        -(R * m[0][3] - m[0][0]),
        -(R * m[1][3] - m[1][0]),
        -(R * m[2][3] - m[2][0]),
        -(R * m[3][3] - m[3][0]),
    };

    const auto T = (1.f - 2.f * (float)tileY / (float)numTilesY);
    const auto B = (2.f * (float)(tileY + 1) / (float)numTilesY - 1.f);

    frustum.bottomFace = {
        -(B * m[0][3] + m[0][1]),
        -(B * m[1][3] + m[1][1]),
        -(B * m[2][3] + m[2][1]),
        -(B * m[3][3] + m[3][1]),
    };

    frustum.topFace = {
        -(T * m[0][3] - m[0][1]),
        -(T * m[1][3] - m[1][1]),
        -(T * m[2][3] - m[2][1]),
        -(T * m[3][3] - m[3][1]),
    };

    return frustum;
}

bool isInFrustum(const Frustum& frustum, const AABB& aabb)
{
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
        if (dist > s.radius) {
            return false;
        } else if (dist > -s.radius) {
            res = true;
        }
    }
    return res;
}

}
