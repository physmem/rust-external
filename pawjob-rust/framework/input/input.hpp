#pragma once
#include <glm/vec2.hpp>

namespace input
{
    bool is_mouse_clicked(bool right_click = false);
    bool is_mouse_down(bool right_click = false);

    bool in_bounds(glm::vec2 base, glm::vec2 pos, glm::vec2 size);

    glm::vec2 mouse_position();
}
