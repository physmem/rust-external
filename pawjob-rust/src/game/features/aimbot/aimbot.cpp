#include "aimbot.hpp"
#include <app/winapp.hpp>
#include <game/cache/cache.hpp>
#include <game/game.hpp>
#include <memory/memory.hpp>
#include <render/render.hpp>
#include <sdk/math/math.hpp>
#include <sdk/offsets.hpp>
#include <sdk/rust/entity/entity.hpp>

#include <algorithm>
#include <app/app.hpp>
#include <controls/checkbox/checkbox.hpp>
#include <controls/multi_dropdown/multi_dropdown.hpp>
#include <controls/slider/slider.hpp>
#include <sdk/decryptions.hpp>
#include <sdk/rust/entity/weapon.hpp>
#include <sdk/unity/unity.hpp>
#include <print>
#include <string_encryption.hpp>

namespace features::aimbot
{
#define ADD_LOG(strfmt) debug_info.push_back(std::format("aimbot: L{} - {}", __LINE__, strfmt));
#define ADD_LOG_RET(strfmt) { debug_info.push_back(std::format("aimbot: L{} - {}", __LINE__, strfmt)); return; }

	static f32 visualized_fov = 100.0f;
	static glm::vec2 target_position_2d = {};

	constexpr const char* weapon_group_names[] = {
		"Rifles", "Snipers", "Shotguns", "Pistols", "Bows", "LMGs", "SMGs"
	};

	static std::string get_group_name(rust::WeaponTier tier)
	{
		switch (tier)
		{
		case rust::WeaponTier::Rifle: return "Rifles";
		case rust::WeaponTier::Sniper: return "Snipers";
		case rust::WeaponTier::Shotgun: return "Shotguns";
		case rust::WeaponTier::Pistol: return "Pistols";
		case rust::WeaponTier::Bow: return "Bows";
		case rust::WeaponTier::LMG: return "LMGs";
		case rust::WeaponTier::SMG: return "SMGs";
		default: return "";
		}
	}

	static ActiveConfig get_config_from_path(std::string_view p1)
	{
		std::string enabled_name = (p1 == xs("General") ? xs("Enabled") : xs("Override general"));

		auto get_bool = [&](std::string_view p2, std::string_view name) -> bool& {
			return app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Aimbot"), p1, p2, name);
			};

		auto get_float = [&](std::string_view p2, std::string_view name) -> float& {
			return app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Aimbot"), p1, p2, name);
			};

		auto get_int = [&](std::string_view p2, std::string_view name) -> int& {
			return app::get_value<gui::Slider<int>>(xs("Pawjob"), xs("Aimbot"), p1, p2, name);
			};

		auto get_int_nested = [&](std::string_view p2, std::string_view p3, std::string_view p4, std::string_view name) -> int& {
			return app::get_value<gui::Slider<int>>(xs("Pawjob"), xs("Aimbot"), p1, p2, p3, p4, name);
			};

		float hit_chance = (float)get_int_nested(xs("Main"), xs("Silent aim"), xs("Options"), xs("Hit chance"));
		float max_distance = (float)get_int_nested(xs("Main"), enabled_name, xs("Options"), xs("Max distance"));

		return {
			get_bool(xs("Main"), enabled_name),
			get_bool(xs("Main"), xs("Silent aim")),
			get_bool(xs("Prerequistes"), xs("Scale by distance")),
			get_int(xs("Prerequistes"), xs("Field of view")),
			get_float(xs("Humanization"), xs("Smoothing")),
			hit_chance,
			max_distance,
			get_bool(xs("Filters"), xs("Skip scientists")),
			get_bool(xs("Filters"), xs("Skip dwellers")),
			get_bool(xs("Filters"), xs("Skip sleepers")),
			get_bool(xs("Filters"), xs("Skip wounded")),
			get_bool(xs("Filters"), xs("Skip animals"))
		};
	}

	ActiveConfig get_active_config()
	{
		auto local = game::impl::local_player;
		if (!local)
		{
			ADD_LOG("no local player");
			return get_config_from_path(xs("General"));
		}

		if (local->item_shortname.empty())
		{
			ADD_LOG("using general (no item)");
			return get_config_from_path(xs("General"));
		}

		if (!rust::item_display_names)
			return get_config_from_path(xs("General"));

		auto it = rust::item_display_names->find(local->item_shortname);
		if (it != rust::item_display_names->end())
		{
			std::string group_name = get_group_name(it->second.tier);
			if (!group_name.empty())
			{
				bool group_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Aimbot"), group_name, xs("Main"), xs("Override general"));
				if (group_enabled)
				{
					ADD_LOG(std::format("using {}", group_name));
					return get_config_from_path(group_name);
				}
			}
		}

		ADD_LOG("using general (fb)");
		return get_config_from_path(xs("General"));
	}

