#pragma once

#include <vulkan/vulkan.h>
#include "Vertex.hpp"

const int MAX_FRAMES_IN_FLIGHT = 2;
const int MAX_PARTICLES = 200;
const VkDeviceSize staging_buffer_size = sizeof(Vertex) * 10000;