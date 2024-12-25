#include <iostream>
#include <stdexcept>
#include <chrono>
#include <algorithm>

#include <vulkan/vulkan.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "VkEngine.hpp"
#include "VkBootstrap.h"
#include "Utils.hpp"
#include "Vertex.hpp"
#include "UniformBufferObject.hpp"
#include "InstanceData.hpp"
#include "Particle.hpp"

VkEngine::VkEngine()
{
    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Bassicraft", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Could not create window");
    }
    if (!glfwVulkanSupported()) {
        throw std::runtime_error("Vulkan not supported");
    }
    glfwMaximizeWindow(window);
    glfwSetWindowUserPointer(window, this);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMakeContextCurrent(window);

    vkb::InstanceBuilder instance_builder;
    auto inst_ret = instance_builder.set_app_name("Vulkan Engine")
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .build();
    if (!inst_ret.has_value()) {
        throw std::runtime_error("Could not create Vulkan instance");
    }

    instance = inst_ret.value();
    
    vk_surface_khr = NULL;
    if (glfwCreateWindowSurface(instance.instance, window, nullptr, &vk_surface_khr) != VK_SUCCESS) {
        throw std::runtime_error("Could not create window surface");
    }
    
    vkb::PhysicalDeviceSelector physical_device_selector{instance};
    VkPhysicalDeviceFeatures required_features{};
    //required_features.samplerAnisotropy = VK_TRUE;
    required_features.geometryShader = VK_TRUE;
    auto phys_ret = physical_device_selector.set_surface(vk_surface_khr)
                        .set_minimum_version(1, 1)
                        .set_required_features(required_features)
                        // .require_dedicated_transfer_queue()
                        .select();
    if (!phys_ret) {
        std::cerr << "Failed to select Vulkan Physical Device. Error: " << phys_ret.error().message() << "\n";
        throw std::runtime_error("Could not find a suitable physical device");
    }
    
    vkb::PhysicalDevice physical_device = phys_ret.value();
    
    vkb::DeviceBuilder device_builder = vkb::DeviceBuilder(physical_device);
    auto dev_ret = device_builder.build();
    if (!dev_ret.has_value()) {
        throw std::runtime_error("Could not create Vulkan device");
    }
    device = dev_ret.value();
}

void VkEngine::create_swapchain()
{
    vkb::SwapchainBuilder swapchain_builder{device};
    auto swapchain_ret = swapchain_builder.use_default_format_selection()
                            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                            // To remove VSYNC, uncomment the line below and comment the line above
                            //.set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
                            .set_desired_extent(width, height)
                            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                            .build();
    if (!swapchain_ret.has_value()) {
        throw std::runtime_error("Could not create swapchain");
    }
    swapchain = swapchain_ret.value();
}

void VkEngine::get_queues()
{
    auto graphics_queue_ret = device.get_queue(vkb::QueueType::graphics);
    if (!graphics_queue_ret.has_value()) {
        throw std::runtime_error("Could not get graphics queue");
    }
    vk_graphics_queue = graphics_queue_ret.value();

    auto present_queue_ret = device.get_queue(vkb::QueueType::present);
    if (!present_queue_ret.has_value()) {
        throw std::runtime_error("Could not get present queue");
    }
    vk_present_queue = present_queue_ret.value();
}

void VkEngine::create_render_pass()
{
    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = find_depth_format();
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain.image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(device.device, &render_pass_info, nullptr, &vk_render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Could not create render pass");
    }
}

void VkEngine::create_framebuffers()
{
    vk_images = swapchain.get_images().value();
    vk_image_views = swapchain.get_image_views().value();

    vk_framebuffers.resize(vk_image_views.size());
    for (size_t i = 0; i < vk_image_views.size(); i++) {
        std::array<VkImageView, 2> attachments = {vk_image_views[i], vk_depth_image_view};

        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk_render_pass;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = swapchain.extent.width;
        framebuffer_info.height = swapchain.extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(device.device, &framebuffer_info, nullptr, &vk_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create framebuffer");
        }
    }
}

void VkEngine::create_all_graphics_pipelines()
{
    create_graphics_pipeline(vk_graphics_pipeline, vk_pipeline_layout, "shaders/blocks_vert.spv", "shaders/blocks_frag.spv");
    create_graphics_pipeline_particles(vk_particles_graphics_pipeline, vk_particles_pipeline_layout, "shaders/particles_vert.spv", "shaders/particles_frag.spv");
}

void VkEngine::create_graphics_pipeline_particles(VkPipeline& pipeline, VkPipelineLayout& pipeline_layout, const char* vert_path, const char* frag_path, const char* geom_path)
{
    auto vert_shader_code = read_file(vert_path);
    auto frag_shader_code = read_file(frag_path);
    std::vector<char> geom_shader_code;
    if (geom_path) {
        geom_shader_code = read_file(geom_path);
    }

    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code, device.device);
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code, device.device);
    VkShaderModule geom_shader_module;
    if (geom_path) {
        geom_shader_module = create_shader_module(geom_shader_code, device.device);
    }

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo geom_shader_stage_info = {};
    if (geom_path) {
        geom_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geom_shader_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geom_shader_stage_info.module = geom_shader_module;
        geom_shader_stage_info.pName = "main";
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};
    if (geom_path) {
        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info, geom_shader_stage_info};
    }

    // auto binding_description = Vertex::get_binding_description();
    // auto attribute_descriptions = Vertex::get_attribute_descriptions();
    std::array<VkVertexInputBindingDescription, 2> binding_descriptions = {Vertex::get_binding_description(), ParticleInstanceData::get_binding_description()};
    std::array<VkVertexInputAttributeDescription, 6> attribute_descriptions = {Vertex::get_attribute_descriptions()[0], Vertex::get_attribute_descriptions()[1], Vertex::get_attribute_descriptions()[2], ParticleInstanceData::get_attribute_descriptions()[0], ParticleInstanceData::get_attribute_descriptions()[1], ParticleInstanceData::get_attribute_descriptions()[2]};

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 2;
    vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    // if (geom_path) {
    //     input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    // }

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    //rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    //rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &vk_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;
    
    if (vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout");
    }
    
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = (geom_path) ? 3 : 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = vk_pipeline_layout;
    pipeline_info.renderPass = vk_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline");
    }

    vkDestroyShaderModule(device.device, frag_shader_module, nullptr);
    vkDestroyShaderModule(device.device, vert_shader_module, nullptr);
    if (geom_path) {
        vkDestroyShaderModule(device.device, geom_shader_module, nullptr);
    }
}

