#include "LoaderGLTF.h"
#include <iostream>
#include <stb_image.h>

namespace Engine {
    std::future<ParsedGLTF> LoaderGLTF::loadAsync(JobSystem &jobSystem, const std::filesystem::path &filePath)
    {
        return jobSystem.enqueue([&jobSystem, filePath]() {
            ParsedGLTF result {};

            if (!std::filesystem::exists(filePath)) {
                std::cerr << "LoaderGLTF: File not found: " << filePath << '\n';
                return result;
            }

            try {
                fastgltf::Asset asset = loadAsset(filePath);

                decodeImages(jobSystem, asset, filePath.parent_path(), result);

                extractMaterials(asset, result);

                extractNodesAndMeshes(asset, result);

                result.success = true;
            } catch (const JobSystemStoppedException &) {
            } catch (const std::exception &e) {
                std::cerr << "LoaderGLTF: Async load failed: " << e.what() << '\n';
            }

            return result;
        });
    }

    void LoaderGLTF::decodeImages(JobSystem &jobSystem,
                                  fastgltf::Asset &asset,
                                  const std::filesystem::path &assetDir,
                                  ParsedGLTF &outData)
    {
        outData.images.resize(asset.images.size());
        std::vector<std::future<void>> decodeJobs;
        try {
            for (std::size_t i = 0; i < asset.images.size(); ++i) {
                decodeJobs.push_back(jobSystem.enqueue([&asset, &assetDir, &outData, i]() {
                    auto &gltfImage = asset.images[i];
                    auto &parsedImg = outData.images[i];

                    const uint8_t *rawData = nullptr;
                    size_t rawSize = 0;
                    std::vector<uint8_t> fileBuffer;

                    std::visit(fastgltf::visitor {
                                   [&](fastgltf::sources::URI &uriSource) {
                                       std::string path = (assetDir / uriSource.uri.path()).string();
                                       std::ifstream file(path, std::ios::binary | std::ios::ate);
                                       if (file) {
                                           rawSize = file.tellg();
                                           file.seekg(0, std::ios::beg);
                                           fileBuffer.resize(rawSize);
                                           file.read(reinterpret_cast<char *>(fileBuffer.data()), rawSize);
                                           rawData = fileBuffer.data();
                                       }
                                   },
                                   [&](fastgltf::sources::Array &arraySource) {
                                       rawData = reinterpret_cast<const uint8_t *>(arraySource.bytes.data());
                                       rawSize = arraySource.bytes.size();
                                   },
                                   [&](fastgltf::sources::BufferView &viewSource) {
                                       auto &bufferView = asset.bufferViews[viewSource.bufferViewIndex];
                                       auto &buffer = asset.buffers[bufferView.bufferIndex];

                                       std::visit(fastgltf::visitor {
                                                      [&](fastgltf::sources::Array &arraySource) {
                                                          rawData = reinterpret_cast<const uint8_t *>(
                                                              arraySource.bytes.data() + bufferView.byteOffset);
                                                          rawSize = bufferView.byteLength;
                                                      },
                                                      [&](fastgltf::sources::Vector &vectorSource) {
                                                          rawData = reinterpret_cast<const uint8_t *>(
                                                              vectorSource.bytes.data() + bufferView.byteOffset);
                                                          rawSize = bufferView.byteLength;
                                                      },
                                                      [&](fastgltf::sources::ByteView &byteViewSource) {
                                                          rawData = reinterpret_cast<const uint8_t *>(
                                                              byteViewSource.bytes.data() + bufferView.byteOffset);
                                                          rawSize = bufferView.byteLength;
                                                      },
                                                      [&](auto &) {
                                                      }},
                                                  buffer.data);
                                   },
                                   [&](auto &) {
                                   }},
                               gltfImage.data);

                    if (rawData && rawSize > 0) {
                        bool isKTX2 = false;
                        if (rawSize >= 12) {
                            const uint8_t ktx2Magic[] = {
                                0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
                            if (memcmp(rawData, ktx2Magic, 12) == 0) {
                                isKTX2 = true;
                            }
                        }

                        if (isKTX2) {
                            parsedImg.isKTX2 = true;

                            ktxTexture *ktxTex = nullptr;
                            if (ktxTexture_CreateFromMemory(
                                    rawData, rawSize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex) == KTX_SUCCESS) {
                                if (ktxTexture_NeedsTranscoding(ktxTex)) {
                                    ktxTexture2 *k2 = reinterpret_cast<ktxTexture2 *>(ktxTex);
                                    ktxTexture2_TranscodeBasis(k2, KTX_TTF_BC7_RGBA, 0);
                                }
                                parsedImg.ktxTexPtr = ktxTex;
                                parsedImg.isValid = true;
                            } else {
                                parsedImg.isValid = false;
                            }
                        } else {
                            int width, height, channels;
                            stbi_uc *pixels = stbi_load_from_memory(
                                rawData, static_cast<int>(rawSize), &width, &height, &channels, STBI_rgb_alpha);
                            if (pixels) {
                                parsedImg.width = static_cast<uint32_t>(width);
                                parsedImg.height = static_cast<uint32_t>(height);
                                size_t imageSize = width * height * 4;
                                parsedImg.data.assign(pixels, pixels + imageSize);
                                parsedImg.isKTX2 = false;
                                parsedImg.isValid = true;
                                stbi_image_free(pixels);
                            }
                        }
                    }
                }));
            }
        } catch (const std::runtime_error &e) {
            if (std::string(e.what()).find("stopped JobSystem") != std::string::npos) {
                std::clog << "LoaderGLTF: Loading canceled because JobSystem stopped.\n";
                return;
            }
            throw;
        }

        for (auto &job: decodeJobs) {
            job.get();
        }
    }

    void LoaderGLTF::extractMaterials(fastgltf::Asset &asset, ParsedGLTF &outData)
    {
        auto getTexIndex = [&](const auto &texInfo, bool isSRGB = false) -> uint32_t {
            if (texInfo.has_value()) {
                auto &texture = asset.textures[texInfo->textureIndex];

                if (texture.basisuImageIndex.has_value()) {
                    uint32_t imgIdx = static_cast<uint32_t>(texture.basisuImageIndex.value());
                    if (isSRGB)
                        outData.images[imgIdx].isSRGB = true;
                    return imgIdx;
                } else if (texture.imageIndex.has_value()) {
                    uint32_t imgIdx = static_cast<uint32_t>(texture.imageIndex.value());
                    if (isSRGB)
                        outData.images[imgIdx].isSRGB = true;
                    return imgIdx;
                }
            }
            return ResourceHeap::INVALID_HANDLE;
        };

        for (auto &mat: asset.materials) {
            ResourceHeap::MaterialData cpuMat {};

            cpuMat.albedoIndex = getTexIndex(mat.pbrData.baseColorTexture, true);
            cpuMat.normalIndex = getTexIndex(mat.normalTexture, false);
            cpuMat.roughnessMetallicIndex = getTexIndex(mat.pbrData.metallicRoughnessTexture, false);
            cpuMat.emissiveIndex = getTexIndex(mat.emissiveTexture, true);
            cpuMat.occlusionIndex = getTexIndex(mat.occlusionTexture, false);

            cpuMat.albedoFactor = glm::vec4(mat.pbrData.baseColorFactor[0],
                                            mat.pbrData.baseColorFactor[1],
                                            mat.pbrData.baseColorFactor[2],
                                            mat.pbrData.baseColorFactor[3]);
            cpuMat.emissiveFactor =
                glm::vec4(mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2], 0.0f);

            cpuMat.roughnessFactor = mat.pbrData.roughnessFactor;
            cpuMat.metallicFactor = mat.pbrData.metallicFactor;
            cpuMat.normalScale = mat.normalTexture.has_value() ? mat.normalTexture->scale : 1.0f;

            cpuMat.flags = 0;
            if (mat.alphaMode == fastgltf::AlphaMode::Mask)
                cpuMat.flags |= (1u << 0);
            if (mat.alphaMode == fastgltf::AlphaMode::Blend)
                cpuMat.flags |= (1u << 1);
            if (mat.doubleSided)
                cpuMat.flags |= (1u << 2);
            cpuMat.alphaCutoff = mat.alphaCutoff;

            outData.materials.push_back(cpuMat);
        }
    }

