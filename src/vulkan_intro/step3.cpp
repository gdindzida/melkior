#include "engine.hpp"
#include <cstring>
#include <vulkan/vulkan.h>

int main() {
  melkior::engine::Engine myEngine("step2_engine");

  myEngine.printDeviceInfo();

  //   myEngine.fillAndCopyPractice();

  return 0;
}
