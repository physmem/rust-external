#include "visuals.hpp"
#include <game/cache/cache.hpp>
#include <sdk/rust/entity/entity.hpp>

#include <app/app.hpp>
#include <controls/checkbox/checkbox.hpp>
#include <controls/color_picker/color_picker.hpp>
#include <controls/slider/slider.hpp>
#include <string_encryption.hpp>
#include <window/window.hpp>

#include <app/winapp.hpp>
#include <controls/multi_dropdown/multi_dropdown.hpp>
#include <format>
#include <print>
#include <render/render.hpp>
#include <sdk/math/math.hpp>

#include <controls/dropdown/dropdown.hpp>
#include <game/game.hpp>
#include <map>
#include <utils/style.hpp>
#include <vector>

namespace features::visuals
{
	void player(rust::Entity* entity);
	void view_line(rust::Entity* entity);
	void render_as(std::string category, std::string type, rust::Entity* entity);

	void render_radar();

	void on_render()
	{
		if (!game::is_in_game())
			return;

		if (!game::impl::local_player)
			return;

		render_radar();

		bool& inventory_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Screen"), xs("Player inventory"));
		rust::Entity* closest_player = nullptr;
		f32 closest_crosshair_dist = 100000.0f;
		glm::vec2 screen_center = { ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f };

		cache::for_each_entity<rust::Entity>([&](rust::Entity* entity) {
			if (entity->type != EntityType::Player || !entity->is_alive() || entity->base_address == game::impl::local_player->base_address)
				return;

			glm::vec2 screen_pos{};
			if (math::world_to_screen(entity->origin, &screen_pos)) {
				f32 dist = glm::distance(screen_center, screen_pos);
				if (dist < closest_crosshair_dist) {
					closest_crosshair_dist = dist;
					closest_player = entity;
				}
			}
			});

		cache::for_each_entity<rust::Entity>([&](rust::Entity* entity) {
			if (entity->type != EntityType::Player)
				return;

			bool is_target = (entity == closest_player && closest_crosshair_dist < 300.0f && inventory_enabled && !entity->is_sleeping);
			entity->inventory_anim.update(is_target);

			float alpha = entity->inventory_anim;
			if (alpha <= 0.0f) return;

			if (entity->belt_items.empty()) return;

			f32 item_size = 58.0f;
			f32 padding = 8.0f;
			f32 total_width = (item_size * entity->belt_items.size()) + (padding * (entity->belt_items.size() - 1));
			glm::vec2 start_pos = { ImGui::GetIO().DisplaySize.x / 2.0f - total_width / 2.0f, 80.0f };

			render::add_text(render::Fonts::Arial14px, entity->name + xs("'s inventory"),
				{ ImGui::GetIO().DisplaySize.x / 2.0f, start_pos.y - 10.0f },
				Color::white().scale_alpha(alpha),
				render::TextFlagsDropShadow, glm::vec2{ 0.5f, 1.0f });

			for (std::size_t i = 0; i < entity->belt_items.size(); ++i)
			{
				auto& item = entity->belt_items[i];
				glm::vec2 pos = start_pos + glm::vec2{ i * (item_size + padding), 0.0f };

				render::add_shadow_rect(pos, glm::vec2{ item_size }, Color::black().scale_alpha(alpha), 25.0f, 10.0f);
				render::add_rect_filled(pos, glm::vec2{ item_size }, Color(15, 15, 15, static_cast<int>(200 * alpha)), 10.0f);
				render::add_rect(pos, glm::vec2{ item_size }, Color(255, 255, 255, static_cast<int>(15 * alpha)), 10.0f, 1.5f);

				if (item.texture && item.texture->is_valid())
				{
					if (auto srv = item.texture->get_srv())
					{
						render::add_image(srv, pos + glm::vec2{ 6.0f }, glm::vec2{ item_size - 12.0f }, 0.0f, Color::white().scale_alpha(alpha));
					}
				}

				if (item.shortname == entity->item_shortname && !item.shortname.empty())
				{
					render::add_rect(pos, glm::vec2{ item_size }, gui::style::colors::accent.scale_alpha(alpha), 10.0f, 1.5f);
				}

				if (item.amount > 1)
				{
					std::string amt_str = (item.amount > 1000) ? std::to_string(item.amount / 1000) + xs("k") : std::to_string(item.amount);
					render::add_text(render::Fonts::Arial14px, amt_str,
						pos + glm::vec2{ item_size - 8.0f, item_size - 8.0f },
						Color::white().scale_alpha(alpha),
						render::TextFlagsDropShadow, glm::vec2{ 1.0f, 1.0f });
				}
			}
			});

		bool& entities_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), xs("Enabled"));

		bool debug_esp = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Debug ESP"));
		Color debug_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Debug ESP"), xs("Color"));

		f32 closest_distance = 100.0f;
		std::string closest_prefab = xs("");

		struct TextStack {
			glm::vec2 pos;
			std::vector<std::pair<uptr, std::string>> names;
		};
		std::vector<TextStack> debug_stacks;

		cache::for_each_entity<rust::Entity>([&](rust::Entity* entity) {
			if (!entity->base_address)
				return;

			if (entity->base_address == game::impl::local_player->base_address)
				return;

			if (debug_esp && !entity->prefab_name.empty() && entity->origin.x != 0.0f) {
				if (entity->prefab_name.find(xs("subents")) != std::string::npos ||
					entity->prefab_name.find(xs("seats")) != std::string::npos)
					return;

				float dist3d = glm::distance(entity->origin, game::impl::local_player->origin);
				if (dist3d < 10.0f)
				{
					glm::vec2 out{};
					if (math::world_to_screen(entity->origin, &out)) {

						bool found_stack = false;
						for (auto& stack : debug_stacks) {
							if (glm::distance(stack.pos, out) < 10.0f) {
								stack.names.push_back(std::make_pair(entity->base_address, entity->prefab_name));
								found_stack = true;
								break;
							}
						}

						if (!found_stack) {
							debug_stacks.push_back({ out, { std::make_pair(entity->base_address, entity->prefab_name) } });
						}

						float dist = glm::distance(out, screen_center);
						if (dist < closest_distance) {
							closest_distance = dist;
							closest_prefab = entity->prefab_name;
						}
					}
				}
			}

			switch (entity->type)
			{
			case EntityType::Stone: render_as(xs("Ores & Collectibles"), xs("Stone"), entity); break;
			case EntityType::Sulfur: render_as(xs("Ores & Collectibles"), xs("Sulfur"), entity); break;
			case EntityType::Metal: render_as(xs("Ores & Collectibles"), xs("Metal"), entity); break;
			case EntityType::Hemp: render_as(xs("Ores & Collectibles"), xs("Hemp"), entity); break;
			case EntityType::Wood: render_as(xs("Ores & Collectibles"), xs("Wood"), entity); break;
			case EntityType::DieselFuel: render_as(xs("Ores & Collectibles"), xs("Diesel fuel"), entity); break;
			case EntityType::GreenKeycard: render_as(xs("Ores & Collectibles"), xs("Green keycard"), entity); break;
			case EntityType::BlueKeycard: render_as(xs("Ores & Collectibles"), xs("Blue keycard"), entity); break;
			case EntityType::RedKeycard: render_as(xs("Ores & Collectibles"), xs("Red keycard"), entity); break;

			case EntityType::EliteCrate: render_as(xs("Crates & Barrels"), xs("Elite crate"), entity); break;
			case EntityType::MilitaryCrate: render_as(xs("Crates & Barrels"), xs("Military crate"), entity); break;
			case EntityType::LockedCrate: render_as(xs("Crates & Barrels"), xs("Locked crate"), entity); break;
			case EntityType::AirDrop: render_as(xs("Crates & Barrels"), xs("Air drop"), entity); break;
			case EntityType::LootBarrel: render_as(xs("Crates & Barrels"), xs("Loot barrel"), entity); break;
			case EntityType::OilBarrel: render_as(xs("Crates & Barrels"), xs("Oil barrel"), entity); break;
			case EntityType::FoodCrate: render_as(xs("Crates & Barrels"), xs("Food crate"), entity); break;
			case EntityType::ToolCrate: render_as(xs("Crates & Barrels"), xs("Tool crate"), entity); break;
			case EntityType::SmallCrate: render_as(xs("Crates & Barrels"), xs("Small crate"), entity); break;
			case EntityType::HealthCrate: render_as(xs("Crates & Barrels"), xs("Health crate"), entity); break;
			case EntityType::SmallFoodCrate: render_as(xs("Crates & Barrels"), xs("Small food crate"), entity); break;
			case EntityType::VehicleParts: render_as(xs("Crates & Barrels"), xs("Vehicle parts"), entity); break;
			case EntityType::Crate: render_as(xs("Crates & Barrels"), xs("Crate"), entity); break;

			case EntityType::WoodenBox: render_as(xs("Deployables"), xs("Wooden box"), entity); break;
			case EntityType::LargeWoodBox: render_as(xs("Deployables"), xs("Large wood box"), entity); break;
			case EntityType::ToolCupboard: render_as(xs("Deployables"), xs("Tool cupboard"), entity); break;
			case EntityType::Workbench1Alt: render_as(xs("Deployables"), xs("Tier 1 workbench"), entity); break;
			case EntityType::Workbench2Alt: render_as(xs("Deployables"), xs("Tier 2 workbench"), entity); break;
			case EntityType::Workbench3Alt: render_as(xs("Deployables"), xs("Tier 3 workbench"), entity); break;
			case EntityType::RepairBench: render_as(xs("Deployables"), xs("Repair bench"), entity); break;
			case EntityType::ResearchTable: render_as(xs("Deployables"), xs("Research table"), entity); break;
			case EntityType::SmallStash: render_as(xs("Deployables"), xs("Small stash"), entity); break;
			case EntityType::SleepingBag: render_as(xs("Deployables"), xs("Sleeping bag"), entity); break;
			case EntityType::Furnace: render_as(xs("Deployables"), xs("Furnace"), entity); break;
			case EntityType::Locker: render_as(xs("Deployables"), xs("Locker"), entity); break;

			case EntityType::AutoTurret: render_as(xs("Traps & Turrets"), xs("Auto turret"), entity); break;
			case EntityType::FlameTurret: render_as(xs("Traps & Turrets"), xs("Flame turret"), entity); break;
			case EntityType::ShotgunTrap: render_as(xs("Traps & Turrets"), xs("Shotgun trap"), entity); break;

			case EntityType::Player: player(entity); break;
			case EntityType::Scientist: render_as(xs("NPCs & Animals"), xs("Scientist"), entity); break;
			case EntityType::Dweller: render_as(xs("NPCs & Animals"), xs("Dweller"), entity); break;
			case EntityType::Bear: render_as(xs("NPCs & Animals"), xs("Bear"), entity); break;
			case EntityType::Wolf: render_as(xs("NPCs & Animals"), xs("Wolf"), entity); break;
			case EntityType::Stag: render_as(xs("NPCs & Animals"), xs("Stag"), entity); break;
			case EntityType::Boar: render_as(xs("NPCs & Animals"), xs("Boar"), entity); break;
			case EntityType::Horse: render_as(xs("NPCs & Animals"), xs("Horse"), entity); break;
			case EntityType::Chicken: render_as(xs("NPCs & Animals"), xs("Chicken"), entity); break;
			case EntityType::Shark: render_as(xs("NPCs & Animals"), xs("Shark"), entity); break;
			case EntityType::Scarecrow: render_as(xs("NPCs & Animals"), xs("Scarecrow"), entity); break;
			case EntityType::Corpse: render_as(xs("NPCs & Animals"), xs("Corpse"), entity); break;

			case EntityType::Recycler: render_as(xs("Static & Monuments"), xs("Recycler"), entity); break;
			case EntityType::PhoneBooth: render_as(xs("Static & Monuments"), xs("Phone booth"), entity); break;
			case EntityType::FuseBox: render_as(xs("Static & Monuments"), xs("Fuse box"), entity); break;
			case EntityType::Refinery: render_as(xs("Static & Monuments"), xs("Refinery"), entity); break;
			case EntityType::Elevator: render_as(xs("Static & Monuments"), xs("Elevator"), entity); break;
			case EntityType::CarLift: render_as(xs("Static & Monuments"), xs("Car lift"), entity); break;
			case EntityType::ComputerStation: render_as(xs("Static & Monuments"), xs("Computer station"), entity); break;

			case EntityType::Motorbike: render_as(xs("Vehicles"), xs("Motorbike"), entity); break;
			case EntityType::SidecarMotorbike: render_as(xs("Vehicles"), xs("Sidecar motorbike"), entity); break;
			case EntityType::PedalBike: render_as(xs("Vehicles"), xs("Pedal bike"), entity); break;
			case EntityType::Snowmobile: render_as(xs("Vehicles"), xs("Snowmobile"), entity); break;
			case EntityType::TomahaSnowmobile: render_as(xs("Vehicles"), xs("Tomaha snowmobile"), entity); break;
			case EntityType::Tugboat: render_as(xs("Vehicles"), xs("Tugboat"), entity); break;
			case EntityType::Rowboat: render_as(xs("Vehicles"), xs("Rowboat"), entity); break;
			case EntityType::RHIB: render_as(xs("Vehicles"), xs("RHIB"), entity); break;
			case EntityType::Kyak: render_as(xs("Vehicles"), xs("Kyak"), entity); break;
			case EntityType::SoloSubmarine: render_as(xs("Vehicles"), xs("Solo submarine"), entity); break;
			case EntityType::DuoSubmarine: render_as(xs("Vehicles"), xs("Duo submarine"), entity); break;
			case EntityType::Minicopter: render_as(xs("Vehicles"), xs("Minicopter"), entity); break;
			case EntityType::ScrapHeli: render_as(xs("Vehicles"), xs("Scrap heli"), entity); break;
			case EntityType::AttackHeli: render_as(xs("Vehicles"), xs("Attack heli"), entity); break;

			case EntityType::DroppedRifle: render_as(xs("Dropped weapons"), xs("Dropped rifles"), entity); break;
			case EntityType::DroppedSniper: render_as(xs("Dropped weapons"), xs("Dropped snipers"), entity); break;
			case EntityType::DroppedSMG: render_as(xs("Dropped weapons"), xs("Dropped SMGs"), entity); break;
			case EntityType::DroppedShotgun: render_as(xs("Dropped weapons"), xs("Dropped shotguns"), entity); break;
			case EntityType::DroppedPistol: render_as(xs("Dropped weapons"), xs("Dropped pistols"), entity); break;
			case EntityType::DroppedLMG: render_as(xs("Dropped weapons"), xs("Dropped LMGs"), entity); break;
			case EntityType::DroppedLauncher: render_as(xs("Dropped weapons"), xs("Dropped launchers"), entity); break;
			case EntityType::DroppedBow: render_as(xs("Dropped weapons"), xs("Dropped bows"), entity); break;
			case EntityType::DroppedMelee: render_as(xs("Dropped weapons"), xs("Dropped melee"), entity); break;
			case EntityType::DroppedMisc: render_as(xs("Dropped weapons"), xs("Dropped misc"), entity); break;
			}

			});

		// Render debug stacks
		for (const auto& stack : debug_stacks) {
			float y_offset = 0.0f;
			for (const auto& name : stack.names) {
				std::string fmt = std::vformat(xs("{:#x} {}"), std::make_format_args(name.first, name.second));
				render::add_text(render::Fonts::Arial14px, fmt, stack.pos + glm::vec2{ 0.0f, y_offset }, debug_color, render::TextFlagsDropShadow, glm::vec2{ 0.5f });
				y_offset += 14.0f;
			}
		}

		if (debug_esp && !closest_prefab.empty()) {
			static std::string last_closest = xs("");
			if (closest_prefab != last_closest) {
				std::println("{}", std::vformat(xs("Closest prefab: {}"), std::make_format_args(closest_prefab)));
				last_closest = closest_prefab;
			}
		}
	}

	void render_radar()
	{
		bool& enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Radar"), xs("Enabled"));
		if (!enabled)
			return;

		if (!game::impl::local_player)
			return;

		const f32 radar_radius = app::get_value<gui::Slider<f32>>(xs("Pawjob"), xs("Visuals"), xs("Radar"), xs("Size"));
		const f32 radar_range = app::get_value<gui::Slider<f32>>(xs("Pawjob"), xs("Visuals"), xs("Radar"), xs("Range"));
		const f32 dot_size = 3.0f;
		const f32 background_opacity = 0.6f;

		const glm::vec2 position{ 25.0f, 25.0f + 50.0f }; // + 50.0f for the watermark
		const glm::vec2 center = position + radar_radius;

		render::add_circle_shadow(center, radar_radius, Color::black(), 25.0f);
		render::add_circle_filled(center, radar_radius, Color::black().scale_alpha(background_opacity));

		render::add_rect_filled(position + glm::vec2{ radar_radius, 0.0f }, glm::vec2{ 1.0f, radar_radius * 2.0f }, Color::black().scale_alpha(0.5f));
		render::add_rect_filled(position + glm::vec2{ 0.0f, radar_radius }, glm::vec2{ radar_radius * 2.0f, 1.0f }, Color::black().scale_alpha(0.5f));

		const f32 fov = app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Field of view"));
		const f32 aspect_ratio = winapp::impl::window_size.x / winapp::impl::window_size.y;
		const f32 horizontal_fov = 2.0f * std::atan(std::tan(glm::radians(fov) * 0.5f) * aspect_ratio);

		const glm::vec2 left_fov_dir = { std::sin(-horizontal_fov * 0.5f), -std::cos(-horizontal_fov * 0.5f) };
		const glm::vec2 right_fov_dir = { std::sin(horizontal_fov * 0.5f), -std::cos(horizontal_fov * 0.5f) };

		render::add_line(center, center + left_fov_dir * radar_radius, Color::white().scale_alpha(0.3f));
		render::add_line(center, center + right_fov_dir * radar_radius, Color::white().scale_alpha(0.3f));

		render::add_circle(center, radar_radius, Color::black().scale_alpha(0.5f));

		const glm::vec3 local_pos = game::impl::local_player->origin;
		const f32 local_yaw = game::impl::local_player->view_angles.y;

		bool players_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Enabled"));
		bool entities_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), xs("Enabled"));

		cache::for_each_entity<rust::Entity>([&](rust::Entity* entity) {
			if (!entity->base_address || entity->base_address == game::impl::local_player->base_address)
				return;

			bool should_render = false;
			Color dot_color = Color::white();

			if (entity->type == EntityType::Player) {
				if (!players_enabled) return;

				bool& radar_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Enabled"), xs("Options"), xs("Include in radar"));
				if (radar_enabled) {
					should_render = true;
					dot_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Color"));
				}
			}
			else {
				std::string category, name;
				switch (entity->type)
				{
				case EntityType::Stone: category = xs("Ores & Collectibles"); name = xs("Stone"); break;
				case EntityType::Sulfur: category = xs("Ores & Collectibles"); name = xs("Sulfur"); break;
				case EntityType::Metal: category = xs("Ores & Collectibles"); name = xs("Metal"); break;
				case EntityType::Hemp: category = xs("Ores & Collectibles"); name = xs("Hemp"); break;
				case EntityType::Wood: category = xs("Ores & Collectibles"); name = xs("Wood"); break;
				case EntityType::DieselFuel: category = xs("Ores & Collectibles"); name = xs("Diesel fuel"); break;
				case EntityType::GreenKeycard: category = xs("Ores & Collectibles"); name = xs("Green keycard"); break;
				case EntityType::BlueKeycard: category = xs("Ores & Collectibles"); name = xs("Blue keycard"); break;
				case EntityType::RedKeycard: category = xs("Ores & Collectibles"); name = xs("Red keycard"); break;

				case EntityType::EliteCrate: category = xs("Crates & Barrels"); name = xs("Elite crate"); break;
				case EntityType::MilitaryCrate: category = xs("Crates & Barrels"); name = xs("Military crate"); break;
				case EntityType::LockedCrate: category = xs("Crates & Barrels"); name = xs("Locked crate"); break;
				case EntityType::AirDrop: category = xs("Crates & Barrels"); name = xs("Air drop"); break;
				case EntityType::LootBarrel: category = xs("Crates & Barrels"); name = xs("Loot barrel"); break;
				case EntityType::OilBarrel: category = xs("Crates & Barrels"); name = xs("Oil barrel"); break;
				case EntityType::FoodCrate: category = xs("Crates & Barrels"); name = xs("Food crate"); break;
				case EntityType::ToolCrate: category = xs("Crates & Barrels"); name = xs("Tool crate"); break;
				case EntityType::SmallCrate: category = xs("Crates & Barrels"); name = xs("Small crate"); break;
				case EntityType::HealthCrate: category = xs("Crates & Barrels"); name = xs("Health crate"); break;
				case EntityType::SmallFoodCrate: category = xs("Crates & Barrels"); name = xs("Small food crate"); break;
				case EntityType::VehicleParts: category = xs("Crates & Barrels"); name = xs("Vehicle parts"); break;
				case EntityType::Crate: category = xs("Crates & Barrels"); name = xs("Crate"); break;

				case EntityType::WoodenBox: category = xs("Deployables"); name = xs("Wooden box"); break;
				case EntityType::LargeWoodBox: category = xs("Deployables"); name = xs("Large wood box"); break;
				case EntityType::ToolCupboard: category = xs("Deployables"); name = xs("Tool cupboard"); break;
				case EntityType::Workbench1Alt: category = xs("Deployables"); name = xs("Tier 1 workbench"); break;
				case EntityType::Workbench2Alt: category = xs("Deployables"); name = xs("Tier 2 workbench"); break;
				case EntityType::Workbench3Alt: category = xs("Deployables"); name = xs("Tier 3 workbench"); break;
				case EntityType::RepairBench: category = xs("Deployables"); name = xs("Repair bench"); break;
				case EntityType::ResearchTable: category = xs("Deployables"); name = xs("Research table"); break;
				case EntityType::SmallStash: category = xs("Deployables"); name = xs("Small stash"); break;
				case EntityType::SleepingBag: category = xs("Deployables"); name = xs("Sleeping bag"); break;
				case EntityType::Furnace: category = xs("Deployables"); name = xs("Furnace"); break;
				case EntityType::Locker: category = xs("Deployables"); name = xs("Locker"); break;

				case EntityType::AutoTurret: category = xs("Traps & Turrets"); name = xs("Auto turret"); break;
				case EntityType::FlameTurret: category = xs("Traps & Turrets"); name = xs("Flame turret"); break;
				case EntityType::ShotgunTrap: category = xs("Traps & Turrets"); name = xs("Shotgun trap"); break;

				case EntityType::Scientist: category = xs("NPCs & Animals"); name = xs("Scientist"); break;
				case EntityType::Dweller: category = xs("NPCs & Animals"); name = xs("Dweller"); break;
				case EntityType::Bear: category = xs("NPCs & Animals"); name = xs("Bear"); break;
				case EntityType::Wolf: category = xs("NPCs & Animals"); name = xs("Wolf"); break;
				case EntityType::Stag: category = xs("NPCs & Animals"); name = xs("Stag"); break;
				case EntityType::Boar: category = xs("NPCs & Animals"); name = xs("Boar"); break;
				case EntityType::Horse: category = xs("NPCs & Animals"); name = xs("Horse"); break;
				case EntityType::Chicken: category = xs("NPCs & Animals"); name = xs("Chicken"); break;
				case EntityType::Shark: category = xs("NPCs & Animals"); name = xs("Shark"); break;
				case EntityType::Scarecrow: category = xs("NPCs & Animals"); name = xs("Scarecrow"); break;
				case EntityType::Corpse: category = xs("NPCs & Animals"); name = xs("Corpse"); break;

				case EntityType::Recycler: category = xs("Static & Monuments"); name = xs("Recycler"); break;
				case EntityType::PhoneBooth: category = xs("Static & Monuments"); name = xs("Phone booth"); break;
				case EntityType::FuseBox: category = xs("Static & Monuments"); name = xs("Fuse box"); break;
				case EntityType::Refinery: category = xs("Static & Monuments"); name = xs("Refinery"); break;
				case EntityType::Elevator: category = xs("Static & Monuments"); name = xs("Elevator"); break;
				case EntityType::CarLift: category = xs("Static & Monuments"); name = xs("Car lift"); break;
				case EntityType::ComputerStation: category = xs("Static & Monuments"); name = xs("Computer station"); break;

				case EntityType::Motorbike: category = xs("Vehicles"); name = xs("Motorbike"); break;
				case EntityType::SidecarMotorbike: category = xs("Vehicles"); name = xs("Sidecar motorbike"); break;
				case EntityType::PedalBike: category = xs("Vehicles"); name = xs("Pedal bike"); break;
				case EntityType::Snowmobile: category = xs("Vehicles"); name = xs("Snowmobile"); break;
				case EntityType::TomahaSnowmobile: category = xs("Vehicles"); name = xs("Tomaha snowmobile"); break;
				case EntityType::Tugboat: category = xs("Vehicles"); name = xs("Tugboat"); break;
				case EntityType::Rowboat: category = xs("Vehicles"); name = xs("Rowboat"); break;
				case EntityType::RHIB: category = xs("Vehicles"); name = xs("RHIB"); break;
				case EntityType::Kyak: category = xs("Vehicles"); name = xs("Kyak"); break;
				case EntityType::SoloSubmarine: category = xs("Vehicles"); name = xs("Solo submarine"); break;
				case EntityType::DuoSubmarine: category = xs("Vehicles"); name = xs("Duo submarine"); break;
				case EntityType::Minicopter: category = xs("Vehicles"); name = xs("Minicopter"); break;
				case EntityType::ScrapHeli: category = xs("Vehicles"); name = xs("Scrap heli"); break;
				case EntityType::AttackHeli: category = xs("Vehicles"); name = xs("Attack heli"); break;

				case EntityType::DroppedRifle: category = xs("Dropped weapons"); name = xs("Dropped rifles"); break;
				case EntityType::DroppedSniper: category = xs("Dropped weapons"); name = xs("Dropped snipers"); break;
				case EntityType::DroppedSMG: category = xs("Dropped weapons"); name = xs("Dropped SMGs"); break;
				case EntityType::DroppedShotgun: category = xs("Dropped weapons"); name = xs("Dropped shotguns"); break;
				case EntityType::DroppedPistol: category = xs("Dropped weapons"); name = xs("Dropped pistols"); break;
				case EntityType::DroppedLMG: category = xs("Dropped weapons"); name = xs("Dropped LMGs"); break;
				case EntityType::DroppedLauncher: category = xs("Dropped weapons"); name = xs("Dropped launchers"); break;
				case EntityType::DroppedBow: category = xs("Dropped weapons"); name = xs("Dropped bows"); break;
				case EntityType::DroppedMelee: category = xs("Dropped weapons"); name = xs("Dropped melee"); break;
				case EntityType::DroppedMisc: category = xs("Dropped weapons"); name = xs("Dropped misc"); break;
				}

				if (!category.empty()) {
					if (!entities_enabled) return;
					//if (!app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, xs("Enabled"))) return;

					bool& radar_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, name, xs("Options"), xs("Include in radar"));
					if (radar_enabled) {
						should_render = true;
						dot_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, name, xs("Color"));
					}
				}
			}

			if (should_render) {
				const f32 dx = entity->origin.x - local_pos.x;
				const f32 dz = entity->origin.z - local_pos.z;

				const f32 dist = std::sqrt(dx * dx + dz * dz);
				if (dist > radar_range)
					return;

				const f32 yaw_rad = glm::radians(local_yaw);
				const f32 cos_y = std::cos(yaw_rad);
				const f32 sin_y = std::sin(yaw_rad);

				const f32 rotated_x = dx * cos_y - dz * sin_y;
				const f32 rotated_z = dx * sin_y + dz * cos_y;

				const f32 scale = radar_radius / radar_range;
				const glm::vec2 point_pos = center + glm::vec2{ rotated_x * scale, -rotated_z * scale };

				render::add_circle_filled(point_pos, dot_size, dot_color);
				render::add_circle(point_pos, dot_size, Color::black().scale_alpha(0.5f), 1.0f);

				// this would be SICK if the dots wasnt so small
				/*const f32 y_diff = entity->origin.y - local_pos.y;
				if (std::abs(y_diff) > 1.5f) {
					const Color arrow_color = dot_color.invert();
					if (y_diff > 0.0f) {
						render::add_triangle_filled(
							{ point_pos.x, point_pos.y - dot_size },
							{ point_pos.x - dot_size, point_pos.y + dot_size * 0.5f },
							{ point_pos.x + dot_size, point_pos.y + dot_size * 0.5f },
							arrow_color
						);
					}
					else {
						render::add_triangle_filled(
							{ point_pos.x, point_pos.y + dot_size },
							{ point_pos.x - dot_size, point_pos.y - dot_size * 0.5f },
							{ point_pos.x + dot_size, point_pos.y - dot_size * 0.5f },
							arrow_color
						);
					}
				}*/
			}
			});
	}

	void render_as(std::string category, std::string type, rust::Entity* entity)
	{
		bool& entities_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), xs("Enabled"));
		if (!entities_enabled)
			return;

		bool& enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, type);
		if (!enabled)
			return;

		if (entity->origin.x == 0.0f &&
			entity->origin.y == 0.0f &&
			entity->origin.z == 0.0f)
			return;

		i32 max_distance = app::get_value<gui::Slider<int>>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, type, xs("Options"), xs("Max distance"));
		const i32 distance = static_cast<i32>(glm::distance(game::impl::local_player->origin, entity->origin));
		if (distance > max_distance)
			return;

		glm::vec2 out{};
		if (!math::world_to_screen(entity->origin, &out))
			return;

		const i32 distance_int = static_cast<i32>(glm::distance(game::impl::local_player->origin, entity->origin));
		const std::string fmt = std::vformat(xs("{} [{}m]"), std::make_format_args(entity->name, distance_int));
		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Entities"), category, type, xs("Color"));
		render::add_text(render::Fonts::Arial14px, fmt, out, color, render::TextFlagsDropShadow, glm::vec2{ 0.5f });
	}

	void name(rust::Entity* entity)
	{
		bool& name = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Name"));
		if (!name)
			return;

		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Name"), xs("Color"));
		const auto& bbox = entity->bounding_box;

		i32& flags = app::get_value<gui::MultiDropdown>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"));
		const bool flags_distance = flags & (1 << 0);

		std::string name_str = entity->name;
		if (flags_distance) {
			const i32 distance_int = static_cast<i32>(entity->distance);
			name_str.append(std::vformat(xs(" [{}m]"), std::make_format_args(distance_int)));
		}

		const glm::vec2 text_size = render::get_text_size(render::Fonts::Arial14px, name_str);

		glm::vec2 rect_size{ text_size.y };

		bool& avatar = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Name"), xs("Options"), xs("Show avatar"));

		const bool avatar_ready = avatar && entity->avatar_status == rust::Entity::AvatarStatus::Ready && entity->avatar_texture.is_valid();
		ID3D11ShaderResourceView* avatar_srv = avatar_ready ? entity->avatar_texture.get_srv() : render::steam_avatar.get_srv();

		if (avatar_ready && avatar_srv)
		{
			const float full_width = rect_size.x + text_size.x + 4.0f;
			render::add_image(avatar_srv, bbox.position + glm::vec2{ bbox.size.x * 0.5f - full_width * 0.5f, -3.0f + -rect_size.y }, rect_size, 4.0f);
			render::add_text(render::Fonts::Arial14px, name_str, bbox.position + glm::vec2{ bbox.size.x * 0.5f - full_width * 0.5f + (rect_size.x + 4.0f), -3.0f }, color, render::TextFlagsDropShadow, glm::vec2{ 0.0f, 1.0f });
		}
		else {
			render::add_text(render::Fonts::Arial14px, name_str, bbox.position + glm::vec2{ bbox.size.x * 0.5f, -3.0f }, color, render::TextFlagsDropShadow, glm::vec2{ 0.5f, 1.0f });
		}
	}

	void item_name(rust::Entity* entity)
	{
		bool& item_name = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Item name"));
		bool& item_icon = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Item icon"));

		if (!item_name && !item_icon)
			return;

		const auto& bbox = entity->bounding_box;
		float y_offset = bbox.size.y + 2.0f;

		if (item_name && !entity->item_name.empty())
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Item name"), xs("Color"));
			render::add_text(render::Fonts::Pixelmix10px, entity->item_name, bbox.position + glm::vec2{ bbox.size.x * 0.5f, y_offset }, color, render::TextFlagsDropShadow, glm::vec2{ 0.5f, 0.0f });
			y_offset += 12.0f;
		}

		if (item_icon && entity->item_texture && entity->item_texture->is_valid())
		{
			if (auto srv = entity->item_texture->get_srv())
			{
				Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Item icon"), xs("Color"));
				render::add_image(srv, bbox.position + glm::vec2{ bbox.size.x * 0.5f - 14.0f, y_offset }, { 28.0f, 28.0f }, 0.0f, color);
			}
		}
	}

	void skeleton(rust::Entity* entity)
	{
		bool& skeleton = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Skeleton"));
		if (!skeleton)
			return;

		constexpr std::pair<rust::BoneList, rust::BoneList> bone_connections[] = {
			{ rust::BoneList::head, rust::BoneList::neck },
			{ rust::BoneList::neck, rust::BoneList::spine4 },
			{ rust::BoneList::spine4, rust::BoneList::spine3 },
			{ rust::BoneList::spine3, rust::BoneList::spine2 },
			{ rust::BoneList::spine2, rust::BoneList::spine1 },
			{ rust::BoneList::spine1, rust::BoneList::pelvis },

			{ rust::BoneList::spine4, rust::BoneList::l_clavicle },
			{ rust::BoneList::l_clavicle, rust::BoneList::l_upperarm },
			{ rust::BoneList::l_upperarm, rust::BoneList::l_forearm },
			{ rust::BoneList::l_forearm, rust::BoneList::l_hand },

			{ rust::BoneList::spine4, rust::BoneList::r_clavicle },
			{ rust::BoneList::r_clavicle, rust::BoneList::r_upperarm },
			{ rust::BoneList::r_upperarm, rust::BoneList::r_forearm },
			{ rust::BoneList::r_forearm, rust::BoneList::r_hand },

			{ rust::BoneList::pelvis, rust::BoneList::l_hip },
			{ rust::BoneList::l_hip, rust::BoneList::l_knee },
			{ rust::BoneList::l_knee, rust::BoneList::l_knee }, // l_foot

			{ rust::BoneList::spine1, rust::BoneList::r_hip },
			{ rust::BoneList::r_hip, rust::BoneList::r_knee },
			{ rust::BoneList::r_knee, rust::BoneList::r_foot }
		};

		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Skeleton"), xs("Color"));
		bool& head_circle = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Head circle"));

		for (const auto& connection : bone_connections)
		{
			if (head_circle && connection.first == rust::BoneList::head && connection.second == rust::BoneList::neck)
				continue;

			if (!entity->bones || entity->bones->find(connection.first) == entity->bones->end() ||
				entity->bones->find(connection.second) == entity->bones->end())
				continue;

			const auto& bone1 = entity->bones->at(connection.first);
			const auto& bone2 = entity->bones->at(connection.second);

			glm::vec2 p1{}, p2{};
			if (math::world_to_screen(bone1.position, &p1) &&
				math::world_to_screen(bone2.position, &p2))
			{
				render::add_line(p1, p2, color, 1.5f);
			}
		}
	}

	void head_circle(rust::Entity* entity)
	{
		bool& enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Head circle"));
		if (!enabled)
			return;

		if (!entity->bones || entity->bones->find(rust::BoneList::head) == entity->bones->end() ||
			entity->bones->find(rust::BoneList::neck) == entity->bones->end())
			return;

		const auto& head = entity->bones->at(rust::BoneList::head);
		const auto& neck = entity->bones->at(rust::BoneList::neck);

		glm::vec2 p_head{}, p_neck{};
		if (math::world_to_screen(head.position, &p_head) &&
			math::world_to_screen(neck.position, &p_neck))
		{
			const float radius = glm::distance(p_head, p_neck);
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Head circle"), xs("Color"));
			render::add_circle(p_head, radius, color, 1.5f);
		}
	}

	void view_line(rust::Entity* entity)
	{
		bool& enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("View line"));
		if (!enabled)
			return;

		if (!entity->bones || entity->bones->find(rust::BoneList::head) == entity->bones->end())
			return;

		const auto& head = entity->bones->at(rust::BoneList::head);

		glm::vec3 forward{};
		math::angle_vectors(entity->view_angles, &forward);

		glm::vec3 end_pos = head.position + (forward * 1.25f);

		glm::vec2 p_start{}, p_end{};
		if (math::world_to_screen(head.position, &p_start) &&
			math::world_to_screen(end_pos, &p_end))
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("View line"), xs("Color"));
			render::add_line(p_start, p_end, color, 1.5f);
		}
	}

	void bounding_box(rust::Entity* entity)
	{
		bool& box = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"));
		if (!box)
			return;

		const auto& bbox = entity->bounding_box;

		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Color"));
		bool& gradient = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Options"), xs("Gradient"));
		Color& gradient_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Options"), xs("Gradient"), xs("Color"));
		bool& fill = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Options"), xs("Fill"));
		Color& fill_color_top = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Options"), xs("Fill"), xs("Color top"));
		Color& fill_color_bottom = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Bounding box"), xs("Options"), xs("Fill"), xs("Color bottom"));

		render::add_rect(bbox.position - glm::vec2{ 1.0f }, bbox.size + glm::vec2{ 2.0f }, Color::black().scale_alpha(color.scalable_alpha()), 0.0f, 1.0f);
		render::add_rect(bbox.position + glm::vec2{ 1.0f }, bbox.size - glm::vec2{ 2.0f }, Color::black().scale_alpha(color.scalable_alpha()), 0.0f, 1.0f);

		if (fill)
		{
			render::add_rect_gradient(bbox.position + glm::vec2{ 2.0f }, bbox.size - glm::vec2{ 4.0f }, render::GradientType::Vertical, fill_color_top, fill_color_bottom);
		}

		if (gradient)
		{
			render::gradient_items(bbox.position, bbox.size, color, gradient_color, [&] {
				render::add_rect(bbox.position, bbox.size, color, 0.0f, 1.0f);

				}, 0.75f);
		}
		else {
			auto make_corner_box = [&](glm::vec2 pos, glm::vec2 size, Color col, f32 add_len = 0.0f) -> void
				{
					const f32 length = (size.x * 0.3f) + add_len;

					// top left
					render::add_rect_filled(pos, glm::vec2{ 1.0f }, Color::red());
					render::add_rect_filled(pos + glm::vec2{ 1.0f, 0.0f }, glm::vec2{ length, 1.0f }, col);
					render::add_rect_filled(pos + glm::vec2{ 0.0f, 1.0f }, glm::vec2{ 1.0f, length }, col);

					// top right
					render::add_rect_filled(pos + glm::vec2{ size.x - 1.0f, 0.0f }, glm::vec2{ 1.0f }, Color::red());
					render::add_rect_filled(pos + glm::vec2{ size.x - length - 1.0f, 0.0f }, glm::vec2{ length, 1.0f }, col);
					render::add_rect_filled(pos + glm::vec2{ size.x - 1.0f, 1.0f }, glm::vec2{ 1.0f, length }, col);

					// bottom left
					render::add_rect_filled(pos + glm::vec2{ 0.0f, size.y - 1.0f }, glm::vec2{ 1.0f }, Color::red());
					render::add_rect_filled(pos + glm::vec2{ 1.0f, size.y - 1.0f }, glm::vec2{ length, 1.0f }, col);
					render::add_rect_filled(pos + glm::vec2{ 0.0f, size.y - length - 1.0f }, glm::vec2{ 1.0f, length }, col);

					// bottom right
					render::add_rect_filled(pos + size - glm::vec2{ 1.0f }, glm::vec2{ 1.0f }, Color::red());
					render::add_rect_filled(pos + glm::vec2{ size.x - length - 1.0f, size.y - 1.0f }, glm::vec2{ length, 1.0f }, col);
					render::add_rect_filled(pos + glm::vec2{ size.x - 1.0f, size.y - length - 1.0f }, glm::vec2{ 1.0f, length }, col);
				};

			//make_corner_box(bbox.position - glm::vec2{ 1.0f }, bbox.size + glm::vec2{ 2.0f }, Color::black());
			//make_corner_box(bbox.position + glm::vec2{ 1.0f }, bbox.size - glm::vec2{ 2.0f }, Color::black());
			//make_corner_box(bbox.position, bbox.size, color);
			render::add_rect(bbox.position, bbox.size, color, 0.0f, 1.0f);
		}
	}

	void flags(rust::Entity* entity)
	{
		i32& flags = app::get_value<gui::MultiDropdown>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"));

		const bool flags_sleeper = flags & (1 << 1);
		const bool flags_wounded = flags & (1 << 2);
		const bool flags_team_id = flags & (1 << 3);
		const bool flags_ads = flags & (1 << 4);

		std::vector<std::pair<std::string, Color>> flag_opts{};

		if (flags_sleeper && entity->is_sleeping)
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"), xs("Options"), xs("Sleeping color"), xs("Color"));
			flag_opts.push_back(std::make_pair(xs("SLEEP"), color));
		}

		if (flags_wounded && entity->is_wounded)
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"), xs("Options"), xs("Wounded color"), xs("Color"));
			flag_opts.push_back(std::make_pair(xs("WOUNDED"), color));
		}

		if (flags_team_id)
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"), xs("Options"), xs("Team ID color"), xs("Color"));
			flag_opts.push_back(std::make_pair(entity->team == 0 ? xs("SOLO") : entity->team == game::impl::local_player->team ? xs("TEAMMATE") : std::vformat(xs("TEAM {}"), std::make_format_args(entity->team)), color));
		}

		if (flags_ads && entity->is_aiming)
		{
			Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Flags"), xs("Options"), xs("Aiming color"), xs("Color"));
			flag_opts.push_back(std::make_pair(xs("AIM"), color));
		}

		auto pos = entity->bounding_box.position;
		auto size = entity->bounding_box.size;

		float offset = 0.0f;
		for (auto [flag_text, flag_color] : flag_opts)
		{
			render::add_text(render::Fonts::Pixelmix10px, flag_text, pos + glm::vec2{ size.x + 3.0f, -3.0f + offset }, flag_color, render::TextFlagsDropShadow);
			offset += 8.0f;
		}
	}

	void snaplines(rust::Entity* entity)
	{
		bool& snaplines = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Snap lines"));
		if (!snaplines)
			return;

		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Snap lines"), xs("Color"));

		glm::vec2 position{ winapp::impl::window_size.x * 0.5f, winapp::impl::window_size.y };
		int& alignment = app::get_value<gui::Dropdown>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Snap lines"), xs("Options"), xs("Alignment"));
		switch (alignment)
		{
		case 0: // top
			position.y = 0.0f;
			break;
		case 1: // middle
			position.y *= 0.5f;
			break;
		}

		render::add_line(position, entity->bounding_box.position + glm::vec2{ entity->bounding_box.size.x * 0.5f, entity->bounding_box.size.y }, color);
	}

	void out_of_view(rust::Entity* entity)
	{
		bool& out_of_view = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Out of view arrows"));
		if (!out_of_view)
			return;

		Color& color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Out of view arrows"), xs("Color"));

		const auto rotate_triangle = [&](std::vector<glm::vec2>& points, f32 rotation) -> void
			{
				glm::vec2 points_center{};
				for (const auto& e : points)
					points_center += e;

				points_center.x /= points.size();
				points_center.y /= points.size();
				const glm::vec2 offset = points_center;

				const f32 theta = rotation * (std::numbers::pi_v<float> / 180.f);
				const f32 c = std::cosf(theta);
				const f32 s = std::sinf(theta);

				for (auto& point : points)
				{
					point -= offset;
					const f32 temp_x = point.x;
					point.x = temp_x * c - point.y * s;
					point.y = temp_x * s + point.y * c;
					point.x += offset.x;
					point.y += offset.y;
				}
			};

		const glm::vec2 center = winapp::impl::window_size * 0.5f;
		const f32 size = app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Out of view arrows"), xs("Options"), xs("Size"));

		const f32 radius = (winapp::impl::window_size.y * 0.45f) * app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Out of view arrows"), xs("Options"), xs("Radius"));

		const f32 target_yaw = math::calc_angle(game::impl::local_player->origin, entity->origin).y;
		f32 relative_yaw = target_yaw - game::impl::local_player->view_angles.y;
		while (relative_yaw > 180.0f) relative_yaw -= 360.0f;
		while (relative_yaw < -180.0f) relative_yaw += 360.0f;

		const f32 angle_yaw = relative_yaw - 90.0f;
		const f32 yaw = angle_yaw * (std::numbers::pi_v<float> / 180.f);

		const f32 x = center.x + radius * std::cos(yaw);
		const f32 y = center.y + radius * std::sin(yaw);

		std::vector<glm::vec2> points{ glm::vec2(x - size, y - size), glm::vec2(x + size, y), glm::vec2(x - size, y + size) };

		rotate_triangle(points, angle_yaw);
		std::vector<glm::vec2> rounded_triangle = math::subdivide_arc(points, 0.3f, 12);

		render::add_shadow_poly(rounded_triangle, color, 25.0f);
		render::add_polyline(rounded_triangle, color, 1.5f);
	}

	void player(rust::Entity* entity)
	{
		bool& enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Enabled"));
		if (!enabled)
			return;

		int& max_dist = app::get_value<gui::Slider<int>>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Enabled"), xs("Options"), xs("Max distance"));
		if (entity->distance > static_cast<float>(max_dist))
			return;

		bool& show_sleepers = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Show sleepers"));
		if (entity->is_sleeping && !show_sleepers)
			return;

		if (!entity->alpha_anim)
			return;

		std::int32_t start_vtx = render::draw_list->VtxBuffer.Size;

		if (!entity->bounding_box.valid)
		{
			out_of_view(entity);

			std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;
			render::modify_alpha(start_vtx, end_vtx, entity->alpha_anim.value);
			return;
		}

		name(entity);
		snaplines(entity);
		bounding_box(entity);
		skeleton(entity);
		head_circle(entity);
		view_line(entity);
		item_name(entity);
		flags(entity);

		out_of_view(entity);

		std::int32_t end_vtx = render::draw_list->VtxBuffer.Size;
		render::modify_alpha(start_vtx, end_vtx, entity->alpha_anim.value);
	}
}