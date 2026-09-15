#include "multi_dropdown.hpp"
#include <render/render.hpp>
#include <input/input.hpp>
#include <utils/style.hpp>
#include <render/assets/font_awesome.hpp>
#include <algorithm>
#include <print>
#include "../color_picker/color_picker.hpp"
#include "../popup/popup.hpp"

namespace gui
{
    MultiDropdown::MultiDropdown(std::string_view label, int init_value, const std::vector<std::string>& items)
        : Object(label), label(label), value(init_value), m_default_value(init_value), items(items)
    {
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);

        YGNodeStyleSetHeight(yoga_node, 16.0f + 6.0f + get_box_height());

        item_anims.resize(items.size());
        item_hovers.resize(items.size(), false);
    }

    void MultiDropdown::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        open_anim.update(is_open ? 1.0f : 0.0f);
        hover_anim.update(is_hovered ? 1.0f : 0.0f);

        Color label_color = style::colors::text_inactive.lerp(style::colors::text_active, open_anim.value);
        render::add_text(render::Fonts::NotoSans16px, label, pos, label_color, render::TextFlagsNone, { 0.0f, 0.0f });

        glm::vec2 box_pos = { pos.x, pos.y + 16.0f + 6.0f };
        glm::vec2 box_size = { size.x, get_box_height() };

        render::add_shadow_rect(box_pos, box_size, style::colors::window_shadow, 10.0f, style::widget_rounding);
        render::add_rect_filled(box_pos, box_size, style::colors::control_bg, style::widget_rounding);
        render::add_rect(box_pos, box_size, style::colors::window_border, style::widget_rounding, 2.0f);

        std::string current_text = "";
        std::vector<std::string_view> selected_options = {};

        for (std::int32_t index = 0; auto& opt : items)
        {
            if (value & (1 << index))
                selected_options.push_back(opt);

            ++index;
        }

        if (selected_options.empty())
        {
            current_text = "";
        }
        else
        {
            for (std::int32_t index = 0; auto& opt : selected_options)
            {
                if (index > 0)
                    current_text += ", ";

                current_text += opt;
                ++index;
            }
        }

        float max_text_width = size.x - (style::padding * 3.0f) - (box_size.y * 0.5f);
        if (render::get_text_size(render::Fonts::NotoSans16px, current_text).x > max_text_width)
        {
            std::string clipped_text = current_text;
            const glm::vec2 ellipsis_size = render::get_text_size(render::Fonts::NotoSans16px, "...");
            const float available_width_with_ellipsis = max_text_width - ellipsis_size.x;

            while (!clipped_text.empty())
            {
                const glm::vec2 current_size = render::get_text_size(render::Fonts::NotoSans16px, clipped_text);
                if (current_size.x <= available_width_with_ellipsis)
                {
                    current_text = clipped_text + "...";
                    break;
                }
                clipped_text.pop_back();
            }

            if (clipped_text.empty())
            {
                current_text = "...";
            }
        }

        render::add_text(render::Fonts::NotoSans16px, current_text, box_pos + glm::vec2{ style::padding, box_size.y * 0.5f }, style::colors::text_active, render::TextFlagsNone, { 0.0f, 0.5f });

        render::rotate_vertices(0.25f + (0.5f * open_anim.value), [&] {
            render::add_text(
                render::Fonts::Icons16px,
                ICON_FA_CHEVRON_DOWN,
                box_pos + glm::vec2{ box_size.x - (style::padding * 1.5f), box_size.y * 0.5f - 1.0f },
                style::colors::text_inactive.lerp(style::colors::accent, open_anim),
                render::TextFlagsNone,
                { 0.5f, 0.5f }
            );
            });

        if (!is_open && open_anim.value <= 1e-3f && Object::g_active_object == this)
        {
            Object* ancestor = this->parent;
            while (ancestor && !dynamic_cast<Popup*>(ancestor)) {
                ancestor = ancestor->parent;
            }

            Object::g_active_object = ancestor;
            Object::g_focused_object = ancestor;
        }

        float deco_size = 14.0f;
        float offset_x = 0.0f;

        for (auto& child : children)
        {
            if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
            {
                glm::vec2 cbox_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (16.0f - deco_size) * 0.5f };
                cp->last_box_pos = cbox_pos;

                render::add_shadow_rect(cbox_pos, glm::vec2{ deco_size }, style::colors::window_shadow, 15.0f, deco_size);
                render::add_rect_filled(cbox_pos, glm::vec2{ deco_size }, cp->value, deco_size);

                offset_x += deco_size + style::padding;
            }
            else if (auto p = dynamic_cast<Popup*>(child.get()))
            {
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeRight, offset_x);
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (16.0f - 16.0f) * 0.5f);

                offset_x += 16.0f + style::padding;
            }
        }

        Object::render();
    }

    void MultiDropdown::render_overlay()
    {
        if (open_anim.value <= 0.01f) return;

        glm::vec2 pos = get_absolute_position();
        float box_h = get_box_height();
        float item_h = get_item_height();

        glm::vec2 box_pos = { pos.x, pos.y + 16.0f + 6.0f };
        glm::vec2 list_pos = { box_pos.x, box_pos.y + box_h + style::padding };
        glm::vec2 list_size = { get_width(), (item_h * items.size()) * open_anim.value };

        std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;

        render::add_shadow_rect(list_pos, list_size, style::colors::window_shadow, 15.0f, style::widget_rounding);
        render::add_rect_filled(list_pos, list_size, style::colors::tab_bg, style::widget_rounding);
        render::add_rect(list_pos, list_size, style::colors::window_border, style::widget_rounding, 2.0f);

        render::push_clip_rect(list_pos, list_size);
        {
            for (int i = 0; i < (int)items.size(); ++i)
            {
                item_anims[i].update(item_hovers[i] ? 1.0f : 0.0f);

                glm::vec2 item_pos = { list_pos.x, list_pos.y + (i * item_h) };

                if (item_anims[i].value > 0.0f)
                {
                    render::add_rect_filled(item_pos + glm::vec2{ 2, 2 }, { list_size.x - 4, item_h - 4 }, Color(20, 20, 20).scale_alpha(item_anims[i].value), style::widget_rounding);
                }

                bool is_selected = (value & (1 << i));
                Color text_color = style::colors::text_inactive.lerp(style::colors::text_active, item_anims[i]).lerp(style::colors::accent, is_selected ? 1.0f : 0.0f);

                render::add_text(
                    render::Fonts::NotoSans16px,
                    items[i],
                    item_pos + glm::vec2{ style::padding + (5.0f * item_anims[i].value), item_h * 0.5f },
                    text_color,
                    render::TextFlagsNone,
                    { 0.0f, 0.5f }
                );
            }
        }
        render::pop_clip_rect();

        std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;
        if (open_anim.value < 1.0f)
            render::modify_alpha(start_vtx, end_vtx, open_anim.value);
    }

    void MultiDropdown::on_event(Event& e)
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        glm::vec2 box_pos = { pos.x, pos.y + 16.0f + 6.0f };
        glm::vec2 box_size = { size.x, get_box_height() };

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, box_pos, box_size);

            if (is_open)
            {
                float item_h = get_item_height();
                glm::vec2 list_pos = { box_pos.x, box_pos.y + box_size.y + style::padding };

                for (int i = 0; i < (int)items.size(); ++i)
                {
                    glm::vec2 item_pos = { list_pos.x, list_pos.y + (i * item_h) };
                    item_hovers[i] = input::in_bounds(me.position, item_pos, { box_size.x, item_h });
                }
                e.handled = true;
            }
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed)
            {
                if (input::in_bounds(me.position, box_pos, box_size) && !is_open)
                {
                    is_open = true;
                    if (is_open) Object::g_active_object = this;
                    e.handled = true;
                    return;
                }

                if (is_open)
                {
                    float item_h = get_item_height();
                    glm::vec2 list_pos = { box_pos.x, box_pos.y + box_size.y + style::padding };
                    glm::vec2 list_size = { size.x, item_h * items.size() };

                    if (input::in_bounds(me.position, list_pos, list_size))
                    {
                        for (int i = 0; i < (int)items.size(); ++i)
                        {
                            glm::vec2 item_pos = { list_pos.x, list_pos.y + (i * item_h) };
                            if (input::in_bounds(me.position, item_pos, { size.x, item_h }))
                            {
                                value ^= (1 << i);
                                if (on_change) on_change(value);
                                e.handled = true;

                                break;
                            }
                        }
                    }
                    else
                    {
                        is_open = false;

                        Object* ancestor = this->parent;
                        while (ancestor && !dynamic_cast<Popup*>(ancestor)) {
                            ancestor = ancestor->parent;
                        }

                        Object::g_active_object = ancestor;
                        Object::g_focused_object = ancestor;
                    }
                    e.handled = true;
                }
            }
        }
    }

    void MultiDropdown::dispatch_event(Event& e)
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
                glm::vec2 cbox_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (16.0f - deco_size) * 0.5f };

                if (e.get_type() == EventType::MouseMove)
                {
                    auto& me = static_cast<MouseMoveEvent&>(e);
                    if (input::in_bounds(me.position, cbox_pos, { deco_size, deco_size })) {
                        is_hovered = true;
                        e.handled = true;
                    }
                }
                else if (e.get_type() == EventType::MouseButton)
                {
                    auto& me = static_cast<MouseButtonEvent&>(e);
                    if (me.button == MouseButton::Left && me.pressed)
                    {
                        if (input::in_bounds(me.position, cbox_pos, { deco_size, deco_size }))
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
                YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (16.0f - 16.0f) * 0.5f);

                offset_x += 16.0f + style::padding;
            }
        }

        on_event(e);
    }
}
