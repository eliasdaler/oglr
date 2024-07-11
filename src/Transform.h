#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "GlobalAxes.h"

struct Transform {
    glm::vec3 position{};
    glm::quat heading{glm::identity<glm::quat>()};
    glm::vec3 scale{1.f};

    const glm::mat4 asMatrix() const
    {
        static const auto I = glm::mat4{1.f};
        auto transformMatrix = glm::translate(I, position);
        if (heading != glm::identity<glm::quat>()) {
            transformMatrix *= glm::mat4_cast(heading);
        }
        transformMatrix = glm::scale(transformMatrix, scale);
        return transformMatrix;
    }

    glm::vec3 getRight() const { return heading * math::GLOBAL_RIGHT_DIR; }
    glm::vec3 getForward() const { return heading * math::GLOBAL_FORWARD_DIR; }
    glm::vec3 getUp() const { return heading * math::GLOBAL_UP_DIR; }
};
