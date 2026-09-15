#include "misc.hpp"
#include <app/app.hpp>
#include <controls/checkbox/checkbox.hpp>
#include <controls/dropdown/dropdown.hpp>
#include <controls/slider/slider.hpp>
#include <game/game.hpp>
#include <memory/memory.hpp>
#include <sdk/offsets.hpp>
#include <sdk/rust/entity/entity.hpp>
#include <sdk/unity/unity.hpp>
#include <string_encryption.hpp>

#include <print>
#include <sdk/decryptions.hpp>
#include <unordered_map>

namespace features::misc
{
	struct ColorState {
		f32 sun[4]{};
		f32 light[4]{};
		f32 ray[4]{};
		f32 sky[4]{};
		f32 cloud[4]{};
		f32 fog[4]{};
		f32 ambient[4]{};
	};

	static ColorState original_day{};
	static ColorState original_night{};
	static bool colors_captured = false;
	static uptr last_tod_sky = 0;

	struct ModulationState {
		bool enabled{};
		Color color{};

		bool operator!=(const ModulationState& other) const {
			return enabled != other.enabled || color.packed != other.color.packed;
		}
	};

	struct AllModulationState {
		ModulationState sun, light, ray, sky, cloud, fog, ambient;

		bool operator!=(const AllModulationState& other) const {
			return sun != other.sun || light != other.light || ray != other.ray ||
				sky != other.sky || cloud != other.cloud || fog != other.fog ||
				ambient != other.ambient;
		}
	};

	static AllModulationState last_day_applied{};
	static AllModulationState last_night_applied{};
	void handle_player_flags()
	{
		bool& tp_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Third person"));

		i32 original_flags = memory::read<i32>(game::impl::local_player->base_address + offsets::BasePlayer::playerFlags);
		i32 new_flags = original_flags;

		if (!(original_flags & PlayerFlags::Wounded) && !(original_flags & PlayerFlags::Sleeping))
		{
			if (tp_enabled)
				new_flags |= PlayerFlags::ThirdPersonViewmode;
			else
				new_flags &= ~PlayerFlags::ThirdPersonViewmode;
		}

		if (new_flags != original_flags)
		{
			memory::write<int>(game::impl::local_player->base_address + offsets::BasePlayer::playerFlags, new_flags);
		}
	}

	/* luckily weapon/arm chams dont need to be cached as they reset everytime your item/clothing is switched*/
	void apply_material_to_render(uptr render, u32 material_id)
	{
		for (i32 idx = 0; idx < 2; idx++) {
			uptr render_entry = memory::read(render + 0x20 + (idx * 0x8));
			if (!render_entry) continue;

			uptr unity_object = memory::read(render_entry + 0x10);
			if (!unity_object) continue;

			uptr material_list_base = memory::read(unity_object + 0x148);
			uptr material_list_size = memory::read(unity_object + 0x148 + 0x10);

			if (!material_list_base || material_list_size < 1 || material_list_size > 5) continue;

			for (u32 i = 0; i < material_list_size; i++) {
				//u32 val = memory::read<u32>(material_list_base + (i * 0x4));
				//std::println("{:#x}", val);
				memory::write<u32>(material_list_base + (i * 0x4), material_id);
			}
		}
	}

