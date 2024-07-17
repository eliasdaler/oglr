#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Camera.h"

struct Frustum;

// keep in sync with light.glsl
inline constexpr int LIGHT_TYPE_DIRECTIONAL = 0;
inline constexpr int LIGHT_TYPE_POINT = 1;
inline constexpr int LIGHT_TYPE_SPOT = 2;

// keep in sync with basic_shader_uniforms.glsl
inline constexpr std::size_t MAX_AFFECTING_LIGHTS = 8;
inline constexpr std::size_t MAX_SHADOW_CASTING_LIGHTS = 32;

struct Light {
    int type{0};
    glm::vec4 color;
    float intensity{0.f};
    float range{0.f}; // point light only

    // spot light only
    float innerConeAngle{0.f};
    float outerConeAngle{0.f};
};

struct CPULightData {
    glm::vec3 position;
    glm::vec3 direction;
    Light light;

    // spot only
    glm::mat4 lightSpaceProj;
    glm::mat4 lightSpaceView;

    std::size_t shadowMapDrawListIdx; // index into shadowMapOpaqueDrawLists
                                      // (for point lights, index of the draw list out of 6)
    std::size_t camerasUboOffset; // offset into SceneData UBO CameraData part
    std::size_t lightSpaceTMsIdx; // index into LightData.lightSpaceTMs (spot light only)
    std::uint32_t shadowMapIdx; // layer of array texture
                                // (for point lights, index of the first slice out of 6)

    // animation
    glm::vec3 rotationOrigin{};
    float rotationAngle{0.f};
    float rotationRadius{1.f};
    float rotationSpeed{0.f};

    bool culled{false};
    bool castsShadow{false};
};

struct GPULightData {
    glm::vec3 position;
    float intensity;

    glm::vec3 dir; // directional only
    float range;

    glm::vec3 color;
    int type;

    glm::vec2 scaleOffset; // spot light only
    std::uint32_t lightSpaceTMsIdx; // spot light only
    std::uint32_t shadowMapIdx;

    glm::vec4 pointLightProjBR; // point light only
};

GPULightData toGPULightData(const glm::vec3& pos, const glm::vec3& dir, const Light& light);

Camera makeSpotLightCamera(
    const glm::vec3& position,
    const glm::vec3& direction,
    float range,
    float outerConeAngle);

bool shouldCullLight(const Frustum& frustum, const CPULightData& lightData);
