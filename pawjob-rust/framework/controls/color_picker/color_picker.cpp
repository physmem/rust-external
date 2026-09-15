#include "color_picker.hpp"
#include <render/render.hpp>
#include <utils/style.hpp>
#include <input/input.hpp>
#include <algorithm>
#include <cmath>

namespace gui
{
    ColorPicker::ColorPicker(std::string_view name, Color color, bool show_alpha)
        : Object(name), value(color), m_default_value(color), show_alpha(show_alpha)
    {
        YGNodeStyleSetDisplay(yoga_node, YGDisplayNone);
        hsv = value.to_hsv();
    }

    namespace
    {
        float ease_out_cubic(float x)
        {
            return 1.0f - std::pow(1.0f - x, 3.0f);
        }
    }

    void ColorPicker::render()
    {
        open_anim.update(is_open ? 1.0f : 0.0f);
        if (open_anim.value <= 1e-3f && Object::g_active_object == this) {
            Object::g_active_object = nullptr;
        }

        Object::render();
    }

    void ColorPicker::render_overlay()
    {
        if (!open_anim) return;

        if (!dragging_sv && !dragging_hue && !dragging_alpha) {
            hsv = value.to_hsv();
        }

        float box_size = 14.0f;
        float total_height = sv_height + style::padding + slider_height + (show_alpha ? (style::padding + slider_height) : 0.0f) + (style::padding * 2.0f);
        glm::vec2 size = { picker_width, total_height };
        
        glm::vec2 pos = { 
            last_box_pos.x + (box_size * 0.5f) - (picker_width * 0.5f),
            last_box_pos.y + box_size + style::padding
        };

        const auto display_size = ImGui::GetIO().DisplaySize;

        if (pos.y + size.y > display_size.y - style::padding) {
            pos.y = last_box_pos.y - size.y - style::padding;
        }

        pos.x = std::clamp(pos.x, style::padding, display_size.x - size.x - style::padding);
        pos.y = std::max(pos.y, style::padding);

        glm::vec2 center = pos + (size * 0.5f);
        float scale = ease_out_cubic(open_anim.value);

        std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;

        render::add_shadow_rect(pos, size, style::colors::window_shadow, 15.0f, style::widget_rounding);
        render::add_rect_filled(pos, size, style::colors::tab_bg, style::widget_rounding);
        render::add_rect(pos, size, style::colors::window_border, style::widget_rounding, 2.0f);

        glm::vec2 draw_pos = pos + glm::vec2{ style::padding, style::padding };
        glm::vec2 draw_size = { picker_width - (style::padding * 2.0f), sv_height };

        Color hue_color = Color::from_hsv(hsv.h, 1.0f, 1.0f);
        
        render::draw_list->AddRectFilledMultiColor(
            draw_pos, 
            draw_pos + draw_size,
            IM_COL32(255, 255, 255, 255),
            hue_color,
            hue_color,
            IM_COL32(255, 255, 255, 255)
        );

        render::draw_list->AddRectFilledMultiColor(
            draw_pos,
            draw_pos + draw_size,
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, 255),
            IM_COL32(0, 0, 0, 255)
        );

        glm::vec2 indicator_pos = draw_pos + glm::vec2{ hsv.s * draw_size.x, (1.0f - hsv.v) * draw_size.y };
        render::add_circle(indicator_pos, 4.0f, Color::black(), 2.0f);
        render::add_circle(indicator_pos, 3.0f, Color::white(), 1.0f);

        draw_pos.y += sv_height + style::padding;
        draw_size.y = slider_height;

        const Color hue_colors[] = {
            Color(255, 0, 0), Color(255, 255, 0), Color(0, 255, 0),
            Color(0, 255, 255), Color(0, 0, 255), Color(255, 0, 255), Color(255, 0, 0)
        };

        for (int i = 0; i < 6; ++i) {
            render::draw_list->AddRectFilledMultiColor(
                draw_pos + glm::vec2{ (i / 6.0f) * draw_size.x, 0.0f },
                draw_pos + glm::vec2{ ((i + 1) / 6.0f) * draw_size.x, draw_size.y },
                hue_colors[i], hue_colors[i + 1], hue_colors[i + 1], hue_colors[i]
            );
        }

        float hue_x = hsv.h * draw_size.x;
        render::add_rect_filled(draw_pos + glm::vec2{ hue_x - 2.0f, -2.0f }, { 4.0f, draw_size.y + 4.0f }, Color::white(), 2.0f);
        render::add_rect(draw_pos + glm::vec2{ hue_x - 2.0f, -2.0f }, { 4.0f, draw_size.y + 4.0f }, Color::black(), 2.0f, 1.0f);

