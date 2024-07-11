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

}
