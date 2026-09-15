#include "entity.hpp"
#include <immintrin.h>
#include <memory/hypervisor/hypercall.hpp>
#include <memory/memory.hpp>
#include <sdk/offsets.hpp>
#include <vector>

#include <print>

#include <sdk/decryptions.hpp>
#include <sdk/unity/unity.hpp>

namespace rust
{
	void Entity::cache_bones()
	{
		if (!base_address) return;

		const auto now = std::chrono::steady_clock::now();
		if (bones_initialized) {
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_bone_update).count() < 8)
				return;
		}

		last_bone_update = now;
		if (!bones) return;
		bones->clear();

		if (!bones_initialized)
		{
			cached_bones.clear();

			const uptr model = memory::read<uptr>(base_address + offsets::BaseEntity::model);
			if (!model) return;

			const uptr transforms = memory::read<uptr>(model + offsets::Model::boneTransforms);
			if (!transforms) return;

			const i32 bone_count = memory::read<i32>(transforms + 0x18);
			const uptr bone_array_ptr = transforms + 0x20;

			if (bone_count <= 0 || bone_count > 512) return;

			std::vector<uptr> entity_bone_ptrs(bone_count);
			const std::size_t array_size = bone_count * sizeof(uptr);
			if (hypercall::read_guest_virtual_memory(entity_bone_ptrs.data(), bone_array_ptr, memory::impl::cr3, array_size) != array_size) return;

			constexpr BoneList bones_to_cache[] = {
				BoneList::pelvis,
				BoneList::l_hip, BoneList::l_knee, BoneList::l_foot,
				BoneList::r_hip, BoneList::r_knee, BoneList::r_foot,
				BoneList::spine1, BoneList::spine2, BoneList::spine3, BoneList::spine4,
				BoneList::neck, BoneList::head,
				BoneList::l_clavicle, BoneList::l_upperarm, BoneList::l_forearm, BoneList::l_hand,
				BoneList::r_clavicle, BoneList::r_upperarm, BoneList::r_forearm, BoneList::r_hand
			};

			uptr shared_transform_data = 0;

			for (auto id : bones_to_cache)
			{
				if (static_cast<i32>(id) >= bone_count) continue;

				const uptr ent_bone = entity_bone_ptrs[static_cast<i32>(id)];
				if (!ent_bone) continue;

				const uptr transform_ptr = memory::read<uptr>(ent_bone + 0x10);
				if (!transform_ptr) continue;

				CachedBone cb{};
				cb.id = id;
				cb.transform_ptr = transform_ptr;

				uptr transform_data = memory::read<uptr>(transform_ptr + 0x38);
				cb.index = memory::read<i32>(transform_ptr + 0x40);

				if (transform_data)
				{
					shared_transform_data = transform_data;
					cached_bones.push_back(cb);
				}
			}

			if (cached_bones.empty() || !shared_transform_data) {
				return;
			}

			uptr index_base = memory::read<uptr>(shared_transform_data + 0x20);
			if (!index_base) {
				return;
			}

			constexpr i32 buffer_entries{ 4096 };
			i32 current_max_index = 0;

			for (auto& cb : cached_bones)
			{
				if (cb.index < 0 || cb.index >= buffer_entries) continue;
				if (cb.index > current_max_index) current_max_index = cb.index;

				i32 transform_index = memory::read<i32>(index_base + cb.index * sizeof(i32));
				int depth = 0;
				while (transform_index >= 0 && transform_index < buffer_entries && depth++ < 128)
				{
					cb.hierarchy.push_back(transform_index);
					if (transform_index > current_max_index) current_max_index = transform_index;

					transform_index = memory::read<i32>(index_base + transform_index * sizeof(i32));
				}
			}

			cached_matrix_base = memory::read<uptr>(shared_transform_data + 0x18);
			cb_max_index = current_max_index;
			bones_initialized = true;
		}

		if (!bones_initialized || !cached_matrix_base) return;

		constexpr i32 buffer_entries{ 4096 };
		if (!frame_matrix_cache) return;
		frame_matrix_cache->clear();

		auto get_matrix = [&](i32 index) -> BoneMatrix3x4* {
			if (index < 0 || index >= buffer_entries) return nullptr;

			auto it = frame_matrix_cache->find(index);
			if (it != frame_matrix_cache->end())
				return &it->second;

			BoneMatrix3x4 matrix = memory::read<BoneMatrix3x4>(cached_matrix_base + index * sizeof(BoneMatrix3x4));
			return &frame_matrix_cache->emplace(index, matrix).first->second;
			};

		const __m128 mul_vec_0 = _mm_setr_ps(-2.f, 2.f, -2.f, 0.f);
		const __m128 mul_vec_1 = _mm_setr_ps(2.f, -2.f, -2.f, 0.f);
		const __m128 mul_vec_2 = _mm_setr_ps(-2.f, -2.f, 2.f, 0.f);

		for (const auto& cb : cached_bones)
		{
			BoneMatrix3x4* base_matrix = get_matrix(cb.index);
			if (!base_matrix) continue;

			__m128 result = _mm_loadu_ps(base_matrix->vec0);

			for (i32 transform_index : cb.hierarchy)
			{
				BoneMatrix3x4* matrix = get_matrix(transform_index);
				if (!matrix) continue;

				__m128 vec0 = _mm_loadu_ps(matrix->vec0);
				__m128 vec1 = _mm_loadu_ps(matrix->vec1);
				__m128 vec2 = _mm_loadu_ps(matrix->vec2);

				__m128 xxxx = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0x00));
				__m128 yyyy = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0x55));
				__m128 zwxy = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0x8E));
				__m128 wzyw = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0xDB));
				__m128 zzzz = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0xAA));
				__m128 yxwy = _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec1), 0x71));
				__m128 tmp7 = _mm_mul_ps(vec2, result);

				result = _mm_add_ps(
					_mm_add_ps(
						_mm_add_ps(
							_mm_mul_ps(
								_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(xxxx, mul_vec_1), zwxy), _mm_mul_ps(_mm_mul_ps(yyyy, mul_vec_2), wzyw)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0xAA))),
							_mm_mul_ps(
								_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(zzzz, mul_vec_2), wzyw), _mm_mul_ps(_mm_mul_ps(xxxx, mul_vec_0), yxwy)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x55)))),
						_mm_add_ps(
							_mm_mul_ps(
								_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(yyyy, mul_vec_0), yxwy), _mm_mul_ps(_mm_mul_ps(zzzz, mul_vec_1), zwxy)),
								_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(tmp7), 0x00))),
							tmp7)),
					vec0);
			}

			float pos[4]{};
			_mm_storeu_ps(pos, result);
			(*bones)[cb.id] = { {pos[0], pos[1], pos[2]}, true };
		}
	}

	uptr Entity::get_container_contents(uptr container_offset)
	{
		uptr object = memory::read(base_address + 0x10);
		if (!object) return 0;

		uptr unity_class = memory::read(object + 0x30);
		if (!unity_class) return 0;

		uptr inventory = unity::get_component_by_id(unity_class, 3);
		if (!inventory) return 0;

		uptr container = memory::read(inventory + container_offset);
		if (!container) return 0;

		uptr item_list = memory::read(container + offsets::ItemContainer::list);
		if (!item_list) return 0;

		return memory::read(item_list + 0x10);
	}

	void Entity::get_belt_items()
	{
		constexpr i32 belt_slots = 6;
		held_items_count = 0;

		uptr contents = get_container_contents(offsets::PlayerInventory::container2);
		if (!contents)
			return;

		uptr items[belt_slots]{};
		memory::read_memory_raw(contents + 0x20, items, belt_slots * sizeof(uptr));

		for (i32 i = 0; i < belt_slots; i++) {
			held_items[i] = items[i];
			if (items[i]) held_items_count++;
		}
	}

	uptr Entity::get_held_item()
	{
		const uint64_t encrypted_active = memory::read(base_address + offsets::BasePlayer::clActiveItem);
		const uint32_t active_id = static_cast<uint32_t>(decryption::cl_active_item(encrypted_active));

		for (const auto& item : held_items)
		{
			if (!item)
				continue;

			u32 def_index = memory::read<u32>(item + offsets::Item::itemUid);
			if (def_index == active_id)
				return item;
		}

		return 0;
	}
}
