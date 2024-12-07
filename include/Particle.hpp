#pragma once

#include <glm/glm.hpp>

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float size;
    float life;
    glm::vec3 offset{};
};
