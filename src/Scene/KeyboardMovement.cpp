
#include "Scene/KeyboardMovement.h"
#include "Vulkan/Device.h"
#include <memory>

#include "System/Input/InputManager.h"

namespace Engine {
    void KeyboardMovementController::moveInPlaneXZ(InputManager& manager, float dt, std::shared_ptr<GameObject> gameObject)
    {
        if (!initialized) {
            glm::vec3 forward = gameObject->transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            yaw = std::atan2(forward.x, forward.z);
            pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
            initialized = true;
        }

        if (manager.IsKeyHeld(KeyCode::Right)) yaw += lookSpeed * dt;
        if (manager.IsKeyHeld(KeyCode::Left))  yaw -= lookSpeed * dt;
        if (manager.IsKeyHeld(KeyCode::Up))    pitch += lookSpeed * dt;
        if (manager.IsKeyHeld(KeyCode::Down))  pitch -= lookSpeed * dt;

        glm::vec2 mouseDelta = manager.GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;

        constexpr float maxPitch = glm::radians(89.0f);
        pitch = glm::clamp(pitch, -maxPitch, maxPitch);

        glm::quat yawQuat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat pitchQuat = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

        gameObject->transform.rotation = glm::normalize(yawQuat * pitchQuat);

        const glm::vec3 forwardDir = glm::normalize(gameObject->transform.rotation * glm::vec3(0, 0, 1));
        const glm::vec3 rightDir = glm::normalize(gameObject->transform.rotation * glm::vec3(1, 0, 0));
        const glm::vec3 upDir {0.f, 1.f, 0.f};

        glm::vec3 forwardDirXZ = glm::normalize(glm::vec3(forwardDir.x, 0.0f, forwardDir.z));
        glm::vec3 rightDirXZ = glm::normalize(glm::vec3(rightDir.x, 0.0f, rightDir.z));

        glm::vec3 moveDir {0.f};
        if (manager.IsKeyHeld(KeyCode::W))
            moveDir += forwardDirXZ;
        if (manager.IsKeyHeld(KeyCode::S))
            moveDir -= forwardDirXZ;
        if (manager.IsKeyHeld(KeyCode::D))
            moveDir += rightDirXZ;
        if (manager.IsKeyHeld(KeyCode::A))
            moveDir -= rightDirXZ;
        if (manager.IsKeyHeld(KeyCode::E))
            moveDir += upDir;
        if (manager.IsKeyHeld(KeyCode::Q))
            moveDir -= upDir;


        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject->transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }
} // namespace Engine
