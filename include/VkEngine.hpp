#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"
#include "Vertex.hpp"
#include "Cube.hpp"
#include "Player.hpp"
#include "Chunk.hpp"

class VkEngine
{
private:
    vkb::Instance instance;
    vkb::Swapchain swapchain;
    vkb::Device device;
    vkb::DispatchTable dispatch_table;

    VkSurfaceKHR vk_surface_khr;

    VkQueue vk_graphics_queue;
    VkQueue vk_present_queue;

    VkRenderPass vk_render_pass;

    std::vector<VkImage> vk_images;
    std::vector<VkImageView> vk_image_views;
    std::vector<VkFramebuffer> vk_framebuffers;

    VkPipelineLayout vk_pipeline_layout;
    VkPipeline vk_graphics_pipeline;

    VkCommandPool vk_command_pool;
    std::vector<VkCommandBuffer> vk_command_buffers;

    std::vector<VkSemaphore> vk_image_available_semaphores;
    std::vector<VkSemaphore> vk_render_finished_semaphores;
    std::vector<VkFence> vk_in_flight_fences;
    //std::vector<VkFence> vk_images_in_flight;
    uint32_t current_frame = 0;

    // VkBuffer vk_vertex_buffer;
    // VkDeviceMemory vk_vertex_buffer_memory;
    // VkBuffer vk_index_buffer;
    // VkDeviceMemory vk_index_buffer_memory;

    bool framebuffer_resized = false;

    std::vector<Vertex> vertices = {
        {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

        {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 15, 14, 14, 13, 12,
        16, 17, 18, 18, 19, 16,
        20, 23, 22, 22, 21, 20
    };

    VkBuffer vk_uniform_buffer;
    VkDeviceMemory vk_uniform_buffer_memory;
    std::vector<VkBuffer> vk_uniform_buffers;
    std::vector<VkDeviceMemory> vk_uniform_buffers_memory;
    std::vector<void *> vk_uniform_buffers_mapped;

    VkDescriptorSetLayout vk_descriptor_set_layout;
    VkDescriptorPool vk_descriptor_pool;
    std::vector<VkDescriptorSet> vk_descriptor_sets;

    VkImage vk_texture_image;
    VkDeviceMemory vk_texture_image_memory;
    VkImageView vk_texture_image_view;
    VkSampler vk_texture_sampler;

    VkImage vk_depth_image;
    VkDeviceMemory vk_depth_image_memory;
    VkImageView vk_depth_image_view;

    const int MAX_FRAMES_IN_FLIGHT = 2;

    int width = 800;
    int height = 600;
public:
    GLFWwindow *window;

    void create_swapchain();
    void create_render_pass();
    void create_framebuffers();
    void create_vertex_buffer();
    void create_index_buffer();
    void create_graphics_pipeline();
    void create_command_pool();
    void create_command_buffers();
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index, std::vector<Chunk>& world);
    void create_uniform_buffers();
    void update_uniform_buffer(uint32_t current_image, Camera& camera);
    void create_descriptor_set_layout();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void create_sync_objects();
    void create_texture_image();
    void create_texture_image_view();
    VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
    void create_texture_sampler();
    void create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory);
    void transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
    void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void create_depth_resources();
    VkFormat find_depth_format();
    VkFormat find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool has_stencil_component(VkFormat format);

    void get_queues();
    void recreate_swapchain();
    void draw_frame(Player& player, std::vector<Chunk>& world);

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
    void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory);
    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
    VkCommandBuffer begin_single_time_commands();
    void end_single_time_commands(VkCommandBuffer command_buffer);
    void recreate_vertex_array();
    void add_cube_to_vertices(Cube& cube, int up, int down, int left, int right, int front, int back, glm::vec2 chunk_pos, Chunk &chunk);
    void remove_cube_from_vertices(glm::vec3 pos, glm::vec2 chunk_pos, Chunk& chunk, Cube& cube);
    void remove_face(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, int i, int indice);
    void free_buffers_chunk(Chunk& chunk);
    void wait_idle();

    void create_vertex_buffer_chunk(Chunk& chunk);
    void create_index_buffer_chunk(Chunk& chunk);
    void recreate_buffers_chunk(Chunk& chunk);

    VkEngine();
    ~VkEngine();
};
