#pragma once

#include "Scene/Model.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include "glm/gtc/quaternion.hpp"

namespace Engine
{
    struct TransformComponent
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{};

        glm::mat4 mat4();
    };

    class GameObject
    {
    public:
        using id_t = unsigned int;

        static GameObject createGameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;

        GameObject(GameObject&&) = default;
        GameObject& operator=(GameObject&&) = default;

        id_t getId() const { return id; }

        TransformComponent transform{};
        SubMesh subMesh{};

    private:
        GameObject(id_t objId);

        id_t id;
    };
}