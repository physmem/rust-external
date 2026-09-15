#include "unity.hpp"
#include <memory/memory.hpp>

namespace unity
{
	uptr get_component_by_id(uptr game_object, i32 index)
	{
		if (!game_object)
			return 0;

		uptr list = memory::read(game_object + 0x30);
		uptr component = memory::read(list + (0x10 * index + 0x8));

		if (!component)
			return 0;

		uptr unk1 = memory::read(component + 0x28);
		if (unk1)
			return unk1;

		return 0;
	}

	void get_components_in_children_for_weapons(uptr game_object,
		std::vector<uptr>& gun_renderers) {

		uptr component_list = memory::read(game_object + 0x30);
		if (!component_list) return;

		i32 component_size = memory::read<i32>(game_object + 0x40);
		if (!component_size || component_size > 10000) return;

		std::vector<uptr> components(component_size);
		std::size_t components_buffer_size = component_size * 0x10;

		std::vector<uint8_t> component_data_block(component_size * 0x10);
		if (memory::read_memory_raw(component_list, component_data_block.data(), component_data_block.size())) {

			for (int idx{ 0 }; idx < component_size; ++idx) {
				uptr component = *reinterpret_cast<uptr*>(&component_data_block[idx * 0x10 + 0x8]);
				if (!component) continue;

				uptr component_ptr = memory::read<uptr>(component + 0x28);
				if (!component_ptr) continue;

				uptr component_name_ptr = memory::read<uptr>(component_ptr + 0x0);
				if (!component_name_ptr) continue;

				uptr component_name_addr = memory::read<uptr>(component_name_ptr + 0x10);
				if (!component_name_addr) continue;

				auto name = memory::read_string(component_name_addr);
				if (name.empty()) continue;

				if (name == "SkinnedMeshRenderer" || name == "MeshRenderer") {
					gun_renderers.push_back(component);
				}

				if (name == "Transform") {
					uptr child_list = memory::read<uptr>(component + 0x70);
					if (!child_list) continue;

					i32 child_size = memory::read<i32>(component + 0x80);
					if (!child_size || child_size > 1000) continue;

					std::vector<std::uintptr_t> children(child_size);
					size_t children_buffer_size = child_size * sizeof(std::uintptr_t);

					if (memory::read_memory_raw(child_list, children.data(), children_buffer_size)) {
						for (int i{ 0 }; i < child_size; ++i) {
							const auto child_transform = children[i];
							if (!child_transform) continue;

							const auto child_game_object = memory::read<std::uint64_t>(child_transform + 0x30);
							if (!child_game_object) continue;

							const auto child_object_name_ptr = memory::read<std::uint64_t>(child_game_object + 0x60);
							if (!child_object_name_ptr) continue;

							const auto child_name = memory::read_string(child_object_name_ptr);

							if (child_name.find("holosight") != std::string::npos) continue;

							get_components_in_children_for_weapons(child_game_object, gun_renderers);
						}
					}
				}
			}
		}
	}

	void get_components_in_children(uptr game_object,
		std::vector<uptr>& gun_renderers,
		std::vector<uptr>& hand_renderers,
		i32 depth, bool weapon) {
		const auto component_list = memory::read<std::uint64_t>(game_object + 0x30);
		if (!component_list) return;

		const auto component_size = memory::read<int>(game_object + 0x40);
		if (!component_size) return;

		for (int idx{ 0 }; idx < component_size; ++idx) {
			if (component_size > 10000) return;
			const auto component = memory::read<std::uint64_t>(component_list + (0x10 * idx + 0x8));
			if (!component) continue;

			const auto component_ptr = memory::read<std::uint64_t>(component + 0x28);
			if (!component_ptr) continue;

			const auto component_name_ptr = memory::read<std::uint64_t>(component_ptr + 0x0);
			if (!component_name_ptr) continue;

			const auto component_name = memory::read<std::uint64_t>(component_name_ptr + 0x10);
			if (!component_name) continue;

			auto name = memory::read_string(component_name);
			if (name.empty()) continue;

			if (name == "SkinnedMeshRenderer" || name == "MeshRenderer") {
				if (weapon) {
					gun_renderers.push_back(component);
				}
				else {
					hand_renderers.push_back(component);
				}
			}

			if (name == "Transform") {
				const auto child_list = memory::read<std::uint64_t>(component + 0x70);
				if (!child_list) continue;

				const auto child_size = memory::read<int>(component + 0x80);
				if (!child_size) continue;

				for (int i{ 0 }; i < child_size; ++i) {
					const auto child_transform = memory::read<std::uint64_t>(child_list + (0x8 * i));
					if (!child_transform) continue;

					const auto child_game_object = memory::read<std::uint64_t>(child_transform + 0x30);
					if (!child_game_object) continue;

					const auto child_object_name = memory::read<std::uint64_t>(child_game_object + 0x60);
					if (!child_object_name) continue;

					const auto child_name = memory::read_string(child_object_name);

					if (child_name.find("holosight") != std::string::npos) continue;

					bool is_weapon = child_name.rfind("v_", 0) == 0;

					if (is_weapon) {
						get_components_in_children_for_weapons(child_game_object, gun_renderers);
					}
					else {
						get_components_in_children(child_game_object, gun_renderers, hand_renderers, depth + 1, false);
					}
				}
			}
		}
	}
}