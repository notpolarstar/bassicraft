#pragma once

#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Cube.hpp"
#include "Vertex.hpp"

class Chunk
{
private:
public:
    VkBuffer vk_vertex_buffer;
    VkDeviceMemory vk_vertex_buffer_memory;
    VkBuffer vk_index_buffer;
    VkDeviceMemory vk_index_buffer_memory;

    glm::vec2 pos;
    std::array<std::array<std::array<Cube, 16>, 100>, 16> blocks{{glm::ivec3(0, 0, 0), 0}};

    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    // void create_vertex_buffer();
    // void create_index_buffer();

    // uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
    // void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory);
    // void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

    Chunk(glm::vec2 pos);
    ~Chunk();
};
