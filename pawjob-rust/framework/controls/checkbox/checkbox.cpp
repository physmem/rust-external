#include "../color_picker/color_picker.hpp"
#include "../popup/popup.hpp"
#include "checkbox.hpp"
#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

namespace gui
{
	Checkbox::Checkbox(std::string_view label_name, bool init_value)
		: Object(label_name), label(label_name), value(init_value), m_default_value(init_value)
	{
		YGNodeStyleSetWidthPercent(yoga_node, 100.0f);
		YGNodeStyleSetHeight(yoga_node, 20.0f);
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionRow);
		YGNodeStyleSetAlignItems(yoga_node, YGAlignFlexStart);

		YGNodeStyleSetMargin(yoga_node, YGEdgeBottom, style::padding);
	}

	void Checkbox::render()
	{
		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };

		float box_size = 16.0f;
		glm::vec2 box_pos = pos + glm::vec2{ 0.0f, (size.y - box_size) * 0.5f };

		active_anim.update(value ? 1.0f : 0.0f);
		hover_anim.update(is_hovered ? 1.0f : 0.0f);

		// shadow
		render::add_shadow_rect(
			box_pos,
			glm::vec2{ box_size },
			style::colors::window_shadow,
			15.0f,
			style::widget_rounding
		);

		// background
		render::add_rect_filled(
			box_pos,
			glm::vec2{ box_size },
			style::colors::control_bg,
			style::widget_rounding
		);

		// outline
		render::add_rect(
			box_pos,
			glm::vec2{ box_size },
			style::colors::window_border,
			style::widget_rounding,
			2.0f
		);

		render::add_rect_filled(
			box_pos,
			glm::vec2{ box_size },
			style::colors::accent.scale_alpha(active_anim),
			style::widget_rounding
		);

		if (active_anim > 0.0f && active_anim < 1.0f)
		{
			const glm::vec2 center = box_pos + (glm::vec2{ box_size } * 0.5f);

			const std::int32_t start_idx = render::draw_list->VtxBuffer.Size;
			render::gradient_items(box_pos, glm::vec2{ box_size }, style::colors::accent, style::colors::accent - 25, [&] {
				render::add_rect_filled(
					box_pos,
					glm::vec2{ box_size },
					style::colors::accent.scale_alpha(active_anim),
					style::widget_rounding);

				}, 0.3f);

			render::add_checkmark(
				box_pos + glm::vec2{ box_size } * 0.2f,
				Color::white(),
				box_size * 0.6f,
				1.0f);

			const std::int32_t end_idx = render::draw_list->VtxBuffer.Size;

			for (std::int32_t i = start_idx; i < end_idx; i++) {
				ImDrawVert& vtx = render::draw_list->VtxBuffer[i];
				vtx.pos = center + (static_cast<glm::vec2>(vtx.pos) - center) * active_anim;
			}
		}
		else {

			render::gradient_items(box_pos, glm::vec2{ box_size }, style::colors::accent, style::colors::accent - 25, [&] {
				render::add_rect_filled(
					box_pos,
					glm::vec2{ box_size },
					style::colors::accent.scale_alpha(active_anim),
					style::widget_rounding);

				}, 0.3f);

			render::add_checkmark(
				box_pos + glm::vec2{ box_size } * 0.2f,
				Color::white().scale_alpha(active_anim),
				box_size * 0.6f,
				1.0f);
		}

		render::add_text(
			render::Fonts::NotoSans16px,
			label,
			pos + glm::vec2{ box_size + style::padding, size.y * 0.5f },
			style::colors::text_inactive.lerp(style::colors::text_active, active_anim),
			render::TextFlagsNone,
			glm::vec2{ 0.0f, 0.5f }
		);

		float deco_size = 14.0f;
		float offset_x = 0.0f;

		for (auto& child : children)
		{
			if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
			{
				glm::vec2 cbox_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (size.y - deco_size) * 0.5f };
				cp->last_box_pos = cbox_pos;

				render::add_shadow_rect(cbox_pos, glm::vec2{ deco_size }, style::colors::window_shadow, 15.0f, deco_size);
				render::add_rect_filled(cbox_pos, glm::vec2{ deco_size }, cp->value, deco_size);

				offset_x += deco_size + style::padding;
			}
			else if (auto p = dynamic_cast<Popup*>(child.get()))
			{
				YGNodeStyleSetPosition(child->yoga_node, YGEdgeRight, offset_x);
				YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (size.y - 16.0f) * 0.5f);

				offset_x += 16.0f + style::padding;
			}
		}

		Object::render();
	}

	void Checkbox::dispatch_event(Event& e)
	{
		Object::dispatch_event(e);
		if (e.handled) return;

		glm::vec2 pos = get_absolute_position();
		glm::vec2 size = { get_width(), get_height() };
		glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans16px, label);
		float clickable_width = 16.0f + style::padding + text_size.x;

		float deco_size = 14.0f;
		float offset_x = 0.0f;

		for (auto& child : children)
		{
			if (auto cp = dynamic_cast<ColorPicker*>(child.get()))
			{
				glm::vec2 cbox_pos = pos + glm::vec2{ size.x - deco_size - offset_x, (size.y - deco_size) * 0.5f };

				if (e.get_type() == EventType::MouseMove) {
					auto& me = static_cast<MouseMoveEvent&>(e);
					if (input::in_bounds(me.position, cbox_pos, { deco_size, deco_size })) {
						is_hovered = true;
						e.handled = true;
					}
				}
				else if (e.get_type() == EventType::MouseButton) {
					auto& me = static_cast<MouseButtonEvent&>(e);
					if (me.button == MouseButton::Left && me.pressed) {
						if (input::in_bounds(me.position, cbox_pos, { deco_size, deco_size })) {
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
				YGNodeStyleSetPosition(child->yoga_node, YGEdgeTop, (size.y - 16.0f) * 0.5f);

				offset_x += 16.0f + style::padding;
			}
		}

		if (e.get_type() == EventType::MouseMove) {
			auto& me = static_cast<MouseMoveEvent&>(e);
			is_hovered = input::in_bounds(me.position, pos, { clickable_width, size.y });
		}
		else if (e.get_type() == EventType::MouseButton) {
			auto& me = static_cast<MouseButtonEvent&>(e);
			if (me.button == MouseButton::Left && me.pressed) {
				if (input::in_bounds(me.position, pos, { clickable_width, size.y })) {
					value = !value;
					if (on_change) on_change(value);
					e.handled = true;
				}
			}
		}
	}

	float Checkbox::get_preferred_width()
	{
		glm::vec2 text_size = render::get_text_size(render::Fonts::NotoSans16px, label);
		return 16.0f + style::padding + text_size.x + style::padding * 2.0f;
	}
}
