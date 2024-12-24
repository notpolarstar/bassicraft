#include <random>
#include <iostream>
#include <algorithm>
#include <chrono>

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "VkEngine.hpp"
#include "Bassicraft.hpp"
#include "VkBootstrap.h"
#include "Utils.hpp"

Bassicraft::Bassicraft(/* args */)
{
    srand(time(NULL));

    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    //noise.SetSeed(rand());
    noise.SetSeed(0);
    noise.SetFrequency(0.01f);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(3);
    noise.SetFractalLacunarity(2.0f);
    noise.SetFractalGain(0.5f);

    biome_noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    biome_noise.SetSeed(rand());
    biome_noise.SetFrequency(0.02f);
    biome_noise.SetFractalType(FastNoiseLite::FractalType_DomainWarpIndependent);
    biome_noise.SetFractalOctaves(3);
    biome_noise.SetFractalLacunarity(2.0f);
    biome_noise.SetFractalGain(0.5f);

    init_engine();
    init_textures();

    for (int x = -render_distance; x < render_distance; x++) {
        for (int z = -render_distance; z < render_distance; z++) {
            world.push_back(Chunk(glm::vec2(x, z), noise, biome_noise));
        }
    }

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
    glfwSetKeyCallback(engine.window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        static_cast<Bassicraft*>(glfwGetWindowUserPointer(window))->key_callback(window, key, scancode, action, mods);
    });

    while (!glfwWindowShouldClose(engine.window)) {
        auto time_point = std::chrono::high_resolution_clock::now();
        glfwPollEvents();
        ImGui_ImplGlfw_MouseButtonCallback(engine.window, 0, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS);

        glfwGetFramebufferSize(engine.window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(engine.window, &width, &height);
            glfwWaitEvents();
        }

        bool open = true;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Debug", &open, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.1f ms", engine.frame_render_duration);
        ImGui::Text("Player position: %.1f %.1f %.1f", player.camera.pos.x, player.camera.pos.y, player.camera.pos.z);
        ImGui::Text("Player chunk: %d %d", (int)player.camera.pos.x / 16, (int)player.camera.pos.z / 16);
        ImGui::Text("Player chunk position: %.1f %.1f", regular_modulo(player.camera.pos.x, 16), regular_modulo(player.camera.pos.z, 16));
        ImGui::Text("Player yaw: %.1f", player.camera.yaw);
        ImGui::Text("Player pitch: %.1f", player.camera.pitch);
        ImGui::Text("Player selected item: %d", player.selected_item);
        ImGui::Text("Player selected slot: %d", inventory.selected_slot);
        ImGui::Text("Player velocity: %.1f %.1f %.1f", player.velocity.x, player.velocity.y, player.velocity.z);
        ImGui::Text("Player front: %.1f %.1f %.1f", player.camera.front.x, player.camera.front.y, player.camera.front.z);
        ImGui::End();

        display_hotbar();

        if (is_inventory_open) {
            display_inventory();
        } else {
            display_crosshair();
        }
        
        ImGui::Render();

        unload_load_new_chunks();
        if (is_cursor_locked) {
            move_player();
            player.mouse_movement(engine.window);
        } else {
            player.update_mouse_pos(engine.window);
        }

        // engine.update_particles();

        engine.draw_frame(player, world);
        engine.frame_render_duration = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - time_point).count();
    }
}

void Bassicraft::init_engine()
{
    engine.create_swapchain();
    engine.get_queues();
    engine.create_render_pass();
    engine.create_descriptor_set_layout();
    engine.create_all_graphics_pipelines();
    engine.create_command_pool();
    engine.create_depth_resources();
    engine.create_framebuffers();
    engine.create_texture_image();
    engine.create_texture_image_view();
    engine.create_texture_sampler();
    engine.create_uniform_buffers();
    engine.create_descriptor_pool();
    engine.create_descriptor_sets();
    engine.create_command_buffers();
    engine.create_sync_objects();
    engine.create_inventory();
    engine.create_particles_buffers();
}

