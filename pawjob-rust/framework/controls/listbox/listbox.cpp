#include "listbox.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>
#include <algorithm>

namespace gui
{
    Listbox::Listbox(std::string_view label_name, const std::vector<std::string>& opts, int max_visible)
        : Object(label_name), label(label_name), options(opts), max_items(max_visible)
    {
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);

        item_hovers.resize(options.size(), false);
        item_hover_anims.resize(options.size());
        item_active_anims.resize(options.size());
    }

    void Listbox::update_layout()
    {
        float box_h = get_item_height() * max_items;
        m_base_height = show_label ? (16.0f + 6.0f + box_h) : box_h;
        Object::update_layout();
    }

    void Listbox::refresh_options(const std::vector<std::string>& new_options)
    {
        options = new_options;
        item_hovers.resize(options.size(), false);
        item_hover_anims.resize(options.size());
        item_active_anims.resize(options.size());

        if (value >= (int)options.size())
            value = options.empty() ? 0 : (int)options.size() - 1;

        scroll_offset = 0.0f;
    }

    void Listbox::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        hover_anim.update(is_hovered ? 1.0f : 0.0f);

        if (item_hover_anims.size() != options.size())
        {
            item_hovers.resize(options.size(), false);
            item_hover_anims.resize(options.size());
            item_active_anims.resize(options.size());
        }

        if (show_label)
        {
            Color label_color = style::colors::text_inactive.lerp(style::colors::text_active, hover_anim.value);
            render::add_text(render::Fonts::NotoSans16px, label, pos, label_color, render::TextFlagsNone, { 0.0f, 0.0f });
        }

        float item_h = get_item_height();
        float box_h = item_h * max_items;
        float box_y_offset = show_label ? (16.0f + 6.0f) : 0.0f;
        glm::vec2 box_pos = { pos.x, pos.y + box_y_offset };
        glm::vec2 box_size = { size.x, box_h };

        render::add_shadow_rect(box_pos, box_size, style::colors::window_shadow, 10.0f, style::widget_rounding);
        render::add_rect_filled(box_pos, box_size, style::colors::control_bg, style::widget_rounding);
        render::add_rect(box_pos, box_size, style::colors::window_border, style::widget_rounding, 2.0f);

        scroll_offset += scroll_velocity;
        scroll_velocity *= 0.85f;

        float total_content = item_h * options.size();
        float max_scroll = std::max(0.0f, total_content - box_h);
        scroll_offset = std::clamp(scroll_offset, 0.0f, max_scroll);

        // Items
        render::push_clip_rect(box_pos, box_size);

        for (int i = 0; i < (int)options.size(); ++i)
        {
            glm::vec2 item_pos = { box_pos.x, box_pos.y + (i * item_h) - scroll_offset };
            glm::vec2 item_size = { box_size.x, item_h };

            if (item_pos.y + item_size.y < box_pos.y || item_pos.y > box_pos.y + box_size.y)
                continue;

            bool selected = (value == i);

            item_hover_anims[i].update(item_hovers[i] ? 1.0f : 0.0f);
            item_active_anims[i].update(selected ? 1.0f : 0.0f);

            if (item_hover_anims[i].value > 0.0f)
            {
                render::add_rect_filled(
                    item_pos + glm::vec2{ 2, 2 },
                    { item_size.x - 4, item_size.y - 4 },
                    Color(20, 20, 20).scale_alpha(item_hover_anims[i].value),
                    style::widget_rounding
                );
            }

            Color text_color = style::colors::text_inactive
                .lerp(style::colors::text_active, item_hover_anims[i])
                .lerp(style::colors::accent, selected ? 1.0f : 0.0f);

            float indent = style::padding + (5.0f * item_hover_anims[i].value);

            render::add_text(
                render::Fonts::NotoSans16px,
                options[i],
                item_pos + glm::vec2{ indent, item_h * 0.5f },
                text_color,
                render::TextFlagsNone,
                { 0.0f, 0.5f }
            );
        }

        render::pop_clip_rect();

        if (total_content > box_h)
        {
            float scroll_scale = box_h / total_content;
            float thumb_h = std::max(10.0f, box_h * scroll_scale);
            float scroll_pct = scroll_offset / max_scroll;
            float thumb_y = box_pos.y + 2.0f + (scroll_pct * (box_h - 4.0f - thumb_h));

            render::add_capsule(
                { box_pos.x + box_size.x - style::scrollbar_width - 4.0f, thumb_y },
                { style::scrollbar_width, thumb_h },
                style::colors::accent.lerp(style::colors::tab_bg, 0.5f)
            );
        }
    }

    void Listbox::dispatch_event(Event& e)
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        float item_h = get_item_height();
        float box_h = item_h * max_items;
        float box_y_offset = show_label ? (16.0f + 6.0f) : 0.0f;
        glm::vec2 box_pos = { pos.x, pos.y + box_y_offset };
        glm::vec2 box_size = { size.x, box_h };

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, box_pos, box_size);

            for (int i = 0; i < (int)options.size(); ++i)
            {
                glm::vec2 item_pos = { box_pos.x, box_pos.y + (i * item_h) - scroll_offset };
                glm::vec2 item_size = { box_size.x, item_h };

                bool in_item = is_hovered && input::in_bounds(me.position, item_pos, item_size);
                item_hovers[i] = in_item;
            }
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed && is_hovered)
            {
                for (int i = 0; i < (int)options.size(); ++i)
                {
                    glm::vec2 item_pos = { box_pos.x, box_pos.y + (i * item_h) - scroll_offset };
                    glm::vec2 item_size = { box_size.x, item_h };

                    if (input::in_bounds(me.position, item_pos, item_size))
                    {
                        value = i;
                        e.handled = true;
                        break;
                    }
                }
            }
        }
        else if (e.get_type() == EventType::MouseScroll)
        {
            if (is_hovered)
            {
                auto& se = static_cast<MouseScrollEvent&>(e);
                scroll_velocity -= se.offset * 10.0f;
                e.handled = true;
            }
        }
    }
}
