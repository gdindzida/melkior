#include "../include/engine.hpp"
#include <cstdint>
#include <variant>
#include <vulkan/vulkan.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace melkior::engine {
namespace {

const std::string g_engineName = "Melkior Engine";

std::string versionToString(uint32_t v) {
  return std::to_string(VK_VERSION_MAJOR(v)) + "." +
         std::to_string(VK_VERSION_MINOR(v)) + "." +
         std::to_string(VK_VERSION_PATCH(v));
}

int findGraphicsQueueFamily(VkPhysicalDevice phys) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
  std::vector<VkQueueFamilyProperties> q(count);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, q.data());
  for (uint32_t i = 0; i < count; i++) {
    if (q[i].queueCount > 0 && (q[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
      return (int)i;
  }
  return -1;
}

template <typename T>
Result<uint32_t> findMemoryTypeIndex(VkPhysicalDevice phys, uint32_t typeBits,
                                     VkMemoryPropertyFlags props) {
  VkPhysicalDeviceMemoryProperties mp{};
  vkGetPhysicalDeviceMemoryProperties(phys, &mp);

  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    if ((typeBits & (1u << i)) &&
        (mp.memoryTypes[i].propertyFlags & props) == props)
      return {i};
  }
  return {VK_ERROR_MEMORY_MAP_FAILED};
}

std::string memoryPropertyFlagsToString(VkMemoryPropertyFlags flags) {
  std::string s;
  if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
    s += "DEVICE_LOCAL ";
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    s += "HOST_VISIBLE ";
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
    s += "HOST_COHERENT ";
  }
  if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
    s += "HOST_CACHED ";
  }
  if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
    s += "LAZILY_ALLOCATED ";
  }
  if (s.empty()) {
    s = "NONE";
  }
  return s;
}

// Helper: create buffer + allocate/bind memory

} // namespace

Engine::Engine(std::string_view name) {
  m_success = true;
  m_result = VK_SUCCESS;

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = name.data();
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = g_engineName.data();
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_2;

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &appInfo;

  m_instance = VK_NULL_HANDLE;
  m_result = vkCreateInstance(&ci, nullptr, &m_instance);
  if (m_result != VK_SUCCESS) {
    // std::cerr << "vkCreateInstance failed: " << r << "\n";
    m_success = false;
    return;
  }

  // ---- Enumerate physical devices ----
  uint32_t gpuCount = 0;
  m_result = vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr);
  if (m_result != VK_SUCCESS || gpuCount == 0) {
    // std::cerr << "No Vulkan physical devices found (r=" << r
    //           << ", count=" << gpuCount << ")\n";
    m_success = false;
    return;
  }

  std::vector<VkPhysicalDevice> gpus(gpuCount);
  vkEnumeratePhysicalDevices(m_instance, &gpuCount, gpus.data());
  // std::cout << "Found " << gpuCount << " Vulkan physical device(s)\n\n";

  size_t physicalDeviceIndex = 0;
  for (size_t currentPhysDeviceIndex = 0; currentPhysDeviceIndex < gpuCount;
       ++currentPhysDeviceIndex) {
    const auto &gpu = gpus[currentPhysDeviceIndex];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gpu, &props);
    if (0x10DE == props.vendorID) {
      physicalDeviceIndex = currentPhysDeviceIndex;
    }
  }
  m_physicalDevice = gpus[physicalDeviceIndex];

  // ---- Logical device (VkDevice) ----
  int m_computeFamilyIndex = findGraphicsQueueFamily(m_physicalDevice);
  if (m_computeFamilyIndex < 0) {
    // std::cerr << "No compute queue family found on chosen device.\n";
    m_result = VK_ERROR_UNKNOWN;
    m_success = false;
    return;
  }

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = (uint32_t)m_computeFamilyIndex;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  // Enabled features
  VkPhysicalDeviceFeatures enabledFeatures{};

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.pEnabledFeatures = &enabledFeatures;

  m_device = VK_NULL_HANDLE;
  m_result = vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device);
  if (m_result != VK_SUCCESS) {
    // std::cerr << "vkCreateDevice failed: " << r << "\n";
    m_success = false;
    return;
  }

  m_queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(m_device, (uint32_t)m_computeFamilyIndex, 0, &m_queue);
}

Engine::~Engine() {
  vkDestroyDevice(m_device, nullptr);
  vkDestroyInstance(m_instance, nullptr);
}

EngineState Engine::getEngineState() const { return {m_success, m_result}; }

std::string Engine::version() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
  return versionToString(props.apiVersion);
}

std::string Engine::vendorName() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
  // Not exhaustive; common ones you might see.
  switch (props.vendorID) {
  case 0x10DE:
    return "NVIDIA";
  case 0x1002:
    return "AMD";
  case 0x8086:
    return "Intel";
  case 0x13B5:
    return "Arm (Mali)";
  case 0x5143:
    return "Qualcomm (Adreno)";
  case 0x106B:
    return "Apple";
  case 0x14E4:
    return "Broadcom";
  default:
    return "Unknown";
  }
}

