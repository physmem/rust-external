#include "slider.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>
#include <algorithm>
#include <format>
#include <sstream>
#include <iomanip>
#include <print>
#include "../color_picker/color_picker.hpp"
#include "../popup/popup.hpp"

namespace gui
{
    template <typename T>
    Slider<T>::Slider(std::string_view label_name, T min_val, T max_val, T init_value, std::string_view suffix_str, int prec, T stp)
        : Object(label_name), label(label_name), min(min_val), max(max_val), value(init_value), m_default_value(init_value), suffix(suffix_str), precision(prec), step(stp)
    {
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
        YGNodeStyleSetHeight(yoga_node, 18.0f + 10.0f);
    }

    template <typename T>
    std::string Slider<T>::get_value_string()
    {
        if (format_value)
            return format_value(value);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(precision) << value << suffix;
        return ss.str();
    }

    template <typename T>
    void Slider<T>::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        hover_anim.update(is_hovered ? 1.0f : 0.0f);

        Color text_clr = style::colors::text_inactive.lerp(style::colors::text_active, hover_anim);
        if (is_dragging || (Object::g_active_object == this))
            text_clr = style::colors::text_active;

        render::add_text(render::Fonts::NotoSans16px, label, pos, text_clr, render::TextFlagsNone, { 0.0f, 0.0f });

        float deco_size = 14.0f;
        float offset_x = 0.0f;

        for (auto& child : children)
        {
            if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
            {
                glm::vec2 box_pos = pos + glm::vec2{ size.x - deco_size - offset_x, -1.0f };
                cp->last_box_pos = box_pos;

                render::add_shadow_rect(box_pos, glm::vec2{ deco_size }, style::colors::window_shadow, 15.0f, deco_size);
                render::add_rect_filled(box_pos, glm::vec2{ deco_size }, cp->value, deco_size);

                offset_x += deco_size + style::padding;
            }
            else if (auto p = dynamic_cast<Popup*>(child.get()))
            {
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeRight, offset_x);
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, -1.0f);

                offset_x += 16.0f + style::padding;
            }
        }

        render::add_text(render::Fonts::NotoSans16px, get_value_string(), pos + glm::vec2{ size.x - offset_x, 0.0f }, text_clr, render::TextFlagsNone, { 1.0f, 0.0f });

        glm::vec2 slider_pos = { pos.x, pos.y + 18.0f + 5.0f };
        glm::vec2 slider_size = { size.x, 5.0f };

        render::add_rect_filled(slider_pos, slider_size, style::colors::control_bg, style::widget_rounding);
        render::add_rect(slider_pos, slider_size, style::colors::window_border, style::widget_rounding, 2.0f);

        float percentage = static_cast<float>(value - min) / static_cast<float>(max - min);
        percentage_anim.update(percentage);

        float fill_width = std::clamp(slider_size.x * percentage_anim.value, 0.0f, slider_size.x);
        if (fill_width > 0.0f) {
            render::add_rect_filled(slider_pos, { fill_width, slider_size.y }, style::colors::accent, style::widget_rounding);
        }

        Object::render();
    }

    template <typename T>
    void Slider<T>::dispatch_event(Event& e)
    {
        Object::dispatch_event(e);
        if (e.handled) return;

        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };
        glm::vec2 slider_pos = { pos.x, pos.y + 18.0f + 5.0f };
        glm::vec2 slider_size = { size.x, 5.0f };

        float deco_size = 14.0f;
        float offset_x = 0.0f;

        for (auto& child : children)
        {
            if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
            {
                glm::vec2 box_pos = pos + glm::vec2{ size.x - deco_size - offset_x, -1.0f };

                if (e.get_type() == EventType::MouseMove) {
                    auto& me = static_cast<MouseMoveEvent&>(e);
                    if (input::in_bounds(me.position, box_pos, { deco_size, deco_size })) {
                        is_hovered = true;
                        e.handled = true;
                    }
                }
                else if (e.get_type() == EventType::MouseButton) {
                    auto& me = static_cast<MouseButtonEvent&>(e);
                    if (me.button == MouseButton::Left && me.pressed) {
                        if (input::in_bounds(me.position, box_pos, { deco_size, deco_size })) {
                            cp->is_open = true;
                            if (cp->is_open) Object::g_active_object = cp;
                            e.handled = true;
                            return;
                        }
                    }
                }
                offset_x += deco_size + style::padding;
            }
            else if (auto p = dynamic_cast<Popup*>(child.get()))
            {
                offset_x += 16.0f + style::padding;
            }
        }

        if (e.handled) return;

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, pos, size);

            if (is_dragging)
            {
                float percentage = std::clamp((me.position.x - slider_pos.x) / slider_size.x, 0.0f, 1.0f);
                T new_val = min + static_cast<T>(percentage * static_cast<float>(max - min));

                if (step > static_cast<T>(0)) {
                    new_val = min + std::round((new_val - min) / step) * step;
                }

                new_val = std::clamp(new_val, min, max);

                if (new_val != value) {
                    value = new_val;
                    if (on_change) on_change(value);
                }
                e.handled = true;
            }
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left)
            {
                if (me.pressed)
                {
                    bool in_bounds = input::in_bounds(me.position, pos, size);
                    if (in_bounds)
                    {
                        is_dragging = true;
                        Object::g_active_object = this;

                        float percentage = std::clamp((me.position.x - slider_pos.x) / slider_size.x, 0.0f, 1.0f);
                        T new_val = min + static_cast<T>(percentage * static_cast<float>(max - min));
                        if (step > static_cast<T>(0)) {
                            new_val = min + std::round((new_val - min) / step) * step;
                        }
                        new_val = std::clamp(new_val, min, max);
                        if (new_val != value) {
                            value = new_val;
                            if (on_change) on_change(value);
                        }
                        e.handled = true;
                    }
                }
                else
                {
                    if (is_dragging) {
                        is_dragging = false;

                        Object* ancestor = this->parent;
                        while (ancestor && !dynamic_cast<Popup*>(ancestor)) {
                            ancestor = ancestor->parent;
                        }

                        Object::g_active_object = ancestor;
                        Object::g_focused_object = ancestor;

                        e.handled = true;
                    }
                }
            }
        }
    }

    template <typename T>
    float Slider<T>::get_preferred_width()
    {
        glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans16px, label);
        return text_size.x + style::padding * 5.0f;
    }

    template class Slider<int>;
    template class Slider<float>;
}
