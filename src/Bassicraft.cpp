#include <random>
#include <iostream>
#include <algorithm>

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include "VkEngine.hpp"
#include "Bassicraft.hpp"
#include "VkBootstrap.h"

Bassicraft::Bassicraft(/* args */)
{
    srand(time(NULL));

    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetSeed(rand());
    noise.SetFrequency(0.01f);

    for (int x = -render_distance; x < render_distance; x++) {
        for (int z = -render_distance; z < render_distance; z++) {
            world.push_back(Chunk(glm::vec2(x, z), noise));
        }
    }

    engine.create_swapchain();
    engine.get_queues();
    engine.create_render_pass();
    engine.create_descriptor_set_layout();
    engine.create_graphics_pipeline();
    engine.create_command_pool();
    engine.create_depth_resources();
    engine.create_framebuffers();
    engine.create_texture_image();
    engine.create_texture_image_view();
    engine.create_texture_sampler();
    // engine.create_vertex_buffer();
    // engine.create_index_buffer();
    engine.create_uniform_buffers();
    engine.create_descriptor_pool();
    engine.create_descriptor_sets();
    engine.create_command_buffers();
    engine.create_sync_objects();

    std::cout << "engine created\n";
    for (auto& chunk : world) {
        set_blocks_in_vertex_buffer(chunk);
        engine.create_vertex_buffer_chunk(chunk);
        engine.create_index_buffer_chunk(chunk);
    }

    glfwSetWindowUserPointer(engine.window, this);
    glfwSetMouseButtonCallback(engine.window, [](GLFWwindow* window, int button, int action, int mods) {
        static_cast<Bassicraft*>(glfwGetWindowUserPointer(window))->mouse_buttons(window, button, action, mods);
    });
    while (!glfwWindowShouldClose(engine.window)) {
        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(engine.window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(engine.window, &width, &height);
            glfwWaitEvents();
        }
        if (glfwGetKey(engine.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(engine.window, GLFW_TRUE);
        }
        if (glfwGetKey(engine.window, GLFW_KEY_KP_6) == GLFW_PRESS) {
            //engine.recreate_vertex_array();
        }
        // if (glfwGetKey(engine.window, GLFW_KEY_Q) == GLFW_PRESS) {
        //     Cube cube{};
        //     cube.type = rand() % 256;
        //     cube.pos = glm::ivec3(rand() % 16, rand() % 100, rand() % 16);
        //     while (world[0].blocks[cube.pos.x][cube.pos.y][cube.pos.z].type != 0) {
        //         cube.pos = glm::ivec3(rand() % 16, rand() % 100, rand() % 16);
        //     }
        //     add_cube(cube);
        //     engine.recreate_vertex_array();
        // }
        // if (glfwGetKey(engine.window, GLFW_KEY_E) == GLFW_PRESS) {
        //     glm::ivec3 pos = glm::ivec3(rand() % 16, rand() % 100, rand() % 16);
        //     while (world[0].blocks[pos.x][pos.y][pos.z].type == 0) {
        //         pos = glm::ivec3(rand() % 16, rand() % 100, rand() % 16);
        //     }
        //     remove_cube(pos);
        //     engine.recreate_vertex_array();
        // }
        unload_load_new_chunks();
        player.processInput(engine.window);
        engine.draw_frame(player, world);
    }
    engine.wait_idle();
    for (auto& chunk : world) {
        engine.free_buffers_chunk(chunk);
    }
    glfwDestroyWindow(engine.window);
    glfwTerminate();
}

void Bassicraft::set_blocks_in_vertex_buffer(Chunk& chunk)
{
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 100; y++) {
            for (int z = 0; z < 16; z++) {
                if (chunk.blocks[x][y][z].type != 0) {
                    if (x == 0 || x == 15 || y == 0 || y == 99 || z == 0 || z == 15) {
                        engine.add_cube_to_vertices(chunk.blocks[x][y][z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
                    } else if (chunk.blocks[x - 1][y][z].type == 0 || chunk.blocks[x + 1][y][z].type == 0 || chunk.blocks[x][y - 1][z].type == 0 || chunk.blocks[x][y + 1][z].type == 0 || chunk.blocks[x][y][z - 1].type == 0 || chunk.blocks[x][y][z + 1].type == 0) {
                        engine.add_cube_to_vertices(chunk.blocks[x][y][z], chunk.blocks[x][y - 1][z].type, chunk.blocks[x][y + 1][z].type, chunk.blocks[x - 1][y][z].type, chunk.blocks[x + 1][y][z].type, chunk.blocks[x][y][z - 1].type, chunk.blocks[x][y][z + 1].type, chunk.pos, chunk);
                        //engine.add_cube_to_vertices(chunk.blocks[x][y][z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
                    }
                }
            }
        }
    }
}

void Bassicraft::unload_load_new_chunks()
{
    glm::vec2 player_chunk = glm::vec2((int)player.camera.pos.x / 16, (int)player.camera.pos.z / 16);
    for (auto& chunk : world) {
        if (chunk.pos.x < player_chunk.x - render_distance || chunk.pos.x > player_chunk.x + render_distance || chunk.pos.y < player_chunk.y - render_distance || chunk.pos.y > player_chunk.y + render_distance) {
            chunk.should_be_deleted = true;
        }
    }
    for (int x = -render_distance; x < render_distance; x++) {
        for (int z = -render_distance; z < render_distance; z++) {
            bool found = false;
            for (auto& chunk : world) {
                if (chunk.pos == glm::vec2(player_chunk.x + x, player_chunk.y + z)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                int siz = world.size();
                world.push_back(Chunk(glm::vec2(player_chunk.x + x, player_chunk.y + z), noise));
                set_blocks_in_vertex_buffer(world[siz]);
                engine.create_vertex_buffer_chunk(world[siz]);
                engine.create_index_buffer_chunk(world[siz]);
            }
        }
    }
}

void Bassicraft::mouse_buttons(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        glm::vec4 pos = get_cube_pointed_at();
        if (pos.w != -42069 && world[pos.w].blocks[pos.x][pos.y][pos.z].type != 0) {
            remove_cube(world[pos.w], pos);
            engine.recreate_buffers_chunk(world[pos.w]);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        glm::vec4 pos = get_cube_pointed_at();
        if (pos.w != -42069 && world[pos.w].blocks[pos.x][pos.y - 1][pos.z].type == 0) {
            Cube cube{};
            cube.type = rand() % 256;
            cube.pos = glm::ivec3(pos.x, pos.y - 1, pos.z);
            add_cube(world[pos.w], cube);
            engine.recreate_buffers_chunk(world[pos.w]);
        }
    }

}

void Bassicraft::add_cube(Chunk& chunk, Cube& cube)
{
    chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z] = cube;
    engine.add_cube_to_vertices(cube, 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
}

void Bassicraft::remove_cube(Chunk& chunk, glm::ivec3 pos)
{
    chunk.blocks[pos.x][pos.y][pos.z] = Cube{glm::ivec3(0, 0, 0), 0};
    engine.remove_cube_from_vertices(pos, chunk.pos, chunk);
}

int regular_modulo(int a, int b)
{
    return (a % b + b) % b;
}

glm::vec4 Bassicraft::get_cube_pointed_at()
{
    //Return vec (16, 100, 16) position in the chunk
    //Return -42069 if no cube is pointed at

    glm::vec3 ray = player.camera.front;
    glm::vec3 start = player.camera.pos;
    glm::vec3 end = start + ray * 10.0f;

    for (float t = 0.0f; t < 10.0f; t += 0.01f) {
        glm::vec3 position = start + ray * t;
        glm::ivec3 block_position = glm::floor(position);
        glm::vec2 chunk_pos = glm::vec2(block_position.x / 16, block_position.z / 16);
        if (block_position.x < 0) {
            chunk_pos.x -= 1;
        }
        if (block_position.z < 0) {
            chunk_pos.y -= 1;
        }
        glm::ivec3 block_position_in_chunk = glm::ivec3(regular_modulo(block_position.x, 16), block_position.y, regular_modulo(block_position.z, 16));

        int index = 0;
        for (auto& chunk : world) {
            if (chunk.pos == chunk_pos) {
                if (chunk.blocks[block_position_in_chunk.x][block_position_in_chunk.y][block_position_in_chunk.z].type != 0) {
                    return glm::vec4(block_position_in_chunk, index);
                }
            }
            index++;
        }
    }
    return glm::vec4(0, 0, 0, -42069);
}

Bassicraft::~Bassicraft()
{
}