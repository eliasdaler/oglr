#include "Camera.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace
{
const auto GLOBAL_UP_DIR = glm::vec3{0.f, 1.f, 0.f};
const auto GLOBAL_FRONT_DIR = glm::vec3{0.f, 0.f, 1.f};
}

void Camera::init(float fovX, float zNear, float zFar, float aspectRatio)
{
    // see 6.1 in Foundations of Game Engine Development by Eric Lengyel
    float s = aspectRatio;
    float g = s / glm::tan(fovX / 2.f);
    fovY = 2.f * glm::atan(1.f / g);

    this->fovX = fovX;
    this->zFar = zFar;
    this->zNear = zNear;
    this->aspectRatio = aspectRatio;

    projection = glm::perspective(fovY, aspectRatio, zNear, zFar);
}

void Camera::lookAt(const glm::vec3& point)
{
    auto dir = glm::normalize(position - point);
    heading = glm::quatLookAt(dir, GLOBAL_UP_DIR);
}

glm::mat4 Camera::getView() const
{
    // TODO: move to globals

    const auto target = position + heading * GLOBAL_FRONT_DIR;
    return glm::lookAt(position, target, GLOBAL_UP_DIR);
}

glm::mat4 Camera::getViewProj() const
{
    return projection * getView();
}
