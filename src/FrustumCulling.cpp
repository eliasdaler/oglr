#include "FrustumCulling.h"

#include "Camera.h"

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
    Frustum frustum;
    const auto camPos = camera.getPosition();
    const auto camFront = camera.getTransform().getForward();
    const auto camUp = camera.getTransform().getUp();
    const auto camRight = camera.getTransform().getRight();

    const auto zNear = camera.getZNear();
    const auto zFar = camera.getZFar();
    const auto halfVSide = zFar * tanf(camera.getFOVY() * .5f);
    const auto halfHSide = halfVSide * camera.getAspectRatio();
    const auto frontMultFar = zFar * camFront;

    frustum.nearFace = {camPos + zNear * camFront, -camFront};
    frustum.farFace = {camPos + frontMultFar, camFront};
    frustum.leftFace = {camPos, glm::cross(camUp, frontMultFar + camRight * halfHSide)};
    frustum.rightFace = {camPos, glm::cross(frontMultFar - camRight * halfHSide, camUp)};
    frustum.bottomFace = {camPos, glm::cross(frontMultFar + camUp * halfVSide, camRight)};
    frustum.topFace = {camPos, glm::cross(camRight, frontMultFar - camUp * halfVSide)};

    return frustum;
}

bool isInFrustum(const Frustum& frustum, const AABB& aabb)
{
    bool ret = true;
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.getPlane(i);

        // Nearest point
        glm::vec3 p;
        p.x = plane.normal.x >= 0 ? aabb.min.x : aabb.max.x;
        p.y = plane.normal.y >= 0 ? aabb.min.y : aabb.max.y;
        p.z = plane.normal.z >= 0 ? aabb.min.z : aabb.max.z;
        if (plane.getSignedDistanceToPlane(p) > 0) {
            return false;
        }

        // Farthest point
        glm::vec3 f;
        f.x = plane.normal.x >= 0 ? aabb.max.x : aabb.min.x;
        f.y = plane.normal.y >= 0 ? aabb.max.y : aabb.min.y;
        f.z = plane.normal.z >= 0 ? aabb.max.z : aabb.min.z;
        if (plane.getSignedDistanceToPlane(f) > 0) {
            ret = true;
        }
    }
    return ret;
}

}
