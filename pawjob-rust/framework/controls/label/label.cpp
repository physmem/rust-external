#include "label.hpp"
#include <utils/style.hpp>
#include <input/input.hpp>
#include "../color_picker/color_picker.hpp"
#include "../popup/popup.hpp"

namespace gui
{
    Label::Label(std::string_view text_to_display) 
        : Object(ConstName("Label")), text(text_to_display)
    {
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetHeight(yoga_node, 20.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
    }

    void Label::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        render::add_text(render::Fonts::NotoSans16px, text, pos + glm::vec2{ 0, size.y * 0.5f }, style::colors::text_active, render::TextFlagsNone, { 0.0f, 0.5f });

        float deco_size = 14.0f;
        float offset_x = 0.0f;

        for (auto& child : children)
        {
            if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
            {
                glm::vec2 box_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (size.y - deco_size) * 0.5f };
                cp->last_box_pos = box_pos;

                render::add_shadow_rect(box_pos, glm::vec2{ deco_size }, style::colors::window_shadow, 15.0f, deco_size);
                render::add_rect_filled(box_pos, glm::vec2{ deco_size }, cp->value, deco_size);

                offset_x += deco_size + style::padding;
            }
            else if (auto p = dynamic_cast<Popup*>(child.get()))
            {
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeRight, offset_x);
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (size.y - 16.0f) * 0.5f);

                offset_x += 16.0f + style::padding;
            }
        }

        Object::render();
    }

    void Label::dispatch_event(Event& e)
    {
        Object::dispatch_event(e);
        if (e.handled) return;

        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };
        float deco_size = 14.0f;
        float offset_x = 0.0f;

        for (auto& child : children)
        {
            if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
            {
                glm::vec2 box_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (size.y - deco_size) * 0.5f };

                if (e.get_type() == EventType::MouseMove)
                {
                    auto& me = static_cast<MouseMoveEvent&>(e);
                    if (input::in_bounds(me.position, box_pos, { deco_size, deco_size })) {
                        is_hovered = true;
                        e.handled = true;
                    }
                }
                else if (e.get_type() == EventType::MouseButton)
                {
                    auto& me = static_cast<MouseButtonEvent&>(e);
                    if (me.button == MouseButton::Left && me.pressed)
                    {
                        if (input::in_bounds(me.position, box_pos, { deco_size, deco_size }))
                        {
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
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeRight, offset_x);
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (size.y - 16.0f) * 0.5f);

                offset_x += 16.0f + style::padding;
            }
        }

        if (e.get_type() == EventType::MouseMove) {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, pos, { size.x - offset_x, size.y });
        }
    }
}
