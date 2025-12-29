#include <vulkan/vulkan.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static std::string versionToString(uint32_t v) {
  return std::to_string(VK_VERSION_MAJOR(v)) + "." +
         std::to_string(VK_VERSION_MINOR(v)) + "." +
         std::to_string(VK_VERSION_PATCH(v));
}

static const char *vendorName(uint32_t vendorID) {
  // Not exhaustive; common ones you might see.
  switch (vendorID) {
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

static void printLimits(const VkPhysicalDeviceLimits &L) {
  std::cout << "  Limits:\n";
  std::cout << "    maxComputeWorkGroupInvocations: "
            << L.maxComputeWorkGroupInvocations << "\n";
  std::cout << "    maxComputeWorkGroupSize:        ["
            << L.maxComputeWorkGroupSize[0] << ", "
            << L.maxComputeWorkGroupSize[1] << ", "
            << L.maxComputeWorkGroupSize[2] << "]\n";
  std::cout << "    maxComputeWorkGroupCount:       ["
            << L.maxComputeWorkGroupCount[0] << ", "
            << L.maxComputeWorkGroupCount[1] << ", "
            << L.maxComputeWorkGroupCount[2] << "]\n";
  std::cout << "    maxComputeSharedMemorySize:     "
            << L.maxComputeSharedMemorySize << " bytes\n";

  std::cout << "    maxPushConstantsSize:           " << L.maxPushConstantsSize
            << " bytes\n";
  std::cout << "    maxBoundDescriptorSets:         "
            << L.maxBoundDescriptorSets << "\n";
  std::cout << "    maxPerStageDescriptorSamplers:  "
            << L.maxPerStageDescriptorSamplers << "\n";
  std::cout << "    maxPerStageDescriptorUniformBuffers: "
            << L.maxPerStageDescriptorUniformBuffers << "\n";
  std::cout << "    maxPerStageDescriptorStorageBuffers: "
            << L.maxPerStageDescriptorStorageBuffers << "\n";
  std::cout << "    maxPerStageResources:           " << L.maxPerStageResources
            << "\n";

  std::cout << "    maxImageDimension2D:            " << L.maxImageDimension2D
            << "\n";
  std::cout << "    maxSamplerAnisotropy:           " << L.maxSamplerAnisotropy
            << "\n";
}

// static void printDeviceExtensions(VkPhysicalDevice phys) {
//   uint32_t count = 0;
//   VkResult r = vkEnumerateDeviceExtensionProperties(phys, nullptr, &count,
//   nullptr); if (r != VK_SUCCESS) {
//     std::cout << "  Extensions: <failed to enumerate>\n";
//     return;
//   }
//   std::vector<VkExtensionProperties> exts(count);
//   vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());

//   std::cout << "  Supported device extensions (" << count << "):\n";
//   for (const auto& e : exts) {
//     std::cout << "    " << e.extensionName << " (spec " << e.specVersion <<
//     ")\n";
//   }
// }

static void printQueueFamilies(VkPhysicalDevice phys) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
  std::vector<VkQueueFamilyProperties> q(count);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, q.data());

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

static int findGraphicsQueueFamily(VkPhysicalDevice phys) {
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
  std::vector<VkQueueFamilyProperties> q(count);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, q.data());
  for (uint32_t i = 0; i < count; i++) {
    if (q[i].queueCount > 0 && (q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
      return (int)i;
  }
  return -1;
}

int main() {
  // ---- Instance ----
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "hello-device";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "none";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0; // safe default

  VkInstanceCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ci.pApplicationInfo = &appInfo;

  VkInstance instance = VK_NULL_HANDLE;
  VkResult r = vkCreateInstance(&ci, nullptr, &instance);
  if (r != VK_SUCCESS) {
    std::cerr << "vkCreateInstance failed: " << r << "\n";
    return 1;
  }

  // ---- Enumerate physical devices ----
  uint32_t gpuCount = 0;
  r = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
  if (r != VK_SUCCESS || gpuCount == 0) {
    std::cerr << "No Vulkan physical devices found (r=" << r
              << ", count=" << gpuCount << ")\n";
    vkDestroyInstance(instance, nullptr);
    return 1;
  }

  std::vector<VkPhysicalDevice> gpus(gpuCount);
  vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

  std::cout << "Found " << gpuCount << " Vulkan physical device(s)\n\n";

  // Pick first device for "hello device" flow, but print them all.
  VkPhysicalDevice chosen = gpus[0];

  for (uint32_t i = 0; i < gpuCount; i++) {
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceFeatures feats{};
    VkPhysicalDeviceMemoryProperties mem{};

    vkGetPhysicalDeviceProperties(gpus[i], &props);
    vkGetPhysicalDeviceFeatures(gpus[i], &feats);
    vkGetPhysicalDeviceMemoryProperties(gpus[i], &mem);

    std::cout << "== GPU [" << i << "] ==\n";
    std::cout << "  deviceName:   " << props.deviceName << "\n";
    std::cout << "  vendorID:     0x" << std::hex << props.vendorID << std::dec
              << " (" << vendorName(props.vendorID) << ")\n";
    std::cout << "  deviceID:     0x" << std::hex << props.deviceID << std::dec
              << "\n";
    std::cout << "  deviceType:   " << props.deviceType
              << " (1=integrated,2=discrete,3=virtual,4=cpu)\n";
    std::cout << "  apiVersion:   " << versionToString(props.apiVersion)
              << "\n";
    std::cout << "  driverVersion:" << props.driverVersion << "\n\n";

    printLimits(props.limits);
    std::cout << "\n";

    printQueueFamilies(gpus[i]);
    std::cout << "\n";

    // printDeviceExtensions(gpus[i]);
    std::cout << "\n";
  }

  // ---- Logical device (VkDevice) ----
  // This is where you "enable features/extensions" for actual use.
  int gfxQ = findGraphicsQueueFamily(chosen);
  if (gfxQ < 0) {
    std::cerr << "No graphics queue family found on chosen device.\n";
    vkDestroyInstance(instance, nullptr);
    return 1;
  }

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = (uint32_t)gfxQ;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  // For embedded, you'll often care about explicitly enabling only what you
  // need.
  VkPhysicalDeviceFeatures enabledFeatures{}; // keep empty for hello-world

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.pEnabledFeatures = &enabledFeatures;

  VkDevice device = VK_NULL_HANDLE;
  r = vkCreateDevice(chosen, &dci, nullptr, &device);
  if (r != VK_SUCCESS) {
    std::cerr << "vkCreateDevice failed: " << r << "\n";
    vkDestroyInstance(instance, nullptr);
    return 1;
  }

  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(device, (uint32_t)gfxQ, 0, &queue);
  std::cout << "Created VkDevice + got graphics queue family " << gfxQ << "\n";

  // Cleanup
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
  return 0;
}
