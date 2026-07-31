#pragma once
#include <deque>
#include <vector>
#include "Scene/GameObject.h"
#include "AssetSystem/Texture.h"

namespace Engine {
    class SceneManager
    {
    private:
        std::deque<Texture2D> sceneTextures;
        std::vector<GameObject> gameObjects;
        bool isDirty = false;

    public:
        SceneManager();
        ~SceneManager();

        void flattenSceneGraph();
        void updateHierarchy(GameObject &obj, std::vector<GameObject> &allObjects, const glm::mat4 &parentMatrix);
        void addGameObjects(std::vector<GameObject>&& newObjects);

        [[nodiscard]] bool isSceneGraphDirty() const { return isDirty; }
        void markClean() { isDirty = false; }

        std::vector<GameObject>& objects() { return gameObjects; }
        std::deque<Texture2D>& textures() { return sceneTextures; }
        [[nodiscard]] const std::vector<GameObject>& objects() const { return gameObjects; }
    };
} // Engine 
