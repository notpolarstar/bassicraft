#include <iostream>
#include <cstring>
#include <stdexcept>

#include "VkEngine.hpp"
#include "Chunk.hpp"
#include "FastNoiseLite.hpp"

Chunk::Chunk(glm::vec2 pos) : pos(pos)
{
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetSeed(1337);
    noise.SetFrequency(0.01);

    for (int x = 0; x < 16; x++)
    {
        for (int z = 0; z < 16; z++)
        {
            int height = (int)(noise.GetNoise((float)(x + pos.x * 16), (float)(z + pos.y * 16)) * 10) + 10;
            //std::cout << height << std::endl;
            for (int y = 20; y > height - 1; y--)
            {
                blocks[x][y][z] = {glm::ivec3(x, y, z), 3};
            }
            if (height >= 16) {
                blocks[x][height][z] = {glm::ivec3(x, height, z), 19};
            } else {
                blocks[x][height][z] = {glm::ivec3(x, height, z), 1};
            }
            for (int y = height - 1; y > 15; y--)
            {
                blocks[x][y][z] = {glm::ivec3(x, y, z), 206};
            }
        }
    }
}

Chunk::~Chunk()
{
}
