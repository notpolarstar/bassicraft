#include <iostream>
#include <cstring>
#include <stdexcept>

#include "VkEngine.hpp"
#include "Chunk.hpp"

Chunk::Chunk(glm::vec2 pos, FastNoiseLite& noise) : pos(pos)
{
    for (int x = 0; x < 16; x++)
    {
        for (int z = 0; z < 16; z++)
        {
            int height = (int)(noise.GetNoise((float)(x + pos.x * 16), (float)(z + pos.y * 16)) * 10) + 10;
            //std::cout << height << std::endl;
            for (int y = 20; y > height - 1; y--)
            {
                blocks[x][y][z] = {glm::ivec3(x, y, z), 3, 0};
            }
            if (height >= 16) {
                blocks[x][height][z] = {glm::ivec3(x, height, z), 19, 0};
            } else {
                blocks[x][height][z] = {glm::ivec3(x, height, z), 1, 0};
            }
            for (int y = height - 1; y > 15; y--)
            {
                blocks[x][y][z] = {glm::ivec3(x, y, z), 206, 0};
            }
            // if (rand() % 100 == 5 && height <= 16) {
            //     put_tree(glm::ivec3(x, height, z));
            // }
        }
    }
}

void Chunk::put_tree(glm::ivec3 pos)
{
    if (pos.y < 5 || pos.y > 95 || pos.x < 2 || pos.x > 13 || pos.z < 2 || pos.z > 13) {
        return;
    }
    for (int y = pos.y - 4; y < pos.y - 2; y++) {
        for (int x = pos.x - 2; x < pos.x + 3; x++) {
            for (int z = pos.z - 2; z < pos.z + 3; z++) {
                blocks[x][y][z] = {glm::ivec3(x, y, z), 54};
            }
        }
    }
    for (int x = pos.x - 1; x < pos.x + 2; x++) {
        for (int z = pos.z - 1; z < pos.z + 2; z++) {
            blocks[x][pos.y - 5][z] = {glm::ivec3(x, pos.y - 5, z), 54};
        }
    }
    for (int y = pos.y - 4; y < pos.y; y++) {
        blocks[pos.x][y][pos.z] = {glm::ivec3(pos.x, y, pos.z), 21};
    }
    blocks[pos.x][pos.y - 6][pos.z] = {glm::ivec3(pos.x, pos.y - 6, pos.z), 54};
    blocks[pos.x - 1][pos.y - 6][pos.z] = {glm::ivec3(pos.x - 1, pos.y - 6, pos.z), 54};
    blocks[pos.x + 1][pos.y - 6][pos.z] = {glm::ivec3(pos.x + 1, pos.y - 6, pos.z), 54};
    blocks[pos.x][pos.y - 6][pos.z - 1] = {glm::ivec3(pos.x, pos.y - 6, pos.z - 1), 54};
    blocks[pos.x][pos.y - 6][pos.z + 1] = {glm::ivec3(pos.x, pos.y - 6, pos.z + 1), 54};
}

Chunk::~Chunk()
{
}