    void LoaderGLTF::extractNodesAndMeshes(fastgltf::Asset &asset, ParsedGLTF &outData)
    {
        outData.nodes.resize(asset.nodes.size());

        for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
            auto &gltfNode = asset.nodes[i];
            auto &parsedNode = outData.nodes[i];

            parsedNode.childrenIndices.assign(gltfNode.children.begin(), gltfNode.children.end());

            std::visit(fastgltf::visitor {[&](const fastgltf::math::fmat4x4 &matrix) {
                                              glm::mat4 bakedMatrix;
                                              memcpy(&bakedMatrix, matrix.data(), sizeof(glm::mat4));
                                              glm::vec3 skew;
                                              glm::vec4 perspective;
                                              glm::decompose(bakedMatrix,
                                                             parsedNode.transform.scale,
                                                             parsedNode.transform.rotation,
                                                             parsedNode.transform.translation,
                                                             skew,
                                                             perspective);
                                          },
                                          [&](const fastgltf::TRS &trs) {
                                              parsedNode.transform.translation = glm::vec3(
                                                  trs.translation[0], -trs.translation[1], trs.translation[2]);
                                              parsedNode.transform.rotation = glm::quat(
                                                  trs.rotation[3], -trs.rotation[0], trs.rotation[1], -trs.rotation[2]);
                                              parsedNode.transform.scale =
                                                  glm::vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
                                          }},
                       gltfNode.transform);

            if (gltfNode.meshIndex.has_value()) {
                auto &mesh = asset.meshes[gltfNode.meshIndex.value()];

                for (auto &prim: mesh.primitives) {
                    auto *positionIt = prim.findAttribute("POSITION");
                    if (positionIt == prim.attributes.end() || !prim.indicesAccessor.has_value())
                        continue;

                    ParsedPrimitive parsedPrim {};
                    parsedPrim.localMaterialIndex =
                        prim.materialIndex.has_value() ? static_cast<uint32_t>(prim.materialIndex.value()) + 1 : 0;

                    auto &posAccessor = asset.accessors[positionIt->accessorIndex];
                    parsedPrim.positions.resize(posAccessor.count);
                    parsedPrim.attributes.resize(posAccessor.count);

                    // --- BASE VERTEX DATA (Positions & Defaults) ---
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, posAccessor, [&](fastgltf::math::fvec3 p, std::size_t idx) {
                            parsedPrim.positions[idx].position = glm::vec3(p.x(), -p.y(), p.z());
                            parsedPrim.attributes[idx].color = glm::vec3(1.0f);
                            parsedPrim.attributes[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                            parsedPrim.attributes[idx].tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                            parsedPrim.attributes[idx].uv = glm::vec2(0.0f);
                        });

                    // --- UVS ---
                    auto uvIt = prim.findAttribute("TEXCOORD_0");
                    if (uvIt != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                            asset,
                            asset.accessors[uvIt->accessorIndex],
                            [&](fastgltf::math::fvec2 uv, std::size_t idx) {
                                parsedPrim.attributes[idx].uv = glm::vec2(uv.x(), uv.y());
                            });
                    }

                    // --- NORMALS ---
                    auto normalIt = prim.findAttribute("NORMAL");
                    if (normalIt != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            asset,
                            asset.accessors[normalIt->accessorIndex],
                            [&](fastgltf::math::fvec3 n, std::size_t idx) {
                                parsedPrim.attributes[idx].normal = glm::vec3(n.x(), -n.y(), n.z());
                            });
                    }

                    // --- TANGENTS ---
                    auto tangentIt = prim.findAttribute("TANGENT");
                    if (tangentIt != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset,
                            asset.accessors[tangentIt->accessorIndex],
                            [&](fastgltf::math::fvec4 t, std::size_t idx) {
                                parsedPrim.attributes[idx].tangent = glm::vec4(t.x(), -t.y(), t.z(), t.w());
                            });
                    }

