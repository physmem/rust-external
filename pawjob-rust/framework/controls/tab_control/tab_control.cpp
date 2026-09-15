#include "tab_control.hpp"
#include <render/render.hpp>
#include <print>
#include <utils/color.hpp>
#include <utils/style.hpp>
#include <input/input.hpp>

namespace gui
{
    TabControl::TabControl() : Object(ConstName("TabControl"))
    {
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetHeightPercent(yoga_node, 100.0f);

        body_container = add_object<Object>(ConstName("TabControlBody"));
        YGNodeStyleSetFlexGrow(body_container->yoga_node, 1.0f);
        YGNodeStyleSetWidthPercent(body_container->yoga_node, 100.0f);
        YGNodeStyleSetMargin(body_container->yoga_node, YGEdgeTop, style::tab_header_height + 1.0f);
        YGNodeStyleSetMargin(body_container->yoga_node, YGEdgeBottom, style::footer_height);
    }

    Tab* TabControl::add_tab(std::string_view label, std::string_view icon)
    {
        auto tab = std::make_shared<Tab>(label);
        tab->icon = icon;
        tabs.push_back(tab);

        YGNodeInsertChild(body_container->yoga_node, tab->yoga_node, static_cast<uint32_t>(body_container->children.size()));
        body_container->children.push_back(tab);
        tab->parent = body_container;

        if (tabs.size() == 1)
        {
            active_tab = tab.get();
            YGNodeStyleSetDisplay(tab->yoga_node, YGDisplayFlex);
        }
        else
        {
            YGNodeStyleSetDisplay(tab->yoga_node, YGDisplayNone);
        }

        return tab.get();
    }

    void TabControl::render()
    {
        glm::vec2 pos = get_absolute_position();

        float header_y = pos.y;
        float header_height = style::tab_header_height;
        float footer_height = style::footer_height;

        render::add_rect_filled({ pos.x, pos.y + header_height - 1.0f }, { get_width(), 2.0f }, style::colors::window_border);
        render::add_rect_filled({ pos.x, pos.y + get_height() - footer_height }, { get_width(), 2.0f }, style::colors::window_border);

        float total_tabs_width = 0.0f;
        const float tab_padding = style::tab_padding;

        for (auto& tab : tabs)
        {
            const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, tab->icon);
            total_tabs_width += icon_size.x  + tab_padding;
        }

        float current_x = (pos.x + get_width() - total_tabs_width) - style::padding * 0.5f;

        for (auto& tab : tabs)
        {
            const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, tab->icon);
            float tab_width = icon_size.x + tab_padding;

            bool is_active = (m_next_tab ? (tab.get() == m_next_tab) : (tab.get() == active_tab));
            tab->active_anim.value = is_active ? 1.0f : 0.0f; 
            
            tab->hover_anim.update(tab->is_hovered ? 1.0f : 0.0f);

            Color text_color = style::colors::text_inactive.lerp(style::colors::text_hover, tab->hover_anim).lerp(style::colors::accent, tab->active_anim);

            render::add_circle_shadow(glm::vec2{ current_x, header_y } + glm::vec2{ tab_width * 0.5f, header_height * 0.5f }, 3.0f, text_color, 25.0f + (25.0f * tab->active_anim));

            render::add_text(
                render::Fonts::Icons20px,
                tab->icon,
                { current_x + tab_width * 0.5f, header_y + header_height * 0.5f },
                text_color,
                render::TextFlagsNone,
                glm::vec2{ 0.5f, 0.5f }
            );

            current_x += tab_width;
        }

        if (m_next_tab) {
            m_fade_anim.update(0.0f);
            if (m_fade_anim.value == 0.0f) {
                if (active_tab) YGNodeStyleSetDisplay(active_tab->yoga_node, YGDisplayNone);
                
                active_tab = m_next_tab;
                YGNodeStyleSetDisplay(active_tab->yoga_node, YGDisplayFlex);
                
                m_next_tab = nullptr;
            }
        } else {
            m_fade_anim.update(1.0f);
        }

        std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;
        Object::render();
        std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;

        render::modify_alpha(start_vtx, end_vtx, m_fade_anim.value);
    }

    void TabControl::dispatch_event(Event& e)
    {
        if (e.handled) return;

        glm::vec2 pos = get_absolute_position();
        const float tab_padding = style::tab_padding;

        float total_tabs_width = 0.0f;
        for (auto& tab : tabs)
        {
            const glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans18px, tab->label);
            const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, tab->icon);
            float tab_width = icon_size.x + tab_padding;

            total_tabs_width += tab_width;
        }
        float start_x = (pos.x + get_width() - total_tabs_width) - style::padding * 0.5f;

        if (e.get_type() == EventType::MouseMove)
        {
            auto& me = static_cast<MouseMoveEvent&>(e);
            float current_x = start_x;

            for (auto& tab : tabs)
            {
                const glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans18px, tab->label);
                const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, tab->icon);
                float tab_width = icon_size.x + tab_padding;
                tab->is_hovered = input::in_bounds(me.position, { current_x, pos.y}, { tab_width, style::tab_header_height});

                current_x += tab_width;
            }
        }
        else if (e.get_type() == EventType::MouseButton)
        {
            auto& me = static_cast<MouseButtonEvent&>(e);
            if (me.button == MouseButton::Left && me.pressed)
            {
                float current_x = start_x;
                for (auto& tab : tabs)
                {
                    const glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans18px, tab->label);
                    const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, tab->icon);

                    float tab_width = icon_size.x + tab_padding;

                    if (input::in_bounds(me.position, { current_x, pos.y }, { tab_width, style::tab_header_height }))
                    {
                        if (active_tab != tab.get() && m_next_tab == nullptr)
                        {
                            m_next_tab = tab.get();
                        }

                        e.handled = true;
                        break;
                    }
                    current_x += tab_width;
                }
            }
        }

        if (!e.handled && m_next_tab == nullptr && (float)m_fade_anim >= 1.0f)
        {
            Object::dispatch_event(e);
        }
    }

    void TabControl::for_each_logical_child(std::function<void(Object*)> callback)
    {
        for (auto& tab : tabs) {
            callback(tab.get());
        }
    }
}
