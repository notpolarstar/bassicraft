#pragma once

#include <glm/glm.hpp>

struct Cube
{
    glm::ivec3 pos;
    uint16_t type;
    uint16_t faces = 0;
    bool is_displayed = false;
};