void VkEngine::create_graphics_pipeline(VkPipeline& pipeline, VkPipelineLayout& pipeline_layout, const char* vert_path, const char* frag_path, const char* geom_path)
{
    auto vert_shader_code = read_file(vert_path);
    auto frag_shader_code = read_file(frag_path);
    std::vector<char> geom_shader_code;
    if (geom_path) {
        geom_shader_code = read_file(geom_path);
    }

    VkShaderModule vert_shader_module = create_shader_module(vert_shader_code, device.device);
    VkShaderModule frag_shader_module = create_shader_module(frag_shader_code, device.device);
    VkShaderModule geom_shader_module;
    if (geom_path) {
        geom_shader_module = create_shader_module(geom_shader_code, device.device);
    }

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo geom_shader_stage_info = {};
    if (geom_path) {
        geom_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geom_shader_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geom_shader_stage_info.module = geom_shader_module;
        geom_shader_stage_info.pName = "main";
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};
    if (geom_path) {
        VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info, geom_shader_stage_info};
    }

    auto binding_description = Vertex::get_binding_description();
    auto attribute_descriptions = Vertex::get_attribute_descriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    // if (geom_path) {
    //     input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    // }

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    //rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    //rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment = {};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_SUBTRACT;

    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &vk_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;
    
    if (vkCreatePipelineLayout(device.device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create pipeline layout");
    }
    
    std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = (geom_path) ? 3 : 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.layout = vk_pipeline_layout;
    pipeline_info.renderPass = vk_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline");
    }

    vkDestroyShaderModule(device.device, frag_shader_module, nullptr);
    vkDestroyShaderModule(device.device, vert_shader_module, nullptr);
    if (geom_path) {
        vkDestroyShaderModule(device.device, geom_shader_module, nullptr);
    }
}

void VkEngine::create_command_pool()
{
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = device.get_queue_index(vkb::QueueType::graphics).value();

    if (vkCreateCommandPool(device.device, &pool_info, nullptr, &vk_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create command pool");
    }
}

void VkEngine::create_command_buffers()
{
    vk_command_buffers_blocks.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = vk_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = (uint32_t) MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(device.device, &alloc_info, vk_command_buffers_blocks.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate command buffers");
    }
}

void VkEngine::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index, std::vector<Chunk>& world, Player& player)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Could not begin recording command buffer");
    }

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = vk_render_pass;
    render_pass_info.framebuffer = vk_framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain.extent;

    std::array<VkClearValue, 2> clear_values = {};
    clear_values[0].color = {0.1f, 0.25f, 1.0f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};

    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);

    VkViewport viewport = {};
    viewport.width = (float) swapchain.extent.width;
    viewport.height = (float) swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // VkBuffer vertex_buffers[] = {vk_vertex_buffer};
    VkDeviceSize offsets[] = {0};

    // vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
    // vkCmdBindIndexBuffer(command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
    // vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets[current_frame], 0, nullptr);
    // vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets_chunks[current_frame], 0, nullptr);
    for (auto& chunk : world) {
        if (chunk.should_be_deleted || glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            continue;
        }
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &chunk.vk_vertex_buffer, offsets);
        vkCmdBindIndexBuffer(command_buffer, chunk.vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(chunk.indices.size()), 1, 0, 0, 0);
    }

    if (vk_particles_vertex_buffer != VK_NULL_HANDLE) {
        // UniformBufferObject old_ubo = {};
        // memcpy(&old_ubo, vk_uniform_buffers_mapped[current_frame], static_cast<size_t>(sizeof(UniformBufferObject)));


        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_particles_graphics_pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets_particles[current_frame], 0, nullptr);
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vk_particles_vertex_buffer, offsets);
        vkCmdBindIndexBuffer(command_buffer, vk_particles_index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindVertexBuffers(command_buffer, 1, 1, &vk_particles_instance_buffer, offsets);

        vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(particles_indices.size()), static_cast<uint32_t>(particles.size()), 0, 0, 0);
        //vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(particles_indices.size()), 1, 0, 0, 0);
        // int i = 0;
        // for (auto& particle : particles) {
        //     vkCmdDrawIndexed(command_buffer, 6, 1, (i > 0) ? i * 4 - 1 : 0, (i > 0) ? i * 6 - 1 : 0, 0);            
        //     i++;
        // }

        // memcpy(vk_uniform_buffers_mapped[current_frame], &old_ubo, sizeof(old_ubo));
    }

    //Render GUI
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_command_buffers_blocks[current_frame]);

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not record command buffer");
    }
}

void VkEngine::create_sync_objects()
{
    vk_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    vk_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    vk_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
    //vk_images_in_flight.resize(vk_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device.device, &semaphore_info, nullptr, &vk_image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device.device, &semaphore_info, nullptr, &vk_render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device.device, &fence_info, nullptr, &vk_in_flight_fences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create synchronization objects");
        }
    }
}

