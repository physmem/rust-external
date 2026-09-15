#include "window.hpp"

#include <input/input.hpp>
#include <render/render.hpp>
#include <utils/style.hpp>

#include <render/assets/font_awesome.hpp>
#include <span>
#include <windows.h>

#include <globals.hpp>
#include <print>

namespace gui
{
	Window::Window(std::string title, glm::vec2 init_pos, glm::vec2 init_size, WindowFlags flags)
		: Object(title), window_title(title), window_flags(flags),
		root_position(init_pos), root_size(init_size)
	{
		YGNodeStyleSetWidth(yoga_node, init_size.x);
		YGNodeStyleSetHeight(yoga_node, init_size.y);
		YGNodeStyleSetFlexDirection(yoga_node, YGFlexDirectionColumn);

		apply_flags(flags);
	}

	Window::~Window()
	{
	}

	void Window::apply_flags(WindowFlags flags)
	{
		struct AlignmentMap {
			WindowFlags flag{};
			YGJustify justify{};
			YGAlign align{};
		};

		static constexpr AlignmentMap align_maps[]{
			{ WindowFlags::AlignCenter,     YGJustifyCenter,    YGAlignCenter },
			{ WindowFlags::AlignTopRight,   YGJustifyFlexStart, YGAlignFlexEnd },
			{ WindowFlags::AlignBottomLeft, YGJustifyFlexEnd,   YGAlignFlexStart }
		};

		for (const auto& map : align_maps) {
			if (flags & map.flag) {
				YGNodeStyleSetJustifyContent(yoga_node, map.justify);
				YGNodeStyleSetAlignItems(yoga_node, map.align);
				break;
			}
		}
	}

	void Window::calculate_layout()
	{
		YGNodeCalculateLayout(yoga_node, root_size.x, root_size.y, YGDirectionLTR);
	}

	void Window::render()
	{
		m_fade_anim.update(m_opened ? 1.0f : 0.0);

		if (!m_opened && !m_fade_anim)
			return;

		std::int32_t start_idx = render::draw_list->VtxBuffer.Size;

		update_layout();
		calculate_layout();

		render::add_shadow_rect(root_position, root_size, style::colors::window_shadow, 25.0f, style::window_rounding);
		render::draw_list->AddImage(render::menu_bg.get_srv(), root_position, root_position + root_size);
		render::add_rect(root_position, root_size, style::colors::window_border, style::window_rounding, 2.0f);
		render::add_rect(root_position - glm::vec2{ 1.0f }, root_size + glm::vec2{ 2.0f }, style::colors::window_shadow, style::window_rounding);

		if (!(window_flags & WindowFlags::NoTitleBar)) {

			const float header_height = style::tab_header_height;
			const glm::vec2 icon_size = render::get_text_size(render::Fonts::Icons20px, ICON_FA_PAW);
			const glm::vec2 title_size = render::get_text_size(render::Fonts::NotoSans18px, window_title_show);
			render::add_circle_shadow(root_position + glm::vec2{ style::padding + icon_size.x * 0.5f, header_height * 0.5f }, 3.0f, style::colors::accent, 25.0f);
			render::rotate_vertices(0.3f, [&] {
				render::add_text(render::Fonts::Icons20px, ICON_FA_PAW, root_position + glm::vec2{ style::padding, header_height * 0.5f }, style::colors::accent, render::TextFlagsNone, glm::vec2{ 0.0f, 0.5f });

				});
			render::add_text(render::Fonts::NotoSans18px, window_title_show, root_position + glm::vec2{ style::padding * 2.0f + icon_size.x, header_height * 0.5f }, style::colors::text_active, render::TextFlagsNone, glm::vec2{ 0.0f, 0.5f });
			render::add_text(render::Fonts::NotoSans18px, window_tld, root_position + glm::vec2{ style::padding * 2.0f + icon_size.x + title_size.x, header_height * 0.5f }, style::colors::accent, render::TextFlagsNone, glm::vec2{ 0.0f, 0.5f });

			/* footer */
			render::add_text(render::Fonts::NotoSans18px, "Rust", root_position + glm::vec2{ style::padding, root_size.y - (style::footer_height * 0.5f) }, style::colors::accent, render::TextFlagsNone, glm::vec2{ 0.0f, 0.5f });
			render::add_text(render::Fonts::NotoSans18px, "Developer", root_position + glm::vec2{ root_size.x - style::padding, root_size.y - (style::footer_height * 0.5f) }, style::colors::text_inactive, render::TextFlagsNone, glm::vec2{ 1.0f, 0.5f });
		}

		for (auto& child : children) {
			child->render();
		}

		std::vector<Object*> overlay_chain;
		for (auto curr = Object::g_active_object; curr; curr = curr->parent)
			overlay_chain.push_back(curr);

		std::reverse(overlay_chain.begin(), overlay_chain.end());

		for (auto* obj : overlay_chain)
			obj->render_overlay();

		if (Object::g_focused_object && Object::g_focused_object != Object::g_active_object)
		{
			// If focused object is separate and not an ancestor of the active object, render it on top
			if (!Object::g_active_object || !Object::g_active_object->is_descendant_of(Object::g_focused_object))
			{
				Object::g_focused_object->render_overlay();
			}
		}

		std::int32_t end_idx = render::draw_list->VtxBuffer.Size;
		render::modify_alpha(start_idx, end_idx, m_fade_anim.value);
	}

	void Window::dispatch_event(Event& e)
	{
		if (e.get_type() == EventType::KeyPress)
		{
			auto& ke = static_cast<KeyPressEvent&>(e);
			if (ke.key == VK_INSERT)
			{
				m_opened = !m_opened;
				e.handled = true;
				return;
			}
		}

		if (!m_opened)
			return;

		if (is_dragging)
		{
			if (e.get_type() == EventType::MouseMove)
			{
				root_position = static_cast<MouseMoveEvent&>(e).position - drag_offset;
				const auto display_size = ImGui::GetIO().DisplaySize;
				root_position.x = std::clamp(root_position.x, 0.0f, display_size.x - root_size.x);
				root_position.y = std::clamp(root_position.y, 0.0f, display_size.y - root_size.y);
				e.handled = true;
				return;
			}

			if (e.get_type() == EventType::MouseButton)
			{
				auto& me = static_cast<MouseButtonEvent&>(e);
				if (me.button == MouseButton::Left && !me.pressed)
				{
					is_dragging = false;
					e.handled = true;
					return;
				}
			}
		}

		if (Object::g_active_object)
		{
			Object::g_active_object->dispatch_event(e);

			if (!e.handled)
			{
				for (auto curr = Object::g_active_object->parent; curr; curr = curr->parent)
				{
					curr->on_event(e);
					if (e.handled) break;
				}
			}

			if (e.handled) {
				return;
			}
		}

		if (Object::g_focused_object)
		{
			Object::g_focused_object->dispatch_event(e);

			if (e.handled) {
				return;
			}
		}

		if (!e.handled && !Object::g_active_object)
		{
			Object::dispatch_event(e);
		}

		if (!e.handled && e.get_type() == EventType::MouseButton && !Object::g_active_object)
		{
			auto& me = static_cast<MouseButtonEvent&>(e);

			if (me.button == MouseButton::Left && me.pressed &&
				input::in_bounds(me.position, root_position, root_size))
			{
				is_dragging = true;
				drag_offset = me.position - root_position;
				e.handled = true;
				return;
			}
		}
	}
}
