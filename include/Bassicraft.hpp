#pragma once

#include <vector>
#include <array>

#include <GLFW/glfw3.h>

#include "VkEngine.hpp"
#include "Cube.hpp"
#include "Player.hpp"
#include "Chunk.hpp"
#include "FastNoiseLite.hpp"

class Bassicraft
{
private:
    VkEngine engine;
    Player player;
    
    FastNoiseLite noise;
    int render_distance = 5;

    std::vector<Chunk> world;
public:
    Bassicraft(/* args */);
    ~Bassicraft();

    void add_cube(Chunk& chunk, Cube& cube);
    void remove_cube(Chunk& chunk, glm::ivec3 pos);
    void set_blocks_in_vertex_buffer(Chunk& chunk);
    void unload_load_new_chunks();
    void mouse_buttons(GLFWwindow* window, int button, int action, int mods);
    glm::vec4 get_cube_pointed_at();
};
