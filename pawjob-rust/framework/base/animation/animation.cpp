#include <base/animation/animation.hpp>
#include <imgui.h>

namespace gui
{
    void Animation::update(const float& target, const float& speed)
    {
        value += (target - value) * (speed * ImGui::GetIO().DeltaTime);

        if (value > target - 0.02f && value < target + 0.02f)
            value = target;
    }
}
