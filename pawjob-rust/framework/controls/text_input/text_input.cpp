#include "text_input.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace gui
{
    TextInput::TextInput(std::string_view label_name, std::string_view placeholder, bool password)
        : Object(label_name), label(label_name), placeholder_text(placeholder), is_password(password), m_default_value("")
    {
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
    }

    void TextInput::update_layout()
    {
        m_base_height = show_label ? (16.0f + 6.0f + get_box_height()) : get_box_height();
        Object::update_layout();
    }

    void TextInput::clear_selection()
    {
        if (selection_start != -1 && selection_start != cursor_pos)
        {
            int start = std::min(selection_start, cursor_pos);
            int end = std::max(selection_start, cursor_pos);
            storage.erase(start, end - start);
            cursor_pos = start;
        }
        selection_start = -1;
    }

    void TextInput::render()
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        hover_anim.update((focused || is_hovered) ? 1.0f : 0.0f);

        if (show_label)
        {
            Color label_color = style::colors::text_inactive.lerp(style::colors::text_active, focused ? 1.0f : hover_anim.value);
            render::add_text(render::Fonts::NotoSans16px, label, pos, label_color, render::TextFlagsNone, { 0.0f, 0.0f });
        }

        float box_y_offset = show_label ? (16.0f + 6.0f) : 0.0f;
        glm::vec2 box_pos = { pos.x, pos.y + box_y_offset };
        glm::vec2 box_size = { size.x, get_box_height() };

        render::add_shadow_rect(box_pos, box_size, style::colors::window_shadow, 10.0f, style::widget_rounding);
        render::add_rect_filled(box_pos, box_size, style::colors::control_bg, style::widget_rounding);

        Color border_color = focused
            ? style::colors::accent.scale_alpha(0.6f)
            : style::colors::window_border;
        render::add_rect(box_pos, box_size, border_color, style::widget_rounding, 2.0f);

        if (focused)
        {
            blink_timer += ImGui::GetIO().DeltaTime;
            if (blink_timer > 1.0f) blink_timer = 0.0f;
        }

        const std::string display_text = is_password ? std::string(storage.length(), '*') : storage;

        render::push_clip_rect(box_pos, box_size);

        if (focused && selection_start != -1 && selection_start != cursor_pos)
        {
            int start_idx = std::min(selection_start, cursor_pos);
            int end_idx = std::max(selection_start, cursor_pos);

            float start_x = render::get_text_size(render::Fonts::NotoSans16px, display_text.substr(0, start_idx)).x;
            float end_x = render::get_text_size(render::Fonts::NotoSans16px, display_text.substr(0, end_idx)).x;

            render::add_rect_filled(
                box_pos + glm::vec2{ style::padding + start_x, 4.0f },
                { end_x - start_x, box_size.y - 8.0f },
                style::colors::accent.scale_alpha(0.3f)
            );
        }

        if (display_text.empty() && !focused)
        {
            render::add_text(
                render::Fonts::NotoSans16px,
                placeholder_text,
                box_pos + glm::vec2{ style::padding, box_size.y * 0.5f },
                style::colors::text_inactive,
                render::TextFlagsNone,
                { 0.0f, 0.5f }
            );
        }
        else
        {
            render::add_text(
                render::Fonts::NotoSans16px,
                display_text,
                box_pos + glm::vec2{ style::padding, box_size.y * 0.5f },
                style::colors::text_active,
                render::TextFlagsNone,
                { 0.0f, 0.5f }
            );

            if (focused && blink_timer < 0.5f)
            {
                float text_width = render::get_text_size(render::Fonts::NotoSans16px, display_text.substr(0, cursor_pos)).x;
                render::add_rect_filled(
                    box_pos + glm::vec2{ style::padding + text_width, 4.0f },
                    { 1.0f, box_size.y - 8.0f },
                    style::colors::text_active
                );
            }
        }

        render::pop_clip_rect();
    }

    void TextInput::dispatch_event(Event& e)
    {
        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        float box_y_offset = show_label ? (16.0f + 6.0f) : 0.0f;
        glm::vec2 box_pos = { pos.x, pos.y + box_y_offset };
        glm::vec2 box_size = { size.x, get_box_height() };

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            is_hovered = input::in_bounds(me.position, box_pos, box_size);
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed)
            {
                bool in_box = input::in_bounds(me.position, box_pos, box_size);
                if (in_box)
                {
                    focused = true;
                    Object::g_focused_object = this;
                    blink_timer = 0.0f;
                    selection_start = -1;
                    e.handled = true;
                }
                else if (focused)
                {
                    focused = false;
                    if (Object::g_focused_object == this)
                        Object::g_focused_object = nullptr;
                }
            }
        }
        else if (e.get_type() == EventType::KeyChar && focused)
        {
            auto& ke = static_cast<KeyCharEvent&>(e);
            if (ke.key >= 32 && ke.key < 0x10000)
            {
                clear_selection();
                storage.insert(cursor_pos++, 1, (char)ke.key);
                blink_timer = 0.0f;
                e.handled = true;
            }
        }
        else if (e.get_type() == EventType::KeyPress && focused)
        {
            auto& ke = static_cast<KeyPressEvent&>(e);
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            if (ke.key == VK_BACK)
            {
                if (selection_start != -1 && selection_start != cursor_pos)
                    clear_selection();
                else if (cursor_pos > 0)
                    storage.erase(--cursor_pos, 1);
                blink_timer = 0.0f;
                e.handled = true;
            }
            else if (ke.key == VK_DELETE)
            {
                if (selection_start != -1 && selection_start != cursor_pos)
                    clear_selection();
                else if (cursor_pos < (int)storage.length())
                    storage.erase(cursor_pos, 1);
                blink_timer = 0.0f;
                e.handled = true;
            }
            else if (ke.key == VK_LEFT)
            {
                if (cursor_pos > 0) cursor_pos--;
                selection_start = -1;
                blink_timer = 0.0f;
                e.handled = true;
            }
            else if (ke.key == VK_RIGHT)
            {
                if (cursor_pos < (int)storage.length()) cursor_pos++;
                selection_start = -1;
                blink_timer = 0.0f;
                e.handled = true;
            }
            else if (ke.key == VK_HOME)
            {
                cursor_pos = 0;
                selection_start = -1;
                e.handled = true;
            }
            else if (ke.key == VK_END)
            {
                cursor_pos = (int)storage.length();
                selection_start = -1;
                e.handled = true;
            }
            else if (ke.key == VK_RETURN || ke.key == VK_ESCAPE)
            {
                focused = false;
                if (Object::g_focused_object == this)
                    Object::g_focused_object = nullptr;
                e.handled = true;
            }
            else if (ctrl && (ke.key == 'V' || ke.key == 'v'))
            {
                clear_selection();
                if (OpenClipboard(nullptr))
                {
                    HANDLE data = GetClipboardData(CF_TEXT);
                    if (data)
                    {
                        char* text = static_cast<char*>(GlobalLock(data));
                        if (text)
                        {
                            std::string pasted(text);
                            storage.insert(cursor_pos, pasted);
                            cursor_pos += (int)pasted.length();
                            GlobalUnlock(data);
                        }
                    }
                    CloseClipboard();
                }
                e.handled = true;
            }
            else if (ctrl && (ke.key == 'C' || ke.key == 'c'))
            {
                std::string copy_text;
                if (selection_start != -1 && selection_start != cursor_pos)
                {
                    int start = std::min(selection_start, cursor_pos);
                    int end = std::max(selection_start, cursor_pos);
                    copy_text = storage.substr(start, end - start);
                }
                else
                {
                    copy_text = storage;
                }

                if (!copy_text.empty() && OpenClipboard(nullptr))
                {
                    EmptyClipboard();
                    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, copy_text.size() + 1);
                    if (mem)
                    {
                        memcpy(GlobalLock(mem), copy_text.c_str(), copy_text.size() + 1);
                        GlobalUnlock(mem);
                        SetClipboardData(CF_TEXT, mem);
                    }
                    CloseClipboard();
                }
                e.handled = true;
            }
            else if (ctrl && (ke.key == 'A' || ke.key == 'a'))
            {
                selection_start = 0;
                cursor_pos = (int)storage.length();
                e.handled = true;
            }

            cursor_pos = std::clamp(cursor_pos, 0, (int)storage.length());
        }
    }
}
