#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Transform.h"

class Camera {
public:
    void init(float fovX, float zNear, float zFar, float aspectRatio);

    void setPosition(const glm::vec3& p) { transform.position = p; }
    const glm::vec3& getPosition() const { return transform.position; }

    void setHeading(const glm::quat& q) { transform.heading = glm::normalize(q); }
    const glm::quat& getHeading() const { return transform.heading; }

    const Transform& getTransform() const { return transform; }

    const glm::mat4& getProjection() const { return projection; }
    glm::mat4 getView() const;
    glm::mat4 getViewProj() const;

    void lookAt(const glm::vec3& point);

    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    glm::vec3 getForward() const;

    float getFOVY() const { return fovY; }
    float getAspectRatio() const { return aspectRatio; }
    float getZNear() const { return zNear; };
    float getZFar() const { return zFar; };

private:
    Transform transform;

    glm::mat4 projection;
    bool orthographic{false};

    float zNear{1.f};
    float zFar{75.f};
    float aspectRatio{16.f / 9.f};
    float fovX{glm::radians(90.f)}; // horizontal fov in radians
    float fovY{glm::radians(60.f)}; // vertical fov in radians
};
