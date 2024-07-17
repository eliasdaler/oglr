#include "Light.h"
#include "FrustumCulling.h"

namespace
{
glm::vec2 calculateSpotLightScaleOffset(float innerConeAngle, float outerConeAngle)
{
    // See KHR_lights_punctual spec - formulas are taken from it
    glm::vec2 scaleOffset;
    scaleOffset.x = 1.f / std::max(0.001f, std::cos(innerConeAngle) - std::cos(outerConeAngle));
    scaleOffset.y = -std::cos(outerConeAngle) * scaleOffset.x;
    return scaleOffset;
}
}

GPULightData toGPULightData(const glm::vec3& pos, const glm::vec3& dir, const Light& light)
{
    auto ld = GPULightData{
        .position = pos,
        .intensity = light.intensity,
        .dir = dir,
        .range = light.range,
        .color = glm::vec3(light.color),
        .type = light.type,
    };
    if (ld.type == LIGHT_TYPE_SPOT) {
        ld.scaleOffset = calculateSpotLightScaleOffset(light.innerConeAngle, light.outerConeAngle);
    }
    return ld;
}

Camera makeSpotLightCamera(
    const glm::vec3& position,
    const glm::vec3& direction,
    float range,
    float outerConeAngle)
{
    const auto fovX = outerConeAngle * 2;
    const auto zNear = 0.1f;
    const auto zFar = range;
    Camera spotLightCamera;
    spotLightCamera.init(fovX, zNear, zFar, 1.f);
    spotLightCamera.setPosition(position);

    // need - in quatLookAt because it assumes -Z forward
    if (std::abs(glm::dot(direction, math::GLOBAL_UP_DIR)) > 0.9999f) {
        spotLightCamera.setHeading(glm::quatLookAt(direction, math::GLOBAL_FORWARD_DIR));
    } else {
        spotLightCamera.setHeading(glm::quatLookAt(direction, math::GLOBAL_UP_DIR));
    }

    return spotLightCamera;
}

bool shouldCullLight(const Frustum& frustum, const CPULightData& lightData)
{
    auto& light = lightData.light;
    if (light.type == LIGHT_TYPE_DIRECTIONAL) {
        return false;
    }

    if (light.type == LIGHT_TYPE_POINT) {
        Sphere s{.center = lightData.position, .radius = light.range};
        return !util::isInFrustum(frustum, s);
    }

    // Spot light
    const auto spotLightPoints = util::calculateFrustumCornersWorldSpace(
        lightData.lightSpaceProj * lightData.lightSpaceView);
    std::vector<glm::vec3> points;
    points.assign(spotLightPoints.begin(), spotLightPoints.end());
    const auto aabb = util::calculateAABB(points);
    return !util::isInFrustum(frustum, aabb);
}
