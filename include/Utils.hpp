#pragma once

#include <vector>
#include <string>
#include <fstream>

#include <vulkan/vulkan.h>

std::vector<char> read_file(const std::string &filename);
VkShaderModule create_shader_module(const std::vector<char> &code, VkDevice device);
int regular_modulo(int a, int b);
float rand_float(float smallNumber, float bigNumber);
