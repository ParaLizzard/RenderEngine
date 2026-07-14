#include "Scene/GameObject.h"
#include <algorithm>

namespace Engine {
    static GameObject::id_t currentId = 0;

    GameObject::GameObject(id_t objId): id(objId)
    {}

    GameObject GameObject::createGameObject()
    {
        return GameObject(currentId++);
    }

    glm::mat4 TransformComponent::mat4()
    {
        glm::quat unnormalizedQuat = glm::quat(rotation);
        const glm::quat q = glm::normalize(unnormalizedQuat);
        const float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
        const float qx2 = qx * qx, qy2 = qy * qy, qz2 = qz * qz;
        const float qxy = qx * qy, qxz = qx * qz, qxw = qx * qw;
        const float qyz = qy * qz, qyw = qy * qw, qzw = qz * qw;

        return glm::mat4 {{
                              scale.x * (1.0f - 2.0f * (qy2 + qz2)),
                              scale.x * (2.0f * (qxy + qzw)),
                              scale.x * (2.0f * (qxz - qyw)),
                              0.0f,
                          },
                          {
                              scale.y * (2.0f * (qxy - qzw)),
                              scale.y * (1.0f - 2.0f * (qx2 + qz2)),
                              scale.y * (2.0f * (qyz + qxw)),
                              0.0f,
                          },
                          {
                              scale.z * (2.0f * (qxz + qyw)),
                              scale.z * (2.0f * (qyz - qxw)),
                              scale.z * (1.0f - 2.0f * (qx2 + qy2)),
                              0.0f,
                          },
                          {translation.x, translation.y, translation.z, 1.0f}};
    }

    bool GameObject::addChild(GameObject &child)
    {
        if (child.parentId != INVALID_ID)
            return false;

        if (child.getId() == INVALID_ID || child.getId() == id)
            return false;

        if (std::ranges::find(childrenIds, child.getId()) != childrenIds.end())
            return false;

        childrenIds.push_back(child.getId());
        child.parentId = this->getId();
        return true;
    }
} // namespace Engine