void Engine::printDeviceInfo() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
  std::cout << "  deviceName:   " << props.deviceName << "\n";
  std::cout << "  vendorID:     0x" << std::hex << props.vendorID << std::dec
            << " (" << vendorName() << ")\n";
  std::cout << "  deviceID:     0x" << std::hex << props.deviceID << std::dec
            << "\n";
  std::cout << "  deviceType:   " << props.deviceType
            << " (1=integrated,2=discrete,3=virtual,4=cpu)\n";
  std::cout << "  apiVersion:   " << versionToString(props.apiVersion) << "\n";
  std::cout << "  driverVersion:" << props.driverVersion << "\n\n";
}

void Engine::printLimits() const {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(m_physicalDevice, &props);

  std::cout << "  Limits:\n";
  std::cout << "    maxComputeWorkGroupInvocations: "
            << props.limits.maxComputeWorkGroupInvocations << "\n";
  std::cout << "    maxComputeWorkGroupSize:        ["
            << props.limits.maxComputeWorkGroupSize[0] << ", "
            << props.limits.maxComputeWorkGroupSize[1] << ", "
            << props.limits.maxComputeWorkGroupSize[2] << "]\n";
  std::cout << "    maxComputeWorkGroupCount:       ["
            << props.limits.maxComputeWorkGroupCount[0] << ", "
            << props.limits.maxComputeWorkGroupCount[1] << ", "
            << props.limits.maxComputeWorkGroupCount[2] << "]\n";
  std::cout << "    maxComputeSharedMemorySize:     "
            << props.limits.maxComputeSharedMemorySize << " bytes\n";

  std::cout << "    maxPushConstantsSize:           "
            << props.limits.maxPushConstantsSize << " bytes\n";
  std::cout << "    maxBoundDescriptorSets:         "
            << props.limits.maxBoundDescriptorSets << "\n";
  std::cout << "    maxPerStageDescriptorSamplers:  "
            << props.limits.maxPerStageDescriptorSamplers << "\n";
  std::cout << "    maxPerStageDescriptorUniformBuffers: "
            << props.limits.maxPerStageDescriptorUniformBuffers << "\n";
  std::cout << "    maxPerStageDescriptorStorageBuffers: "
            << props.limits.maxPerStageDescriptorStorageBuffers << "\n";
  std::cout << "    maxPerStageResources:           "
            << props.limits.maxPerStageResources << "\n";

  std::cout << "    maxImageDimension2D:            "
            << props.limits.maxImageDimension2D << "\n";
  std::cout << "    maxSamplerAnisotropy:           "
            << props.limits.maxSamplerAnisotropy << "\n";
}