void Bassicraft::init_textures()
{
    bool error = engine.LoadTextureFromFile("img/selected_slot.png", &selected_slot_tex);
    IM_ASSERT(error);
    error = engine.LoadTextureFromFile("img/regular_slot.png", &regular_slot_tex);
    IM_ASSERT(error);
    error = engine.LoadTextureFromFile("img/crosshair.png", &crosshair);
    IM_ASSERT(error);
    error = engine.LoadTextureFromFile("img/texture_atlas.png", &blocks_tex);
    IM_ASSERT(error);
}

void Bassicraft::set_blocks_in_vertex_buffer(Chunk& chunk)
{
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 100; y++) {
            for (int z = 0; z < 16; z++) {
                if (chunk.blocks[x][y][z].type != 0) {
                    int up = 0;
                    int down = 0;
                    int left = 0;
                    int right = 0;
                    int front = 0;
                    int back = 0;
                    if (y > 0) {
                        up = chunk.blocks[x][y - 1][z].type;
                    }
                    if (y < 99) {
                        down = chunk.blocks[x][y + 1][z].type;
                    }
                    if (x > 0) {
                        left = chunk.blocks[x - 1][y][z].type;
                    }
                    if (x < 15) {
                        right = chunk.blocks[x + 1][y][z].type;
                    }
                    if (z > 0) {
                        front = chunk.blocks[x][y][z - 1].type;
                    }
                    if (z < 15) {
                        back = chunk.blocks[x][y][z + 1].type;
                    }
                    if (up || down || left || right || front || back) {
                        engine.add_cube_to_vertices(chunk.blocks[x][y][z], up, down, left, right, front, back, chunk.pos, chunk);
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
                world.push_back(Chunk(glm::vec2(player_chunk.x + x, player_chunk.y + z), noise, biome_noise));
                set_blocks_in_vertex_buffer(world[siz]);
                engine.create_vertex_buffer_chunk(world[siz]);
                engine.create_index_buffer_chunk(world[siz]);
            }
        }
    }
}

