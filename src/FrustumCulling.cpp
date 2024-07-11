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

    frustum.nearFace = {camPos + zNear * camFront, camFront};
    frustum.farFace = {camPos + frontMultFar, -camFront};

    // FIXME: why is there - in from of glm::cross here?
    // (need it to make normals point inside)
    frustum.leftFace = {camPos, -glm::cross(camUp, frontMultFar + camRight * halfHSide)};
    frustum.rightFace = {camPos, -glm::cross(frontMultFar - camRight * halfHSide, camUp)};
    frustum.bottomFace = {camPos, -glm::cross(frontMultFar + camUp * halfVSide, camRight)};
    frustum.topFace = {camPos, -glm::cross(camRight, frontMultFar - camUp * halfVSide)};

    return frustum;
}

bool isInFrustum(const Frustum& frustum, const AABB& aabb)
{
    glm::vec3 vmin, vmax;
    bool ret = true;
    for (int i = 0; i < 6; ++i) {
        const auto& plane = frustum.getPlane(i);
        // X axis
        if (plane.normal.x < 0) {
            vmin.x = aabb.min.x;
            vmax.x = aabb.max.x;
        } else {
            vmin.x = aabb.max.x;
            vmax.x = aabb.min.x;
        }
        // Y axis
        if (plane.normal.y < 0) {
            vmin.y = aabb.min.y;
            vmax.y = aabb.max.y;
        } else {
            vmin.y = aabb.max.y;
            vmax.y = aabb.min.y;
        }
        // Z axis
        if (plane.normal.z < 0) {
            vmin.z = aabb.min.z;
            vmax.z = aabb.max.z;
        } else {
            vmin.z = aabb.max.z;
            vmax.z = aabb.min.z;
        }
        if (plane.getSignedDistanceToPlane(vmin) < 0) {
            return false;
        }
        if (plane.getSignedDistanceToPlane(vmax) <= 0) {
            ret = true;
        }
    }
    return ret;
}

}
