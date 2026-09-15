#include "../globals.hpp"
#include "gui.hpp"

#include "../app/winapp.hpp"

#include <format>
#include <imgui.h>
#include <print>

#include <string_encryption.hpp>

#include <app/app.hpp>
#include <window/window.hpp>

#include <controls/checkbox/checkbox.hpp>
#include <controls/color_picker/color_picker.hpp>
#include <controls/dropdown/dropdown.hpp>

#include <render/render.hpp>
#include <utils/style.hpp>

#include <game/cache/cache.hpp>
#include <game/features/visuals/visuals.hpp>
#include <sdk/math/math.hpp>
#include <sdk/rust/entity/entity.hpp>

#include <game/features/aimbot/aimbot.hpp>
#include <game/game.hpp>

namespace gui
{
	void draw_debug_info()
	{
		/*f32 offset{};
		cache::for_each_entity<rust::Entity>([&](rust::Entity* e) {
			if (e->base_address == 0)
				return;

			if (e->type != EntityType::Player)
				return;

			auto fmt = std::format("{} (id:{}) - bones cached: {} - origin: {}, {}, {}", e->name, (u32)e->prefab_id, (e->bones && e->bones->size() > 0) ? "valid" : "invalid", e->origin.x, e->origin.y, e->origin.z);
			render::add_text(render::Fonts::NotoSans16px, fmt, glm::vec2{ 15.0f, 65.0f + offset }, Color::white(), render::TextFlagsDropShadow);
			offset += 16.0f;
			});*/

	}

	void on_render()
	{
		render::draw_list = ImGui::GetBackgroundDrawList();

		draw_debug_info();

		features::visuals::on_render();
		features::aimbot::on_render();

		bool& crosshair = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Screen"), xs("Crosshair"));
		if (crosshair)
		{
			Color& crosshair_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Screen"), xs("Crosshair"), xs("Color"));

			render::add_circle_filled(winapp::impl::window_size * 0.5f, 3.0f, Color::black().scale_alpha(crosshair_color.scalable_alpha()));
			render::add_circle_filled(winapp::impl::window_size * 0.5f, 2.0f, crosshair_color);
		}

		is_open = app::windows[0]->m_opened;
		winapp::impl::window_size = glm::vec2{ ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y };

		app::render();

		bool& vertical_sync = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Settings"), xs("UI"), xs("Vertical sync"));
		winapp::impl::present_with_virtual_sync = vertical_sync;

		bool& override_accent = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Settings"), xs("UI"), xs("Override accent"));
		Color& accent = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Settings"), xs("UI"), xs("Override accent"), xs("Color"));
		gui::style::colors::accent = override_accent ? accent : Color(255, 85, 200);
	}
}