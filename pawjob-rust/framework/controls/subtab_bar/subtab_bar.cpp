#include "../container/container.hpp"
#include "subtab_bar.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

namespace gui
{
	SubTabBar::SubTabBar()
	{
		YGNodeStyleSetPadding(yoga_node, YGEdgeTop, style::container_header_height + style::padding);
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
	}

	SubTabBar::TabPage::TabPage(std::string_view name) : Object(name)
	{
		YGNodeStyleSetFlexGrow(yoga_node, 1.0f);
		YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
	}

	void SubTabBar::TabPage::ensure_columns()
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

	Container* SubTabBar::TabPage::add_container(std::string_view label_name)
	{
		ensure_columns();

		Object* target = (m_child_count++ % 2 == 0) ? m_left_column : m_right_column;
		auto container = target->add_object<Container>(label_name);

		YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);

		return container;
	}

	Container* SubTabBar::TabPage::add_full_container(std::string_view label_name)
	{
		auto container = add_object<Container>(label_name);
		YGNodeStyleSetWidthPercent(container->yoga_node, 100.0f);
		return container;
	}

	SubTabBar* SubTabBar::TabPage::add_subtab_bar(std::string_view label_name)
	{
		return add_object<SubTabBar>();
	}

	SubTabBar* SubTabBar::TabPage::add_full_subtab_bar(std::string_view label_name)
	{
		auto bar = add_object<SubTabBar>();
		YGNodeStyleSetFlexGrow(bar->yoga_node, 1.0f);
		YGNodeStyleSetWidthPercent(bar->yoga_node, 100.0f);
		return bar;
	}

	SubTabBar::TabPage* SubTabBar::add_tab(std::string_view name, std::string_view icon)
	{
		auto page = add_object<SubTabBar::TabPage>(name);

		if (m_tabs.empty()) {
			YGNodeStyleSetDisplay(page->yoga_node, YGDisplayFlex);
			page->display_anim.value = 1.0f;
		}
		else {
			YGNodeStyleSetDisplay(page->yoga_node, YGDisplayNone);
			page->display_anim.value = 0.0f;
		}

		m_tabs.push_back({ std::string(name), std::string(icon), page });
		return page;
	}

	void SubTabBar::render()
	{
		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };
		float header_h = style::container_header_height;

		render::add_shadow_rect(pos, { size.x, header_h }, style::colors::window_shadow, 15.0f, style::container_rounding);
		render::add_rect_filled(pos, { size.x, header_h }, style::colors::tab_bg, style::container_rounding);

		if (!m_tabs.empty())
		{
			std::vector<Tab*> visible_tabs;
			for (auto& sub : m_tabs)
			{
				if (sub.page && sub.page->should_display)
					visible_tabs.push_back(&sub);
			}

			if (!visible_tabs.empty())
			{
				float sub_width = size.x / static_cast<float>(visible_tabs.size());
				for (int i = 0; i < static_cast<int>(visible_tabs.size()); ++i)
				{
					auto& sub = *visible_tabs[i];
					bool is_active = (m_next_index != -1 ? m_tabs[m_next_index].page == sub.page : m_tabs[m_active_tab].page == sub.page);

					sub.hover_anim.update(sub.is_hovered ? 1.0f : 0.0f);
					sub.active_anim.update(is_active ? 1.0f : 0.0f);

					glm::vec2 sub_pos = pos + glm::vec2{ i * sub_width, 0.0f };
					glm::vec2 sub_size = { sub_width, header_h };

					if (sub.active_anim > 0.0f)
					{
						render::add_rect_filled(sub_pos + glm::vec2{ 4.0f }, sub_size - glm::vec2{ 8.0f }, style::colors::hovered_control_bg.scale_alpha(sub.active_anim), style::container_rounding - 1.0f);
					}

					Color text_color = style::colors::text_inactive.lerp(style::colors::text_hover, sub.hover_anim).lerp(style::colors::text_active, sub.active_anim);

					glm::vec2 label_size = render::get_text_size(render::Fonts::NotoSans16px, sub.name);
					glm::vec2 icon_size = sub.icon.empty() ? glm::vec2{ 0.0f } : render::get_text_size(render::Fonts::Icons16px, sub.icon);

					float content_width = icon_size.x + label_size.x + (!sub.icon.empty() && !sub.name.empty() ? 5.0f : 0.0f);
					float draw_x = sub_pos.x + (sub_width - content_width) * 0.5f;

					if (!sub.icon.empty())
					{
						render::add_text(render::Fonts::Icons16px, sub.icon, { draw_x, sub_pos.y + header_h * 0.5f }, text_color, render::TextFlagsNone, { 0.0f, 0.5f });
						draw_x += icon_size.x + 5.0f;
					}

					if (!sub.name.empty())
					{
						render::add_text(render::Fonts::NotoSans16px, sub.name, { draw_x, sub_pos.y + header_h * 0.5f }, text_color, render::TextFlagsNone, { 0.0f, 0.5f });
					}
				}
			}
		}

		render::add_rect(pos, { size.x, header_h }, style::colors::window_border, style::container_rounding, 2.0f);

		if (m_next_tab)
		{
			m_fade_anim.update(0.0f);
			if (m_fade_anim.value == 0.0f)
			{
				YGNodeStyleSetDisplay(m_tabs[m_active_tab].page->yoga_node, YGDisplayNone);
				m_active_tab = m_next_index;
				YGNodeStyleSetDisplay(m_tabs[m_active_tab].page->yoga_node, YGDisplayFlex);

				YGNodeCalculateLayout(yoga_node, YGUndefined, YGUndefined, YGDirectionLTR);

				m_next_tab = nullptr;
				m_next_index = -1;
			}
		}
		else
		{
			m_fade_anim.update(1.0f);
		}

		std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;
		{
			for (auto& child : children)
			{
				if (child->display_anim.value <= 0.001f)
					continue;

				std::int32_t child_start_vtx = render::draw_list->VtxBuffer.Size;
				if (YGNodeStyleGetDisplay(child->yoga_node) != YGDisplayNone)
				{
					child->render();
				}
				std::int32_t child_end_vtx = render::draw_list->VtxBuffer.Size;
				render::modify_alpha(child_start_vtx, child_end_vtx, child->display_anim.value);
			}
		}
		std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;

		if (m_fade_anim.value < 1.0f)
			render::modify_alpha(start_vtx, end_vtx, m_fade_anim.value);
	}

	void SubTabBar::dispatch_event(Event& e)
	{
		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };
		float header_h = style::container_header_height;

		if (e.get_type() == EventType::MouseMove)
		{
			auto& me = static_cast<MouseMoveEvent&>(e);
			std::vector<Tab*> visible_tabs;
			for (auto& sub : m_tabs)
			{
				if (sub.page && sub.page->should_display)
					visible_tabs.push_back(&sub);
			}

			if (!visible_tabs.empty())
			{
				float sub_width = size.x / static_cast<float>(visible_tabs.size());
				for (int i = 0; i < static_cast<int>(visible_tabs.size()); ++i)
				{
					auto& sub = *visible_tabs[i];
					glm::vec2 sub_pos = pos + glm::vec2{ i * sub_width, 0.0f };
					sub.is_hovered = input::in_bounds(me.position, sub_pos, { sub_width, header_h });
				}
			}
		}
		else if (e.get_type() == EventType::MouseButton)
		{
			auto& me = static_cast<MouseButtonEvent&>(e);
			if (me.button == MouseButton::Left && me.pressed)
			{
				std::vector<Tab*> visible_tabs;
				for (auto& sub : m_tabs)
				{
					if (sub.page && sub.page->should_display)
						visible_tabs.push_back(&sub);
				}

				if (!visible_tabs.empty())
				{
					float sub_width = size.x / static_cast<float>(visible_tabs.size());
					for (int i = 0; i < static_cast<int>(visible_tabs.size()); ++i)
					{
						auto& sub = *visible_tabs[i];
						glm::vec2 sub_pos = pos + glm::vec2{ i * sub_width, 0.0f };
						if (input::in_bounds(me.position, sub_pos, { sub_width, header_h }))
						{
							// Find the actual index in m_tabs
							int actual_index = -1;
							for (int j = 0; j < (int)m_tabs.size(); ++j) {
								if (m_tabs[j].page == sub.page) {
									actual_index = j;
									break;
								}
							}

							if (m_active_tab != actual_index && m_next_tab == nullptr)
							{
								m_next_tab = sub.page;
								m_next_index = actual_index;
							}
							e.handled = true;
							break;
						}
					}
				}
			}
		}

		if (!e.handled && m_next_tab == nullptr && m_fade_anim.value >= 1.0f)
		{
			Object::dispatch_event(e);
		}
	}
}