	void handle_local_chams()
	{
		if (!game::impl::local_player)
			return;

		static uptr last_active_item_ptr = 0;
		static std::string last_active_item_name = "";

		i32 weapon_chams = app::get_value<gui::Dropdown>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Weapon chams"));
		i32 arm_chams = app::get_value<gui::Dropdown>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Arm chams"));

		static i32 last_weapon_chams = -1;
		static i32 last_arm_chams = -1;

		const uptr active_item = game::impl::local_player->active_item;
		if (active_item == last_active_item_ptr &&
			game::impl::local_player->item_name == last_active_item_name &&
			weapon_chams == last_weapon_chams &&
			arm_chams == last_arm_chams)
			return;

		last_active_item_ptr = 0;
		last_active_item_name = "";
		last_weapon_chams = -1;
		last_arm_chams = -1;

		uptr base_projectile = memory::read(game::impl::local_player->active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		uptr viewmodel = memory::read(base_projectile + offsets::BaseProjectile::viewModel);
		if (!viewmodel)
			return;

		uptr viewmodel_instance = memory::read(viewmodel + 0x28);
		if (!viewmodel_instance)
			return;

		uptr base_viewmodel = memory::read(viewmodel_instance + 0x10);
		if (!base_viewmodel)
			return;

		memory::write<bool>(viewmodel_instance + 0x40, false);

		uptr viewmodel_object = memory::read(base_viewmodel + 0x30);
		if (!viewmodel_object)
			return;

		std::vector<uintptr_t> render_weapon;
		std::vector<uintptr_t> render_hand;
		unity::get_components_in_children(viewmodel_object, render_weapon, render_hand);

		if (weapon_chams != 0)
		{
			for (auto& render : render_weapon)
			{
				apply_material_to_render(render, g_materials[weapon_chams]);
			}
		}

		if (arm_chams != 0)
		{
			for (auto& render : render_hand)
			{
				apply_material_to_render(render, g_materials[arm_chams]);
			}
		}

		// only update these if the material was actually set
		last_active_item_ptr = active_item;
		last_active_item_name = game::impl::local_player->item_name;
		last_weapon_chams = weapon_chams;
		last_arm_chams = arm_chams;
	}

	void capture_color(uptr param_ptr, u32 offset, float* out)
	{
		uptr gradient = memory::read(param_ptr + offset);
		if (!gradient) return;
		uptr color_base = memory::read(gradient + 0x10);
		if (!color_base) return;
		memory::read_memory_raw(color_base, out, sizeof(float) * 4);
	}

	void apply_color(uptr param_ptr, u32 offset, const ModulationState& state, float* original)
	{
		uptr gradient = memory::read(param_ptr + offset);
		if (!gradient) return;
		uptr color_base = memory::read(gradient + 0x10);
		if (!color_base) return;

		if (state.enabled) {
			f32 color[4] = {
				static_cast<f32>(state.color.r) / 255.0f,
				static_cast<f32>(state.color.g) / 255.0f,
				static_cast<f32>(state.color.b) / 255.0f,
				1.0f
			};
			memory::write_memory_raw(color_base, color, sizeof(f32) * 4);
		}
		else {
			memory::write_memory_raw(color_base, original, sizeof(f32) * 4);
		}
	}

	void handle_tod_sky()
	{
		if (!game::impl::tod_sky) {
			colors_captured = false;
			return;
		}

		if (game::impl::tod_sky != last_tod_sky) {
			colors_captured = false;
			last_tod_sky = game::impl::tod_sky;
		}

		uptr day = memory::read(game::impl::tod_sky + offsets::TOD_Sky::day);
		uptr night = memory::read(game::impl::tod_sky + offsets::TOD_Sky::night);
		if (!day || !night) return;

		if (!colors_captured) {
			capture_color(day, 0x10, original_day.sun);
			capture_color(day, 0x18, original_day.light);
			capture_color(day, 0x20, original_day.ray);
			capture_color(day, 0x28, original_day.sky);
			capture_color(day, 0x30, original_day.cloud);
			capture_color(day, 0x38, original_day.fog);
			capture_color(day, 0x40, original_day.ambient);

			capture_color(night, 0x10, original_night.sun);
			capture_color(night, 0x18, original_night.light);
			capture_color(night, 0x20, original_night.ray);
			capture_color(night, 0x28, original_night.sky);
			capture_color(night, 0x30, original_night.cloud);
			capture_color(night, 0x38, original_night.fog);
			capture_color(night, 0x40, original_night.ambient);

			colors_captured = true;
		}

		bool time_changer = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("World"), xs("Time changer"));
		if (time_changer)
		{
			float time = app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("World"), xs("Time"));

			uptr cycle = memory::read(game::impl::tod_sky + offsets::TOD_Sky::cycle);
			if (cycle) {
				memory::write<float>(cycle + 0x10, time);
			}

			uptr ambient = memory::read(game::impl::tod_sky + offsets::TOD_Sky::ambient);
			if (ambient)
			{
				memory::write<i32>(ambient + 0x10, 0);
				memory::write<f32>(ambient + 0xa0, time);
			}
		}