void Bassicraft::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_KP_6 && action == GLFW_PRESS) {
        if (is_cursor_locked) {
            is_cursor_locked = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            is_cursor_locked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; key++) {
        if (glfwGetKey(window, key) == GLFW_PRESS) {
            inventory.selected_slot = key - GLFW_KEY_1;
            player.selected_item = inventory.hotbar[inventory.selected_slot];
        }
    }
    if (key == GLFW_KEY_E && action == GLFW_PRESS) {
        if (is_inventory_open) {
            is_inventory_open = false;
            is_cursor_locked = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            is_inventory_open = true;
            is_cursor_locked = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void Bassicraft::mouse_buttons(GLFWwindow* window, int button, int action, int mods)
{
    if (!is_cursor_locked) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            io.AddMouseButtonEvent(button, action);
        }
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        glm::vec4 pos = get_cube_pointed_at(false);
        if (pos.w != -42069 && world[pos.w].blocks[pos.x][pos.y][pos.z].type != 0) {
            engine.create_particles(world[pos.w].blocks[pos.x][pos.y][pos.z].pos, world[pos.w].blocks[pos.x][pos.y][pos.z].type, player);
            // engine.create_particles_buffers();
            remove_cube(world[pos.w], pos, world[pos.w].blocks[pos.x][pos.y][pos.z]);
            engine.recreate_buffers_chunk(world[pos.w]);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        glm::vec4 pos = get_cube_pointed_at(true);
        //std::cout << pos.x << " " << pos.y << " " << pos.z << " " << pos.w << std::endl;
        if (pos.w != -42069 && world[pos.w].blocks[pos.x][pos.y][pos.z].type == 0) {
            Cube cube{};
            cube.type = player.selected_item;
            if (cube.type == 0) {
                cube.type = 1;
            }
            cube.pos = glm::ivec3(pos.x, pos.y, pos.z);
            add_cube(world[pos.w], cube);
            engine.recreate_buffers_chunk(world[pos.w]);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        glm::vec4 pos = get_cube_pointed_at(false);
        if (pos.w != -42069) {
            std::cout << "Block type: " << world[pos.w].blocks[pos.x][pos.y][pos.z].type << std::endl;
        }
    }
}

void Bassicraft::add_cube(Chunk& chunk, Cube& cube)
{
    chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z] = cube;
    if (cube.pos.x == 0 || cube.pos.x == 15 || cube.pos.y == 0 || cube.pos.y == 99 || cube.pos.z == 0 || cube.pos.z == 15) {
        engine.add_cube_to_vertices(chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    } else if (chunk.blocks[cube.pos.x - 1][cube.pos.y][cube.pos.z].type == 0 || chunk.blocks[cube.pos.x + 1][cube.pos.y][cube.pos.z].type == 0 || chunk.blocks[cube.pos.x][cube.pos.y - 1][cube.pos.z].type == 0 || chunk.blocks[cube.pos.x][cube.pos.y + 1][cube.pos.z].type == 0 || chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z - 1].type == 0 || chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z + 1].type == 0) {
        engine.add_cube_to_vertices(chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z], chunk.blocks[cube.pos.x][cube.pos.y - 1][cube.pos.z].type, chunk.blocks[cube.pos.x][cube.pos.y + 1][cube.pos.z].type, chunk.blocks[cube.pos.x - 1][cube.pos.y][cube.pos.z].type, chunk.blocks[cube.pos.x + 1][cube.pos.y][cube.pos.z].type, chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z - 1].type, chunk.blocks[cube.pos.x][cube.pos.y][cube.pos.z + 1].type, chunk.pos, chunk);
    }
}

