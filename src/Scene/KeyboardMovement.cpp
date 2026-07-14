
#include "KeyboardMovement.h"
#include "Core/Device.h"
#include <memory>

#include "Core/InputManager.h"

namespace Engine {
    void KeyboardMovementController::moveInPlaneXZ(InputManager& manager, float dt, std::shared_ptr<GameObject> gameObject)
    {
        glm::vec3 rotate {0};

        if (manager.IsKeyHeld(KeyCode::Right))
            rotate.y += 1.f;
        if (manager.IsKeyHeld(KeyCode::Left))
            rotate.y -= 1.f;
        if (manager.IsKeyHeld(KeyCode::Up))
            rotate.x -= 1.f;
        if (manager.IsKeyHeld(KeyCode::Down))
            rotate.x += 1.f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            const glm::vec3 normalizedRotate = glm::normalize(rotate);
            const float rotationAmount = lookSpeed * dt;

            glm::quat yawQuat = glm::angleAxis(normalizedRotate.y * rotationAmount, glm::vec3(0, 1, 0));

            glm::vec3 rightAxis = gameObject->transform.rotation * glm::vec3(1, 0, 0);
            glm::quat pitchQuat = glm::angleAxis(normalizedRotate.x * rotationAmount, rightAxis);

            gameObject->transform.rotation = glm::normalize(pitchQuat * yawQuat * gameObject->transform.rotation);

            glm::vec3 forward = gameObject->transform.rotation * glm::vec3(0, 0, 1);
            float currentPitch = glm::asin(glm::clamp(forward.y, -1.0f, 1.0f));

            constexpr float maxPitch = glm::radians(85.0f);
            if (glm::abs(currentPitch) > maxPitch) {
                glm::vec3 euler = glm::eulerAngles(gameObject->transform.rotation);
                euler.x = glm::clamp(euler.x, -maxPitch, maxPitch);
                gameObject->transform.rotation = glm::quat(euler);
            }
        }

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
