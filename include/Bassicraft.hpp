#pragma once

#include <vector>
#include <array>

#include <GLFW/glfw3.h>

#include "VkEngine.hpp"
#include "Cube.hpp"
#include "Player.hpp"
#include "Chunk.hpp"
#include "FastNoiseLite.hpp"
#include "TextureDataStruct.hpp"
#include "Inventory.hpp"

class Bassicraft
{
private:
    VkEngine engine;
    Player player;
    
    FastNoiseLite noise;
    FastNoiseLite biome_noise;

    int render_distance = 12;
    bool is_cursor_locked = true;

    std::vector<Chunk> world;

    MyTextureData crosshair;

    Inventory inventory;
    MyTextureData blocks_tex;
    MyTextureData selected_slot_tex;
    MyTextureData regular_slot_tex;
    bool is_inventory_open = false;

    int width;
    int height;
public:
    Bassicraft(/* args */);
    ~Bassicraft();

    void init_engine();
    void init_textures();
    void add_cube(Chunk& chunk, Cube& cube);
    void remove_cube(Chunk& chunk, glm::ivec3 pos, Cube& cube);
    void set_blocks_in_vertex_buffer(Chunk& chunk);
    void unload_load_new_chunks();
    void mouse_buttons(GLFWwindow* window, int button, int action, int mods);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    glm::vec4 get_cube_pointed_at(bool for_placing);
    void display_hotbar();
    void display_crosshair();
    void display_inventory();
    void move_player();
    bool chunk_collision(glm::vec3 pos);
};
