#pragma once

#include <glm/vec3.hpp>

namespace math
{
inline constexpr auto GLOBAL_UP_DIR = glm::vec3{0.f, 1.f, 0.f};
inline constexpr auto GLOBAL_FORWARD_DIR = glm::vec3{0.f, 0.f, 1.f};
inline constexpr auto GLOBAL_RIGHT_DIR = glm::vec3{1.f, 0.f, 0.f};
}
