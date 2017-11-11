#pragma once

#include <bitset>
#include <array>

enum QueueFlags {
  Graphics,
  Compute,
  Transfer,
  Present,
};

enum QueueFlagBit {
    GraphicsBit = 1 << 0,
    ComputeBit = 1 << 1,
    TransferBit = 1 << 2,
    PresentBit = 1 << 3,
};

using QueueFlagBits = std::bitset<sizeof(QueueFlags)>;
using QueueFamilyIndices = std::array<int, sizeof(QueueFlags)>;

class VulkanInstance;
class VulkanDevice;
class VulkanSwapChain;