#include "cache.hpp"
#include <game/game.hpp>
#include <globals.hpp>
#include <memory/hypervisor/hypercall.hpp>
#include <memory/memory.hpp>
#include <sdk/rust/entity/entity.hpp>

#include <sdk/decryptions.hpp>
#include <sdk/offsets.hpp>
#include <sdk/unity/unity.hpp>

#include <algorithm>
#include <filesystem>
#include <print>
#include <sdk/math/math.hpp>
#include <thread>
#include <vector>

#include <app/app.hpp>
#include <app/winapp.hpp>
#include <controls/checkbox/checkbox.hpp>
#include <controls/dropdown/dropdown.hpp>
#include <curl/curl.h>
#include <sdk/rust/entity/weapon.hpp>
#include <string_encryption.hpp>

#include <condition_variable>
#include <queue>
#include <render/render.hpp>

namespace cache
{
	static uptr cached_buffer_list = 0;
	static i32 failed_reads = 0;

	struct avatar_data_t {
		std::vector<unsigned char> buffer;
	};

	class avatar_manager_t {
	public:
		void start() {
			std::thread([this] {
				m_default_avatar = download_image(xs("https://avatars.steamstatic.com/fef49e7fa7e1997310d705b2a6158ff8dc1cdfeb_medium.jpg"));
				if (m_default_avatar.empty()) {
				}

				while (!g_should_terminate) {
					uint64_t steam_id = 0;
					{
						std::unique_lock lock(m_queue_mutex);
						m_cv.wait(lock, [this] { return !m_queue.empty() || g_should_terminate; });
						if (g_should_terminate) break;
						steam_id = m_queue.front();
						m_queue.pop();
					}

					std::string url = fetch_url(steam_id);
					std::vector<unsigned char> buffer;

					if (!url.empty()) {
						buffer = download_image(url);
					}

					if (buffer.empty()) {
						if (!m_default_avatar.empty()) {
							buffer = m_default_avatar;
						}
						else {
						}
					}

					if (!buffer.empty()) {
						std::unique_lock lock(m_results_mutex);
						(*m_results)[steam_id] = std::move(buffer);
					}
				}
				}).detach();
		}

		void request(uint64_t steam_id) {
			std::unique_lock lock(m_queue_mutex);
			m_queue.push(steam_id);
			m_cv.notify_one();
		}

		std::vector<unsigned char> pop_result(uint64_t steam_id) {
			std::unique_lock lock(m_results_mutex);
			if (m_results->contains(steam_id)) {
				auto res = std::move((*m_results)[steam_id]);
				m_results->erase(steam_id);
				return res;
			}
			return {};
		}

	private:
		static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
			((std::string*)userp)->append((char*)contents, size * nmemb);
			return size * nmemb;
		}

		std::string fetch_url(uint64_t steam_id) {
			std::string url = xs("https://steamcommunity.com/profiles/") + std::to_string(steam_id) + xs("/?xml=1");
			std::string xml;

			CURL* curl = curl_easy_init();
			if (curl) {
				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xml);
				curl_easy_setopt(curl, CURLOPT_USERAGENT, xs("Mozilla/5.0"));
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

				CURLcode res = curl_easy_perform(curl);
				curl_easy_cleanup(curl);

				if (res != CURLE_OK) return "";
			}

			if (xml.empty()) return "";

			auto parse = [&](std::string tag) -> std::string {
				size_t start = xml.find("<" + tag + ">");
				size_t end = xml.find("</" + tag + ">");
				if (start == std::string::npos || end == std::string::npos) return "";

				std::string content = xml.substr(start + tag.length() + 2, end - (start + tag.length() + 2));

				if (content.find(xs("![CDATA[")) != std::string::npos) {
					size_t c_start = content.find(xs("![CDATA[")) + 8;
					size_t c_end = content.find(xs("]]"), c_start);
					if (c_start == std::string::npos || c_end == std::string::npos) return content;
					return content.substr(c_start, c_end - c_start);
				}
				return content;
				};

			std::string avatar = parse(xs("avatarMedium"));
			if (avatar.empty()) avatar = parse(xs("avatarFull"));

