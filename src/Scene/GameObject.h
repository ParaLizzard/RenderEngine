#pragma once

#include "AssetSystem/Model.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include <string>
#include <limits>

namespace Engine {
    struct TransformComponent
    {
        glm::vec3 translation {};
        glm::vec3 scale {1.f, 1.f, 1.f};
        glm::quat rotation {1.f, 0.f, 0.f, 0.f};

        glm::mat4 mat4();
    };

    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    class GameObject
    {
    public:
        using id_t = unsigned int;
        static constexpr id_t INVALID_ID = std::numeric_limits<id_t>::max();

        static GameObject createGameObject();
        bool addChild(GameObject &child);

        GameObject(const GameObject &) = delete;
        GameObject &operator=(const GameObject &) = delete;

        GameObject(GameObject &&) = default;
        GameObject &operator=(GameObject &&) = default;

        id_t getId() const
        {
            return id;
        }

        TransformComponent transform {};
        Model::SubMesh subMesh {};

        id_t parentId = INVALID_ID;
        size_t parentCacheIndex = std::numeric_limits<size_t>::max();
        std::vector<id_t> childrenIds;

        glm::mat4 currentWorldMatrix {1.0f};

        AlphaMode alphaMode = AlphaMode::Opaque;
        bool doubleSided = false;
        glm::vec4 boundingSphere {0.0f, 0.0f, 0.0f, 0.0f};

    private:
        GameObject(id_t objId);

        id_t id;
    };
} // namespace Engine
