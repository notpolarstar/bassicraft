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
                        //engine.add_cube_to_vertices(chunk.blocks[x][y][z], chunk.blocks[x][y - 1][z].type, chunk.blocks[x][y + 1][z].type, chunk.blocks[x - 1][y][z].type, chunk.blocks[x + 1][y][z].type, chunk.blocks[x][y][z - 1].type, chunk.blocks[x][y][z + 1].type, chunk.pos);
                        engine.add_cube_to_vertices(chunk.blocks[x][y][z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
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

void Bassicraft::add_cube(Chunk& chunk, Cube& cube)
{
    chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z] = cube;
    engine.add_cube_to_vertices(cube, 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
}

void Bassicraft::remove_cube(Chunk& chunk, glm::ivec3 pos)
{
    chunk.blocks[pos.x][pos.y][pos.z] = Cube{glm::ivec3(0, 0, 0), 0};
    engine.remove_cube_from_vertices(pos, chunk.pos);
}

// glm::ivec3 Bassicraft::get_cube_pointed_at()
// {
//     glm::vec3 pos = player.camera.pos;
//     glm::vec3 dir = player.camera.front;
//     for (int i = 0; i < 10; i++) {
//         pos += dir * 0.1f;
//         if (world[0].blocks[(int)pos.x][(int)pos.y][(int)pos.z].type != 0) {
//             return glm::ivec3((int)pos.x, (int)pos.y, (int)pos.z);
//         }
//     }
//     return glm::ivec3(0, 0, 0);
// }

Bassicraft::~Bassicraft()
{
}