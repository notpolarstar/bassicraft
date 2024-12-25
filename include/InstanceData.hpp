#pragma once

#include <glm/glm.hpp>

struct ParticleInstanceData
{
    glm::vec3 pos;
    float size;
    uint16_t block_type;

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 1;
        bindingDescription.stride = sizeof(ParticleInstanceData);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

        attributeDescriptions[0].binding = 1;
        attributeDescriptions[0].location = 3;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(ParticleInstanceData, pos);

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 4;
        attributeDescriptions[1].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(ParticleInstanceData, size);

        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 5;
        attributeDescriptions[2].format = VK_FORMAT_R16_UINT;
        attributeDescriptions[2].offset = offsetof(ParticleInstanceData, block_type);

        return attributeDescriptions;
    }
};