void VkEngine::recreate_swapchain()
{
    vkDeviceWaitIdle(device.device);

    vkDestroyCommandPool(device.device, vk_command_pool, nullptr);

    vkDestroyPipeline(device.device, vk_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(device.device, vk_pipeline_layout, nullptr);

    vkDestroyRenderPass(device.device, vk_render_pass, nullptr);
    for (auto framebuffer : vk_framebuffers) {
        vkDestroyFramebuffer(device.device, framebuffer, nullptr);
    }
    for (auto image_view : vk_image_views) {
        vkDestroyImageView(device.device, image_view, nullptr);
    }

    vkb::destroy_swapchain(swapchain);

    create_swapchain();
    get_queues();
    create_render_pass();
    create_framebuffers();
    create_graphics_pipeline(vk_graphics_pipeline, vk_pipeline_layout, "shaders/blocks_vert.spv", "shaders/blocks_frag.spv");
    create_graphics_pipeline(vk_particles_graphics_pipeline, vk_particles_pipeline_layout, "shaders/particles_vert.spv", "shaders/particles_frag.spv");
    create_command_pool();
    create_command_buffers();
}

void VkEngine::draw_frame(Player& player, std::vector<Chunk>& world)
{
    vkWaitForFences(device.device, 1, &vk_in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

    int p = 0;
    for (auto& chunk : world) {
        if (chunk.should_be_deleted && p < world.size() - 1) {
            vkWaitForFences(device.device, MAX_FRAMES_IN_FLIGHT, vk_in_flight_fences.data(), VK_TRUE, UINT64_MAX);
            vkDestroyBuffer(device.device, chunk.vk_vertex_buffer, nullptr);
            vkFreeMemory(device.device, chunk.vk_vertex_buffer_memory, nullptr);
            vkDestroyBuffer(device.device, chunk.vk_index_buffer, nullptr);
            vkFreeMemory(device.device, chunk.vk_index_buffer_memory, nullptr);
            if (&chunk != &world.back()) {
                chunk = world.back();
            }
            world.pop_back();
        }
        p++;
    }

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, vk_image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Could not acquire swapchain image");
    }

    update_uniform_buffer(current_frame, player.camera);

    vkResetFences(device.device, 1, &vk_in_flight_fences[current_frame]);

    vkResetCommandBuffer(vk_command_buffers_blocks[current_frame], 0);

    record_command_buffer(vk_command_buffers_blocks[current_frame], image_index, world, player);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {vk_image_available_semaphores[current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_command_buffers_blocks[current_frame];

    VkSemaphore signal_semaphores[] = {vk_render_finished_semaphores[current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(vk_graphics_queue, 1, &submit_info, vk_in_flight_fences[current_frame]) != VK_SUCCESS) {
        throw std::runtime_error("Could not submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = {swapchain.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    result = vkQueuePresentKHR(vk_present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
        framebuffer_resized = false;
        recreate_swapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not present swapchain image");
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t VkEngine::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device.physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Could not find suitable memory type");
}

void VkEngine::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory)
{
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    //std::cout << "Creating Buffer size: " << size << std::endl;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device.device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Could not create buffer");
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device.device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device.device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate buffer memory");
    }

    vkBindBufferMemory(device.device, buffer, buffer_memory, 0);
}

void VkEngine::copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferCopy copy_region = {};
    copy_region.size = size;
    //std::cout << "Copy region size: " << size << std::endl;
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(command_buffer);
}

VkCommandBuffer VkEngine::begin_single_time_commands()
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = vk_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device.device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void VkEngine::end_single_time_commands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(vk_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk_graphics_queue);

    vkFreeCommandBuffers(device.device, vk_command_pool, 1, &command_buffer);
}

// void VkEngine::create_vertex_buffer()
// {
//     VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

//     VkBuffer staging_buffer;
//     VkDeviceMemory staging_buffer_memory;
//     create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

//     void* data;
//     vkMapMemory(device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
//     memcpy(data, vertices.data(), (size_t) buffer_size);
//     vkUnmapMemory(device.device, staging_buffer_memory);

//     create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_vertex_buffer, vk_vertex_buffer_memory);

//     copy_buffer(staging_buffer, vk_vertex_buffer, buffer_size);

//     vkDestroyBuffer(device.device, staging_buffer, nullptr);
//     vkFreeMemory(device.device, staging_buffer_memory, nullptr);
// }

// void VkEngine::create_index_buffer()
// {
//     VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

//     VkBuffer staging_buffer;
//     VkDeviceMemory staging_buffer_memory;
//     create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

//     void* data;
//     vkMapMemory(device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
//     memcpy(data, indices.data(), (size_t) buffer_size);
//     vkUnmapMemory(device.device, staging_buffer_memory);

//     create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_index_buffer, vk_index_buffer_memory);

//     copy_buffer(staging_buffer, vk_index_buffer, buffer_size);

//     vkDestroyBuffer(device.device, staging_buffer, nullptr);
//     vkFreeMemory(device.device, staging_buffer_memory, nullptr);
// }

// void VkEngine::recreate_vertex_array()
// {
//     //TODO Not remake the entire buffers but just update the changed vertices and indices

//     vkDeviceWaitIdle(device.device);

//     //Recreate the vertex buffer
//     vkDestroyBuffer(device.device, vk_vertex_buffer, nullptr);
//     vkFreeMemory(device.device, vk_vertex_buffer_memory, nullptr);
//     create_vertex_buffer();

//     //Recreate the index buffer
//     vkDestroyBuffer(device.device, vk_index_buffer, nullptr);
//     vkFreeMemory(device.device, vk_index_buffer_memory, nullptr);
//     create_index_buffer();
// }

void VkEngine::create_descriptor_set_layout()
{

    VkDescriptorSetLayoutBinding ubo_layout_binding = {};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    ubo_layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding sampler_layout_binding = {};
    sampler_layout_binding.binding = 1;
    sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.descriptorCount = 1;
    sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    sampler_layout_binding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {ubo_layout_binding, sampler_layout_binding};

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device.device, &layout_info, nullptr, &vk_descriptor_set_layout) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor set layout");
    }
}

void VkEngine::create_uniform_buffers()
{
    VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    vk_uniform_buffers_blocks.resize(vk_images.size());
    vk_uniform_buffers_particles.resize(vk_images.size());
    vk_uniform_buffers_memory.resize(vk_images.size() * 2);
    vk_uniform_buffers_mapped.resize(vk_images.size() * 2);

    for (size_t i = 0; i < vk_images.size(); i++) {
        create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vk_uniform_buffers_blocks[i], vk_uniform_buffers_memory[i]);

        vkMapMemory(device.device, vk_uniform_buffers_memory[i], 0, buffer_size, 0, &vk_uniform_buffers_mapped[i]);
    }
    for (size_t i = 0; i < vk_images.size(); i++) {
        create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vk_uniform_buffers_particles[i], vk_uniform_buffers_memory[i + vk_images.size()]);

        vkMapMemory(device.device, vk_uniform_buffers_memory[i + vk_images.size()], 0, buffer_size, 0, &vk_uniform_buffers_mapped[i + vk_images.size()]);
    }
}

