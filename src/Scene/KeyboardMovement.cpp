
#include "Scene/KeyboardMovement.h"
#include "Vulkan/Device.h"
#include <memory>
#include <cmath>

#include "System/Input/InputSubsystem.h"

namespace Engine {
    void KeyboardMovementController::moveInPlaneXZ(InputSubsystem& input, float dt, std::shared_ptr<GameObject> gameObject)
    {
        if (!initialized) {
            glm::vec3 forward = gameObject->transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
            yaw = std::atan2(forward.x, forward.z);
            pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
            initialized = true;
        }

        if (input.IsKeyPressed(KeyCode::Right)) yaw += lookSpeed * dt;
        if (input.IsKeyPressed(KeyCode::Left))  yaw -= lookSpeed * dt;
        if (input.IsKeyPressed(KeyCode::Up))    pitch += lookSpeed * dt;
        if (input.IsKeyPressed(KeyCode::Down))  pitch -= lookSpeed * dt;

        glm::vec2 mouseDelta = input.GetMouseDelta();
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

        float forwardAxis = input.GetAxis("MoveForward");
        float rightAxis   = input.GetAxis("MoveRight");
        float upAxis      = input.GetAxis("MoveUp");

        if (std::abs(forwardAxis) > 1e-4f) {
            moveDir += forwardDirXZ * forwardAxis;
        } else {
            if (input.IsKeyPressed(KeyCode::W)) moveDir += forwardDirXZ;
            if (input.IsKeyPressed(KeyCode::S)) moveDir -= forwardDirXZ;
        }

        if (std::abs(rightAxis) > 1e-4f) {
            moveDir += rightDirXZ * rightAxis;
        } else {
            if (input.IsKeyPressed(KeyCode::D)) moveDir += rightDirXZ;
            if (input.IsKeyPressed(KeyCode::A)) moveDir -= rightDirXZ;
        }

        if (std::abs(upAxis) > 1e-4f) {
            moveDir += upDir * upAxis;
        } else {
            if (input.IsKeyPressed(KeyCode::E)) moveDir += upDir;
            if (input.IsKeyPressed(KeyCode::Q)) moveDir -= upDir;
        }

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject->transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }
} // namespace Engine
