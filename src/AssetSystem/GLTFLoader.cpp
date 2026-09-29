#include "AssetSystem/GLTFLoader.h"

#include "Core/Assert.h"
#include "Core/Log.h"
#include "Core/EngineConstants.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

#include <glm/gtc/packing.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>

namespace Engine {

namespace {

    static std::optional<fastgltf::Asset> loadGLTFAsset(const std::filesystem::path& path)
    {
        static constexpr auto supportedExtensions =
            fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform |
            fastgltf::Extensions::KHR_materials_variants |
            fastgltf::Extensions::KHR_lights_punctual;

        fastgltf::Parser parser(supportedExtensions);

        constexpr auto gltfOptions =
            fastgltf::Options::DontRequireValidAssetMember |
            fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::GenerateMeshIndices |
            fastgltf::Options::DecomposeNodeMatrices;

        auto parseFromGetter = [&](fastgltf::GltfDataGetter& dataGetter) -> std::optional<fastgltf::Asset> {
            auto asset = parser.loadGltf(dataGetter, path.parent_path(), gltfOptions);
            if (asset.error() != fastgltf::Error::None) {
                LOG_WARN("GLTFLoader", "Failed to parse glTF {}: {}", path.string(), fastgltf::getErrorMessage(asset.error()));
                return std::nullopt;
            }
            return std::move(asset.get());
        };

        if (auto mappedFile = fastgltf::MappedGltfFile::FromPath(path)) {
            return parseFromGetter(mappedFile.get());
        } else if (auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path)) {
            return parseFromGetter(dataBuffer.get());
        }

        LOG_WARN("GLTFLoader", "Failed to open glTF file: {}", path.string());
        return std::nullopt;
    }

    static void extractTextures(
        const fastgltf::Asset& asset,
        const std::filesystem::path& baseDir,
        ParsedSceneResult& result)
    {
        result.textures.reserve(asset.textures.size());

        for (size_t i = 0; i < asset.textures.size(); ++i) {
            const auto& gltfTex = asset.textures[i];
            ParsedTextureSource texSource{};
            texSource.name = gltfTex.name.empty() ? std::to_string(i) : std::string(gltfTex.name);

            size_t imageIndex = gltfTex.imageIndex.value_or(gltfTex.basisuImageIndex.value_or(std::numeric_limits<size_t>::max()));
            if (imageIndex < asset.images.size()) {
                const auto& img = asset.images[imageIndex];

                std::visit(fastgltf::visitor{
                    [&](const fastgltf::sources::URI& uri) {
                        std::filesystem::path uriPath(uri.uri.path());
                        texSource.uri = (baseDir / uriPath).string();
                        std::string ext = uriPath.extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        texSource.isKTX2 = (ext == ".ktx2");
                    },
                    [&](const fastgltf::sources::BufferView& view) {
                        const auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                        const auto& buffer = asset.buffers[bufferView.bufferIndex];

                        std::visit(fastgltf::visitor{
                            [&](const fastgltf::sources::Array& array) {
                                const uint8_t* start = reinterpret_cast<const uint8_t*>(array.bytes.data() + bufferView.byteOffset);
                                texSource.embeddedData.assign(start, start + bufferView.byteLength);
                            },
                            [&](const fastgltf::sources::Vector& vec) {
                                const uint8_t* start = reinterpret_cast<const uint8_t*>(vec.bytes.data() + bufferView.byteOffset);
                                texSource.embeddedData.assign(start, start + bufferView.byteLength);
                            },
                            [&](const fastgltf::sources::ByteView& byteView) {
                                const uint8_t* start = reinterpret_cast<const uint8_t*>(byteView.bytes.data() + bufferView.byteOffset);
                                texSource.embeddedData.assign(start, start + bufferView.byteLength);
                            },
                            [&](const auto&) {}
                        }, buffer.data);

                        if (view.mimeType == fastgltf::MimeType::KTX2) {
                            texSource.isKTX2 = true;
                        }
                    },
                    [&](const fastgltf::sources::Array& array) {
                        const uint8_t* start = reinterpret_cast<const uint8_t*>(array.bytes.data());
                        texSource.embeddedData.assign(start, start + array.bytes.size());
                    },
                    [&](const auto&) {}
                }, img.data);
            }

            result.textures.push_back(std::move(texSource));
        }
    }

