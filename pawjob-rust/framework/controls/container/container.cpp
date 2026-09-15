#include "container.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

namespace gui
{
	Container::Container(std::string_view label_name) : Object(label_name), label(label_name)
	{
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);

		YGNodeStyleSetPadding(yoga_node, YGEdgeAll, style::padding);
		YGNodeStyleSetPadding(yoga_node, YGEdgeTop, style::container_header_height + style::padding);
		YGNodeStyleSetPadding(yoga_node, YGEdgeBottom, 0.0f);
		YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
	}

	Object* Container::add_subtab(std::string_view name, std::string_view icon)
	{
		auto page = add_object<Object>(name);
		YGNodeStyleSetFlexDirection(page->yoga_node, YGFlexDirectionColumn);
		YGNodeStyleSetWidthPercent(page->yoga_node, 100.0f);

		if (!subtabs.empty())
			YGNodeStyleSetDisplay(page->yoga_node, YGDisplayNone);

		subtabs.push_back({ std::string(name), std::string(icon), page });
		return page;
	}

	void Container::render()
	{
		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };
		float header_h = style::container_header_height;

		render::add_shadow_rect(pos, size, style::colors::window_shadow, 15.0f, style::container_rounding);
		render::add_rect_filled(pos, size, style::colors::tab_bg, style::container_rounding);

		glm::vec2 image_pos = pos - glm::vec2{ 5.0f, 0.0f };
		glm::vec2 image_size = pos + glm::vec2{ size.x + 5.0f, header_h + 9.0f };
		render::draw_list->AddImage(render::container_header.get_srv(), image_pos, image_size);

		render::add_rect_filled(pos + glm::vec2{ 0.0f, header_h - 2.0f }, { size.x, 2.0f }, style::colors::window_border);

		render::add_rect(pos - glm::vec2{ 1.0f }, size + glm::vec2{ 2.0f }, style::colors::window_shadow, style::container_rounding);
		render::add_rect(pos, size, style::colors::window_border, style::container_rounding, 2.0f);

		render::add_text(render::Fonts::NotoSans18px, label, pos + glm::vec2{ style::padding, header_h * 0.5f }, style::colors::text_active, render::TextFlagsNone, { 0.0f, 0.5f });

		float current_x = pos.x + size.x - style::padding;
		for (int i = static_cast<int>(subtabs.size()) - 1; i >= 0; --i)
		{
			auto& sub = subtabs[i];
			bool is_active = (next_index != -1 ? next_index == i : active_subtab == i);

			sub.hover_anim.update(sub.is_hovered ? 1.0f : 0.0f);
			sub.active_anim.update(is_active ? 1.0f : 0.0f);

			glm::vec2 label_size = (!hide_subtab_labels) ? render::get_text_size(render::Fonts::NotoSans16px, sub.name) : glm::vec2{0.0f};
			glm::vec2 icon_size = sub.icon.empty() ? glm::vec2{ 0.0f } : render::get_text_size(render::Fonts::Icons16px, sub.icon);

			float sub_width = icon_size.x + label_size.x + (!sub.icon.empty() && !sub.name.empty() && !hide_subtab_labels ? 5.0f : 0.0f);
			Color text_color = style::colors::text_inactive.lerp(style::colors::text_hover, sub.hover_anim).lerp(style::colors::text_active, sub.active_anim);

			float draw_x = current_x - sub_width;

			if (!sub.icon.empty())
			{
				render::add_text(render::Fonts::Icons16px, sub.icon, { draw_x, pos.y + header_h * 0.5f }, text_color, render::TextFlagsNone, { 0.0f, 0.5f });
				draw_x += icon_size.x + (!hide_subtab_labels && !sub.name.empty() ? 5.0f : 0.0f);
			}

			if (!hide_subtab_labels && !sub.name.empty())
			{
				render::add_text(render::Fonts::NotoSans16px, sub.name, { draw_x, pos.y + header_h * 0.5f }, text_color, render::TextFlagsNone, { 0.0f, 0.5f });
			}

			current_x -= sub_width + style::padding;
		}

		if (next_subtab)
		{
			fade_anim.update(0.0f);
			if (fade_anim.value == 0.0f)
			{
				YGNodeStyleSetDisplay(subtabs[active_subtab].page->yoga_node, YGDisplayNone);
				active_subtab = next_index;
				YGNodeStyleSetDisplay(subtabs[active_subtab].page->yoga_node, YGDisplayFlex);

				YGNodeCalculateLayout(yoga_node, YGUndefined, YGUndefined, YGDirectionLTR);

				next_subtab = nullptr;
				next_index = -1;
			}
		}
		else
		{
			fade_anim.update(1.0f);
		}

		std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;
		{
			for (auto& child : children)
			{
				if (!child->display_anim)
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

		if (fade_anim.value < 1.0f)
			render::modify_alpha(start_vtx, end_vtx, fade_anim.value);
	}

	void Container::dispatch_event(Event& e)
	{
		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };
		float header_h = style::container_header_height;

		if (e.get_type() == EventType::MouseMove)
		{
			auto& me = static_cast<MouseMoveEvent&>(e);

			float current_x = pos.x + size.x - style::padding;
			for (int i = static_cast<int>(subtabs.size()) - 1; i >= 0; --i)
			{
				auto& sub = subtabs[i];
				glm::vec2 label_size = (!hide_subtab_labels) ? render::get_text_size(render::Fonts::NotoSans16px, sub.name) : glm::vec2{0.0f};
				glm::vec2 icon_size = sub.icon.empty() ? glm::vec2{ 0.0f } : render::get_text_size(render::Fonts::Icons16px, sub.icon);
				float sub_width = icon_size.x + label_size.x + (!sub.icon.empty() && !sub.name.empty() && !hide_subtab_labels ? 5.0f : 0.0f);

				glm::vec2 sub_pos = { current_x - sub_width, pos.y };
				sub.is_hovered = input::in_bounds(me.position, sub_pos, { sub_width, header_h });

				current_x -= sub_width + style::padding;
			}
		}
		else if (e.get_type() == EventType::MouseButton)
		{
			auto& me = static_cast<MouseButtonEvent&>(e);
			if (me.button == MouseButton::Left && me.pressed)
			{
				float current_x = pos.x + size.x - style::padding;
				for (int i = static_cast<int>(subtabs.size()) - 1; i >= 0; --i)
				{
					auto& sub = subtabs[i];
					glm::vec2 label_size = (!hide_subtab_labels) ? render::get_text_size(render::Fonts::NotoSans16px, sub.name) : glm::vec2{0.0f};
					glm::vec2 icon_size = sub.icon.empty() ? glm::vec2{ 0.0f } : render::get_text_size(render::Fonts::Icons16px, sub.icon);
					float sub_width = icon_size.x + label_size.x + (!sub.icon.empty() && !sub.name.empty() && !hide_subtab_labels ? 5.0f : 0.0f);

					glm::vec2 sub_pos = { current_x - sub_width, pos.y };
					if (input::in_bounds(me.position, sub_pos, { sub_width, header_h }))
					{
						if (active_subtab != i && next_subtab == nullptr)
						{
							next_subtab = sub.page;
							next_index = i;
						}
						e.handled = true;
						break;
					}

					current_x -= sub_width + style::padding;
				}
			}
		}

		if (!e.handled && next_subtab == nullptr && fade_anim.value >= 1.0f)
		{
			Object::dispatch_event(e);
		}
	}
}
