#include "engine.hpp"
#include <cstring>
#include <iostream>
#include <vulkan/vulkan.h>

int main() {
  melkior::engine::Engine myEngine("step2_engine");

  myEngine.printDeviceInfo();
  myEngine.printMemoryTypes();

  const VkDeviceSize kSize =
      1024 * 4; // 4 KB, must be multiple of 4 for easy viewing

  auto resultA = myEngine.createBuffer(
      kSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (!resultA.isValid()) {
    std::cout << "Buffer A not allocated!" << std::endl;
    return 1;
  }

  auto resultB =
      myEngine.createBuffer(kSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (!resultB.isValid()) {
    std::cout << "Buffer B not allocated!" << std::endl;
    return 1;
  }

  std::cout << "\nSuccessfull buffer creation!\n";

  auto bufferA = resultA.getValue();
  auto bufferB = resultB.getValue();

  myEngine.destroyBuffer(bufferA);
  myEngine.destroyBuffer(bufferB);

  std::cout << "Successfull buffer deletion!\n";

  return 0;
}
