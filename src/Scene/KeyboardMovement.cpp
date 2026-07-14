
#include "KeyboardMovement.h"
#include "Core/Device.h"
#include <memory>

namespace Engine {
    void KeyboardMovementController::moveInPlaneXZ(GLFWwindow *window, float dt, std::shared_ptr<GameObject> gameObject)
    {
        glm::vec3 rotate {0};
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS)
            rotate.y += 1.f;
        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS)
            rotate.y -= 1.f;
        if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS)
            rotate.x -= 1.f;
        if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS)
            rotate.x += 1.f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            const glm::vec3 normalizedRotate = glm::normalize(rotate);
            const float rotationAmount = lookSpeed * dt;

            // Apply yaw in world space (around world up axis)
            glm::quat yawQuat = glm::angleAxis(normalizedRotate.y * rotationAmount, glm::vec3(0, 1, 0));

            // Apply pitch in local space (around camera's local right axis)
            glm::vec3 rightAxis = gameObject->transform.rotation * glm::vec3(1, 0, 0);
            glm::quat pitchQuat = glm::angleAxis(normalizedRotate.x * rotationAmount, rightAxis);

            // Combine rotations: pitch (local) * yaw (world) * current rotation
            gameObject->transform.rotation = glm::normalize(pitchQuat * yawQuat * gameObject->transform.rotation);

            // Clamp pitch to prevent camera flip
            glm::vec3 forward = gameObject->transform.rotation * glm::vec3(0, 0, 1);
            float currentPitch = glm::asin(glm::clamp(forward.y, -1.0f, 1.0f));

            constexpr float maxPitch = glm::radians(85.0f); // ~1.48 radians
            if (glm::abs(currentPitch) > maxPitch) {
                // Reconstruct rotation with clamped pitch
                glm::vec3 euler = glm::eulerAngles(gameObject->transform.rotation);
                euler.x = glm::clamp(euler.x, -maxPitch, maxPitch);
                gameObject->transform.rotation = glm::quat(euler);
            }
        }

        // Extract actual forward and right directions from the quaternion
        const glm::vec3 forwardDir = glm::normalize(gameObject->transform.rotation * glm::vec3(0, 0, 1));
        const glm::vec3 rightDir = glm::normalize(gameObject->transform.rotation * glm::vec3(1, 0, 0));
        const glm::vec3 upDir {0.f, 1.f, 0.f};

        // For FPS movement, use forward projected onto XZ plane
        glm::vec3 forwardDirXZ = glm::normalize(glm::vec3(forwardDir.x, 0.0f, forwardDir.z));
        glm::vec3 rightDirXZ = glm::normalize(glm::vec3(rightDir.x, 0.0f, rightDir.z));

        glm::vec3 moveDir {0.f};
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS)
            moveDir += forwardDirXZ;
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS)
            moveDir -= forwardDirXZ;
        if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS)
            moveDir += rightDirXZ;
        if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS)
            moveDir -= rightDirXZ;
        if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS)
            moveDir += upDir;
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS)
            moveDir -= upDir;

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject->transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }
} // namespace Engine
