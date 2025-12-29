#ifndef MELKIOR_ENGINE_HPP
#define MELKIOR_ENGINE_HPP

#include <string_view>
#include <variant>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace melkior::engine {

// constants
constexpr VkBufferUsageFlags USAGE_TRANSFER_SRC =
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
constexpr VkBufferUsageFlags USAGE_TRANSFER_DST =
    VK_BUFFER_USAGE_TRANSFER_DST_BIT;
constexpr VkBufferUsageFlags USAGE_TRANSFER_SRC_DST =
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

constexpr VkMemoryPropertyFlags MEM_CPU_VISIBLE_COHERENT =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

constexpr VkMemoryPropertyFlags MEM_GPU_ONLY =
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

// utility data structs
struct EngineState {
  bool _ready;
  VkResult _result;
};

template <typename T> struct Result {
  std::variant<T, VkResult> _result;

  bool isValid() { return !std::holds_alternative<VkResult>(_result); }

  VkResult getError() { return std::get<VkResult>(_result); }

  T getValue() { return std::get<T>(_result); }
};

struct Buffer {
  VkBuffer _buffer = VK_NULL_HANDLE;
  VkDeviceMemory _memory = VK_NULL_HANDLE;
  VkDeviceSize _size = 0;
};

// the engine
class Engine {
public:
  Engine(std::string_view name);
  ~Engine();

  EngineState getEngineState() const;
  std::string version() const;
  std::string vendorName() const;
  void printDeviceInfo() const;
  void printLimits() const;
  void printQueueFamilies() const;
  void printMemoryTypes();

  Result<Buffer> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memProps);
  void destroyBuffer(Buffer buffer);

  // void fillAndCopyPractice();

private:
  VkInstance m_instance;
  VkPhysicalDevice m_physicalDevice;
  VkDevice m_device;
  VkQueue m_queue;
  uint32_t m_computeFamilyIndex;
  VkResult m_result;
  bool m_success;
};

} // namespace melkior::engine

#endif