#include <imgui.h>
#include <input/input.hpp>

namespace input
{
    bool is_mouse_clicked(bool right_click)
    {
        return ImGui::IsMouseClicked(right_click ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
    }

    bool is_mouse_down(bool right_click)
    {
        return ImGui::IsMouseDown(right_click ? ImGuiMouseButton_Right : ImGuiMouseButton_Left);
    }

    bool in_bounds(glm::vec2 base, glm::vec2 pos, glm::vec2 size)
    {
        return base.x > pos.x && base.y > pos.y && base.x <= pos.x + size.x && base.y <= pos.y + size.y;
    }

    glm::vec2 mouse_position()
    {
        return glm::vec2{ ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y };
    }
}
