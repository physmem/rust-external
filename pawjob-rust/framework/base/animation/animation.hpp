#pragma once

namespace gui
{
    class Animation
    {
    public:
        void update(const float& target, const float& speed = 25.0f);

        constexpr operator float() const
        {
            return value;
        }

        constexpr float operator~() const
        {
            return 1.0f - value;
        }

        float value = 0.0f;
    };
}
