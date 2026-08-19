#pragma once

#include "System/Window/Window.h"
#include "Scene/GameObject.h"

namespace Engine {
    class KeyboardMovementController
    {
    public:


        void moveInPlaneXZ(InputManager& manager, float dt, std::shared_ptr<GameObject> gameObject);

        float moveSpeed = 3.f;
        float lookSpeed = 1.5f;
        float mouseSensitivity = 0.0025f;

        float yaw = 0.0f;
        float pitch = 0.0f;
        bool initialized = false;
    };
} // namespace Engine
