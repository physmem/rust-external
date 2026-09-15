#include "tab.hpp"
#include "../container/container.hpp"
#include "../subtab_bar/subtab_bar.hpp"
#include <render/render.hpp>
#include <input/input.hpp>
#include <utils/style.hpp>
#include <algorithm>

namespace gui
{
    Tab::Tab(std::string_view label_name) : Object(label_name), label(label_name)
    {
        YGNodeStyleSetFlexGrow(yoga_node, 1.0f);
        YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
        YGNodeStyleSetHeightPercent(yoga_node, 100.0f);
        
        YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetAlignItems(yoga_node, YGAlignFlexStart);
        YGNodeStyleSetPadding(yoga_node, YGEdgeAll, style::padding);
    }

    glm::vec2 Tab::get_child_origin()
    {
        return get_absolute_position() - glm::vec2{ 0, m_scroll_anim.value };
    }

    void Tab::render()
    {
        if (YGNodeStyleGetDisplay(yoga_node) == YGDisplayNone)
            return;

        glm::vec2 pos = get_absolute_position();
        glm::vec2 size = { get_width(), get_height() };

        m_content_height = 0.0f;
        for_each_logical_child([&](Object* child) {
            if (YGNodeStyleGetDisplay(child->yoga_node) == YGDisplayNone) return;
            float child_bottom = (child->get_absolute_position().y - pos.y + m_scroll_anim.value) + child->get_height();
            m_content_height = std::max(m_content_height, child_bottom);
        });
        m_content_height += style::padding;

        bool was_showing = m_show_scrollbar;
        m_show_scrollbar = m_content_height > size.y;

        if (was_showing != m_show_scrollbar) {
            YGNodeStyleSetPadding(yoga_node, YGEdgeRight, m_show_scrollbar ? (style::padding + style::scrollbar_width) : style::padding);
        }

        float max_scroll = std::max(0.0f, m_content_height - size.y);
        m_target_scroll = std::max(0.0f, std::min(m_target_scroll, max_scroll));
        m_scroll_anim.update(m_target_scroll);

        render::push_clip_rect(pos, size);

        for (auto& child : children) {
            child->render();
        }

        render::pop_clip_rect();

        if (m_show_scrollbar) {
            float spacing = style::padding;
            float track_h = size.y - (spacing * 2.0f);
            float scroll_scale = track_h / m_content_height;
            
            float thumb_h = std::max(10.0f, size.y * scroll_scale);
            float max_scroll = std::max(1.0f, m_content_height - size.y);
            float scroll_pct = m_scroll_anim.value / max_scroll;
            
            float thumb_y = pos.y + spacing + (scroll_pct * (track_h - thumb_h));
            
            render::add_capsule(
                { pos.x + size.x - style::scrollbar_width - 5.0f, thumb_y },
                { style::scrollbar_width, thumb_h },
                style::colors::accent.lerp(style::colors::tab_bg, 0.5f)
            );
        }
    }

    void Tab::on_event(Event& e)
    {
        if (e.get_type() == EventType::MouseScroll) {
            auto& se = static_cast<MouseScrollEvent&>(e);
            m_target_scroll -= se.offset * 40.0f;
            e.handled = true;
        }
    }

    void Tab::dispatch_event(Event& e)
    {
        if (YGNodeStyleGetDisplay(yoga_node) == YGDisplayNone)
            return;

        if (e.get_type() == EventType::MouseMove || e.get_type() == EventType::MouseButton || e.get_type() == EventType::MouseScroll) {
            glm::vec2 mouse_pos{};
            
            if (e.get_type() == EventType::MouseMove) 
                mouse_pos = static_cast<MouseMoveEvent&>(e).position;
            else if (e.get_type() == EventType::MouseButton) 
                mouse_pos = static_cast<MouseButtonEvent&>(e).position;
            else if (e.get_type() == EventType::MouseScroll) 
                mouse_pos = input::mouse_position();

            glm::vec2 pos = get_absolute_position();
            glm::vec2 size = { get_width(), get_height() };

            if (!input::in_bounds(mouse_pos, pos, size))
                return;
        }

        Object::dispatch_event(e);
    }

    void Tab::for_each_logical_child(std::function<void(Object*)> callback)
    {
        for (auto& child : children) {
            if (child.get() == m_column_wrapper)
                continue;
            
            callback(child.get());
        }

        if (m_left_column)
        {
            for (auto& child : m_left_column->children)
                callback(child.get());
        }

        if (m_right_column)
        {
            for (auto& child : m_right_column->children)
                callback(child.get());
        }
    }

    void Tab::ensure_columns()
    {
        if (m_column_wrapper)
            return;

        m_column_wrapper = add_object<Object>();
        YGNodeStyleSetFlexDirection(m_column_wrapper->yoga_node, YGFlexDirectionRow);
        YGNodeStyleSetWidthPercent(m_column_wrapper->yoga_node, 100.0f);

        m_left_column = m_column_wrapper->add_object<Object>();
        YGNodeStyleSetFlexDirection(m_left_column->yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(m_left_column->yoga_node, 50.0f);
        YGNodeStyleSetPadding(m_left_column->yoga_node, YGEdgeRight, style::padding * 0.5f);

        m_right_column = m_column_wrapper->add_object<Object>();
        YGNodeStyleSetFlexDirection(m_right_column->yoga_node, YGFlexDirectionColumn);
        YGNodeStyleSetWidthPercent(m_right_column->yoga_node, 50.0f);
        YGNodeStyleSetPadding(m_right_column->yoga_node, YGEdgeLeft, style::padding * 0.5f);
    }

    Container* Tab::add_container(std::string_view label_name)
    {
        ensure_columns();

        Object* target = (m_child_count++ % 2 == 0) ? m_left_column : m_right_column;
        auto container = target->add_object<Container>(label_name);
        
        YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);
        
        return container;
    }

    Container* Tab::add_full_container(std::string_view label_name)
    {
        auto container = add_object<Container>(label_name);
        YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);
        return container;
    }

    SubTabBar* Tab::add_subtab_bar(std::string_view label_name)
    {
        ensure_columns();

        Object* target = (m_child_count++ % 2 == 0) ? m_left_column : m_right_column;
        auto container = target->add_object<SubTabBar>();
        
        YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);
        
        return container;
    }

    SubTabBar* Tab::add_full_subtab_bar(std::string_view label_name)
    {
        auto container = add_object<SubTabBar>();
        YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);
        return container;
    }
}