	glm::vec3 get_bone_position(rust::Entity* entity, rust::BoneList bone_id)
	{
		// probably an animal or wtv
		if (!entity->bones || entity->bones->empty())
		{
			return entity->origin - glm::vec3{ 0.0f, 1.0f, 0.0f };
		}

		auto bone = entity->bones->find(bone_id);
		if (bone == entity->bones->end())
		{
			return entity->origin - glm::vec3{ 0.0f, 1.0f, 0.0f };
		}

		return bone->second.position;
	}

	bool valid_position(glm::vec3 input)
	{
		return input.x != 0.0f && input.y != 0.0f && input.z != 0.0f;
	}

	f32 get_field_of_view(glm::vec2 target_position)
	{
		const glm::vec2 center = winapp::impl::window_size * 0.5f;

		return glm::distance(target_position, center);
	}

	rust::Entity* get_best_target()
	{
		ActiveConfig config = get_active_config();
		rust::Entity* best_target{ nullptr };

		f32 best_field_of_view = config.field_of_view;
		i32 best_priority = 99;
		visualized_fov = config.field_of_view;

		auto is_animal = [](EntityType type) -> bool {
			return type == EntityType::Bear || type == EntityType::Wolf || type == EntityType::Stag ||
				type == EntityType::Boar || type == EntityType::Horse || type == EntityType::Chicken ||
				type == EntityType::Shark;
			};

		cache::for_each_entity<rust::Entity>([&](rust::Entity* entity) {
			if (!entity->base_address)
				return;

			if (entity->base_address == game::impl::local_player->base_address)
				return;

			if (entity->distance > config.max_distance && config.max_distance < 500.0f)
				return;

			i32 priority = 99;
			if (entity->type == EntityType::Player) {
				if (entity->is_sleeping && config.skip_sleepers) return;
				if (entity->is_wounded && config.skip_wounded) return;
				priority = 0;
			}
			else if (entity->type == EntityType::Scientist) {
				if (config.skip_scientists) return;
				priority = 1;
			}
			else if (entity->type == EntityType::Dweller) {
				if (config.skip_dwellers) return;
				priority = 1;
			}
			else if (is_animal(entity->type)) {
				if (config.skip_animals) return;
				priority = 2;
			}
			else {
				return;
			}

			glm::vec3 target_bone = get_bone_position(entity, rust::BoneList::head);
			if (!valid_position(target_bone))
				return;

			glm::vec2 out{};
			if (!math::world_to_screen(target_bone, &out))
				return;

			const f32 field_of_view = get_field_of_view(out);

			f32 target_fov = config.field_of_view;
			if (config.distance_fov_scaling && entity->distance > 20.0f)
			{
				target_fov = (config.field_of_view * 20.0f) / entity->distance;
				if (target_fov < 1.0f) target_fov = 1.0f;
			}

			if (field_of_view <= target_fov)
			{
				if (priority < best_priority)
				{
					best_priority = priority;
					best_target = entity;
					best_field_of_view = field_of_view;
					visualized_fov = target_fov;
				}
				else if (priority == best_priority && field_of_view <= best_field_of_view)
				{
					best_target = entity;
					best_field_of_view = field_of_view;
					visualized_fov = target_fov;
				}
			}
			});

		return best_target;
	}

	glm::vec2 calculate_angle(const glm::vec3& local_head_pos, const glm::vec3& target_position)
	{
		glm::vec3 direction = glm::vec3(local_head_pos.x - target_position.x, local_head_pos.y - target_position.y, local_head_pos.z - target_position.z);
		return glm::vec2(RAD2DEG(asin(direction.y / glm::length(direction))), RAD2DEG(-atan2(direction.x, -direction.z)));
	}

	// paste or be pasted i say
	glm::vec4 to_quat(glm::vec3 euler) {
		f32 c1 = std::cos(DEG2RAD(euler.x) / 2.0f);
		f32 s1 = std::sin(DEG2RAD(euler.x) / 2.0f);
		f32 c2 = std::cos(DEG2RAD(euler.y) / 2.0f);
		f32 s2 = std::sin(DEG2RAD(euler.y) / 2.0f);
		f32 c3 = std::cos(DEG2RAD(euler.z) / 2.0f);
		f32 s3 = std::sin(DEG2RAD(euler.z) / 2.0f);
		f32 c1c2 = c1 * c2;
		f32 s1s2 = s1 * s2;
		f32 c1s2 = c1 * s2;
		f32 s1c2 = s1 * c2;
		glm::vec4 quat = {
			s1c2 * c3 + c1s2 * s3,
			c1s2 * c3 - s1c2 * s3,
			c1c2 * s3 - s1s2 * c3,
			c1c2 * c3 + s1s2 * s3
		};
		return quat;
	}