        if (show_alpha) {
            draw_pos.y += slider_height + style::padding;
            
            float check_size = slider_height / 2.0f;
            for (float x = 0; x < draw_size.x; x += check_size) {
                for (float y = 0; y < draw_size.y; y += check_size) {
                    bool dark = (static_cast<int>(x / check_size) + static_cast<int>(y / check_size)) % 2 == 0;
                    render::add_rect_filled(draw_pos + glm::vec2{ x, y }, { std::min(check_size, draw_size.x - x), std::min(check_size, draw_size.y - y) }, dark ? Color(50, 50, 50) : Color(100, 100, 100));
                }
            }

            Color opaque = Color::from_hsv(hsv.h, hsv.s, hsv.v);
            Color transparent = opaque.override_alpha(0.0f);

            render::draw_list->AddRectFilledMultiColor(
                draw_pos,
                draw_pos + draw_size,
                transparent, opaque, opaque, transparent
            );

            float alpha_x = (value.a / 255.0f) * draw_size.x;
            render::add_rect_filled(draw_pos + glm::vec2{ alpha_x - 2.0f, -2.0f }, { 4.0f, draw_size.y + 4.0f }, Color::white(), 2.0f);
            render::add_rect(draw_pos + glm::vec2{ alpha_x - 2.0f, -2.0f }, { 4.0f, draw_size.y + 4.0f }, Color::black(), 2.0f, 1.0f);
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

    void ColorPicker::on_event(Event& e)
    {
        if (!is_open) return;

        float box_size = 14.0f;
        float total_height = sv_height + style::padding + slider_height + (show_alpha ? (style::padding + slider_height) : 0.0f) + (style::padding * 2.0f);
        glm::vec2 size = { picker_width, total_height };

        glm::vec2 pos = {
            last_box_pos.x + (box_size * 0.5f) - (picker_width * 0.5f),
            last_box_pos.y + box_size + style::padding
        };

        const auto display_size = ImGui::GetIO().DisplaySize;

        if (pos.y + size.y > display_size.y - style::padding) {
            pos.y = last_box_pos.y - size.y - style::padding;
        }

        pos.x = std::clamp(pos.x, style::padding, display_size.x - size.x - style::padding);
        pos.y = std::max(pos.y, style::padding);

        if (e.get_type() == EventType::MouseMove) {
            auto& me = static_cast<MouseMoveEvent&>(e);
            
            if (dragging_sv) {
                glm::vec2 sv_pos = pos + glm::vec2{ style::padding, style::padding };
                glm::vec2 sv_size = { picker_width - (style::padding * 2.0f), sv_height };
                
                hsv.s = std::clamp((me.position.x - sv_pos.x) / sv_size.x, 0.0f, 1.0f);
                hsv.v = 1.0f - std::clamp((me.position.y - sv_pos.y) / sv_size.y, 0.0f, 1.0f);
                update_color_from_hsv();
                e.handled = true;
            }
            else if (dragging_hue) {
                glm::vec2 hue_pos = pos + glm::vec2{ style::padding, style::padding + sv_height + style::padding };
                float hue_width = picker_width - (style::padding * 2.0f);
                
                hsv.h = std::clamp((me.position.x - hue_pos.x) / hue_width, 0.0f, 0.999f);
                update_color_from_hsv();
                e.handled = true;
            }
            else if (dragging_alpha) {
                glm::vec2 alpha_pos = pos + glm::vec2{ style::padding, style::padding + sv_height + style::padding + slider_height + style::padding };
                float alpha_width = picker_width - (style::padding * 2.0f);
                
                float a = std::clamp((me.position.x - alpha_pos.x) / alpha_width, 0.0f, 1.0f);
                value.a = static_cast<std::uint8_t>(a * 255.0f);
                e.handled = true;
            }
        }
        else if (e.get_type() == EventType::MouseButton) {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left) {
                if (me.pressed) {
                    glm::vec2 sv_pos = pos + glm::vec2{ style::padding, style::padding };
                    glm::vec2 sv_size = { picker_width - (style::padding * 2.0f), sv_height };
                    
                    if (input::in_bounds(me.position, sv_pos, sv_size)) {
                        dragging_sv = true;
                        hsv.s = std::clamp((me.position.x - sv_pos.x) / sv_size.x, 0.0f, 1.0f);
                        hsv.v = 1.0f - std::clamp((me.position.y - sv_pos.y) / sv_size.y, 0.0f, 1.0f);
                        update_color_from_hsv();
                        e.handled = true;
                    }

                    glm::vec2 hue_pos = pos + glm::vec2{ style::padding, style::padding + sv_height + style::padding };
                    glm::vec2 hue_size = { picker_width - (style::padding * 2.0f), slider_height };
                    
                    if (input::in_bounds(me.position, hue_pos, hue_size)) {
                        dragging_hue = true;
                        hsv.h = std::clamp((me.position.x - hue_pos.x) / hue_size.x, 0.0f, 0.999f);
                        update_color_from_hsv();
                        e.handled = true;
                    }

                    if (show_alpha) {
                        glm::vec2 alpha_pos = pos + glm::vec2{ style::padding, style::padding + sv_height + style::padding + slider_height + style::padding };
                        glm::vec2 alpha_size = { picker_width - (style::padding * 2.0f), slider_height };
                        
                        if (input::in_bounds(me.position, alpha_pos, alpha_size)) {
                            dragging_alpha = true;
                            float a = std::clamp((me.position.x - alpha_pos.x) / alpha_size.x, 0.0f, 1.0f);
                            value.a = static_cast<std::uint8_t>(a * 255.0f);
                            e.handled = true;
                        }
                    }

                    if (!e.handled && !input::in_bounds(me.position, pos, size)) {
                        is_open = false;
                        e.handled = true; 
                    }
                }
                else {
                    dragging_sv = false;
                    dragging_hue = false;
                    dragging_alpha = false;
                }
            }
        }
    }

    void ColorPicker::dispatch_event(Event& e)
    {
        on_event(e);
        Object::dispatch_event(e);
    }

    void ColorPicker::update_hsv_from_color()
    {
        hsv = value.to_hsv();
    }

    void ColorPicker::update_color_from_hsv()
    {
        std::uint8_t old_a = value.a;
        value = hsv.to_color();
        value.a = old_a;
    }
}