void Engine::printQueueFamilies() const {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);
  std::vector<VkQueueFamilyProperties> q(count);
  vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, q.data());

  std::cout << "  Queue families (" << count << "):\n";
  for (uint32_t i = 0; i < count; i++) {
    std::cout << "    [" << i << "] queues=" << q[i].queueCount << " flags=";
    if (q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      std::cout << "G";
    if (q[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
      std::cout << "C";
    if (q[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
      std::cout << "T";
    if (q[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
      std::cout << "S";
    std::cout << "\n";
  }
}

Result<Buffer> Engine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                    VkMemoryPropertyFlags memProps) {
  Buffer out{};
  out._size = size;

  VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bci.size = size;
  bci.usage = usage;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  auto result = vkCreateBuffer(m_device, &bci, nullptr, &out._buffer);
  if (result != VK_SUCCESS) {
    return {result};
  }

  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(m_device, out._buffer, &req);

  auto memoryTypeIndex = findMemoryTypeIndex<uint32_t>(
      m_physicalDevice, req.memoryTypeBits, memProps);

  if (!memoryTypeIndex.isValid()) {
    return {memoryTypeIndex.getError()};
  }

  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = memoryTypeIndex.getValue();

  result = vkAllocateMemory(m_device, &mai, nullptr, &out._memory);
  if (result != VK_SUCCESS) {
    return {result};
  }

  result = vkBindBufferMemory(m_device, out._buffer, out._memory, 0);
  if (result != VK_SUCCESS) {
    return {result};
  }

  return {out};
}

void Engine::destroyBuffer(Buffer buffer) {
  vkDestroyBuffer(m_device, buffer._buffer, nullptr);
  vkFreeMemory(m_device, buffer._memory, nullptr);
}

void Engine::printMemoryTypes() {
  VkPhysicalDeviceMemoryProperties mp{};
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mp);

  std::cout << "=== Vulkan Memory Heaps ===\n";
  for (uint32_t i = 0; i < mp.memoryHeapCount; i++) {
    const auto &heap = mp.memoryHeaps[i];
    std::cout << "Heap " << i << " | Size: " << (heap.size / (1024 * 1024))
              << " MB"
              << " | Flags: "
              << ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                      ? "DEVICE_LOCAL"
                      : "NONE")
              << "\n";
  }

  std::cout << "\n=== Vulkan Memory Types ===\n";
  for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
    const auto &type = mp.memoryTypes[i];
    std::cout << "Type " << std::setw(2) << i << " | Heap " << type.heapIndex
              << " | Flags: " << memoryPropertyFlagsToString(type.propertyFlags)
              << "\n";
  }
}

// void Engine::fillAndCopyPractice() {
//   const VkDeviceSize kSize =
//       1024 * 4; // 4 KB, must be multiple of 4 for easy viewing
//   const uint32_t fillValue = 0xDEADBEEF;

//   // For easy verification, use host visible/coherent memory (not fastest,
//   but
//   // simple)
//   auto A = CreateBuffer(m_device, m_physicalDevice, kSize,
//                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
//                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

//   auto B = CreateBuffer(m_device, m_physicalDevice, kSize,
//                         VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

//   // Command pool (resettable is convenient)
//   VkCommandPool pool = VK_NULL_HANDLE;
//   VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
//   cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//   cpci.queueFamilyIndex = m_computeFamilyIndex;

//   if (vkCreateCommandPool(m_device, &cpci, nullptr, &pool) != VK_SUCCESS)
//     throw std::runtime_error("vkCreateCommandPool failed");

//   // Allocate one primary command buffer
//   VkCommandBuffer cmd = VK_NULL_HANDLE;
//   VkCommandBufferAllocateInfo cbai{
//       VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
//   cbai.commandPool = pool;
//   cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//   cbai.commandBufferCount = 1;

//   if (vkAllocateCommandBuffers(m_device, &cbai, &cmd) != VK_SUCCESS)
//     throw std::runtime_error("vkAllocateCommandBuffers failed");

//   // Begin recording
//   VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//   cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

//   if (vkBeginCommandBuffer(cmd, &cbbi) != VK_SUCCESS)
//     throw std::runtime_error("vkBeginCommandBuffer failed");

//   // 1) Fill buffer A with a value
//   vkCmdFillBuffer(cmd, A.buf, 0, A.size, fillValue);

//   // // Barrier: make fill (transfer write) visible to subsequent copy
//   (transfer
//   // // read)
//   VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
//   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//   barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//   barrier.buffer = A.buf;

//   barrier.offset = 0;
//   barrier.size = A.size;

//   vkCmdPipelineBarrier(cmd,
//                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
//                        VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
//                        0, 0, nullptr, 1, &barrier, 0, nullptr);

//   // 2) Copy A -> B
//   VkBufferCopy region{};
//   region.srcOffset = 0;
//   region.dstOffset = 0;
//   region.size = kSize;
//   vkCmdCopyBuffer(cmd, A.buf, B.buf, 1, &region);

//   // End recording
//   if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
//     throw std::runtime_error("vkEndCommandBuffer failed");

//   // Submit + fence
//   VkFence fence = VK_NULL_HANDLE;
//   VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
//   if (vkCreateFence(m_device, &fci, nullptr, &fence) != VK_SUCCESS)
//     throw std::runtime_error("vkCreateFence failed");

//   VkCommandBufferSubmitInfo
//   cbsi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO}; cbsi.commandBuffer =
//   cmd;

//   VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
//   si.commandBufferInfoCount = 1;
//   si.pCommandBufferInfos = &cbsi;

//   if (vkQueueSubmit2(m_queue, 1, &si, fence) != VK_SUCCESS)
//     throw std::runtime_error("vkQueueSubmit2 failed");

//   vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

//   // Optional: verify B by mapping and checking a few uint32s
//   void *mapped = nullptr;
//   vkMapMemory(m_device, B.mem, 0, B.size, 0, &mapped);

//   auto *u32 = reinterpret_cast<const uint32_t *>(mapped);
//   // Check first few values
//   for (int i = 0; i < 8; i++) {
//     if (u32[i] != fillValue) {
//       vkUnmapMemory(m_device, B.mem);
//       throw std::runtime_error("Verification failed: copy didn't match
//       fill");
//     }
//   }
//   std::cout << "Successfull copy!" << std::endl;
//   vkUnmapMemory(m_device, B.mem);

//   // Cleanup
//   vkDestroyFence(m_device, fence, nullptr);
//   vkFreeCommandBuffers(m_device, pool, 1, &cmd);
//   vkDestroyCommandPool(m_device, pool, nullptr);

//   vkDestroyBuffer(m_device, A.buf, nullptr);
//   vkFreeMemory(m_device, A.mem, nullptr);
//   vkDestroyBuffer(m_device, B.buf, nullptr);
//   vkFreeMemory(m_device, B.mem, nullptr);
// }

} // namespace melkior::engine