		AllModulationState settings{};
		auto get_mod = [&](const std::string& name) -> ModulationState {
			return { app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("World"), name),
					 app::get_value<gui::ColorPicker>(xs("Pawjob"), xs("Visuals"), xs("World"), name, xs("Color")) };
			};

		settings.sun = get_mod(xs("Modulate sun"));
		settings.light = get_mod(xs("Modulate lights"));
		settings.ray = get_mod(xs("Modulate rays"));
		settings.sky = get_mod(xs("Modulate sky"));
		settings.cloud = get_mod(xs("Modulate clouds"));
		settings.fog = get_mod(xs("Modulate fog"));
		settings.ambient = get_mod(xs("Modulate ambience"));

		if (settings != last_day_applied) {
			apply_color(day, 0x10, settings.sun, original_day.sun);
			apply_color(day, 0x18, settings.light, original_day.light);
			apply_color(day, 0x20, settings.ray, original_day.ray);
			apply_color(day, 0x28, settings.sky, original_day.sky);
			apply_color(day, 0x30, settings.cloud, original_day.cloud);
			apply_color(day, 0x38, settings.fog, original_day.fog);
			apply_color(day, 0x40, settings.ambient, original_day.ambient);
			last_day_applied = settings;
		}

		if (settings != last_night_applied) {
			apply_color(night, 0x10, settings.sun, original_night.sun);
			apply_color(night, 0x18, settings.light, original_night.light);
			apply_color(night, 0x20, settings.ray, original_night.ray);
			apply_color(night, 0x28, settings.sky, original_night.sky);
			apply_color(night, 0x30, settings.cloud, original_night.cloud);
			apply_color(night, 0x38, settings.fog, original_night.fog);
			apply_color(night, 0x40, settings.ambient, original_night.ambient);
			last_night_applied = settings;
		}
	}

	void remove_water_drag()
	{
		bool remove_water_drag = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("Movement"), xs("Remove water drag"));
		if (!remove_water_drag)
			return;

		uptr water_body = memory::read(game::impl::local_player->base_address + offsets::BasePlayer::waterBody);
		if (!water_body)
			return;

		f32 submerge_amount = memory::read<f32>(water_body + 0x20);
		if (submerge_amount >= 0.95f)
			return;

		memory::write<f32>(water_body + 0x20, 0.0f);
	}

	void remove_flashbang_overlay()
	{
#if 0
		bool remove_flashbang_overlay = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Remove flashbang overlay"));
		if (!remove_flashbang_overlay)
			return;

		uptr flashbang_typeinfo = memory::read(game::impl::game_assembly + offsets::FlashBangOverlay::typeinfo);
		if (!flashbang_typeinfo)
			return;

		uptr instance = memory::read(flashbang_typeinfo + offsets::FlashBangOverlay::instance);
		if (!instance)
			return;

		memory::write<float>(instance + offsets::FlashBangOverlay::flashLength, 0.0f);
#endif
	}

	void spider_man()
	{
#if 0
		bool spider_man = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("Movement"), xs("Spider man"));
		if (!spider_man)
			return;

		uptr base_movement = memory::read(game::impl::local_player->base_address + offsets::BasePlayer::baseMovement);
		if (!base_movement)
			return;

		u32 encrypted_value = decryption::encrypt_base_movement_float_108(0.0f);
		memory::write<u32>(base_movement + 0x108, encrypted_value);
