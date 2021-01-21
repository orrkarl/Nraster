#include "TriangleSetupAPI.h"

#include "autogenerated/GPUCodeResources.h"

namespace vkr {
namespace gpu {

TriangleSetupAPI::TriangleSetupAPI()
    : m_stageCode(VK_NULL_HANDLE)
    , m_argSetDescriptions({ VK_NULL_HANDLE, VK_NULL_HANDLE })
    , m_stageDescription(VK_NULL_HANDLE) {
}

VkResult TriangleSetupAPI::init(VkDevice dev, const VkAllocationCallbacks* allocator) {
    VkResult status = VK_SUCCESS;

    status = initStageCode(dev, allocator);
    if (status != VK_SUCCESS) {
        return status;
    }

    status = initArgSetConfigDescription(dev, allocator);
    if (status != VK_SUCCESS) {
        vkDestroyShaderModule(dev, m_stageCode, allocator);
        return status;
    }

    status = initArgSetProcessedDescription(dev, allocator);
    if (status != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(dev, m_argSetDescriptions[RASTER_CONFIG_POS.first], allocator);
        vkDestroyShaderModule(dev, m_stageCode, allocator);
        return status;
    }

    status = initStageDescription(dev, allocator);
    if (status != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(dev, m_argSetDescriptions[VERTECIES_POS.first], allocator);
        vkDestroyDescriptorSetLayout(dev, m_argSetDescriptions[RASTER_CONFIG_POS.first], allocator);
        vkDestroyShaderModule(dev, m_stageCode, allocator);
        return status;
    }

    return VK_SUCCESS;
}

void TriangleSetupAPI::destroy(VkDevice dev, const VkAllocationCallbacks* allocator) {
    vkDestroyPipelineLayout(dev, m_stageDescription, allocator);
    for (auto it = m_argSetDescriptions.rbegin(); it != m_argSetDescriptions.rend(); ++it) {
        vkDestroyDescriptorSetLayout(dev, *it, allocator);
    }
    vkDestroyShaderModule(dev, m_stageCode, allocator);
}

TriangleSetupAPI::ArgumentsLayout TriangleSetupAPI::describeArguments() {
    return m_argSetDescriptions;
}

VkComputePipelineCreateInfo TriangleSetupAPI::describeStage() {
    return { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
             nullptr,
             0,
             VkPipelineShaderStageCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                               nullptr,
                                               0,
                                               VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                                               m_stageCode,
                                               "main",
                                               nullptr },
             m_stageDescription,
             VK_NULL_HANDLE,
             0 };
}

VkWriteDescriptorSet
TriangleSetupAPI::describeRasterConfigUpdate(const TriangleSetupAPI::Arguments& args,
                                             const VkDescriptorBufferInfo& rasterConfigBuffer) {
    return { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             args[RASTER_CONFIG_POS.first],
             RASTER_CONFIG_POS.second,
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             nullptr,
             &rasterConfigBuffer,
             nullptr };
}

VkWriteDescriptorSet TriangleSetupAPI::describeVertexUpdate(const TriangleSetupAPI::Arguments& args,
                                                            const VkDescriptorBufferInfo& vertexBuffer) {
    return { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             args[VERTECIES_POS.first],
             VERTECIES_POS.second,
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             nullptr,
             &vertexBuffer,
             nullptr };
}

VkWriteDescriptorSet TriangleSetupAPI::describeColorUpdate(const TriangleSetupAPI::Arguments& args,
                                                           const VkDescriptorBufferInfo& colorBuffer) {
    return { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             args[COLORS_POS.first],
             COLORS_POS.second,
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             nullptr,
             &colorBuffer,
             nullptr };
}

VkWriteDescriptorSet
TriangleSetupAPI::describeTriangleBlocksUpdate(const TriangleSetupAPI::Arguments& args,
                                               const VkDescriptorBufferInfo& triangleBlocks) {
    return { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             args[TRIANGLE_BLOCKS_POS.first],
             TRIANGLE_BLOCKS_POS.second,
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             nullptr,
             &triangleBlocks,
             nullptr };
}

void TriangleSetupAPI::cmdUpdateVertexCount(VkCommandBuffer cmdBuffer, uint32_t vertexCount) {
    vkCmdPushConstants(cmdBuffer,
                       m_stageDescription,
                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                       VERTEX_COUNT_OFFSET,
                       VERTEX_COUNT_SIZE,
                       &vertexCount);
}

void TriangleSetupAPI::cmdUpdateMVP(VkCommandBuffer cmdBuffer, float* mvp4x4) {
    vkCmdPushConstants(cmdBuffer,
                       m_stageDescription,
                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                       MVP_OFFSET,
                       MVP_SIZE,
                       mvp4x4);
}

VkResult TriangleSetupAPI::initStageCode(VkDevice dev, const VkAllocationCallbacks* allocator) {
    VkShaderModuleCreateInfo stageCodeInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                             nullptr,
                                             0,
                                             device_side::TriangleSetup_comp_count * sizeof(uint32_t),
                                             device_side::TriangleSetup_comp };

    return vkCreateShaderModule(dev, &stageCodeInfo, allocator, &m_stageCode);
}

VkResult TriangleSetupAPI::initArgSetConfigDescription(VkDevice dev, const VkAllocationCallbacks* allocator) {
    VkDescriptorSetLayoutBinding rasterConfigBinding { RASTER_CONFIG_POS.second,
                                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                       1,
                                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                                                       nullptr };
    VkDescriptorSetLayoutCreateInfo layoutInfo {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &rasterConfigBinding
    };
    return vkCreateDescriptorSetLayout(
        dev, &layoutInfo, allocator, &m_argSetDescriptions[RASTER_CONFIG_POS.first]);
}

VkResult TriangleSetupAPI::initArgSetProcessedDescription(VkDevice dev,
                                                          const VkAllocationCallbacks* allocator) {
    std::array<VkDescriptorSetLayoutBinding, 3> processedBindings {
        VkDescriptorSetLayoutBinding { VERTECIES_POS.second,
                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       1,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr },
        VkDescriptorSetLayoutBinding { COLORS_POS.second,
                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       1,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr },
        VkDescriptorSetLayoutBinding { TRIANGLE_BLOCKS_POS.second,
                                       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       1,
                                       VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT,
                                       nullptr },
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 nullptr,
                                                 0,
                                                 processedBindings.size(),
                                                 processedBindings.data() };

    return vkCreateDescriptorSetLayout(
        dev, &layoutInfo, allocator, &m_argSetDescriptions[VERTECIES_POS.first]);
}

VkResult TriangleSetupAPI::initStageDescription(VkDevice dev, const VkAllocationCallbacks* allocator) {
    VkPushConstantRange pushArgsDesc { VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, 4 * 4 * 4 + 4 };
    VkPipelineLayoutCreateInfo pipelineDesc { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                              nullptr,
                                              0,
                                              static_cast<uint32_t>(m_argSetDescriptions.size()),
                                              m_argSetDescriptions.data(),
                                              1,
                                              &pushArgsDesc };

    return vkCreatePipelineLayout(dev, &pipelineDesc, allocator, &m_stageDescription);
}

}
}
