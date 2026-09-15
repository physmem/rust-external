#include "dropdown.hpp"
#include <algorithm>
#include <controls/popup/popup.hpp>
#include <input/input.hpp>
#include <render/assets/font_awesome.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

namespace gui
{
	Dropdown::Dropdown(std::string_view label, int init_value, const std::vector<std::string>& items)
		: Object(label), label(label), value(init_value), m_default_value(init_value), items(items)
	{
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);
		YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
		YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);

		YGNodeStyleSetHeight(yoga_node, 16.0f + 6.0f + get_box_height());

		item_anims.resize(items.size());
		item_hovers.resize(items.size(), false);
	}

	void Dropdown::render()
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

		if (value >= 0 && value < (int)items.size())
		{
			render::add_text(render::Fonts::NotoSans16px, items[value], box_pos + glm::vec2{ style::padding, box_size.y * 0.5f }, style::colors::text_active, render::TextFlagsNone, { 0.0f, 0.5f });
		}

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

		Object::render();
	}

	void Dropdown::render_overlay()
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

				bool is_selected = (value == i);
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

	void Dropdown::on_event(Event& e)
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
				if (input::in_bounds(me.position, box_pos, box_size))
				{
					is_open = !is_open;
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
								value = i;
								if (on_change) on_change(value);
								is_open = false;

								Object* ancestor = this->parent;
								while (ancestor && !dynamic_cast<Popup*>(ancestor)) {
									ancestor = ancestor->parent;
								}

								Object::g_active_object = ancestor;
								Object::g_focused_object = ancestor;
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

	void Dropdown::dispatch_event(Event& e)
	{
		Object::dispatch_event(e);
		if (e.handled) return;

		on_event(e);
	}
}
