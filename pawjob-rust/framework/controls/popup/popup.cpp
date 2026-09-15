#include "popup.hpp"
#include <render/render.hpp>
#include <input/input.hpp>
#include <utils/style.hpp>
#include <render/assets/font_awesome.hpp>
#include <imgui_internal.h>
#include <cmath>

#include <print>
namespace
{
    float ease_out_cubic(float x)
    {
        return 1.0f - std::pow(1.0f - x, 3.0f);
    }
}

namespace gui
{
    Popup::Popup(std::string_view name) : Object(name), label(name)
    {
        content_node = YGNodeNew();
        YGNodeStyleSetFlexDirection(content_node, YGFlexDirectionColumn);
        YGNodeStyleSetPadding(content_node, YGEdgeLeft, style::padding);
        YGNodeStyleSetPadding(content_node, YGEdgeRight, style::padding);
        YGNodeStyleSetPadding(content_node, YGEdgeTop, style::padding);
        YGNodeStyleSetPadding(content_node, YGEdgeBottom, 0.0f);
        YGNodeStyleSetWidth(content_node, popup_width);

        YGNodeStyleSetWidth(yoga_node, 16.0f);
        YGNodeStyleSetHeight(yoga_node, 16.0f);
        YGNodeStyleSetPositionType(yoga_node, YGPositionTypeAbsolute);
    }

    Popup::~Popup()
    {
        if (content_node) {
            YGNodeFree(content_node);
        }
    }

    void Popup::attach_child(std::shared_ptr<Object> child)
    {
        YGNodeInsertChild(content_node, child->yoga_node, YGNodeGetChildCount(content_node));
    }

    void Popup::update_layout()
    {
        constexpr float max_child_content_width = 200.0f; 

        calculated_width = max_child_content_width; 
        
        YGNodeStyleSetWidth(content_node, calculated_width);
        YGNodeCalculateLayout(content_node, calculated_width, YGUndefined, YGDirectionLTR);
        calculated_height = YGNodeLayoutGetHeight(content_node);

        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };
        glm::vec2 open_size = { calculated_width, calculated_height };

        content_pos = pos + (size * 0.5f) - (open_size * 0.5f);
        content_pos.x = std::clamp(content_pos.x, style::padding, ImGui::GetIO().DisplaySize.x - open_size.x - style::padding);
        content_pos.y = std::clamp(content_pos.y, style::padding, ImGui::GetIO().DisplaySize.y - open_size.y - style::padding);
    }

    glm::vec2 Popup::get_child_origin()
    {
        if (open_anim.value > 1e-3f) {
            update_layout();
            return content_pos;
        }
        return get_absolute_position();
    }

    void Popup::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        open_anim.update(is_open ? 1.0f : 0.0f);
        hover_anim.update(is_hovered ? 1.0f : 0.0f);

        if (!hide_icon)
        {
            Color icon_color = style::colors::text_inactive.lerp(style::colors::text_active, hover_anim.value);
            icon_color = icon_color.lerp(style::colors::accent, open_anim.value);

            render::rotate_vertices(0.25f + (0.5f * open_anim.value), [&] {
                render::add_text(
                    render::Fonts::Icons16px,
                    ICON_FA_GEAR,
                    pos + (size * 0.5f),
                    icon_color,
                    render::TextFlagsNone,
                    { 0.5f, 0.5f }
                );
            });
        }
    }

    void Popup::render_overlay()
    {
        if (open_anim.value <= 1e-3f) return;

        update_layout();

        glm::vec2 open_size = { calculated_width, calculated_height };
        glm::vec2 center = content_pos + (open_size * 0.5f);
        float scale = ease_out_cubic(open_anim.value);

        std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;

        render::add_shadow_rect(content_pos, open_size, style::colors::window_shadow, 25.0f, style::widget_rounding);
        render::add_rect_filled(content_pos, open_size, style::colors::tab_bg, style::widget_rounding);
        render::add_rect(content_pos, open_size, style::colors::window_border, style::widget_rounding, 2.0f);

        for (auto& child : children)
        {
            child->render();
        }

        std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;

        for (int i = start_vtx; i < end_vtx; i++) {
            ImDrawVert& vtx = render::draw_list->VtxBuffer[i];
            vtx.pos = ImVec2(
                center.x + (vtx.pos.x - center.x) * scale,
                center.y + (vtx.pos.y - center.y) * scale
            );

            const auto alpha = static_cast<unsigned int>((vtx.col >> 24) & 0xFF);
            vtx.col = (vtx.col & 0x00FFFFFF) | (static_cast<unsigned int>(alpha * open_anim.value) << 24);
        }
    }

    void Popup::on_event(Event& e)
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, pos, size);
            
            if (is_open)
                e.handled = true;
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed)
            {
                if (is_open)
                {
                    update_layout();

                    bool in_content = input::in_bounds(me.position, content_pos, { calculated_width, calculated_height });
                    if (!in_content)
                    {
                        is_open = false;

                        Object* ancestor = this->parent;
                        while (ancestor && !dynamic_cast<Popup*>(ancestor)) {
                            ancestor = ancestor->parent;
                        }

                        Object::g_active_object = ancestor;
                        Object::g_focused_object = ancestor;
                        e.handled = true;
                    }
                    else
                    {
                        e.handled = true;
                    }
                }
                else
                {
                    if (input::in_bounds(me.position, pos, size))
                    {
                        is_open = true;
                        Object::g_active_object = this;
                        Object::g_focused_object = this;
                        e.handled = true;
                        return;
                    }
                }
            }
        }
    }

    void Popup::dispatch_event(Event& e)
    {
        if (is_open)
        {
            update_layout();
            
            for (auto it = children.rbegin(); it != children.rend(); ++it)
            {
                (*it)->dispatch_event(e);
                if (e.handled) break;
            }
        }

        if (!e.handled)
        {
            on_event(e);
        }
    }
}
