#include "button.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

namespace gui
{
    Button::Button(std::string_view label_name)
        : Object(label_name), label(label_name)
    {
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetHeight(yoga_node, 28.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
    }

    void Button::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        hover_anim.update(is_hovered ? 1.0f : 0.0f);

        render::add_shadow_rect(pos, size, style::colors::window_shadow, 10.0f, style::widget_rounding);
        render::add_rect_filled(pos, size, style::colors::control_bg.lerp(style::colors::hovered_control_bg, hover_anim), style::widget_rounding);
        render::add_rect(pos, size, style::colors::window_border.lerp(style::colors::accent.scale_alpha(0.4f), hover_anim), style::widget_rounding, 2.0f);

        if (press_anim.value > 0.01f)
        {
            render::add_rect_filled(pos, size, style::colors::accent.scale_alpha(press_anim.value * 0.3f), style::widget_rounding);
        }
        press_anim.update(0.0f);

        Color text_color = style::colors::text_inactive.lerp(style::colors::text_active, hover_anim);
        render::add_text(
            render::Fonts::NotoSans16px,
            label,
            pos + glm::vec2{ size.x * 0.5f, size.y * 0.5f },
            text_color,
            render::TextFlagsNone,
            { 0.5f, 0.5f }
        );
    }

    void Button::dispatch_event(Event& e)
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, pos, size);
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed)
            {
                if (input::in_bounds(me.position, pos, size))
                {
                    press_anim = Animation(1.0f);
                    if (on_press) on_press();
                    e.handled = true;
                }
            }
        }
    }
}
