#pragma once

#include <array>

#include <glm/vec3.hpp>

class Camera;
namespace util
{
std::array<glm::vec3, 8> calculateFrustumCornersWorldSpace(const Camera& camera);
}