                    // --- VERTEX COLORS ---
                    auto colorIt = prim.findAttribute("COLOR_0");
                    if (colorIt != prim.attributes.end()) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset,
                            asset.accessors[colorIt->accessorIndex],
                            [&](fastgltf::math::fvec4 c, std::size_t idx) {
                                parsedPrim.attributes[idx].color = glm::vec3(c.x(), c.y(), c.z());
                            });
                    }

                    // --- INDICES ---
                    auto &indexAccessor = asset.accessors[prim.indicesAccessor.value()];
                    parsedPrim.indices.resize(indexAccessor.count);

                    fastgltf::iterateAccessorWithIndex<uint32_t>(
                        asset, indexAccessor, [&](uint32_t idxValue, std::size_t idx) {
                            parsedPrim.indices[idx] = idxValue;
                        });

                    for (size_t i = 0; i < parsedPrim.indices.size(); i += 3) {
                        std::swap(parsedPrim.indices[i + 1], parsedPrim.indices[i + 2]);
                    }

                    parsedNode.primitives.push_back(std::move(parsedPrim));
                }
            }
        }
    }

    std::vector<GameObject> LoaderGLTF::finalize(ParsedGLTF &parsedData,
                                                 Device &device,
                                                 Model &megaBuffer,
                                                 ResourceHeap &resourceHeap,
                                                 std::deque<Texture2D> &outTextures)
    {
        if (!parsedData.success) {
            return {};
        }

        std::vector<uint32_t> imageIndexToHeapSlot(parsedData.images.size(), ResourceHeap::INVALID_HANDLE);

        for (size_t i = 0; i < parsedData.images.size(); ++i) {
            auto &parsedImg = parsedData.images[i];
            bool hasData = parsedImg.isKTX2 ? (parsedImg.ktxTexPtr != nullptr) : (!parsedImg.data.empty());
            if (!parsedImg.isValid || !hasData)
                continue;

            outTextures.emplace_back();
            Texture2D &tex = outTextures.back();

            if (parsedImg.isKTX2) {
                tex.fromKTXPtr(parsedImg.ktxTexPtr, &device, resourceHeap, parsedImg.isSRGB);
            } else {
                // Fallback for standard uncompressed PNG/JPGs
                tex.fromBuffer(parsedImg.data.data(),
                               parsedImg.data.size(),
                               parsedImg.isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                               parsedImg.width,
                               parsedImg.height,
                               &device,
                               resourceHeap);
            }

            imageIndexToHeapSlot[i] = tex.heapHandle.index;
        }

        const uint32_t WHITE_SLOT = resourceHeap.getFallbackWhiteSlot();
        const uint32_t FLAT_NORMAL_SLOT = resourceHeap.getFallbackFlatNormalSlot();

        std::vector<uint32_t> materialIndexToGlobalID;
        materialIndexToGlobalID.reserve(parsedData.materials.size() + 1);

        ResourceHeap::MaterialData defaultMat {};
        defaultMat.albedoIndex = WHITE_SLOT;
        defaultMat.normalIndex = FLAT_NORMAL_SLOT;
        defaultMat.roughnessMetallicIndex = WHITE_SLOT;
        defaultMat.emissiveIndex = WHITE_SLOT;
        defaultMat.albedoFactor = glm::vec4(1.0f);
        defaultMat.roughnessFactor = 1.0f;
        defaultMat.metallicFactor = 0.0f;
        materialIndexToGlobalID.push_back(resourceHeap.pushMaterial(defaultMat));

        auto resolveTex = [&](uint32_t localIdx, uint32_t fallback) -> uint32_t {
            if (localIdx == ResourceHeap::INVALID_HANDLE || localIdx >= imageIndexToHeapSlot.size())
                return fallback;
            uint32_t globalSlot = imageIndexToHeapSlot[localIdx];
            return (globalSlot == ResourceHeap::INVALID_HANDLE) ? fallback : globalSlot;
        };

        for (auto &cpuMat: parsedData.materials) {
            cpuMat.albedoIndex = resolveTex(cpuMat.albedoIndex, WHITE_SLOT);
            cpuMat.normalIndex = resolveTex(cpuMat.normalIndex, FLAT_NORMAL_SLOT);
            cpuMat.roughnessMetallicIndex = resolveTex(cpuMat.roughnessMetallicIndex, WHITE_SLOT);
            cpuMat.emissiveIndex = resolveTex(cpuMat.emissiveIndex, WHITE_SLOT);
            cpuMat.occlusionIndex = resolveTex(cpuMat.occlusionIndex, WHITE_SLOT);

            materialIndexToGlobalID.push_back(resourceHeap.pushMaterial(cpuMat));
        }

        std::vector<GameObject> loadedNodes;
        loadedNodes.reserve(parsedData.nodes.size());

        for (size_t i = 0; i < parsedData.nodes.size(); ++i) {
            loadedNodes.push_back(GameObject::createGameObject());
        }

        std::vector<GameObject> extraNodes;

        for (size_t i = 0; i < parsedData.nodes.size(); ++i) {
            auto &parsedNode = parsedData.nodes[i];
            auto &obj = loadedNodes[i];

            for (auto childIdx: parsedNode.childrenIndices) {
                obj.addChild(loadedNodes[childIdx]);
            }

            obj.transform = parsedNode.transform;

            if (!parsedNode.primitives.empty()) {
                for (size_t p = 0; p < parsedNode.primitives.size(); ++p) {
                    auto &prim = parsedNode.primitives[p];
                    uint32_t globalMatID = materialIndexToGlobalID[prim.localMaterialIndex];

                    bool isMask = (resourceHeap.getMaterials()[globalMatID].flags & 1u) != 0u;
                    bool isBlend = (resourceHeap.getMaterials()[globalMatID].flags & 2u) != 0u;
                    bool isDoubleSided = (resourceHeap.getMaterials()[globalMatID].flags & 4u) != 0u;

                    AlphaMode mode = AlphaMode::Opaque;
                    if (isBlend)
                        mode = AlphaMode::Blend;
                    else if (isMask)
                        mode = AlphaMode::Mask;

                    std::vector<Model::VertexPosition> positions = prim.positions;
                    std::vector<Model::VertexAttribute> attributes = prim.attributes;

                    for (auto &v: attributes)
                        v.texId = globalMatID;

                    glm::vec3 minAABB = glm::vec3(std::numeric_limits<float>::max());
                    glm::vec3 maxAABB = glm::vec3(std::numeric_limits<float>::lowest());
                    for (const auto &v: positions) {
                        minAABB = glm::min(minAABB, v.position);
                        maxAABB = glm::max(maxAABB, v.position);
                    }
                    glm::vec3 center = (minAABB + maxAABB) * 0.5f;
                    float radius = 0.0f;
                    for (const auto &v: positions) {
                        radius = glm::max(radius, glm::distance(center, v.position));
                    }
                    glm::vec4 boundingSphere = glm::vec4(center, radius);

                    if (p == 0) {
                        obj.subMesh = megaBuffer.registerMesh(positions, attributes, prim.indices);
                        obj.alphaMode = mode;
                        obj.doubleSided = isDoubleSided;
                        obj.boundingSphere = boundingSphere;
                    } else {
                        GameObject child = GameObject::createGameObject();

                        child.transform = TransformComponent {};

                        child.subMesh = megaBuffer.registerMesh(positions, attributes, prim.indices);
                        child.alphaMode = mode;
                        child.doubleSided = isDoubleSided;
                        child.boundingSphere = boundingSphere;

                        obj.addChild(child);

                        extraNodes.push_back(std::move(child));
                    }
                }
            }
        }

        for (auto &child: extraNodes) {
            loadedNodes.push_back(std::move(child));
        }

        resourceHeap.flushPendingUpdates();

        return loadedNodes;
    }

    fastgltf::Asset LoaderGLTF::loadAsset(const std::filesystem::path &filePath)
    {
        static constexpr auto supportedExtensions = fastgltf::Extensions::KHR_mesh_quantization |
            fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_variants |
            fastgltf::Extensions::KHR_texture_basisu;

        fastgltf::Parser parser(supportedExtensions);

        constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
            fastgltf::Options::LoadExternalBuffers |
            // fastgltf::Options::LoadExternalImages   |
            fastgltf::Options::GenerateMeshIndices;

        auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
        if (!bool(gltfFile)) {
            std::cerr << "LoaderGLTF: Failed to open: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        auto asset = parser.loadGltf(gltfFile.get(), filePath.parent_path(), gltfOptions);
        if (asset.error() != fastgltf::Error::None) {
            std::cerr << "LoaderGLTF: Parse error: " << fastgltf::getErrorMessage(asset.error()) << '\n';
            throw std::runtime_error("Failed to load " + filePath.string());
        }

        return std::move(asset.get());
    }
} // namespace Engine
