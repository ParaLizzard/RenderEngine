
#include "Scene/Camera.h"

#include <cassert>
#include <limits>

namespace Engine {
    void Camera::setOrthographicProjection(float left, float right, float top, float bottom, float near, float far)
    {
        farClip = far;
        nearClip = near;

        projectionMatrix = glm::mat4 {1.0f};
        projectionMatrix[0][0] = 2.f / (right - left);
        projectionMatrix[1][1] = 2.f / (bottom - top);
        projectionMatrix[2][2] = 1.f / (far - near);
        projectionMatrix[3][0] = -(right + left) / (right - left);
        projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
        projectionMatrix[3][2] = -near / (far - near);
    }

    void Camera::setPerspectiveProjection(float fovy, float aspect, float near, float far)
    {
        assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        farClip = far;
        nearClip = near;

        const float tanHalfFovy = tan(fovy / 2.f);
        projectionMatrix = glm::mat4 {0.0f};
        projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
        projectionMatrix[1][1] = 1.f / (tanHalfFovy);
        projectionMatrix[2][2] = far / (far - near);
        projectionMatrix[2][3] = 1.f;
        projectionMatrix[3][2] = -(far * near) / (far - near);
    }

    void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
    {
        const glm::vec3 w {glm::normalize(direction)};
        const glm::vec3 u {glm::normalize(glm::cross(w, up))};
        const glm::vec3 v {glm::cross(w, u)};

        viewMatrix = glm::mat4 {1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);

        inverseViewMatrix = glm::mat4 {1.f};
        inverseViewMatrix[0][0] = u.x;
        inverseViewMatrix[0][1] = u.y;
        inverseViewMatrix[0][2] = u.z;
        inverseViewMatrix[1][0] = v.x;
        inverseViewMatrix[1][1] = v.y;
        inverseViewMatrix[1][2] = v.z;
        inverseViewMatrix[2][0] = w.x;
        inverseViewMatrix[2][1] = w.y;
        inverseViewMatrix[2][2] = w.z;
        inverseViewMatrix[3][0] = position.x;
        inverseViewMatrix[3][1] = position.y;
        inverseViewMatrix[3][2] = position.z;
    }

    void Camera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up)
    {
        setViewDirection(position, target - position, up);
    }

    void Camera::setViewYXZ(glm::vec3 position, glm::quat rotation)
    {
        const glm::quat q = glm::normalize(rotation);

        const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
        const float qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        const float qxy = qx * qy, qxz = qx * qz, qxw = qx * qw;
        const float qyz = qy * qz, qyw = qy * qw, qzw = qz * qw;

        const glm::vec3 right(1.0f - 2.0f * (qy2 + qz2), 2.0f * (qxy + qzw), 2.0f * (qxz - qyw));
        const glm::vec3 up(2.0f * (qxy - qzw), 1.0f - 2.0f * (qx2 + qz2), 2.0f * (qyz + qxw));
        const glm::vec3 forward(2.0f * (qxz + qyw), 2.0f * (qyz - qxw), 1.0f - 2.0f * (qx2 + qy2));

        viewMatrix = glm::mat4(1.0f);
        viewMatrix[0][0] = right.x;
        viewMatrix[1][0] = right.y;
        viewMatrix[2][0] = right.z;
        viewMatrix[0][1] = up.x;
        viewMatrix[1][1] = up.y;
        viewMatrix[2][1] = up.z;
        viewMatrix[0][2] = forward.x;
        viewMatrix[1][2] = forward.y;
        viewMatrix[2][2] = forward.z;
        viewMatrix[3][0] = -glm::dot(right, position);
        viewMatrix[3][1] = -glm::dot(up, position);
        viewMatrix[3][2] = -glm::dot(forward, position);

        inverseViewMatrix = glm::mat4(1.0f);
        inverseViewMatrix[0][0] = right.x;
        inverseViewMatrix[0][1] = right.y;
        inverseViewMatrix[0][2] = right.z;
        inverseViewMatrix[1][0] = up.x;
        inverseViewMatrix[1][1] = up.y;
        inverseViewMatrix[1][2] = up.z;
        inverseViewMatrix[2][0] = forward.x;
        inverseViewMatrix[2][1] = forward.y;
        inverseViewMatrix[2][2] = forward.z;
        inverseViewMatrix[3][0] = position.x;
        inverseViewMatrix[3][1] = position.y;
        inverseViewMatrix[3][2] = position.z;
    }
} // namespace Engine