	void on_tick()
	{
		std::unique_lock<std::shared_mutex> lock(debug_mut);
		debug_info.swap(safe_debug_info);
		debug_info.clear();

		if (!game::is_in_game())
			ADD_LOG_RET("awaiting map");

		if (!game::impl::local_player)
			ADD_LOG_RET("invalid local player");

		ActiveConfig config = get_active_config();
		if (!config.enabled)
			ADD_LOG_RET("config disabled");

		glm::vec3 local_head_position = get_bone_position(game::impl::local_player, rust::BoneList::head);
		if (!valid_position(local_head_position))
			ADD_LOG_RET(std::format("invalid local position [{}, {}, {}]", local_head_position.x, local_head_position.y, local_head_position.z));

		rust::Entity* target = get_best_target();
		if (!target)
		{
			target_position_2d = {};

			ADD_LOG("no target");
			return;
		}

		ADD_LOG(std::format("target: {} [{}m]", target->name, static_cast<i32>(target->distance)));

		glm::vec3 target_bone = get_bone_position(target, rust::BoneList::head);
		if (!valid_position(target_bone))
			ADD_LOG_RET("no target position");

		glm::vec2 out{};
		if (!math::world_to_screen(target_bone, &out))
			ADD_LOG_RET("can't see");

		target_position_2d = out;

		glm::vec2 target_angle = calculate_angle(local_head_position, target_bone);
		if (target_angle.x == 0.0f && target_angle.y == 0.0f)
			ADD_LOG_RET("invalid math operation");

		uptr input = memory::read(game::impl::local_player->base_address + offsets::BasePlayer::playerInput);
		if (!input)
			ADD_LOG_RET("no player input");

		glm::vec2 current_angles = memory::read<glm::vec2>(input + offsets::PlayerInput::bodyAngles);

		const f32 field_of_view = get_field_of_view(out);
		const f32 fov_delta = std::clamp(field_of_view / config.field_of_view, 0.0f, 1.0f);
		const f32 additional = (config.smoothing * 4.0f) * fov_delta;

		glm::vec2 delta = target_angle - current_angles;
		delta.x = std::clamp(delta.x, -89.0f, 89.0f);
		while (delta.y > 180.0f)  delta.y -= 360.0f;
		while (delta.y < -180.0f) delta.y += 360.0f;

		if (glm::length(delta) < 0.01f)
			ADD_LOG_RET("delta len < 0.0f");

		glm::vec2 smoothed_target_angle = current_angles + (delta / (config.smoothing + additional));

		if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {

			if (config.silent_aim)
			{
				uptr unk1 = memory::read(game::impl::local_player->base_address + 0x10);
				if (!unk1)
					ADD_LOG_RET("no class instance");

				uptr unity_class = memory::read(unk1 + 0x30);
				if (!unity_class)
					ADD_LOG_RET("no unity class");

				uptr player_eyes = unity::get_component_by_id(unity_class, 4);
				if (!player_eyes)
					ADD_LOG_RET("no player eyes");

				memory::write<glm::vec4>(player_eyes + offsets::PlayerEyes::bodyRotation, to_quat(glm::vec3{ target_angle.x, target_angle.y, 0.0f }));
				ADD_LOG("wrote angle [silent]");
			}
			else {
				memory::write<glm::vec2>(input + offsets::PlayerInput::bodyAngles, smoothed_target_angle);
				ADD_LOG("wrote angle [not silent]");
			}
		}

		ADD_LOG_RET("aimbot ret");
	}

	void on_render()
	{
		f32 offset{ 0.0f };
		for (auto& item : safe_debug_info)
		{
			glm::vec2 position = glm::vec2{ winapp::impl::window_size.x - 5.0f, 5.0f + offset };
			render::add_text(render::Fonts::Arial14px, item, position, Color::white(), render::TextFlagsDropShadow, glm::vec2{ 1.0f, 0.0f });

			offset += 14.0f;
		}

		ActiveConfig config = get_active_config();
		if (!config.enabled)
			return;

		bool fov_circle = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Aimbot"), xs("Visualization"), xs("Field of view circle"));
		if (!fov_circle)
			return;

		Color color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Aimbot"), xs("Visualization"), xs("Field of view circle"), xs("Color"));


		bool target_line = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Aimbot"), xs("Visualization"), xs("Field of view circle"), xs("Options"), xs("Target line"));
		Color target_line_color = app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Aimbot"), xs("Visualization"), xs("Field of view circle"), xs("Options"), xs("Target line"), xs("Color"));

		static gui::Animation anim{ visualized_fov };
		anim.update(visualized_fov);

		glm::vec2 center = winapp::impl::window_size * 0.5f;

		if (target_line && target_position_2d.x != 0.0f && target_position_2d.y != 0.0f)
		{
			render::add_line(center, target_position_2d, target_line_color);
		}

		render::add_circle(center, anim, color);
	}
}