    static void extractMaterials(
        const fastgltf::Asset& asset,
        ParsedSceneResult& result)
    {
        result.materials.reserve(asset.materials.size());

        auto resolveTexturePath = [&](const auto& info, bool isSRGB) -> std::string {
            if (!info.has_value() || info->textureIndex >= result.textures.size()) return "";
            auto& tex = result.textures[info->textureIndex];
            if (isSRGB) tex.isSRGB = true;
            return tex.uri.empty() ? tex.name : tex.uri;
        };

        for (size_t i = 0; i < asset.materials.size(); ++i) {
            const auto& gltfMat = asset.materials[i];
            PBRMaterialDesc mat{};
            mat.name = gltfMat.name.empty() ? std::to_string(i) : std::string(gltfMat.name);

            mat.baseColorFactor = glm::vec4(
                gltfMat.pbrData.baseColorFactor[0],
                gltfMat.pbrData.baseColorFactor[1],
                gltfMat.pbrData.baseColorFactor[2],
                gltfMat.pbrData.baseColorFactor[3]
            );
            mat.metallicFactor = gltfMat.pbrData.metallicFactor;
            mat.roughnessFactor = gltfMat.pbrData.roughnessFactor;
            mat.emissiveFactor = glm::vec3(
                gltfMat.emissiveFactor[0],
                gltfMat.emissiveFactor[1],
                gltfMat.emissiveFactor[2]
            );
            mat.alphaCutoff = gltfMat.alphaCutoff;
            mat.doubleSided = gltfMat.doubleSided;

            if (gltfMat.alphaMode == fastgltf::AlphaMode::Mask) {
                mat.alphaMode = AlphaMode::Mask;
            } else if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend) {
                mat.alphaMode = AlphaMode::Blend;
            } else {
                mat.alphaMode = AlphaMode::Opaque;
            }

            mat.albedoTexturePath = resolveTexturePath(gltfMat.pbrData.baseColorTexture, true);
            mat.emissiveTexturePath = resolveTexturePath(gltfMat.emissiveTexture, true);
            mat.normalTexturePath = resolveTexturePath(gltfMat.normalTexture, false);
            mat.metallicRoughnessTexturePath = resolveTexturePath(gltfMat.pbrData.metallicRoughnessTexture, false);
            mat.occlusionTexturePath = resolveTexturePath(gltfMat.occlusionTexture, false);

            if (gltfMat.pbrData.baseColorTexture.has_value()) {
                mat.albedoTextureIndex = static_cast<int32_t>(gltfMat.pbrData.baseColorTexture->textureIndex);
            }
            if (gltfMat.emissiveTexture.has_value()) {
                mat.emissiveTextureIndex = static_cast<int32_t>(gltfMat.emissiveTexture->textureIndex);
            }
            if (gltfMat.normalTexture.has_value()) {
                mat.normalTextureIndex = static_cast<int32_t>(gltfMat.normalTexture->textureIndex);
            }
            if (gltfMat.pbrData.metallicRoughnessTexture.has_value()) {
                mat.metallicRoughnessTextureIndex = static_cast<int32_t>(gltfMat.pbrData.metallicRoughnessTexture->textureIndex);
            }
            if (gltfMat.occlusionTexture.has_value()) {
                mat.occlusionTextureIndex = static_cast<int32_t>(gltfMat.occlusionTexture->textureIndex);
            }

            result.materials.push_back(std::move(mat));
        }
    }

    struct PrimitiveTask {
        size_t meshIndex;
        size_t submeshIndex;
        size_t primitiveIndex;
    };

    static void processPrimitive(
        const fastgltf::Asset& asset,
        const PrimitiveTask& task,
        ParsedSceneResult& result)
    {
        const auto& gltfMesh = asset.meshes[task.meshIndex];
        const auto& prim = gltfMesh.primitives[task.primitiveIndex];
        auto& submesh = result.meshes[task.meshIndex].submeshes[task.submeshIndex];

        const auto* posIt = prim.findAttribute("POSITION");
        if (posIt == prim.attributes.end() || !prim.indicesAccessor.has_value()) {
            return;
        }

        submesh.name = result.meshes[task.meshIndex].name.empty()
            ? std::to_string(task.primitiveIndex)
            : (result.meshes[task.meshIndex].name + "_" + std::to_string(task.primitiveIndex));
        submesh.materialIndex = prim.materialIndex.has_value() ? static_cast<uint32_t>(prim.materialIndex.value()) : 0;

        const auto& posAccessor = asset.accessors[posIt->accessorIndex];
        submesh.positions.resize(posAccessor.count);
        submesh.attributes.resize(posAccessor.count);

        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset, posAccessor,
            [&](fastgltf::math::fvec3 pos, size_t idx) {
                submesh.positions[idx].position = glm::vec3(pos.x(), -pos.y(), pos.z());
                glm::vec2 defaultOct = EncodeOctNormal(glm::vec3(0.0f, 1.0f, 0.0f));
                submesh.attributes[idx].normalOct = glm::packHalf2x16(defaultOct);
                submesh.attributes[idx].tangentLo = glm::packHalf2x16(glm::vec2(1.0f, 0.0f));
                submesh.attributes[idx].tangentHi = glm::packHalf2x16(glm::vec2(0.0f, 1.0f));
                submesh.attributes[idx].uv = glm::packHalf2x16(glm::vec2(0.0f));
            }
        );

        if (const auto* normIt = prim.findAttribute("NORMAL"); normIt != prim.attributes.end()) {
            const auto& normAccessor = asset.accessors[normIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, normAccessor,
                [&](fastgltf::math::fvec3 norm, size_t idx) {
                    glm::vec3 n(norm.x(), -norm.y(), norm.z());
                    submesh.attributes[idx].normalOct = glm::packHalf2x16(EncodeOctNormal(n));
                }
            );
        }

        if (const auto* uvIt = prim.findAttribute("TEXCOORD_0"); uvIt != prim.attributes.end()) {
            const auto& uvAccessor = asset.accessors[uvIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                asset, uvAccessor,
                [&](fastgltf::math::fvec2 uv, size_t idx) {
                    submesh.attributes[idx].uv = glm::packHalf2x16(glm::vec2(uv.x(), uv.y()));
                }
            );
        }

        if (const auto* tanIt = prim.findAttribute("TANGENT"); tanIt != prim.attributes.end()) {
            const auto& tanAccessor = asset.accessors[tanIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                asset, tanAccessor,
                [&](fastgltf::math::fvec4 tan, size_t idx) {
                    glm::vec4 t(tan.x(), -tan.y(), tan.z(), tan.w());
                    submesh.attributes[idx].tangentLo = glm::packHalf2x16(glm::vec2(t.x, t.y));
                    submesh.attributes[idx].tangentHi = glm::packHalf2x16(glm::vec2(t.z, t.w));
                }
            );
        }

        const auto& idxAccessor = asset.accessors[prim.indicesAccessor.value()];
        submesh.indices.resize(idxAccessor.count);
        fastgltf::iterateAccessorWithIndex<uint32_t>(
            asset, idxAccessor,
            [&](uint32_t val, size_t idx) {
                submesh.indices[idx] = val;
            }
        );

        for (size_t i = 0; i < submesh.indices.size(); i += 3) {
            std::swap(submesh.indices[i + 1], submesh.indices[i + 2]);
        }

        size_t maxMeshlets = meshopt_buildMeshletsBound(
            submesh.indices.size(),
            Constants::Meshlet::MAX_MESHLET_VERTICES,
            Constants::Meshlet::MAX_MESHLET_TRIANGLES);

        std::vector<meshopt_Meshlet> rawMeshlets(maxMeshlets);
        std::vector<unsigned int> rawVertices(maxMeshlets * Constants::Meshlet::MAX_MESHLET_VERTICES);
        std::vector<unsigned char> rawTriangles(maxMeshlets * Constants::Meshlet::MAX_MESHLET_TRIANGLES * 3);

        size_t meshletCount = meshopt_buildMeshlets(
            rawMeshlets.data(),
            rawVertices.data(),
            rawTriangles.data(),
            submesh.indices.data(),
            submesh.indices.size(),
            reinterpret_cast<const float*>(submesh.positions.data()),
            submesh.positions.size(),
            sizeof(VertexPositionGPU),
            Constants::Meshlet::MAX_MESHLET_VERTICES,
            Constants::Meshlet::MAX_MESHLET_TRIANGLES,
            Constants::Meshlet::CONE_WEIGHT
        );
        rawMeshlets.resize(meshletCount);

        submesh.meshlets.reserve(meshletCount);
        for (size_t k = 0; k < meshletCount; ++k) {
            const auto& src = rawMeshlets[k];
            meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                &rawVertices[src.vertex_offset],
                &rawTriangles[src.triangle_offset],
                src.triangle_count,
                reinterpret_cast<const float*>(submesh.positions.data()),
                submesh.positions.size(),
                sizeof(VertexPositionGPU)
            );

            MeshletGPU dst{};
            dst.centerX = bounds.center[0];
            dst.centerY = bounds.center[1];
            dst.centerZ = bounds.center[2];
            dst.radius  = bounds.radius;
            dst.coneAxisCutoff = PackConeAxisCutoff(bounds.cone_axis, bounds.cone_cutoff);
            dst.vertexOffset   = src.vertex_offset;
            dst.triangleOffset = src.triangle_offset;
            dst.vertexCount    = src.vertex_count;
            dst.triangleCount  = src.triangle_count;

            submesh.meshlets.push_back(dst);
        }

        if (!rawMeshlets.empty()) {
            const auto& last = rawMeshlets.back();
            size_t vertEnd = last.vertex_offset + last.vertex_count;
            size_t triEnd  = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);

            submesh.meshletVertices.assign(rawVertices.begin(), rawVertices.begin() + vertEnd);
            submesh.meshletTriangles.assign(rawTriangles.begin(), rawTriangles.begin() + triEnd);
        }

        // 7. Submesh bounding volumes
        ComputeBoundingVolumes(submesh.positions, submesh.aabbMin, submesh.aabbMax, submesh.boundingSphere);
    }

    static std::optional<JobHandle> dispatchMeshExtraction(
        const fastgltf::Asset& asset,
        ParsedSceneResult& result,
        JobSystem* jobSystem)
    {
        result.meshes.resize(asset.meshes.size());

        std::vector<PrimitiveTask> tasks;
        for (size_t m = 0; m < asset.meshes.size(); ++m) {
            const auto& gltfMesh = asset.meshes[m];
            auto& meshData = result.meshes[m];
            meshData.name = gltfMesh.name.empty() ? std::to_string(m) : std::string(gltfMesh.name);

            std::vector<size_t> validPrims;
            validPrims.reserve(gltfMesh.primitives.size());
            for (size_t p = 0; p < gltfMesh.primitives.size(); ++p) {
                const auto& prim = gltfMesh.primitives[p];
                const auto* posIt = prim.findAttribute("POSITION");
                if (posIt != prim.attributes.end() && prim.indicesAccessor.has_value()) {
                    validPrims.push_back(p);
                }
            }

            meshData.submeshes.resize(validPrims.size());
            for (size_t s = 0; s < validPrims.size(); ++s) {
                tasks.push_back(PrimitiveTask{
                    .meshIndex = m,
                    .submeshIndex = s,
                    .primitiveIndex = validPrims[s]
                });
            }
        }

        if (tasks.empty()) {
            return std::nullopt;
        }

        if (jobSystem && tasks.size() > 1) {
            uint32_t workerCount = std::max(1u, jobSystem->GetWorkerCount());
            uint32_t groupSize = 1;
            if (tasks.size() > 256 * workerCount) {
                groupSize = static_cast<uint32_t>((tasks.size() + workerCount * 8 - 1) / (workerCount * 8));
            }

            auto tasksPtr = std::make_shared<const std::vector<PrimitiveTask>>(std::move(tasks));
            return jobSystem->Dispatch(
                static_cast<uint32_t>(tasksPtr->size()),
                groupSize,
                [&asset, &result, tasksPtr](uint32_t startIndex, uint32_t endIndex) {
                    for (uint32_t i = startIndex; i < endIndex; ++i) {
                        processPrimitive(asset, (*tasksPtr)[i], result);
                    }
                },
                JobPriority::Normal
            );
        } else {
            for (const auto& task : tasks) {
                processPrimitive(asset, task, result);
            }
            return std::nullopt;
        }
    }

    static void extractNodes(
        const fastgltf::Asset& asset,
        ParsedSceneResult& result)
    {
        result.nodes.resize(asset.nodes.size());

        for (size_t i = 0; i < asset.nodes.size(); ++i) {
            const auto& gltfNode = asset.nodes[i];
            auto& nodeData = result.nodes[i];
            nodeData.name = gltfNode.name.empty() ? std::to_string(i) : std::string(gltfNode.name);

            std::visit(fastgltf::visitor{
                [&](const fastgltf::TRS& trs) {
                    nodeData.translation = glm::vec3(trs.translation[0], -trs.translation[1], trs.translation[2]);
                    nodeData.rotation = glm::quat(trs.rotation[3], -trs.rotation[0], trs.rotation[1], -trs.rotation[2]);
                    nodeData.scale = glm::vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
                },
                [&](const fastgltf::math::fmat4x4& mat) {
                    glm::mat4 m;
                    std::memcpy(&m, mat.data(), sizeof(glm::mat4));
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    glm::decompose(m, nodeData.scale, nodeData.rotation, nodeData.translation, skew, perspective);
                    nodeData.translation.y = -nodeData.translation.y;
                }
            }, gltfNode.transform);

            if (gltfNode.meshIndex.has_value()) {
                nodeData.meshIndex = static_cast<uint32_t>(gltfNode.meshIndex.value());
            }
            if (gltfNode.lightIndex.has_value()) {
                nodeData.lightIndex = static_cast<uint32_t>(gltfNode.lightIndex.value());
            }
            if (gltfNode.cameraIndex.has_value()) {
                nodeData.cameraIndex = static_cast<uint32_t>(gltfNode.cameraIndex.value());
            }

            nodeData.childrenIndices.assign(gltfNode.children.begin(), gltfNode.children.end());
        }

        for (size_t i = 0; i < result.nodes.size(); ++i) {
            for (uint32_t childIdx : result.nodes[i].childrenIndices) {
                if (childIdx < result.nodes.size()) {
                    result.nodes[childIdx].parentIndex = static_cast<int32_t>(i);
                }
            }
        }

        size_t sceneIdx = asset.defaultScene.value_or(0);
        if (sceneIdx < asset.scenes.size()) {
            const auto& scene = asset.scenes[sceneIdx];
            result.rootNodeIndices.assign(scene.nodeIndices.begin(), scene.nodeIndices.end());
        } else {
            for (size_t i = 0; i < result.nodes.size(); ++i) {
                if (result.nodes[i].parentIndex == -1) {
                    result.rootNodeIndices.push_back(static_cast<uint32_t>(i));
                }
            }
        }
    }

    static void extractLights(
        const fastgltf::Asset& asset,
        ParsedSceneResult& result)
    {
        result.lights.reserve(asset.lights.size());

        for (size_t i = 0; i < asset.lights.size(); ++i) {
            const auto& gltfLight = asset.lights[i];
            ParsedLightData lightData{};
            lightData.name = gltfLight.name.empty() ? std::to_string(i) : std::string(gltfLight.name);
            lightData.color = glm::vec3(gltfLight.color[0], gltfLight.color[1], gltfLight.color[2]);
            lightData.intensity = gltfLight.intensity;
            lightData.range = gltfLight.range.value_or(0.0f);

            switch (gltfLight.type) {
                case fastgltf::LightType::Directional:
                    lightData.type = ParsedLightType::Directional;
                    break;
                case fastgltf::LightType::Point:
                    lightData.type = ParsedLightType::Point;
                    break;
                case fastgltf::LightType::Spot:
                    lightData.type = ParsedLightType::Spot;
                    lightData.innerConeAngle = gltfLight.innerConeAngle.value_or(0.0f);
                    lightData.outerConeAngle = gltfLight.outerConeAngle.value_or(0.785398f);
                    break;
            }

            result.lights.push_back(std::move(lightData));
        }
    }

}