void Bassicraft::remove_cube(Chunk& chunk, glm::ivec3 pos, Cube& cube)
{
    engine.remove_cube_from_vertices(pos, chunk.pos, chunk, cube);
    chunk.blocks[pos.x][pos.y][pos.z].type = 0;
    if (pos.x > 0 && chunk.blocks[pos.x - 1][pos.y][pos.z].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x - 1, pos.y, pos.z), chunk.pos, chunk, chunk.blocks[pos.x - 1][pos.y][pos.z]);
        chunk.blocks[pos.x - 1][pos.y][pos.z].pos = glm::ivec3(pos.x - 1, pos.y, pos.z);
        engine.add_cube_to_vertices(chunk.blocks[pos.x - 1][pos.y][pos.z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
    if (pos.x < 15 && chunk.blocks[pos.x + 1][pos.y][pos.z].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x + 1, pos.y, pos.z), chunk.pos, chunk, chunk.blocks[pos.x + 1][pos.y][pos.z]);
        chunk.blocks[pos.x + 1][pos.y][pos.z].pos = glm::ivec3(pos.x + 1, pos.y, pos.z);
        engine.add_cube_to_vertices(chunk.blocks[pos.x + 1][pos.y][pos.z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
    if (pos.y > 0 && chunk.blocks[pos.x][pos.y - 1][pos.z].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x, pos.y - 1, pos.z), chunk.pos, chunk, chunk.blocks[pos.x][pos.y - 1][pos.z]);
        chunk.blocks[pos.x][pos.y - 1][pos.z].pos = glm::ivec3(pos.x, pos.y - 1, pos.z);
        engine.add_cube_to_vertices(chunk.blocks[pos.x][pos.y - 1][pos.z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
    if (pos.y < 99 && chunk.blocks[pos.x][pos.y + 1][pos.z].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x, pos.y + 1, pos.z), chunk.pos, chunk, chunk.blocks[pos.x][pos.y + 1][pos.z]);
        chunk.blocks[pos.x][pos.y + 1][pos.z].pos = glm::ivec3(pos.x, pos.y + 1, pos.z);
        engine.add_cube_to_vertices(chunk.blocks[pos.x][pos.y + 1][pos.z], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
    if (pos.z > 0 && chunk.blocks[pos.x][pos.y][pos.z - 1].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x, pos.y, pos.z - 1), chunk.pos, chunk, chunk.blocks[pos.x][pos.y][pos.z - 1]);
        chunk.blocks[pos.x][pos.y][pos.z - 1].pos = glm::ivec3(pos.x, pos.y, pos.z - 1);
        engine.add_cube_to_vertices(chunk.blocks[pos.x][pos.y][pos.z - 1], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
    if (pos.z < 15 && chunk.blocks[pos.x][pos.y][pos.z + 1].type != 0) {
        engine.remove_cube_from_vertices(glm::ivec3(pos.x, pos.y, pos.z + 1), chunk.pos, chunk, chunk.blocks[pos.x][pos.y][pos.z + 1]);
        chunk.blocks[pos.x][pos.y][pos.z + 1].pos = glm::ivec3(pos.x, pos.y, pos.z + 1);
        engine.add_cube_to_vertices(chunk.blocks[pos.x][pos.y][pos.z + 1], 0, 0, 0, 0, 0, 0, chunk.pos, chunk);
    }
}

glm::vec4 Bassicraft::get_cube_pointed_at(bool for_placing)
{
    //Return vec (16, 100, 16) position in the chunk
    //Return -42069 if no cube is pointed at

    glm::vec3 ray = player.camera.front;
    glm::vec3 start = player.camera.pos;

    for (float t = 0.0f; t < 10.0f; t += 0.01f) {
        glm::vec3 position = start + ray * t;
        glm::ivec3 block_position = glm::floor(position);
        glm::vec2 chunk_pos = glm::vec2((int)block_position.x / 16, (int)block_position.z / 16);
        glm::ivec3 block_position_in_chunk = glm::ivec3(regular_modulo(block_position.x, 16), block_position.y, regular_modulo(block_position.z, 16));

        if (block_position.x < 0 && block_position_in_chunk.x != 0) {
            chunk_pos.x--;
        }
        if (block_position.z < 0 && block_position_in_chunk.z != 0) {
            chunk_pos.y--;
        }
        int index = 0;
        for (auto& chunk : world) {
            if (chunk.pos == chunk_pos) {
                if (chunk.blocks[block_position_in_chunk.x][block_position_in_chunk.y][block_position_in_chunk.z].type != 0) {
                    if (for_placing) {
                        glm::ivec3 old_block = block_position;
                        while (block_position == old_block) {
                            position -= ray * 0.01f;
                            block_position = glm::floor(position);
                        }
                        block_position_in_chunk = glm::ivec3(regular_modulo(block_position.x, 16), block_position.y, regular_modulo(block_position.z, 16));
                        chunk_pos = glm::vec2((int)block_position.x / 16, (int)block_position.z / 16);
                        if (block_position.x < 0 && block_position_in_chunk.x != 0) {
                            chunk_pos.x--;
                        }                            
                        if (block_position.z < 0 && block_position_in_chunk.z != 0) {
                            chunk_pos.y--;
                        }
                        index = 0;
                        for (auto& chunk : world) {
                            if (chunk.pos == chunk_pos) {
                                return glm::vec4(block_position_in_chunk, index);
                            }
                            index++;
                        }
                    }
                    return glm::vec4(block_position_in_chunk, index);
                }
            }
            index++;
        }
    }
    return glm::vec4(0, 0, 0, -42069);
}

void Bassicraft::display_hotbar()
{
    ImGui::Begin("Hotbar", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNav);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetWindowPos(ImVec2(io.DisplaySize.x / 2 - 9 * 32, io.DisplaySize.y - 32 * 3));
    ImGui::SetWindowSize(ImVec2(width, 32 * 3));
    ImVec2 cursor_base = ImGui::GetCursorPos();
    for (int i = 0; i < 9; i++) {
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(cursor_base.x + i * 32 * 2, cursor_base.y));
        if (i == inventory.selected_slot) {
            ImGui::Image((ImTextureID)selected_slot_tex.DS, ImVec2(selected_slot_tex.Width * 3.0f, selected_slot_tex.Height * 3.0f));
        } else {
            ImGui::Image((ImTextureID)regular_slot_tex.DS, ImVec2(regular_slot_tex.Width * 3.0f, regular_slot_tex.Height * 3.0f));
        }
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(cursor_base.x + i * 32 * 2 + 8, cursor_base.y + 8));
        ImGui::Image((ImTextureID)blocks_tex.DS, ImVec2(32, 32), ImVec2(((inventory.hotbar[i] - 1) % 16) / 16.0f, ((inventory.hotbar[i] - 1) / 16) / 16.0f), ImVec2(((inventory.hotbar[i] - 1) % 16 + 1) / 16.0f, ((inventory.hotbar[i] - 1) / 16 + 1) / 16.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    ImGui::End();

}

void Bassicraft::display_crosshair()
{
    ImGui::Begin("Crosshair", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNav);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetWindowPos(ImVec2(io.DisplaySize.x / 2 - 16, io.DisplaySize.y / 2 - 16));
    ImGui::SetWindowSize(ImVec2(64, 64));
    ImGui::Image((ImTextureID)crosshair.DS, ImVec2(crosshair.Width * 2.0f, crosshair.Height * 2.0f));
    ImGui::End();
}

void Bassicraft::display_inventory()
{
    ImGui::Begin("Inventory", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNav);
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetWindowPos(ImVec2(io.DisplaySize.x / 2 - 9 * 32, io.DisplaySize.y / 2 - 9 * 32));
    ImGui::SetWindowSize(ImVec2(32 * 9 * 2, 32 * 9 * 2));
    ImGui::Text("Inventory");
    ImVec2 cursor_base = ImGui::GetCursorPos();
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 8; j++) {
            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(cursor_base.x + j * 32 * 2, cursor_base.y + i * 32 * 2));
            ImGui::Image((ImTextureID)regular_slot_tex.DS, ImVec2(regular_slot_tex.Width * 3.0f, regular_slot_tex.Height * 3.0f));
            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(cursor_base.x + j * 32 * 2 + 8, cursor_base.y + i * 32 * 2 + 8));
            int item = i * 8 + j;
            ImGui::Image((ImTextureID)blocks_tex.DS, ImVec2(32, 32), ImVec2((item % 16) / 16.0f, (item / 16) / 16.0f), ImVec2((item % 16 + 1) / 16.0f, (item / 16 + 1) / 16.0f));
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Item %d", item);
                ImGui::EndTooltip();
                if (ImGui::IsItemClicked()) {
                    inventory.hotbar[inventory.selected_slot] = item + 1;
                    player.selected_item = inventory.hotbar[inventory.selected_slot];
                }
            }
        }
    }
    ImGui::End();
}

