#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.h>

static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("failed to open file");
  size_t size = (size_t)file.tellg();
  std::vector<char> buffer(size);
  file.seekg(0);
  file.read(buffer.data(), size);
  return buffer;
}

int main() {
  // --- Instance ---
  VkApplicationInfo app{};
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.apiVersion = VK_API_VERSION_1_1;

  const char *layers[] = {"VK_LAYER_KHRONOS_validation"};

  VkInstanceCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &app;
  ici.enabledLayerCount = 1;
  ici.ppEnabledLayerNames = layers;

  VkInstance instance;
  vkCreateInstance(&ici, nullptr, &instance);

  // --- Physical device ---
  uint32_t physCount = 1;
  vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
  assert(physCount > 0);

  std::vector<VkPhysicalDevice> phys(physCount);
  vkEnumeratePhysicalDevices(instance, &physCount, phys.data());
  VkPhysicalDevice physicalDevice = phys[0];

  // --- Queue family ---
  uint32_t qCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qCount, nullptr);
  std::vector<VkQueueFamilyProperties> qProps(qCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qCount,
                                           qProps.data());

  uint32_t computeQueueFamily = 0;
  for (uint32_t i = 0; i < qCount; i++) {
    if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      computeQueueFamily = i;
      break;
    }
  }

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = computeQueueFamily;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  VkPhysicalDeviceFeatures features{};
  vkGetPhysicalDeviceFeatures(physicalDevice, &features);

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.pEnabledFeatures = &features;

  VkDevice device;
  vkCreateDevice(physicalDevice, &dci, nullptr, &device);

  // --- Load shader ---
  auto code = readFile("compute.spv");

  VkShaderModuleCreateInfo smci{};
  smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  smci.codeSize = code.size();
  smci.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shader;
  vkCreateShaderModule(device, &smci, nullptr, &shader);

  std::cout << "Vulkan compute setup OK\n";

  vkDestroyShaderModule(device, shader, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
  return 0;
}
