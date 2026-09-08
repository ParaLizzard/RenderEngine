#pragma once

#include "Scene/GameObject.h"
#include "System/Input/InputSubsystem.h"

namespace Engine {
    class KeyboardMovementController
    {
    public:
        void moveInPlaneXZ(InputSubsystem& input, float dt, std::shared_ptr<GameObject> gameObject);

        float moveSpeed = 3.f;
        float lookSpeed = 1.5f;
        float mouseSensitivity = 0.0025f;

        float yaw = 0.0f;
        float pitch = 0.0f;
        bool initialized = false;
    };
} // namespace Engine