#endif
	}

	void field_of_view()
	{
		uptr convar_graphics = memory::read(game::impl::convar_graphics);
		if (!convar_graphics)
			return;

		uptr static_field = memory::read(convar_graphics + 0xb8);
		if (!static_field)
			return;

		f32 field_of_view = app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Field of view"));
		u32 encrypted_value = decryption::encrypt_convar_fov_encryption(field_of_view);
		memory::write<u32>(static_field + 0x430, encrypted_value); // 0x70
	}

	void handle_weapon_spread()
	{
		if (!game::impl::local_player->active_item)
			return;

		uptr active_item = game::impl::local_player->active_item;
		uptr base_projectile = memory::read(active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		struct SpreadCache {
			f32 aim_cone;
			f32 hip_aim_cone;
			f32 penalty_per_shot;
			f32 penalty_max;
			f32 penalty_recover_time;
			f32 penalty_recover_delay;
			f32 stance_penalty_scale;
			f32 burst_aim_cone_scale;
		};
		static std::unordered_map<uptr, SpreadCache>* spread_map{ nullptr };
		if (!spread_map)
			spread_map = new std::unordered_map<uptr, SpreadCache>;

		bool spread_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override weapon spread"));
		i32 wanted_spread_perc = app::get_value<gui::Slider<i32>>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override weapon spread"), xs("Options"), xs("Amount"));

		static bool last_state = false;
		static i32 last_value = 100;
		static uptr last_active_item = 0;

		if (spread_enabled == last_state && wanted_spread_perc == last_value && active_item == last_active_item)
			return;

		if (spread_enabled) {
			if (spread_map->find(active_item) == spread_map->end()) {
				(*spread_map)[active_item] = {
					memory::read<f32>(base_projectile + offsets::BaseProjectile::aimCone),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::hipAimCone),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyPerShot),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::aimConePenaltyMax),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverTime),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverDelay),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::stancePenaltyScale),
					memory::read<f32>(base_projectile + offsets::BaseProjectile::internalBurstAimConeScale)
				};
			}

			const auto& original = (*spread_map)[active_item];
			const f32 multiplier = static_cast<f32>(wanted_spread_perc) / 100.0f;
			memory::write<f32>(base_projectile + offsets::BaseProjectile::aimCone, original.aim_cone * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::hipAimCone, original.hip_aim_cone * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyPerShot, original.penalty_per_shot * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::aimConePenaltyMax, original.penalty_max * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverTime, original.penalty_recover_time * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverDelay, original.penalty_recover_delay * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::stancePenaltyScale, original.stance_penalty_scale * multiplier);
			memory::write<f32>(base_projectile + offsets::BaseProjectile::internalBurstAimConeScale, original.burst_aim_cone_scale * multiplier);
		}
		else {
			auto it = spread_map->find(active_item);
			if (it != spread_map->end()) {
				memory::write<f32>(base_projectile + offsets::BaseProjectile::aimCone, it->second.aim_cone);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::hipAimCone, it->second.hip_aim_cone);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyPerShot, it->second.penalty_per_shot);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::aimConePenaltyMax, it->second.penalty_max);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverTime, it->second.penalty_recover_time);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::aimconePenaltyRecoverDelay, it->second.penalty_recover_delay);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::stancePenaltyScale, it->second.stance_penalty_scale);
				memory::write<f32>(base_projectile + offsets::BaseProjectile::internalBurstAimConeScale, it->second.burst_aim_cone_scale);
				spread_map->erase(it);
			}
		}

		last_state = spread_enabled;
		last_value = wanted_spread_perc;
		last_active_item = active_item;
	}

	void handle_weapon_recoil()
	{
		struct RecoilCache {
			f32 yaw_min, yaw_max, pitch_min, pitch_max;
		};
		static std::unordered_map<uptr, RecoilCache>* recoil_map{ nullptr };
		if (!recoil_map)
			recoil_map = new std::unordered_map<uptr, RecoilCache>;

		bool recoil_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override weapon recoil"));
		i32 wanted_recoil_perc = app::get_value<gui::Slider<i32>>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override weapon recoil"), xs("Options"), xs("Amount"));

		static bool last_state = false;
		static i32 last_value = 100;
		static uptr last_active_item = 0;

		if (recoil_enabled == last_state && wanted_recoil_perc == last_value &&
			(!game::impl::local_player->active_item || game::impl::local_player->active_item == last_active_item))
			return;

		last_state = false;
		last_value = 0;
		last_active_item = 0;

		auto process_recoil = [&](uptr prop, f32 mult, bool enabled) {
			if (!prop) return;
			auto it = recoil_map->find(prop);
			if (enabled) {
				if (it == recoil_map->end()) {
					(*recoil_map)[prop] = {
						memory::read<f32>(prop + offsets::RecoilProperties::recoilYawMin),
						memory::read<f32>(prop + offsets::RecoilProperties::recoilYawMax),
						memory::read<f32>(prop + offsets::RecoilProperties::recoilPitchMin),
						memory::read<f32>(prop + offsets::RecoilProperties::recoilPitchMax)
					};
					it = recoil_map->find(prop);
				}
				const auto& original = it->second;
				memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMin, original.yaw_min * mult);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMax, original.yaw_max * mult);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMin, original.pitch_min * mult);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMax, original.pitch_max * mult);
			}
			else {
				if (it != recoil_map->end()) {
					memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMin, it->second.yaw_min);
					memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMax, it->second.yaw_max);
					memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMin, it->second.pitch_min);
					memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMax, it->second.pitch_max);
					recoil_map->erase(it);
				}
			}
			};

		if (!recoil_enabled) {
			for (auto it = recoil_map->begin(); it != recoil_map->end(); ) {
				uptr prop = it->first;
				memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMin, it->second.yaw_min);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilYawMax, it->second.yaw_max);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMin, it->second.pitch_min);
				memory::write<f32>(prop + offsets::RecoilProperties::recoilPitchMax, it->second.pitch_max);
				it = recoil_map->erase(it);
			}
			return;
		}

		if (!game::impl::local_player->active_item)
			return;

		uptr base_projectile = memory::read(game::impl::local_player->active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		uptr recoil_prop = memory::read(base_projectile + offsets::BaseProjectile::recoilProp);
		if (!recoil_prop)
			return;

		const f32 multiplier = static_cast<f32>(wanted_recoil_perc) / 100.0f;
		process_recoil(recoil_prop, multiplier, true);
		process_recoil(memory::read(recoil_prop + offsets::RecoilProperties::newRecoilOverride), multiplier, true);

		last_state = recoil_enabled;
		last_value = wanted_recoil_perc;
		last_active_item = game::impl::local_player->active_item;
	}

	void handle_instant_bow()
	{
		bool enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Instant bow"));
		if (!enabled)
			return;

		if (!game::impl::local_player->active_item)
			return;

		std::string_view shortname = game::impl::local_player->item_shortname;
		bool is_valid_bow = (shortname == xs("bow.hunting") || shortname == xs("bow.compound"));
		if (!is_valid_bow)
			return;

		uptr active_item = game::impl::local_player->active_item;
		uptr base_projectile = memory::read(active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		memory::write<bool>(base_projectile + offsets::BaseProjectile::isCharged, true);
	}

	void handle_instant_eoka()
	{
		bool enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Instant eoka"));
		if (!enabled)
			return;

		if (!game::impl::local_player->active_item)
			return;

		if (game::impl::local_player->item_shortname.contains(xs("eoka")) == std::string::npos)
			return;

		uptr active_item = game::impl::local_player->active_item;
		uptr base_projectile = memory::read(active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		memory::write<f32>(base_projectile + offsets::FlintStrikeWeapon::successFraction, 1.0f);
		if (!memory::read<bool>(base_projectile + offsets::FlintStrikeWeapon::didSparkThisFrame))
			memory::write<bool>(base_projectile + offsets::FlintStrikeWeapon::didSparkThisFrame, true);
	}

	void handle_automatic_weapons()
	{
		bool enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Automatic weapons"));
		if (!enabled)
			return;

		if (!game::impl::local_player->active_item)
			return;

		if (game::impl::local_player->item_shortname.find(xs("bow")) != std::string::npos)
			return;

		uptr active_item = game::impl::local_player->active_item;
		uptr base_projectile = memory::read(active_item + offsets::Item::heldEntity);
		if (!base_projectile)
			return;

		memory::write<bool>(base_projectile + offsets::BaseProjectile::automatic, true);
	}

	void handle_melee_range()
	{
		if (!game::impl::local_player->active_item)
			return;

		uptr active_item = game::impl::local_player->active_item;
		uptr base_melee = memory::read(active_item + offsets::Item::heldEntity);
		if (!base_melee)
			return;

		struct MeleeCache {
			f32 max_distance;
			f32 attack_radius;
		};
		static std::unordered_map<uptr, MeleeCache>* melee_map{ nullptr };
		if (!melee_map)
			melee_map = new std::unordered_map<uptr, MeleeCache>;

		bool melee_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override melee range"));
		i32 wanted_range_perc = app::get_value<gui::Slider<i32>>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Override melee range"), xs("Options"), xs("Amount"));

		static bool last_state = false;
		static i32 last_value = 100;
		static uptr last_active_item = 0;

		if (melee_enabled == last_state && wanted_range_perc == last_value && active_item == last_active_item)
			return;

		if (melee_enabled) {
			if (melee_map->find(active_item) == melee_map->end()) {
				(*melee_map)[active_item] = {
					memory::read<f32>(base_melee + offsets::BaseMelee::maxDistance),
					memory::read<f32>(base_melee + offsets::BaseMelee::attackRadius)
				};
			}

			const auto& original = (*melee_map)[active_item];
			const f32 multiplier = static_cast<f32>(wanted_range_perc) / 100.0f;

			if (original.max_distance > 0.0f && original.max_distance < 10.0f) {
				memory::write<f32>(base_melee + offsets::BaseMelee::maxDistance, original.max_distance * multiplier);
				memory::write<f32>(base_melee + offsets::BaseMelee::attackRadius, original.attack_radius * multiplier);
			}
		}
		else {
			auto it = melee_map->find(active_item);
			if (it != melee_map->end()) {
				memory::write<f32>(base_melee + offsets::BaseMelee::maxDistance, it->second.max_distance);
				memory::write<f32>(base_melee + offsets::BaseMelee::attackRadius, it->second.attack_radius);
				melee_map->erase(it);
			}
		}

		last_state = melee_enabled;
		last_value = wanted_range_perc;
		last_active_item = active_item;
	}

	void handle_thick_bullet()
	{
		bool enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Thick bullet"));
		if (!enabled)
			return;

		f32 value = app::get_value<gui::Slider<f32>>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Thick bullet"), xs("Options"), xs("Amount"));

		uptr list_component = memory::read<uptr>(game::impl::game_assembly + offsets::ListComponent_Projectile::ListComponent_C);
		if (!list_component) return;

		uptr static_fields = memory::read<uptr>(list_component + offsets::ListComponent_Projectile::static_fields);
		if (!static_fields) return;

		uptr list = memory::read<uptr>(static_fields + 0x20);
		if (!list) return;

		uptr buffer = memory::read<uptr>(list + offsets::ListComponent_Projectile::buffer);
		if (!buffer) return;

		i32 size = memory::read<i32>(buffer + 0x18);
		if (size <= 0) return;

		uptr base_list = memory::read(buffer + 0x10);

		for (i32 i = 0; i < size; i++)
		{
			uptr projectile = memory::read<uptr>(base_list + 0x20 + (i * 0x8));
			if (!projectile) continue;

			memory::write<f32>(projectile + offsets::Projectile::currentThickness, value);
		}
	}

	void on_tick()
	{
		if (!game::impl::local_player || !game::impl::local_player->base_address)
			return;

		handle_player_flags();
		handle_local_chams();
		handle_tod_sky();
		remove_water_drag();
		remove_flashbang_overlay();
		spider_man();
		field_of_view();
		handle_weapon_spread();
		handle_weapon_recoil();
		handle_instant_bow();
		handle_instant_eoka();
		handle_automatic_weapons();
		handle_melee_range();
		handle_thick_bullet();
	}
}