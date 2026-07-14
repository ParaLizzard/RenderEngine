#pragma once

#include "Core/Window.h"
#include "Scene/GameObject.h"

namespace Engine {
    class KeyboardMovementController
    {
    public:


        void moveInPlaneXZ(InputManager& manager, float dt, std::shared_ptr<GameObject> gameObject);

        float moveSpeed = 3.f;
        float lookSpeed = 1.5f;
    };
} // namespace Engine