			return avatar;
		}

		std::vector<unsigned char> download_image(std::string url) {
			CURL* curl = curl_easy_init();
			if (!curl) return {};

			std::string buffer;
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, xs("Mozilla/5.0"));
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

			CURLcode res = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			if (res != CURLE_OK || buffer.empty()) return {};

			return std::vector<unsigned char>(buffer.begin(), buffer.end());
		}

		std::queue<uint64_t> m_queue;
		std::mutex m_queue_mutex;
		std::condition_variable m_cv;

		std::unordered_map<uint64_t, std::vector<unsigned char>>* m_results = new std::unordered_map<uint64_t, std::vector<unsigned char>>();
		std::mutex m_results_mutex;

		std::vector<unsigned char> m_default_avatar;
	};

	static avatar_manager_t g_avatar_manager;

	void set_material(rust::Entity* entity, u32 material_id, bool active)
	{
		uptr model = memory::read(entity->base_address + offsets::BasePlayer::playerModel);
		if (!model)
			return;

		uptr skinned_mesh = memory::read(model + offsets::PlayerModel::SkinnedMultiMesh);
		if (!skinned_mesh)
			return;

		uptr skinned_renderers_list = memory::read(skinned_mesh + offsets::SkinnedMultiMesh::rendererList);
		if (!skinned_renderers_list) return;

		uptr skinned_list = memory::read(skinned_renderers_list + 0x10);
		i32 materials_count = memory::read<i32>(skinned_renderers_list + 0x18);
		if (!skinned_list || materials_count <= 0 || materials_count > 100) return;

		for (i32 idx = 0; idx < materials_count; idx++) {
			uptr render_entry = memory::read(skinned_list + 0x20 + (idx * 0x8));
			if (!render_entry) continue;

			uptr unity_object = memory::read(render_entry + 0x10);
			if (!unity_object) continue;

			uptr material_list_base = memory::read(unity_object + 0x148);
			uptr material_list_size = memory::read(unity_object + 0x148 + 0x10);

			if (!material_list_base || material_list_size < 1 || material_list_size > 5 || !entity->original_materials) continue;

			auto& cached = (*entity->original_materials)[material_list_base];

			if (active) {
				if (cached.empty()) {
					for (u32 i = 0; i < material_list_size; i++) {
						cached.push_back(memory::read<u32>(material_list_base + (i * 0x4)));
					}
				}

				for (u32 i = 0; i < material_list_size; i++) {
					memory::write<u32>(material_list_base + (i * 0x4), material_id);
				}
			}
			else {
				if (!cached.empty()) {
					for (u32 i = 0; i < cached.size(); i++) {
						memory::write<u32>(material_list_base + (i * 0x4), cached[i]);
					}
				}
			}
		}
	}

	std::string get_item_name_short(uptr item)
	{
		uptr item_def = memory::read(item + offsets::Item::itemDefinition);
		if (!item_def)
			return "Empty [0]";

		uptr short_name_ptr = memory::read(item_def + offsets::ItemDefinition::shortName);
		if (!short_name_ptr)
			return "Empty [1]";

		i32 len = memory::read<i32>(short_name_ptr + 0x10);
		if (len <= 0 || len > 256)
			return "Empty [2]";

		std::wstring item_name = memory::read_wstring<2>(short_name_ptr + 0x14, len);
		return std::string{ item_name.begin(), item_name.end() };
	}

	std::string get_prefab_name(uptr entity)
	{
		uptr name_buffer = memory::chain_read<uptr>(entity, { 0x10, 0x30, 0x60 });
		return memory::read_string(name_buffer);
	}

	std::unordered_map<std::string, std::unique_ptr<texture_t>>* g_item_textures = {};

	bool create_instance()
	{
		curl_global_init(CURL_GLOBAL_ALL);

		entity_list = new std::unordered_map<std::uintptr_t, rust::Entity*>;
		g_item_textures = new std::unordered_map<std::string, std::unique_ptr<texture_t>>;

		g_avatar_manager.start();

		std::thread([&] {
			while (!g_should_terminate)
			{
				gather();
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			}).detach();

		return true;
	}

	void gather()
	{
		if (!game::is_in_game() || game::impl::game_assembly == 0 || g_should_refresh_cache)
		{
			cached_buffer_list = 0;
			failed_reads = 0;

			std::unique_lock lock(cache_mut);
			for (auto& pair : *entity_list)
				delete pair.second;

			entity_list->clear();
			game::impl::local_player = nullptr;
			g_should_refresh_cache = false;
			return;
		}
		//#define VALUE_DBG

#if defined(VALUE_DBG)
#define PRINT_VALUE(val) std::println("{} -> {:#x}", #val, val);
#else
#define PRINT_VALUE(val) void(0);
#endif

		if (!cached_buffer_list)
		{
			uptr base_networkable = memory::read(game::impl::game_assembly + offsets::base_networkable::typeinfo);
			PRINT_VALUE(base_networkable);
			if (!base_networkable) return;

			uptr static_fields = memory::read(base_networkable + offsets::base_networkable::static_fields);
			PRINT_VALUE(static_fields);
			if (!static_fields) return;

			uptr client_entities_ptr = memory::read(static_fields + offsets::base_networkable::client_entities);
			PRINT_VALUE(client_entities_ptr);
			if (!client_entities_ptr) return;

			uptr decrypted_client_entities = decryption::client_entities(client_entities_ptr);
			PRINT_VALUE(decrypted_client_entities);
			if (!decrypted_client_entities) return;

			uptr entity_list_ptr = memory::read(decrypted_client_entities + offsets::base_networkable::entity_list);
			PRINT_VALUE(entity_list_ptr);
			if (!entity_list_ptr) return;

			uptr decrypted_entity_list = decryption::entity_list(entity_list_ptr);
			PRINT_VALUE(decrypted_entity_list);
			if (!decrypted_entity_list) return;

			cached_buffer_list = memory::read(decrypted_entity_list + offsets::base_networkable::buffer);
			PRINT_VALUE(cached_buffer_list);
		}

		uptr buffer_list = cached_buffer_list;
		if (!buffer_list) return;

		uptr ent_list_start = memory::read(buffer_list + offsets::base_networkable::entListBase);
		i32 ent_list_count = memory::read<i32>(buffer_list + offsets::base_networkable::entLS);

		if (ent_list_count <= 0 || ent_list_count > 10000)
			return;

		std::vector<uptr> entity_ptrs(ent_list_count);
		size_t buffer_size = ent_list_count * sizeof(uptr);
		if (hypercall::read_guest_virtual_memory(entity_ptrs.data(), ent_list_start + 0x20, memory::impl::cr3, buffer_size) != buffer_size)
		{
			failed_reads++;
			if (failed_reads >= 3)
			{
				cached_buffer_list = 0;
				failed_reads = 0;
			}

			std::unique_lock lock(cache_mut);
			for (auto& pair : *entity_list)
				delete pair.second;

			entity_list->clear();
			return;
		}

		failed_reads = 0;

		bool debug_esp = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Debug ESP"));
		std::vector<uptr> found_this_tick = {};
		uptr local_player_ptr = entity_ptrs.empty() ? 0 : entity_ptrs[0];

		for (const auto& entity_ptr : entity_ptrs)
		{
			if (!entity_ptr) continue;

			EntityPrefabs prefab_id = memory::read<EntityPrefabs>(entity_ptr + 0x30);
			if (!prefabs)
				continue;

			auto it = prefabs->find(prefab_id);
			bool is_local = (entity_ptr == local_player_ptr);

			if (it == prefabs->end() && !is_local && !debug_esp)
			{
				std::string prefab_name = get_prefab_name(entity_ptr);
				size_t world_pos = prefab_name.find(" (world)");
				if (world_pos != std::string::npos)
				{
					std::string short_name = prefab_name.substr(0, world_pos);
					auto item_it = rust::item_display_names->find(short_name);
					if (item_it != rust::item_display_names->end())
					{
						found_this_tick.push_back(entity_ptr);
						std::unique_lock lock(cache_mut);
						if (entity_list->find(entity_ptr) == entity_list->end())
						{
							EntityType type = EntityType::DroppedMisc;
							switch (item_it->second.tier)
							{
							case rust::Rifle: type = EntityType::DroppedRifle; break;
							case rust::Sniper: type = EntityType::DroppedSniper; break;
							case rust::SMG: type = EntityType::DroppedSMG; break;
							case rust::Shotgun: type = EntityType::DroppedShotgun; break;
							case rust::Pistol: type = EntityType::DroppedPistol; break;
							case rust::LMG: type = EntityType::DroppedLMG; break;
							case rust::Launcher: type = EntityType::DroppedLauncher; break;
							case rust::Bow: type = EntityType::DroppedBow; break;
							case rust::Melee: type = EntityType::DroppedMelee; break;
							}

							rust::Entity* new_entity = new rust::Entity(entity_ptr, prefab_id, type);
							new_entity->name = item_it->second.display_name;
							new_entity->prefab_name = prefab_name;
							(*entity_list)[entity_ptr] = new_entity;
						}
						continue;
					}
				}
				continue;
			}

			found_this_tick.push_back(entity_ptr);

			std::unique_lock lock(cache_mut);
			if (entity_list->find(entity_ptr) == entity_list->end())
			{
				rust::Entity* new_entity = new rust::Entity(entity_ptr,
					is_local ? EntityPrefabs::player : prefab_id,
					is_local ? EntityType::Player : (it != prefabs->end() ? it->second.type : EntityType::None));

				if (new_entity->type != EntityType::Player)
					new_entity->name = (it != prefabs->end() ? std::string(it->second.name) : "Unknown");

				if (debug_esp)
					new_entity->prefab_name = get_prefab_name(entity_ptr);

				(*entity_list)[entity_ptr] = new_entity;

				if (is_local) {
					std::println("found local player: {:#x}", entity_ptr);
					game::impl::local_player = new_entity;
				}
			}
			else if (is_local && !game::impl::local_player) {
				game::impl::local_player = (*entity_list)[entity_ptr];
				std::println("re-assigned local player: {:#x}", entity_ptr);
			}
		}

		std::unique_lock lock(cache_mut);
		for (auto it = entity_list->begin(); it != entity_list->end();)
		{
			if (std::find(found_this_tick.begin(), found_this_tick.end(), it->first) == found_this_tick.end())
			{
				if (it->second == game::impl::local_player)
					game::impl::local_player = nullptr;

				delete it->second;
				it = entity_list->erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void update()
	{
		if (!game::is_in_game())
			return;

		if (!game::impl::local_player)
			return;

		std::unique_lock lock(cache_mut);
		bool& visuals_enabled = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Enabled"));
		i32& chams_material = app::get_value<gui::Dropdown>(xs("Pawjob"), xs("Visuals"), xs("Players"), xs("Chams"));

		bool chams_enabled = chams_material != 0 && visuals_enabled;
		bool debug_esp = app::get_value<gui::Checkbox>(xs("Pawjob"), xs("Misc"), xs("General"), xs("Debug ESP"));
		auto now = std::chrono::steady_clock::now();

		glm::vec2 window_size = winapp::impl::window_size;

		for (auto& [ptr, entity] : *entity_list)
		{
			if (!entity) continue;

			if (debug_esp && entity->prefab_name.empty()) {
				entity->prefab_name = get_prefab_name(ptr);
			}

			entity->alpha_anim.update(entity->is_alive() ? 1.0f : 0.0f);

			entity->origin = memory::chain_read<glm::vec3>(entity->base_address, { 0x10, 0x30, 0x30, 0x8, 0x38, 0x90 });
			entity->distance = game::impl::local_player ? glm::distance(game::impl::local_player->origin, entity->origin) : 0.0f;

			switch (entity->type)
			{
			case EntityType::Player:
			case EntityType::Scientist:
			case EntityType::Dweller:
				entity->cache_bones();
				break;
			}

			if (entity->prefab_id == EntityPrefabs::player)
			{

				if (entity->base_address != game::impl::local_player->base_address)
				{
					bool state_changed = (chams_enabled != entity->was_chams_enabled);
					bool should_refresh = (chams_enabled && std::chrono::duration_cast<std::chrono::milliseconds>(now - entity->last_material_update).count() > 500);

					if (state_changed || should_refresh)
					{
						set_material(entity, g_materials[chams_material], chams_enabled);
						entity->last_material_update = now;
						entity->was_chams_enabled = chams_enabled;
					}
				}

				entity->flags = memory::read<i32>(entity->base_address + offsets::BasePlayer::playerFlags);

				entity->is_sleeping = (entity->flags & PlayerFlags::Sleeping) != 0;
				entity->is_wounded = (entity->flags & PlayerFlags::Wounded) != 0;
				entity->is_spectating = (entity->flags & PlayerFlags::Spectating) != 0;
				entity->is_connected = (entity->flags & PlayerFlags::Connected) != 0;
				entity->is_incapacitated = (entity->flags & PlayerFlags::Incapacitated) != 0;
				entity->is_aiming = (entity->flags & PlayerFlags::Aiming) != 0;

				if (entity->name.empty())
				{
					uptr base_display_name = memory::read(ptr + offsets::BasePlayer::username);
					i32 len = memory::read<i32>(base_display_name + 0x10);
					std::wstring player_name = memory::read_wstring<2>(base_display_name + 0x14, len);
					entity->name = std::string(player_name.begin(), player_name.end());
				}

				if (entity->steam_id == 0)
				{
					//u32 start = 0;
					//while (start < 0x4000)
					//{
					//	// https://steamcommunity.com/profiles/76561199605669470/home
					//	uptr base_steamid = memory::read(entity->base_address + start);
					//	if (!base_steamid)
					//	{
					//		start += 1;
					//		continue;
					//	}

					//	i32 len = memory::read<i32>(base_steamid + 0x10);
					//	if (len <= 0 || len > 32) {
					//		start += 1;
					//		continue;
					//	}

					//	std::wstring wide_steam_id = memory::read_wstring<2>(base_steamid + 0x14, len);
					//	if (wide_steam_id.empty())
					//		continue;

					//	if (wide_steam_id.data() == nullptr)
					//		continue;

					//	auto steam_id = std::string{ wide_steam_id.begin(), wide_steam_id.end() };
					//	if (steam_id.contains("76561199605669470"))
					//	{
					//		std::println("found steam id offset {:#x}", start);
					//		break;
					//	}

					//	start += 1;
					//}

					uptr base_steamid = memory::read(entity->base_address + offsets::BasePlayer::userID);

					i32 len = memory::read<i32>(base_steamid + 0x10);
					if (len > 0)
					{
						std::wstring wide_steam_id = memory::read_wstring<2>(base_steamid + 0x14, len);
						try {
							entity->steam_id = std::stoull(std::string{ wide_steam_id.begin(), wide_steam_id.end() });
						}
						catch (...) {
							//std::println("failed to read steam id");
							entity->steam_id = 0;
						}
					}
				}

				if (entity->steam_id != 0 && entity->avatar_status == rust::Entity::AvatarStatus::None)
				{
					entity->avatar_status = rust::Entity::AvatarStatus::Pending;
					g_avatar_manager.request(entity->steam_id);
				}

				if (entity->avatar_status == rust::Entity::AvatarStatus::Pending)
				{
					auto buffer = g_avatar_manager.pop_result(entity->steam_id);
					if (!buffer.empty())
					{
						if (entity->avatar_texture.load_from_memory(buffer.data(), buffer.size())) {
							entity->avatar_status = rust::Entity::AvatarStatus::Ready;
						}
						else {
							entity->avatar_status = rust::Entity::AvatarStatus::Failed;
						}
					}
				}

				entity->team = memory::read<u64>(entity->base_address + offsets::BasePlayer::team);

				if (entity->type == EntityType::Player || entity->type == EntityType::Scientist)
					entity->cache_bones();

				if (entity->bones && !entity->bones->empty())
				{
					glm::vec2 min_point{ 10000.0f, 10000.0f };
					glm::vec2 max_point{ -10000.0f, -10000.0f };
					bool found_bone = false;

					for (const auto& [id, bone] : *entity->bones)
					{

						glm::vec2 screen_pos{};
						if (math::world_to_screen(bone.position, &screen_pos))
						{
							min_point.x = std::min(min_point.x, screen_pos.x);
							min_point.y = std::min(min_point.y, screen_pos.y);
							max_point.x = std::max(max_point.x, screen_pos.x);
							max_point.y = std::max(max_point.y, screen_pos.y);
							found_bone = true;
						}
					}

					if (found_bone)
					{
						float height = max_point.y - min_point.y;
						float padding = height * 0.15f;

						entity->bounding_box.position = min_point - glm::vec2{ padding, padding };
						entity->bounding_box.size = (max_point + glm::vec2{ padding, padding }) - entity->bounding_box.position;
						bool has_dimensions = entity->bounding_box.size.x > 0.0f &&
							entity->bounding_box.size.y > 0.0f;
						bool is_inside_viewport =
							(entity->bounding_box.position.x + entity->bounding_box.size.x > 0.0f) &&
							(entity->bounding_box.position.x < window_size.x) &&
							(entity->bounding_box.position.y + entity->bounding_box.size.y > 0.0f) &&
							(entity->bounding_box.position.y < window_size.y) &&
							entity->bounding_box.size.x < window_size.x &&
							entity->bounding_box.size.y < window_size.y;

						entity->bounding_box.valid = has_dimensions && is_inside_viewport;
					}
					else
					{
						entity->bounding_box.valid = false;
					}
				}

				entity->life_state = memory::read<u8>(entity->base_address + offsets::BaseCombatEntity::lifeState);
				entity->player_input = memory::read(entity->base_address + offsets::BasePlayer::playerInput);

				if (entity->base_address == game::impl::local_player->base_address)
				{
					entity->view_angles = entity->player_input ? memory::read<glm::vec3>(entity->player_input + offsets::PlayerInput::bodyAngles) : glm::vec3{};
				}
				else
				{
					uptr unk1 = memory::read(entity->base_address + 0x10);
					if (unk1)
					{
						uptr game_object = memory::read(unk1 + 0x30);
						if (game_object)
						{
							uptr eyes = unity::get_component_by_id(game_object, 4);
							if (eyes)
							{
								glm::vec4 rotation = memory::read<glm::vec4>(eyes + offsets::PlayerEyes::bodyRotation);
								entity->view_angles = math::quaternion_to_euler(rotation);
							}
						}
					}
				}
				entity->get_belt_items();
				entity->belt_items.clear();

				for (i32 i = 0; i < 6; i++) {
					uptr item = entity->held_items[i];
					if (!item) continue;

					std::string short_name = get_item_name_short(item);

					if (short_name.empty() || short_name == "Empty")
						continue;

					if (!g_item_textures->contains(short_name)) {
						auto tex = std::make_unique<texture_t>();
						std::string path = memory::get_game_directory() + "\\Bundles\\items\\" + short_name + ".png";
						if (tex->load_from_file(path)) {
							(*g_item_textures)[short_name] = std::move(tex);
						}
						else {
							(*g_item_textures)[short_name] = nullptr;
						}
					}

					texture_t* tex_ptr = nullptr;
					if (g_item_textures->contains(short_name) && (*g_item_textures)[short_name]) {
						tex_ptr = (*g_item_textures)[short_name].get();
					}

					i32 amount = memory::read<i32>(item + 0x20);
					if (amount <= 0)
						amount = memory::read<i32>(item + 0x70);

					if (amount > 10000)
						amount = 0;

					entity->belt_items.push_back({ short_name, tex_ptr, amount });
				}

				entity->active_item = entity->get_held_item();
				if (entity->active_item)
				{
					std::string short_name = get_item_name_short(entity->active_item);
					entity->item_shortname = short_name;
					//std::println("{}", short_name);

					if (rust::item_display_names)
					{
						auto it = rust::item_display_names->find(short_name);
						if (it != rust::item_display_names->end())
							entity->item_name = it->second.display_name;
						else
							entity->item_name = short_name;
					}
					else
						entity->item_name = short_name;

					if (g_item_textures->contains(short_name) && (*g_item_textures)[short_name]) {
						entity->item_texture = (*g_item_textures)[short_name].get();
					}
					else {
						entity->item_texture = nullptr;
					}
				}
				else {
					entity->item_name = "Empty";
					entity->item_shortname = "";
					entity->item_texture = nullptr;
				}
			}
		}
	}

	rust::Entity* get_local_player()
	{
		return game::impl::local_player;
	}
}