void Bassicraft::move_player()
{
    // float offset = -0.5;
    // player.camera.pos = glm::vec3(player.camera.pos.x + offset, player.camera.pos.y, player.camera.pos.z + offset);

    if (glfwGetKey(engine.window, GLFW_KEY_W) == GLFW_PRESS) {
        player.velocity.x += player.camera.front.x * player.camera.speed;
        player.velocity.z += player.camera.front.z * player.camera.speed;
    }
    if (glfwGetKey(engine.window, GLFW_KEY_S) == GLFW_PRESS) {
        player.velocity.x -= player.camera.front.x * player.camera.speed;
        player.velocity.z -= player.camera.front.z * player.camera.speed;
    }
    if (glfwGetKey(engine.window, GLFW_KEY_A) == GLFW_PRESS) {
        player.velocity.x -= glm::normalize(glm::cross(player.camera.front, player.camera.up)).x * player.camera.speed;
        player.velocity.z -= glm::normalize(glm::cross(player.camera.front, player.camera.up)).z * player.camera.speed;
    }
    if (glfwGetKey(engine.window, GLFW_KEY_D) == GLFW_PRESS) {
        player.velocity.x += glm::normalize(glm::cross(player.camera.front, player.camera.up)).x * player.camera.speed;
        player.velocity.z += glm::normalize(glm::cross(player.camera.front, player.camera.up)).z * player.camera.speed;
    }
    if (glfwGetKey(engine.window, GLFW_KEY_SPACE) == GLFW_PRESS && chunk_collision(player.camera.pos + glm::vec3(0, 2, 0)) && player.velocity.y == 0) {
        player.velocity += player.camera.up * 0.5f;
        player.is_jumping = true;
    }
    if (glfwGetKey(engine.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && !chunk_collision(player.camera.pos + glm::vec3(0, -2, 0)) && !chunk_collision(player.camera.pos + glm::vec3(0, -3, 0)) && player.is_jumping == false) {
        player.velocity -= player.camera.up * player.camera.speed;
    }
    if (chunk_collision(player.camera.pos +  glm::vec3(0, 2, 0)) && player.velocity.y > 0) {
        player.velocity.y = 0;
        player.is_jumping = false;
    }
    if (!chunk_collision(player.camera.pos + glm::vec3(0, 1.8, 0))) {
        player.velocity.y += 0.02f;
        player.velocity.y *= 1.5f;
        player.is_jumping = true;
    }
    if (chunk_collision(player.camera.pos + glm::vec3(0, -0.2, 0)) && player.velocity.y < 0) {
        player.velocity.y = 0;
    }
    if (chunk_collision(player.camera.pos + glm::vec3(player.velocity.x, player.velocity.y + 1, 0)) || chunk_collision(player.camera.pos + glm::vec3(player.velocity.x, player.velocity.y, 0))) {
        player.velocity.x = 0;
    }
    if (chunk_collision(player.camera.pos + glm::vec3(0, player.velocity.y + 1, player.velocity.z)) || chunk_collision(player.camera.pos + glm::vec3(0, player.velocity.y, player.velocity.z))) {
        player.velocity.z = 0;
    }
    player.camera.pos += player.velocity;
    player.velocity *= 0.6f;

    //player.camera.pos = glm::vec3(player.camera.pos.x - offset, player.camera.pos.y, player.camera.pos.z - offset);
}

bool Bassicraft::chunk_collision(glm::vec3 pos)
{
    glm::vec2 chunk_pos = glm::vec2((int)pos.x / 16, (int)pos.z / 16);
    glm::ivec3 block_pos = glm::ivec3(regular_modulo(floor(pos.x), 16), floor(pos.y), regular_modulo(floor(pos.z), 16));

    if (pos.x < 0) {
        chunk_pos.x--;
    }
    if (pos.z < 0) {
        chunk_pos.y--;
    }
    for (auto& chunk : world) {
        if (chunk.pos == chunk_pos) {
            if (chunk.blocks[block_pos.x][block_pos.y][block_pos.z].type != 0) {
                return true;
            } else {
                return false;
            }
        }
    }
    return true;
}

Bassicraft::~Bassicraft()
{
    engine.wait_idle();

    engine.RemoveTexture(&selected_slot_tex);
    engine.RemoveTexture(&regular_slot_tex);
    engine.RemoveTexture(&crosshair);
    engine.RemoveTexture(&blocks_tex);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    for (auto& chunk : world) {
        engine.free_buffers_chunk(chunk);
    }
    glfwDestroyWindow(engine.window);
    glfwTerminate();
}