ParsedSceneResult GLTFLoader::LoadScene(const std::filesystem::path& path, JobSystem* jobSystem)
{
    ParsedSceneResult result{};
    result.sourcePath = path;
    result.name = path.stem().string();

    if (!std::filesystem::exists(path)) {
        result.errorMessage = "File does not exist: " + path.string();
        LOG_WARN("GLTFLoader", "{}", result.errorMessage);
        return result;
    }

    auto assetOpt = loadGLTFAsset(path);
    if (!assetOpt.has_value()) {
        result.errorMessage = "glTF parsing failed for: " + path.string();
        return result;
    }

    const auto& asset = *assetOpt;

    auto meshJob = dispatchMeshExtraction(asset, result, jobSystem);

    extractTextures(asset, path.parent_path(), result);
    extractMaterials(asset, result);
    extractNodes(asset, result);
    extractLights(asset, result);

    if (meshJob.has_value() && jobSystem) {
        jobSystem->Wait(*meshJob);
    }

    for (auto& mesh : result.meshes) {
        ComputeMeshBoundingVolumes(mesh.submeshes, mesh.aabbMin, mesh.aabbMax, mesh.boundingSphere);
    }

    result.success = true;
    LOG_INFO("GLTFLoader", "Loaded glTF '{}': {} meshes, {} meshlets, {} materials, {} nodes",
             result.name, result.meshes.size(), result.GetTotalMeshletCount(),
             result.materials.size(), result.nodes.size());

    return result;
}

JobHandle GLTFLoader::LoadSceneAsync(
    JobSystem& jobSystem,
    const std::filesystem::path& path,
    std::function<void(ParsedSceneResult&&)> onComplete)
{
    return jobSystem.Execute([&jobSystem, path, onComplete = std::move(onComplete)]() {
        ParsedSceneResult result = GLTFLoader::LoadScene(path, &jobSystem);
        if (onComplete) {
            onComplete(std::move(result));
        }
    }, JobPriority::Normal);
}

} // namespace Engine