void VkEngine::update_uniform_buffer(uint32_t current_image, Camera& camera)
{
    // static auto start_time = std::chrono::high_resolution_clock::now();

    // auto current_time = std::chrono::high_resolution_clock::now();
    // float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

    UniformBufferObject ubo = {};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
    ubo.proj = glm::perspective(camera.fov, swapchain.extent.width / (float) swapchain.extent.height, 0.1f, 800.0f);
    ubo.proj[1][1] *= -1;

    memcpy(vk_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));

    if (vk_particles_vertex_buffer == VK_NULL_HANDLE || particles.size() <= 0) {
        return;
    }

    UniformBufferObject ubo_particle = {};
    ubo_particle.model = glm::mat4(1.0f);
    ubo_particle.model = glm::translate(glm::mat4(1.0f), particles[0].position);
    ubo_particle.model = glm::translate(ubo_particle.model, particles[0].offset);
    ubo_particle.view = glm::lookAt(camera.pos, camera.pos + camera.front, camera.up);
    ubo_particle.proj = glm::perspective(camera.fov, swapchain.extent.width / (float) swapchain.extent.height, 0.1f, 800.0f);
    ubo_particle.proj[1][1] *= -1;
    memcpy(vk_uniform_buffers_mapped[current_frame + vk_images.size()], &ubo_particle, sizeof(ubo_particle));
}

void VkEngine::create_descriptor_pool()
{
    // std::array<VkDescriptorPoolSize, 2> pool_sizes = {};
    // pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // pool_sizes[0].descriptorCount = static_cast<uint32_t>(vk_images.size());
    // pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // pool_sizes[1].descriptorCount = static_cast<uint32_t>(vk_images.size());

    // VkDescriptorPoolCreateInfo pool_info = {};
    // pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    // pool_info.pPoolSizes = pool_sizes.data();
    // pool_info.maxSets = 99;
    // pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    // if (vkCreateDescriptorPool(device.device, &pool_info, nullptr, &vk_descriptor_pool) != VK_SUCCESS) {
    //     throw std::runtime_error("Could not create descriptor pool");
    // }

    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 1000;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device.device, &pool_info, nullptr, &vk_descriptor_pool) != VK_SUCCESS) {
        throw std::runtime_error("Could not create descriptor pool");
    }
}

void VkEngine::create_descriptor_sets()
{
    std::vector<VkDescriptorSetLayout> layouts(vk_images.size(), vk_descriptor_set_layout);
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = vk_descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<uint32_t>(vk_images.size());
    alloc_info.pSetLayouts = layouts.data();

    vk_descriptor_sets_chunks.resize(vk_images.size());
    vk_descriptor_sets_particles.resize(vk_images.size());
    if (vkAllocateDescriptorSets(device.device, &alloc_info, vk_descriptor_sets_chunks.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate descriptor sets");
    }
    if (vkAllocateDescriptorSets(device.device, &alloc_info, vk_descriptor_sets_particles.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate descriptor sets");
    }

    for (size_t i = 0; i < vk_images.size(); i++) {
        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = vk_uniform_buffers_blocks[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = vk_texture_image_view;
        image_info.sampler = vk_texture_sampler;

        std::array<VkWriteDescriptorSet, 2> descriptor_writes = {};
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = vk_descriptor_sets_chunks[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = vk_descriptor_sets_chunks[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(device.device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
    }
    for (size_t i = 0; i < vk_images.size(); i++) {
        VkDescriptorBufferInfo buffer_info = {};
        buffer_info.buffer = vk_uniform_buffers_particles[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo image_info = {};
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = vk_texture_image_view;
        image_info.sampler = vk_texture_sampler;

        std::array<VkWriteDescriptorSet, 2> descriptor_writes = {};
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].dstSet = vk_descriptor_sets_particles[i];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].dstSet = vk_descriptor_sets_particles[i];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].pImageInfo = &image_info;

        vkUpdateDescriptorSets(device.device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
    }
}

void VkEngine::add_cube_to_vertices(Cube& cube, int up, int down, int left, int right, int front, int back, glm::vec2 chunk_pos, Chunk& chunk)
{
    //std::cout << cube.pos.x << " " << cube.pos.y << " " << cube.pos.z << std::endl;
    float tex_x = fmodf((cube.type - 1), 16.0f) / 16.0f;
    float tex_y = floor((cube.type - 1) / 16.0f) / 16.0f;
    std::vector<std::array<float, 2>> texture_coords = {
        {tex_x, tex_y},
        {tex_x, tex_y},
        {tex_x, tex_y},
        {tex_x, tex_y},
        {tex_x, tex_y},
        {tex_x, tex_y}
    };

    float offset = 1.0f / 16.0f;

    cube.pos.x += chunk_pos.x * 16;
    cube.pos.z += chunk_pos.y * 16;

    std::vector<glm::vec3> colors = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };

    if (cube.type == 1) {
        colors[2] = {0.3f, 0.9f, 0.1f};
        texture_coords[0] = {fmodf((4 - 1), 16.0f) / 16.0f, floor((4 - 1) / 16.0f) / 16.0f};
        texture_coords[1] = {fmodf((4 - 1), 16.0f) / 16.0f, floor((4 - 1) / 16.0f) / 16.0f};
        //texture_coords[2] = {fmodf((4 - 1), 16.0f) / 16.0f, floor((4 - 1) / 16.0f) / 16.0f};
        texture_coords[3] = {fmodf((3 - 1), 16.0f) / 16.0f, floor((3 - 1) / 16.0f) / 16.0f};
        texture_coords[4] = {fmodf((4 - 1), 16.0f) / 16.0f, floor((4 - 1) / 16.0f) / 16.0f};
        texture_coords[5] = {fmodf((4 - 1), 16.0f) / 16.0f, floor((4 - 1) / 16.0f) / 16.0f};
    }
    if (cube.type == 21) {
        texture_coords[2] = {fmodf((22 - 1), 16.0f) / 16.0f, floor((22 - 1) / 16.0f) / 16.0f};
        texture_coords[3] = {fmodf((22 - 1), 16.0f) / 16.0f, floor((22 - 1) / 16.0f) / 16.0f};
    }
    if (cube.type == 53 || cube.type == 54) {
        colors[0] = {0.25f, 0.95f, 0.05f};
        colors[1] = {0.25f, 0.95f, 0.05f};
        colors[2] = {0.25f, 0.95f, 0.05f};
        colors[3] = {0.25f, 0.95f, 0.05f};
        colors[4] = {0.25f, 0.95f, 0.05f};
        colors[5] = {0.25f, 0.95f, 0.05f};
    }

    int i = 0;

    //back good
    if (front == 0) {
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }
    i++;
    //front good
    if (back == 0) {
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0], texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }
    i++;
    //up good
    if (up == 0) {
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0], texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }
    i++;
    //down good
    if (down == 0) {
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0], texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }
    i++;
    //left
    if (right == 0) {
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]  + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x + 1.0f, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }
    i++;
    //right
    if (left == 0) {
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z}, colors[i], {texture_coords[i][0], texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y + 1.0f, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1] + offset}, {cube.pos.x, cube.pos.y, cube.pos.z}});
        chunk.vertices.push_back({{cube.pos.x, cube.pos.y, cube.pos.z + 1.0f}, colors[i], {texture_coords[i][0] + offset, texture_coords[i][1]}, {cube.pos.x, cube.pos.y, cube.pos.z}});

        size_t len = chunk.vertices.size();
        chunk.indices.push_back(len - 4);
        chunk.indices.push_back(len - 1);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 2);
        chunk.indices.push_back(len - 3);
        chunk.indices.push_back(len - 4);
        cube.faces += 1;
    }

    cube.is_displayed = true;

    // std::array<uint16_t, 36> cube_indices = {
    //     4, 1, 2, 2, 3, 4,
    //     8, 7, 6, 6, 5, 8,
    //     12, 9, 10, 10, 11, 12,
    //     16, 15, 14, 14, 13, 16,
    //     20, 19, 18, 18, 17, 20,
    //     24, 21, 22, 22, 23, 24};
    // size_t vect_len = chunk.vertices.size();

    // cube.pos.x -= chunk_pos.x * 16;
    // cube.pos.z -= chunk_pos.y * 16;
}

