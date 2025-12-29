#include <vulkan/vulkan.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void vkCheck(VkResult r, const char* what) {
    if (r != VK_SUCCESS) {
        std::cerr << what << " failed with VkResult=" << r << "\n";
        throw std::runtime_error(what);
    }
}

static std::vector<uint32_t> readFileU32(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error(std::string("Cannot open file: ") + path);
    auto size = file.tellg();
    if (size <= 0 || (size % 4) != 0) throw std::runtime_error("SPIR-V file size invalid");
    std::vector<uint32_t> data(static_cast<size_t>(size) / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static uint32_t findMemoryTypeIndex(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && ((mp.memoryTypes[i].propertyFlags & props) == props)) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found");
}

int main() {
    // --- Parameters for the clear
    const uint32_t N = 1024;         // number of uints
    const uint32_t value = 0xDEADBEEFu;

    // --- Instance
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "vk_clear";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "none";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    vkCheck(vkCreateInstance(&ici, nullptr, &instance), "vkCreateInstance");

    // --- Pick a physical device
    uint32_t pdCount = 0;
    vkCheck(vkEnumeratePhysicalDevices(instance, &pdCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (pdCount == 0) throw std::runtime_error("No Vulkan physical devices found");

    std::vector<VkPhysicalDevice> pds(pdCount);
    vkCheck(vkEnumeratePhysicalDevices(instance, &pdCount, pds.data()), "vkEnumeratePhysicalDevices(list)");

    VkPhysicalDevice phys = pds[0];

    // --- Find a compute queue family
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qfCount, qfps.data());

    uint32_t computeQF = UINT32_MAX;
    for (uint32_t i = 0; i < qfCount; i++) {
        if (qfps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeQF = i;
            break;
        }
    }
    if (computeQF == UINT32_MAX) throw std::runtime_error("No compute queue family found");

    // --- Device + queue
    float prio = 1.0f;
    VkDeviceQueueCreateInfo dqci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    dqci.queueFamilyIndex = computeQF;
    dqci.queueCount = 1;
    dqci.pQueuePriorities = &prio;

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqci;

    VkDevice device = VK_NULL_HANDLE;
    vkCheck(vkCreateDevice(phys, &dci, nullptr, &device), "vkCreateDevice");

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeQF, 0, &queue);

    // --- Create output buffer (host-visible for easy readback)
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = VkDeviceSize(N) * sizeof(uint32_t);
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; // used by compute shader
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer outBuf = VK_NULL_HANDLE;
    vkCheck(vkCreateBuffer(device, &bci, nullptr, &outBuf), "vkCreateBuffer");

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device, outBuf, &mr);

    uint32_t memType = findMemoryTypeIndex(
        phys,
        mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = memType;

    VkDeviceMemory outMem = VK_NULL_HANDLE;
    vkCheck(vkAllocateMemory(device, &mai, nullptr, &outMem), "vkAllocateMemory");
    vkCheck(vkBindBufferMemory(device, outBuf, outMem, 0), "vkBindBufferMemory");

    // Initialize buffer to something else so you can see the clear worked
    void* mapped = nullptr;
    vkCheck(vkMapMemory(device, outMem, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory");
    std::memset(mapped, 0xAB, size_t(bci.size));
    vkUnmapMemory(device, outMem);

    // --- Descriptor set layout: binding 0 = storage buffer
    VkDescriptorSetLayoutBinding bind0{};
    bind0.binding = 0;
    bind0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bind0.descriptorCount = 1;
    bind0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dsci.bindingCount = 1;
    dsci.pBindings = &bind0;

    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorSetLayout(device, &dsci, nullptr, &dsl), "vkCreateDescriptorSetLayout");

    // --- Pipeline layout with push constants
    struct PushConstants { uint32_t N; uint32_t value; };

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vkCheck(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout), "vkCreatePipelineLayout");

    // --- Shader module
    auto spirv = readFileU32("clear.spv");

    VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    smci.codeSize = spirv.size() * sizeof(uint32_t);
    smci.pCode = spirv.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCheck(vkCreateShaderModule(device, &smci, nullptr, &shaderModule), "vkCreateShaderModule");

    // --- Compute pipeline
    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shaderModule;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpci{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    cpci.stage = stage;
    cpci.layout = pipelineLayout;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCheck(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline), "vkCreateComputePipelines");

    // --- Descriptor pool + set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;

    VkDescriptorPool dpool = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorPool(device, &dpci, nullptr, &dpool), "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dsai.descriptorPool = dpool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dsl;

    VkDescriptorSet dset = VK_NULL_HANDLE;
    vkCheck(vkAllocateDescriptorSets(device, &dsai, &dset), "vkAllocateDescriptorSets");

    VkDescriptorBufferInfo dbi{};
    dbi.buffer = outBuf;
    dbi.offset = 0;
    dbi.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet = dset;
    wds.dstBinding = 0;
    wds.dstArrayElement = 0;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds.descriptorCount = 1;
    wds.pBufferInfo = &dbi;

    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    // --- Command pool + command buffer
    VkCommandPoolCreateInfo cpci2{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci2.queueFamilyIndex = computeQF;
    cpci2.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    vkCheck(vkCreateCommandPool(device, &cpci2, nullptr, &cmdPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkCheck(vkAllocateCommandBuffers(device, &cbai, &cmd), "vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo cbbi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkCheck(vkBeginCommandBuffer(cmd, &cbbi), "vkBeginCommandBuffer");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &dset, 0, nullptr);

    PushConstants pc{ N, value };
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &pc);

    // local_size_x = 256 => number of workgroups = ceil(N / 256)
    uint32_t groupCountX = (N + 256u - 1u) / 256u;
    vkCmdDispatch(cmd, groupCountX, 1, 1);

    // Barrier: make shader writes visible to host reads
    VkBufferMemoryBarrier bmb{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    bmb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bmb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.buffer = outBuf;
    bmb.offset = 0;
    bmb.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0,
        0, nullptr,
        1, &bmb,
        0, nullptr
    );

    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // --- Submit and wait
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    vkCheck(vkCreateFence(device, &fci, nullptr, &fence), "vkCreateFence");

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkCheck(vkQueueSubmit(queue, 1, &si, fence), "vkQueueSubmit");
    vkCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

    // --- Read back results
    vkCheck(vkMapMemory(device, outMem, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory(readback)");
    const uint32_t* u = reinterpret_cast<const uint32_t*>(mapped);

    std::cout << "First 8 values:\n";
    for (int i = 0; i < 8; i++) {
        std::cout << "  out[" << i << "] = 0x" << std::hex << u[i] << std::dec << "\n";
    }

    // Simple check
    for (uint32_t i = 0; i < N; i++) {
        if (u[i] != value) {
            std::cerr << "Mismatch at " << i << ": got " << u[i] << ", expected " << value << "\n";
            std::abort();
        }
    }

    vkUnmapMemory(device, outMem);
    std::cout << "OK: buffer cleared.\n";

    // --- Cleanup
    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    vkDestroyDescriptorPool(device, dpool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, dsl, nullptr);
    vkDestroyBuffer(device, outBuf, nullptr);
    vkFreeMemory(device, outMem, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
