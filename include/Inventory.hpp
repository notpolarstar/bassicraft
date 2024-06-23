#pragma once

#include <vector>
#include <array>

class Inventory
{
private:
public:
    std::array<int, 9> hotbar{1, 2, 3, 5, 8, 17, 18, 19, 20};
    int selected_slot = 0;

    // void inventory_add_item(int item_id, int amount);

    // Inventory(/* args */);
    // ~Inventory();
};

// Inventory::Inventory(/* args */)
// {
// }

// void Inventory::inventory_add_item(int item_id, int amount)
// {
// }

// Inventory::~Inventory()
// {
// }
