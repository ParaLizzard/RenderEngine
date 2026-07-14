#include "Core/SceneManager.h"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <stb_image.h>

#include "Scene/LoaderGLTF.h"

namespace Engine {
    SceneManager::SceneManager()
    {

    }

    SceneManager::~SceneManager()
    {
        for (auto &tex: sceneTextures) {
            tex.destroy();
        }
    }

    void SceneManager::flattenSceneGraph()
    {
        std::vector<GameObject> sorted;
        sorted.reserve(gameObjects.size());

        std::unordered_map<unsigned int, size_t> originalIdToIndex;
        for (size_t i = 0; i < gameObjects.size(); ++i) {
            originalIdToIndex[gameObjects[i].getId()] = i;
        }

        std::vector<size_t> stack;
        for (size_t i = 0; i < gameObjects.size(); ++i) {
            if (gameObjects[i].parentId == GameObject::INVALID_ID) {
                stack.push_back(i);
            }
        }

        std::unordered_map<unsigned int, size_t> newIndexMap;

        while (!stack.empty()) {
            size_t currIdx = stack.back();
            stack.pop_back();

            size_t newIdx = sorted.size();
            newIndexMap[gameObjects[currIdx].getId()] = newIdx;

            auto childrenIds = gameObjects[currIdx].childrenIds;

            sorted.push_back(std::move(gameObjects[currIdx]));

            for (auto childId: childrenIds) {
                if (originalIdToIndex.count(childId)) {
                    stack.push_back(originalIdToIndex[childId]);
                }
            }
        }

        for (auto &obj: sorted) {
            if (obj.parentId != GameObject::INVALID_ID && newIndexMap.count(obj.parentId)) {
                obj.parentCacheIndex = newIndexMap[obj.parentId];
            } else {
                obj.parentCacheIndex = std::numeric_limits<size_t>::max();
            }
        }

        gameObjects = std::move(sorted);
    }

    void SceneManager::updateHierarchy(GameObject &obj, std::vector<GameObject> &allObjects, const glm::mat4 &parentMatrix)
    {
        obj.currentWorldMatrix = parentMatrix * obj.transform.mat4();

        for (auto childId: obj.childrenIds) {
            auto it = std::find_if(
                allObjects.begin(),
                allObjects.end(),
                [childId](const GameObject &g) {
                    return g.getId() == childId;
                });

            if (it != allObjects.end()) {
                updateHierarchy(*it, allObjects, obj.currentWorldMatrix);
            }
        }
    }

    void SceneManager::integrateLoadedModels(Device &device, std::vector<ParsedGLTF> &parsedModels, Model &megaBuffer, ResourceHeap &resourceHeap)
    {
        for (auto parsedModel : parsedModels) {
            auto newObjects = LoaderGLTF::finalize(parsedModel, device, megaBuffer, resourceHeap, sceneTextures);

            for (auto &obj: newObjects)
                gameObjects.push_back(std::move(obj));
            flattenSceneGraph();
            isDirty = true;

            resourceHeap.markMaterialsDirty();

            megaBuffer.uploadToGPU();

            // Needs probably get called somewhere
            //cullPass.markSceneDirty();

            std::cout << "Successfully streamed in async model!" << std::endl;
        }
    }
} // Engine