void VkEngine::remove_face(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, int i, int indice)
{
    vertices.erase(vertices.begin() + i, vertices.begin() + i + 4);
    indices.erase(indices.begin() + indice * 6, indices.begin() + indice * 6 + 6);
    for (int j = 0; j < indices.size(); j++) {
        if (indices[j] > indice * 4) {
            indices[j] -= 4;
        }
    }
}

void VkEngine::remove_cube_from_vertices(glm::vec3 pos, glm::vec2 chunk_pos, Chunk& chunk, Cube& cube)
{
    for (int i = 0; i < chunk.vertices.size(); i += 4) {
        int indice = i / 4;
        // std::cout << chunk.vertices[i].actual_block.x << " " << chunk.vertices[i].actual_block.y << " " << chunk.vertices[i].actual_block.z << std::endl;
        // std::cout << cube.pos.x << " " << cube.pos.y << " " << cube.pos.z << std::endl;
        if (chunk.vertices[i].actual_block == cube.pos) {
            //std::cout << cube.faces << std::endl;
            chunk.vertices.erase(chunk.vertices.begin() + i, chunk.vertices.begin() + i + (4 * cube.faces));
            chunk.indices.erase(chunk.indices.begin() + indice * 6, chunk.indices.begin() + indice * 6 + (6 * cube.faces));
            for (int j = 0; j < chunk.indices.size(); j++) {
                if (chunk.indices[j] > i) {
                    chunk.indices[j] -= 4 * cube.faces;
                }
            }
            cube.faces = 0;
            return;
        }
    }
}

void VkEngine::create_texture_image()
{
    int tex_width, tex_height, tex_channels;
    stbi_uc* pixels = stbi_load("img/texture_atlas.png", &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
    VkDeviceSize image_size = tex_width * tex_height * 4;

    if (!pixels) {
        throw std::runtime_error("Could not load texture image");
    }

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    void* data;
    vkMapMemory(device.device, staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(image_size));
    vkUnmapMemory(device.device, staging_buffer_memory);

    stbi_image_free(pixels);

    create_image(tex_width, tex_height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_texture_image, vk_texture_image_memory);

    transition_image_layout(vk_texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer, vk_texture_image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));
    transition_image_layout(vk_texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device.device, staging_buffer, nullptr);
    vkFreeMemory(device.device, staging_buffer_memory, nullptr);
}

void VkEngine::create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& image_memory)
{
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device.device, &image_info, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image");
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device.device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device.device, &alloc_info, nullptr, &image_memory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate image memory");
    }

    vkBindImageMemory(device.device, image, image_memory, 0);
}

void VkEngine::transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;


    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;
 
    if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (has_stencil_component(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    end_single_time_commands(command_buffer);
}

void VkEngine::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_single_time_commands(command_buffer);
}

