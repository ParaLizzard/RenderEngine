#include <gtest/gtest.h>
#include "AssetSystem/AssetHandle.h"
#include "AssetSystem/AssetMetadata.h"

using namespace Engine;

TEST(AssetHandleTest, HandlePackingAndUnpacking) {
    // Test case 1 from Step 3.1 spec:
    // Create handle with index 42, generation 3, type 1; assert bitfield extraction returns exact values.
    auto handle = AssetHandle<Mesh>::Create(42, 3, 1);

    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(handle.GetIndex(), 42u);
    EXPECT_EQ(handle.GetGeneration(), 3u);
    EXPECT_EQ(handle.GetTypeID(), 1u);
    EXPECT_EQ(handle.GetAssetType(), AssetType::Mesh);
}

TEST(AssetHandleTest, StaleHandleDetection) {
    // Test case 2 from Step 3.1 spec:
    // Increment generation; assert old handle is recognized as invalid/stale.
    AssetMetadata metadata;
    metadata.index = 42;
    metadata.generation = 3;
    metadata.type = AssetType::Mesh;

    auto oldHandle = AssetHandle<Mesh>::Create(42, 3, AssetType::Mesh);
    EXPECT_TRUE(metadata.MatchesHandle(oldHandle));
    EXPECT_FALSE(metadata.IsStale(oldHandle));
    EXPECT_TRUE(oldHandle.MatchesMetadata(metadata));
    EXPECT_FALSE(oldHandle.IsStale(metadata));

    // Simulate asset unregister / slot reuse: generation increments
    metadata.generation++; // now generation == 4

    // The old handle (gen 3) must now be recognized as stale against metadata (gen 4)
    EXPECT_FALSE(metadata.MatchesHandle(oldHandle));
    EXPECT_TRUE(metadata.IsStale(oldHandle));
    EXPECT_FALSE(oldHandle.MatchesMetadata(metadata));
    EXPECT_TRUE(oldHandle.IsStale(metadata));
    EXPECT_TRUE(oldHandle.IsStale(metadata.generation));

    // A newly allocated handle with the new generation matches
    auto newHandle = AssetHandle<Mesh>::Create(42, metadata.generation, AssetType::Mesh);
    EXPECT_TRUE(metadata.MatchesHandle(newHandle));
    EXPECT_FALSE(metadata.IsStale(newHandle));
}

TEST(AssetHandleTest, TriviallyCopyableAndSize) {
    EXPECT_EQ(sizeof(AssetHandle<Mesh>), 8u);
    EXPECT_EQ(sizeof(AssetHandle<void>), 8u);
    EXPECT_TRUE(std::is_trivially_copyable_v<AssetHandle<Mesh>>);
    EXPECT_TRUE(std::is_trivially_copyable_v<AssetHandle<void>>);
    EXPECT_TRUE(std::is_standard_layout_v<AssetHandle<Mesh>>);
}

TEST(AssetHandleTest, UntypedConversionsAndEquality) {
    auto meshHandle = AssetHandle<Mesh>::Create(10, 1, AssetType::Mesh);
    AssetHandle<void> untyped = meshHandle.AsUntyped();

    EXPECT_EQ(untyped.GetIndex(), 10u);
    EXPECT_EQ(untyped.GetGeneration(), 1u);
    EXPECT_EQ(untyped.GetTypeID(), static_cast<uint16_t>(AssetType::Mesh));
    EXPECT_EQ(untyped.GetRaw(), meshHandle.GetRaw());

    auto meshHandle2 = untyped.As<Mesh>();
    EXPECT_EQ(meshHandle, meshHandle2);
}

TEST(AssetHandleTest, MetadataLifecycleQueries) {
    AssetMetadata metadata;
    metadata.index = 5;
    metadata.generation = 1;
    metadata.type = AssetType::Texture;
    metadata.state = AssetState::Unloaded;
    metadata.filePath = "textures/brick_albedo.png";

    EXPECT_TRUE(metadata.IsValid());
    EXPECT_FALSE(metadata.IsReady());
    EXPECT_FALSE(metadata.IsLoading());

    metadata.state = AssetState::Loading;
    EXPECT_TRUE(metadata.IsLoading());
    EXPECT_FALSE(metadata.IsReady());

    metadata.state = AssetState::Ready;
    EXPECT_TRUE(metadata.IsReady());
    EXPECT_FALSE(metadata.IsLoading());
}
