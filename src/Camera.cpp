#include "Camera.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "GlobalAxes.h"

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
    auto dir = glm::normalize(point - transform.position);
    transform.heading = glm::quatLookAt(dir, math::GLOBAL_UP_DIR);
}

glm::mat4 Camera::getView() const
{
    /* const auto lookDir = transform.getForward();
    if (std::abs(glm::dot(lookDir, math::GLOBAL_UP_DIR)) > 0.9999f) {
        if (glm::dot(lookDir, math::GLOBAL_UP_DIR) > 0.9999f) {
            return glm::
                lookAt(transform.position, transform.position + lookDir, math::GLOBAL_FORWARD_DIR);
        }
        return glm::
            lookAt(transform.position, transform.position + lookDir, -math::GLOBAL_FORWARD_DIR);
    }
    return glm::lookAt(transform.position, transform.position + lookDir, math::GLOBAL_UP_DIR); */
    const auto rot = glm::transpose(glm::mat4_cast(transform.heading));
    return glm::translate(rot, -transform.position);
}

glm::mat4 Camera::getViewProj() const
{
    return projection * getView();
}

glm::vec3 Camera::getRight() const
{
    return transform.getRight();
}

glm::vec3 Camera::getUp() const
{
    return transform.getUp();
}

glm::vec3 Camera::getForward() const
{
    return transform.getForward();
}