void VkEngine::create_texture_image_view()
{
    vk_texture_image_view = create_image_view(vk_texture_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkImageView VkEngine::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView image_view;
    if (vkCreateImageView(device.device, &view_info, nullptr, &image_view) != VK_SUCCESS) {
        throw std::runtime_error("Could not create image view");
    }

    return image_view;
}

void VkEngine::create_texture_sampler()
{
    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    if (vkCreateSampler(device.device, &sampler_info, nullptr, &vk_texture_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Could not create texture sampler");
    }
}

void VkEngine::create_depth_resources()
{
    VkFormat depth_format = find_depth_format();

    create_image(swapchain.extent.width, swapchain.extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_depth_image, vk_depth_image_memory);
    vk_depth_image_view = create_image_view(vk_depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

    transition_image_layout(vk_depth_image, depth_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat VkEngine::find_depth_format()
{
    return find_supported_format({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat VkEngine::find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device.physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Could not find supported format");
}

bool VkEngine::has_stencil_component(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VkEngine::create_vertex_buffer_chunk(Chunk& chunk)
{
    VkDeviceSize buffer_size = sizeof(chunk.vertices[0]) * chunk.vertices.size();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    void* data;
    vkMapMemory(device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, chunk.vertices.data(), (size_t) buffer_size);
    vkUnmapMemory(device.device, staging_buffer_memory);

    //std::cout << "Buffer size chunk: " << buffer_size << std::endl;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, chunk.vk_vertex_buffer, chunk.vk_vertex_buffer_memory);

    copy_buffer(staging_buffer, chunk.vk_vertex_buffer, buffer_size);

    vkDestroyBuffer(device.device, staging_buffer, nullptr);
    vkFreeMemory(device.device, staging_buffer_memory, nullptr);
}

void VkEngine::create_index_buffer_chunk(Chunk& chunk)
{
    VkDeviceSize buffer_size = sizeof(chunk.indices[0]) * chunk.indices.size();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    void* data;
    vkMapMemory(device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, chunk.indices.data(), (size_t) buffer_size);
    vkUnmapMemory(device.device, staging_buffer_memory);

    //std::cout << "Buffer size chunk indices: " << buffer_size << std::endl;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, chunk.vk_index_buffer, chunk.vk_index_buffer_memory);

    copy_buffer(staging_buffer, chunk.vk_index_buffer, buffer_size);

    vkDestroyBuffer(device.device, staging_buffer, nullptr);
    vkFreeMemory(device.device, staging_buffer_memory, nullptr);
}

void VkEngine::free_buffers_chunk(Chunk& chunk)
{
    wait_idle();
    vkDestroyBuffer(device.device, chunk.vk_vertex_buffer, nullptr);
    vkFreeMemory(device.device, chunk.vk_vertex_buffer_memory, nullptr);
    vkDestroyBuffer(device.device, chunk.vk_index_buffer, nullptr);
    vkFreeMemory(device.device, chunk.vk_index_buffer_memory, nullptr);
}

void VkEngine::recreate_buffers_chunk(Chunk& chunk)
{
    wait_idle();
    free_buffers_chunk(chunk);
    create_vertex_buffer_chunk(chunk);
    create_index_buffer_chunk(chunk);
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

bool VkEngine::LoadTextureFromFile(const char* filename, MyTextureData* tex_data)
{
    // Specifying 4 channels forces stb to load the image in RGBA which is an easy format for Vulkan
    tex_data->Channels = 4;
    unsigned char* image_data = stbi_load(filename, &tex_data->Width, &tex_data->Height, 0, tex_data->Channels);

    if (image_data == NULL) {
        fprintf(stderr, "Could not load texture file %s\n", filename);
        return false;
    }

    // Calculate allocation size (in number of bytes)
    size_t image_size = tex_data->Width * tex_data->Height * tex_data->Channels;

    VkResult err;

    // Create the Vulkan image.
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_SRGB;
        info.extent.width = tex_data->Width;
        info.extent.height = tex_data->Height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(device.device, &info, nullptr, &tex_data->Image);
        check_vk_result(err);
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device.device, tex_data->Image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        err = vkAllocateMemory(device.device, &alloc_info, nullptr, &tex_data->ImageMemory);
        check_vk_result(err);
        err = vkBindImageMemory(device.device, tex_data->Image, tex_data->ImageMemory, 0);
        check_vk_result(err);
    }

    // Create the Image View
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = tex_data->Image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_SRGB;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(device.device, &info, nullptr, &tex_data->ImageView);
        check_vk_result(err);
    }

    // Create Sampler
    tex_data->Sampler = vk_texture_sampler;

    // Create Descriptor Set using ImGUI's implementation
    tex_data->DS = ImGui_ImplVulkan_AddTexture(vk_texture_sampler, tex_data->ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Create Upload Buffer
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = image_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(device.device, &buffer_info, nullptr, &tex_data->UploadBuffer);
        check_vk_result(err);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device.device, tex_data->UploadBuffer, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        err = vkAllocateMemory(device.device, &alloc_info, nullptr, &tex_data->UploadBufferMemory);
        check_vk_result(err);
        err = vkBindBufferMemory(device.device, tex_data->UploadBuffer, tex_data->UploadBufferMemory, 0);
        check_vk_result(err);
    }

    // Upload to Buffer:
    {
        void* map = NULL;
        err = vkMapMemory(device.device, tex_data->UploadBufferMemory, 0, image_size, 0, &map);
        check_vk_result(err);
        memcpy(map, image_data, image_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = tex_data->UploadBufferMemory;
        range[0].size = image_size;
        err = vkFlushMappedMemoryRanges(device.device, 1, range);
        check_vk_result(err);
        vkUnmapMemory(device.device, tex_data->UploadBufferMemory);
    }

    // Release image memory using stb
    stbi_image_free(image_data);

    // Create a command buffer that will perform following steps when hit in the command queue.
    // TODO: this works in the example, but may need input if this is an acceptable way to access the pool/create the command buffer.
    VkCommandPool command_pool = vk_command_pool;
    VkCommandBuffer command_buffer;
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = vk_command_pool;
        alloc_info.commandBufferCount = 1;

        err = vkAllocateCommandBuffers(device.device, &alloc_info, &command_buffer);
        check_vk_result(err);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);
    }

    // Copy to Image
    {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = tex_data->Image;
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = tex_data->Width;
        region.imageExtent.height = tex_data->Height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(command_buffer, tex_data->UploadBuffer, tex_data->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = tex_data->Image;
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
    }

    // End command buffer
    {
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(vk_graphics_queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);
        err = vkDeviceWaitIdle(device.device);
        check_vk_result(err);
    }

    return true;
}

// Helper function to cleanup an image loaded with LoadTextureFromFile
void VkEngine::RemoveTexture(MyTextureData* tex_data)
{
    vkFreeMemory(device.device, tex_data->UploadBufferMemory, nullptr);
    vkDestroyBuffer(device.device, tex_data->UploadBuffer, nullptr);
    //vkDestroySampler(device.device, tex_data->Sampler, nullptr);
    vkDestroyImageView(device.device, tex_data->ImageView, nullptr);
    vkDestroyImage(device.device, tex_data->Image, nullptr);
    vkFreeMemory(device.device, tex_data->ImageMemory, nullptr);
    ImGui_ImplVulkan_RemoveTexture(tex_data->DS);
}

void VkEngine::create_inventory()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.WantCaptureMouse = true;
    io.WantCaptureKeyboard = true;
    io.WantTextInput = true;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance.instance;
    init_info.PhysicalDevice = device.physical_device;
    init_info.Device = device.device;
    init_info.QueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();
    init_info.Queue = vk_graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = vk_descriptor_pool;
    init_info.Subpass = 0;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = swapchain.image_count;
    init_info.ImageCount = swapchain.image_count;
    init_info.RenderPass = vk_render_pass;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    ImGui_ImplVulkan_Init(&init_info);
}

void VkEngine::create_particles_buffers()
{
    //     wait_idle();
    // if (vk_particles_vertex_buffer != VK_NULL_HANDLE) {
    //     vkDestroyBuffer(device.device, vk_particles_vertex_buffer, nullptr);
    //     vkFreeMemory(device.device, vk_particles_vertex_buffer_memory, nullptr);
    //     vkDestroyBuffer(device.device, vk_particles_index_buffer, nullptr);
    //     vkFreeMemory(device.device, vk_particles_index_buffer_memory, nullptr);
    // }
    if (vk_particles_vertex_buffer != VK_NULL_HANDLE) {
        return;
    }

    float offset = 1.0f / 16.0f;
    particles_vertices.push_back({{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    particles_vertices.push_back({{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {offset, 0.0f}, {0.0f, 0.0f, 0.0f}});
    particles_vertices.push_back({{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {offset, offset}, {0.0f, 0.0f, 0.0f}});
    particles_vertices.push_back({{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, offset}, {0.0f, 0.0f, 0.0f}});

    size_t len = particles_vertices.size();
    particles_indices.push_back(len - 4);
    particles_indices.push_back(len - 3);
    particles_indices.push_back(len - 2);
    particles_indices.push_back(len - 2);
    particles_indices.push_back(len - 1);
    particles_indices.push_back(len - 4);

    VkDeviceSize buffer_size = sizeof(Vertex) * particles_vertices.size();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_buffer_memory);

    void* data;
    vkMapMemory(device.device, staging_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, particles_vertices.data(), (size_t) buffer_size);
    vkUnmapMemory(device.device, staging_buffer_memory);

    create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_particles_vertex_buffer, vk_particles_vertex_buffer_memory);

    copy_buffer(staging_buffer, vk_particles_vertex_buffer, buffer_size);

    vkDestroyBuffer(device.device, staging_buffer, nullptr);
    vkFreeMemory(device.device, staging_buffer_memory, nullptr);

    VkDeviceSize buffer_size_i = sizeof(uint32_t) * particles_indices.size();

    VkBuffer staging_buffer_i;
    VkDeviceMemory staging_buffer_memory_i;
    create_buffer(buffer_size_i, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer_i, staging_buffer_memory_i);

    void* data_i;
    vkMapMemory(device.device, staging_buffer_memory_i, 0, buffer_size_i, 0, &data_i);
    memcpy(data_i, particles_indices.data(), (size_t) buffer_size_i);
    vkUnmapMemory(device.device, staging_buffer_memory_i);

    create_buffer(buffer_size_i, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_particles_index_buffer, vk_particles_index_buffer_memory);

    copy_buffer(staging_buffer_i, vk_particles_index_buffer, buffer_size_i);

    vkDestroyBuffer(device.device, staging_buffer_i, nullptr);
    vkFreeMemory(device.device, staging_buffer_memory_i, nullptr);

    create_particles_instance_buffers();
}

void VkEngine::create_particles_instance_buffers()
{
    if (vk_particles_instance_buffer != VK_NULL_HANDLE) {
        wait_idle();
        vkDestroyBuffer(device.device, vk_particles_instance_buffer, nullptr);
        vkFreeMemory(device.device, vk_particles_instance_buffer_memory, nullptr);
    }

    VkDeviceSize buffer_size_instance = sizeof(ParticleInstanceData) * MAX_PARTICLES;

    // VkBuffer staging_buffer_instance;
    // VkDeviceMemory staging_buffer_memory_instance;
    // create_buffer(buffer_size_instance, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer_instance, staging_buffer_memory_instance);

    // void* data_instance;
    // vkMapMemory(device.device, staging_buffer_memory_instance, 0, buffer_size_instance, 0, &data_instance);
    // memcpy(data_instance, particles_instance_data.data(), (size_t) buffer_size_instance);
    // vkUnmapMemory(device.device, staging_buffer_memory_instance);

    create_buffer(buffer_size_instance, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_particles_instance_buffer, vk_particles_instance_buffer_memory);

    // copy_buffer(staging_buffer_instance, vk_particles_instance_buffer, buffer_size_instance);

    // vkDestroyBuffer(device.device, staging_buffer_instance, nullptr);
    // vkFreeMemory(device.device, staging_buffer_memory_instance, nullptr);
}

void VkEngine::create_particles(glm::vec3 pos, uint16_t type, Player& player)
{
    std::array<Particle, 4> parts{
        Particle{{pos.x + rand_float(0.0f, 1.0f), pos.y + rand_float(0.0f, 0.2f), pos.z + rand_float(0.0f, 1.0f)}, {rand_float(-0.05, 0.05), rand_float(-0.05, 0.01), rand_float(-0.05, 0.05)}, {1.0f, 1.0f, 1.0f}, rand_float(0.1f, 0.3f), 0},
        Particle{{pos.x + rand_float(0.0f, 1.0f), pos.y + rand_float(0.0f, 0.2f), pos.z + rand_float(0.0f, 1.0f)}, {rand_float(-0.05, 0.05), rand_float(-0.05, 0.01), rand_float(-0.05, 0.05)}, {1.0f, 1.0f, 1.0f}, rand_float(0.1f, 0.3f), 0},
        Particle{{pos.x + rand_float(0.0f, 1.0f), pos.y + rand_float(0.0f, 0.2f), pos.z + rand_float(0.0f, 1.0f)}, {rand_float(-0.05, 0.05), rand_float(-0.05, 0.01), rand_float(-0.05, 0.05)}, {1.0f, 1.0f, 1.0f}, rand_float(0.1f, 0.3f), 0},
        Particle{{pos.x + rand_float(0.0f, 1.0f), pos.y + rand_float(0.0f, 0.2f), pos.z + rand_float(0.0f, 1.0f)}, {rand_float(-0.05, 0.05), rand_float(-0.05, 0.01), rand_float(-0.05, 0.05)}, {1.0f, 1.0f, 1.0f}, rand_float(0.1f, 0.3f), 0}
    };

    for (auto& part : parts) {
        particles.push_back(part);
        particles_instance_data.push_back({{part.position.x, part.position.y, part.position.z}, part.size, type});
    }

    VkCommandBuffer command_buffer = begin_single_time_commands();

    vkCmdUpdateBuffer(command_buffer, vk_particles_instance_buffer, 0, sizeof(ParticleInstanceData) * particles_instance_data.size(), particles_instance_data.data());

    end_single_time_commands(command_buffer);
}

void VkEngine::update_particles()
{
    if (particles.size() == 0 || particles_instance_data.size() == 0) {
        return;
    }

    int index = 0;

    for (auto& particle : particles) {
        particle.position += particle.velocity;
        particle.velocity.y += 0.01f;
        particle.life += 0.01f;
        particles_instance_data[index].pos = {particle.position.x, particle.position.y, particle.position.z};
        if (particle.life > 0.5f) {
            particles.erase(particles.begin() + index);
            particles_instance_data.erase(particles_instance_data.begin() + index);
        }
        index++;
    }

    if (particles.size() == 0 || particles_instance_data.size() == 0) {
        return;
    }

    VkCommandBuffer command_buffer = begin_single_time_commands();

    vkCmdUpdateBuffer(command_buffer, vk_particles_instance_buffer, 0, sizeof(ParticleInstanceData) * particles_instance_data.size(), particles_instance_data.data());

    end_single_time_commands(command_buffer);
}

void VkEngine::wait_idle()
{
    vkDeviceWaitIdle(device.device);
}

VkEngine::~VkEngine()
{
    wait_idle();

    if (vk_particles_vertex_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device.device, vk_particles_vertex_buffer, nullptr);
        vkFreeMemory(device.device, vk_particles_vertex_buffer_memory, nullptr);
        vkDestroyBuffer(device.device, vk_particles_index_buffer, nullptr);
        vkFreeMemory(device.device, vk_particles_index_buffer_memory, nullptr);
        vkDestroyBuffer(device.device, vk_particles_instance_buffer, nullptr);
        vkFreeMemory(device.device, vk_particles_instance_buffer_memory, nullptr);
    }

    vkDestroyImageView(device.device, vk_depth_image_view, nullptr);
    vkDestroyImage(device.device, vk_depth_image, nullptr);
    vkFreeMemory(device.device, vk_depth_image_memory, nullptr);

    vkDestroySampler(device.device, vk_texture_sampler, nullptr);
    vkDestroyImageView(device.device, vk_texture_image_view, nullptr);
    vkDestroyImage(device.device, vk_texture_image, nullptr);
    vkFreeMemory(device.device, vk_texture_image_memory, nullptr);

    for (size_t i = 0; i < vk_images.size() * 2; i++) {
        vkUnmapMemory(device.device, vk_uniform_buffers_memory[i]);
    }

    vkDestroyDescriptorPool(device.device, vk_descriptor_pool, nullptr);

    for (size_t i = 0; i < vk_images.size(); i++) {
        vkDestroyBuffer(device.device, vk_uniform_buffers_blocks[i], nullptr);
        vkFreeMemory(device.device, vk_uniform_buffers_memory[i], nullptr);
    }
    for (size_t i = 0; i < vk_images.size(); i++) {
        vkDestroyBuffer(device.device, vk_uniform_buffers_particles[i], nullptr);
        vkFreeMemory(device.device, vk_uniform_buffers_memory[i + vk_images.size()], nullptr);
    }

    vkDestroyDescriptorSetLayout(device.device, vk_descriptor_set_layout, nullptr);
    
    // vkDestroyBuffer(device.device, vk_vertex_buffer, nullptr);
    // vkFreeMemory(device.device, vk_vertex_buffer_memory, nullptr);
    // vkDestroyBuffer(device.device, vk_index_buffer, nullptr);
    // vkFreeMemory(device.device, vk_index_buffer_memory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device.device, vk_render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(device.device, vk_image_available_semaphores[i], nullptr);
        vkDestroyFence(device.device, vk_in_flight_fences[i], nullptr);
    }

    vkDestroyCommandPool(device.device, vk_command_pool, nullptr);

    vkDestroyPipeline(device.device, vk_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(device.device, vk_pipeline_layout, nullptr);
    vkDestroyPipeline(device.device, vk_particles_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(device.device, vk_particles_pipeline_layout, nullptr);

    vkDestroyRenderPass(device.device, vk_render_pass, nullptr);
    for (auto framebuffer : vk_framebuffers) {
        vkDestroyFramebuffer(device.device, framebuffer, nullptr);
    }
    for (auto image_view : vk_image_views) {
        vkDestroyImageView(device.device, image_view, nullptr);
    }

    vkb::destroy_swapchain(swapchain);
    vkb::destroy_surface(instance.instance, vk_surface_khr);
    vkb::destroy_device(device);
    vkb::destroy_instance(instance);
}
