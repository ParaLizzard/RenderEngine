#include "Descriptor.h"

// std
#include <cassert>
#include <stdexcept>

namespace Engine {

// *************** Descriptor Set Layout Builder *********************

LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count) {
  assert(bindings.count(binding) == 0 && "Binding already in use");
  VkDescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = descriptorType;
  layoutBinding.descriptorCount = count;
  layoutBinding.stageFlags = stageFlags;
  bindings[binding] = layoutBinding;
  return *this;
}

  LveDescriptorSetLayout::Builder& LveDescriptorSetLayout::Builder::setBindingFlags(
    uint32_t binding,
    VkDescriptorBindingFlags flags) {
  assert(bindings.count(binding) == 1 && "Binding must exist before setting flags");
  bindingFlags[binding] = flags;
  return *this;
}

  std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const {
  return std::make_unique<LveDescriptorSetLayout>(device, bindings, bindingFlags);
}

// *************** Descriptor Set Layout *********************

LveDescriptorSetLayout::LveDescriptorSetLayout(
    Device& device,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings,
    std::unordered_map<uint32_t, VkDescriptorBindingFlags> bindingFlags)
    : device{device}, bindings{bindings} {

  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
  for (auto kv : bindings) {
    setLayoutBindings.push_back(kv.second);
  }

  std::vector<VkDescriptorBindingFlags> descriptorBindingFlags;
  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};

  bool hasBindingFlags = !bindingFlags.empty();
  if (hasBindingFlags) {
    descriptorBindingFlags.resize(setLayoutBindings.size(), 0);

    for (size_t i = 0; i < setLayoutBindings.size(); i++) {
      uint32_t binding = setLayoutBindings[i].binding;
      if (bindingFlags.count(binding)) {
        descriptorBindingFlags[i] = bindingFlags.at(binding);
      }
    }

    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
    bindingFlagsInfo.pBindingFlags = descriptorBindingFlags.data();
  }

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
  descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
  descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

  if (hasBindingFlags) {
    descriptorSetLayoutInfo.pNext = &bindingFlagsInfo;
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  }

  if (vkCreateDescriptorSetLayout(
          device.getDevice(),
          &descriptorSetLayoutInfo,
          nullptr,
          &descriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

LveDescriptorSetLayout::~LveDescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(device.getDevice(), descriptorSetLayout, nullptr);
}

// *************** Descriptor Pool Builder *********************

LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
  poolSizes.push_back({descriptorType, count});
  return *this;
}

LveDescriptorPool::Builder &LveDescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
  poolFlags = flags;
  return *this;
}
LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(uint32_t count) {
  maxSets = count;
  return *this;
}

std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const {
  return std::make_unique<LveDescriptorPool>(device, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

LveDescriptorPool::LveDescriptorPool(
    Device &device,
    uint32_t maxSets,
    VkDescriptorPoolCreateFlags poolFlags,
    const std::vector<VkDescriptorPoolSize> &poolSizes)
    : device{device} {
  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  descriptorPoolInfo.pPoolSizes = poolSizes.data();
  descriptorPoolInfo.maxSets = maxSets;
  descriptorPoolInfo.flags = poolFlags;

  if (vkCreateDescriptorPool( device.getDevice(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

LveDescriptorPool::~LveDescriptorPool() {
  vkDestroyDescriptorPool( device.getDevice(), descriptorPool, nullptr);
}

bool LveDescriptorPool::allocateDescriptor(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const {
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.pSetLayouts = &descriptorSetLayout;
  allocInfo.descriptorSetCount = 1;

  if (vkAllocateDescriptorSets( device.getDevice(), &allocInfo, &descriptor) != VK_SUCCESS) {
    return false;
  }
  return true;
}

void LveDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
  vkFreeDescriptorSets(
       device.getDevice(),
      descriptorPool,
      static_cast<uint32_t>(descriptors.size()),
      descriptors.data());
}

void LveDescriptorPool::resetPool() {
  vkResetDescriptorPool( device.getDevice(), descriptorPool, 0);
}

// *************** Descriptor Writer *********************

LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
    : setLayout{setLayout}, pool{pool} {}

LveDescriptorWriter &LveDescriptorWriter::writeBuffer(
    uint32_t binding, VkDescriptorBufferInfo *bufferInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pBufferInfo = bufferInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

LveDescriptorWriter &LveDescriptorWriter::writeImage(
    uint32_t binding, VkDescriptorImageInfo *imageInfo) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto &bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount == 1 &&
      "Binding single descriptor info, but binding expects multiple");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfo;
  write.descriptorCount = 1;

  writes.push_back(write);
  return *this;
}

  LveDescriptorWriter& LveDescriptorWriter::writeImageArray(
      uint32_t binding,
      VkDescriptorImageInfo* imageInfos,
      uint32_t count) {
  assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

  auto& bindingDescription = setLayout.bindings[binding];

  assert(
      bindingDescription.descriptorCount >= count &&
      "Trying to write more descriptors than binding can hold");

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.descriptorType = bindingDescription.descriptorType;
  write.dstBinding = binding;
  write.pImageInfo = imageInfos;
  write.descriptorCount = count;

  writes.push_back(write);
  return *this;
}

bool LveDescriptorWriter::build(VkDescriptorSet &set) {
  bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
  if (!success) {
    return false;
  }
  overwrite(set);
  return true;
}

void LveDescriptorWriter::overwrite(VkDescriptorSet &set) {
  for (auto &write : writes) {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(pool.device.getDevice(), writes.size(), writes.data(), 0, nullptr);
}

}