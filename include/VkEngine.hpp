#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"
#include "Vertex.hpp"
#include "Cube.hpp"
#include "Player.hpp"
#include "Chunk.hpp"
#include "TextureDataStruct.hpp"
#include "Particle.hpp"
#include "InstanceData.hpp"

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
    VkPipelineLayout vk_particles_pipeline_layout;
    VkPipeline vk_particles_graphics_pipeline;

    VkCommandPool vk_command_pool;
    std::vector<VkCommandBuffer> vk_command_buffers_blocks;

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

    std::vector<Particle> particles{};
    std::vector<Vertex> particles_vertices{};
    std::vector<uint32_t> particles_indices{};
    std::vector<ParticleInstanceData> particles_instance_data{};
    VkBuffer vk_particles_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_particles_vertex_buffer_memory = VK_NULL_HANDLE;
    VkBuffer vk_particles_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_particles_index_buffer_memory = VK_NULL_HANDLE;
    VkBuffer vk_particles_instance_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vk_particles_instance_buffer_memory = VK_NULL_HANDLE;


    VkBuffer vk_uniform_buffer;
    VkDeviceMemory vk_uniform_buffer_memory;
    std::vector<VkBuffer> vk_uniform_buffers_blocks;
    std::vector<VkBuffer> vk_uniform_buffers_particles;
    std::vector<VkDeviceMemory> vk_uniform_buffers_memory;
    std::vector<void *> vk_uniform_buffers_mapped;

    VkDescriptorSetLayout vk_descriptor_set_layout;
    VkDescriptorPool vk_descriptor_pool;
    std::vector<VkDescriptorSet> vk_descriptor_sets_chunks;
    std::vector<VkDescriptorSet> vk_descriptor_sets_particles;

    VkImage vk_texture_image;
    VkDeviceMemory vk_texture_image_memory;
    VkImageView vk_texture_image_view;
    VkSampler vk_texture_sampler;

    VkImage vk_depth_image;
    VkDeviceMemory vk_depth_image_memory;
    VkImageView vk_depth_image_view;

    const int MAX_FRAMES_IN_FLIGHT = 2;

public:
    int width = 1920;
    int height = 1080;
    GLFWwindow *window;
    float frame_render_duration = 0.0f;

    const int MAX_PARTICLES = 200;

    void create_swapchain();
    void create_render_pass();
    void create_framebuffers();
    void create_vertex_buffer();
    void create_index_buffer();
    void create_all_graphics_pipelines();
    void create_graphics_pipeline(VkPipeline& pipeline, VkPipelineLayout& pipeline_layout, const char* vert_path, const char* frag_path, const char* geom_path = nullptr);
    void create_command_pool();
    void create_command_buffers();
    void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index, std::vector<Chunk>& world, Player& player);
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

    void create_inventory();
    bool LoadTextureFromFile(const char* filename, MyTextureData* tex_data);
    void RemoveTexture(MyTextureData* tex_data);

    void create_particles(glm::vec3 pos, uint16_t type, Player& player);
    void create_particles_buffers();
    void update_particles();
    void create_particles_instance_buffers();
    void create_graphics_pipeline_particles(VkPipeline& pipeline, VkPipelineLayout& pipeline_layout, const char* vert_path, const char* frag_path, const char* geom_path = nullptr);

    VkEngine();
    ~VkEngine();
};
