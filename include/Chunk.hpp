#pragma once

#include <vector>
#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Cube.hpp"
#include "Vertex.hpp"
#include "FastNoiseLite.hpp"

class Chunk
{
private:
public:
    VkBuffer vk_vertex_buffer;
    VkDeviceMemory vk_vertex_buffer_memory;
    VkBuffer vk_index_buffer;
    VkDeviceMemory vk_index_buffer_memory;

    VkDeviceSize vertex_buffer_size;
    VkDeviceSize index_buffer_size;

    glm::vec2 pos;
    std::array<std::array<std::array<Cube, 16>, 100>, 16> blocks{{glm::ivec3(0, 0, 0), 0}};

    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};

    bool should_be_deleted = false;

    Chunk* left = nullptr;
    Chunk* right = nullptr;
    Chunk* front = nullptr;
    Chunk* back = nullptr;

    void put_tree(glm::ivec3 pos);

    Chunk(glm::vec2 pos, FastNoiseLite& noise, FastNoiseLite& biome_noise);
    ~Chunk();
};
