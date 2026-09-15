#include "../memory/memory.hpp"
#include "game.hpp"

#include "cache/cache.hpp"
#include <memory/hypervisor/hook/hook.hpp>
#include <memory/hypervisor/hypercall.hpp>
#include <memory/hypervisor/system/system.hpp>
#include <print>
#include <sdk/offsets.hpp>

#include "features/aimbot/aimbot.hpp"
#include <fstream>
#include <iostream>
#include <sdk/decryptions.hpp>
#include <sdk/encryption_dumper.hpp>
#include <sdk/rust/entity/entity.hpp>
#include <sdk/rust/entity/weapon.hpp>
#include <sdk/unity/unity.hpp>


#include <app/app.hpp>
#include <controls/slider/slider.hpp>
#include <globals.hpp>
#include <string_encryption.hpp>

/*
__int64 __fastcall sub_181A69F60(__int64 a1, __int64 a2)
{
  __int64 v2; // rdx
  double v3; // xmm0_8
  __int64 v4; // rdx
  unsigned int v5; // xmm1_4
  int *v6; // rdx
  int v7; // r8d
  int v8; // eax
  __int64 v9; // rax
  __int64 v10; // rcx
  __int64 result; // rax
  signed __int32 v12[8]; // [rsp+0h] [rbp-48h] BYREF
  __int64 v13; // [rsp+20h] [rbp-28h] BYREF

  v3 = sub_186CCA360(a2, a2, 0LL);
  if ( !byte_18E7BF9AF )
  {
	sub_1806F4FF0(&qword_18E3D5A78, v2);
	_InterlockedOr(v12, 0);
	sub_1806F4FF0(&ConVar_Graphics, v4);
	_InterlockedOr(v12, 0);
	byte_18E7BF9AF = 1;
  }
  v5 = 1116471296;
  if ( *(float *)&v3 >= 70.0 )
  {
	v5 = 1119092736;
	if ( *(float *)&v3 <= 90.0 )
	  v5 = LODWORD(v3);
  }
  v6 = (int *)&v13;
  v13 = v5;
  v7 = 1;
  do
  {
	v8 = *v6++;
	*(v6 - 1) = (((v8 ^ 0x731A6B75) << 24) | ((v8 ^ 0x731A6B75u) >> 8)) - 1340028230;
	--v7;
  }
  while ( v7 );
  v9 = ConVar_Graphics;
  if ( !*(_DWORD *)(ConVar_Graphics + 224) )
  {
	sub_180759FD0(ConVar_Graphics);
	v9 = ConVar_Graphics;
  }
  v10 = *(_QWORD *)(v9 + 184);
  result = v13;
  *(_QWORD *)(v10 + 112) = v13;
  return result;
}
*/


namespace game
{

	inline constexpr std::uint64_t MAGIC_HEADER = 0xDEADBEEFDEADBEEF;

	void dump_materials()
	{
		std::ofstream out_file("materials_out.txt");

		std::uintptr_t current_addr = 0x25000000000;
		const std::uintptr_t max_addr = 0x26000000000;

		constexpr std::size_t page_size = 4096;
		std::vector<std::uint8_t> buffer(page_size);

		while (current_addr < max_addr)
		{
			/*if (current_addr % (0x10000000) == 0) {
				std::println("[*] Current: {:#x}...", current_addr);
			}*/

			memory::read_memory_raw(current_addr, buffer.data(), page_size);

			for (std::size_t i = 0; i <= page_size - 8; ++i)
			{
				if (*reinterpret_cast<const std::uint64_t*>(&buffer[i]) == MAGIC_HEADER)
				{
					const std::uintptr_t magic_addr = current_addr + i;
					const std::uintptr_t mat_base = magic_addr - 0x88;

					const auto mat_id = memory::read<std::uint32_t>(mat_base + 0x8);
					const auto name_ptr = memory::read<std::uintptr_t>(mat_base + 0x30);

					if (name_ptr > 0x1000000 && name_ptr < 0x7FFFFFFFFFFF)
					{
						char _name[255];
						memory::read_memory_raw(name_ptr, _name, 255);
						_name[254] = '\0';
						std::string mat_name(_name);

						if (!mat_name.empty() && mat_name.length() > 1)
						{
							//std::println("found: {:<30} | ID: {}", mat_name, mat_id);
							out_file << mat_name << ": " << mat_id << "\n";
							out_file.flush();
						}
					}
				}
			}

			current_addr += page_size;
		}

		std::println("finished dump");
		out_file.close();
	}

	void create_instance()
	{
		if (impl::created_instance)
			return;

		{
			auto [base_address_easy, s_easy] = memory::get_module_easy(L"GameAssembly.dll");
			impl::game_assembly = base_address_easy;

			// if eac or wtv is blocking enum modules then we go balls deep.
			if (impl::game_assembly == 0)
			{
				auto [base_address, s] = memory::get_module(L"GameAssembly.dll");
				impl::game_assembly = base_address;
			}

			std::println("GameAssembly.dll - {:#x}", impl::game_assembly);
		}

		const std::vector<std::uintptr_t> required_offsets = {
		   impl::game_assembly,
		};

		impl::created_instance = std::all_of(required_offsets.begin(), required_offsets.end(), [](std::uintptr_t ptr) {
			return ptr != 0;
			});

		// dump_materials();
		//encryption_dumper::dump_all(impl::game_assembly);

		{
			//uptr sig_result = memory::pattern_scan(impl::game_assembly,
			//	"48 8B 05 ? ? ? ? 83 B8 ? ? ? ? ? 75 ? 48 8B C8 E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 8B 80 ? ? ? ? 8B 98 ? ? ? ? 83 FB");
			//
			//if (sig_result)
			//{
			//	impl::convar_graphics = memory::resolve_lea(sig_result);
			//	std::println("ConVar_Graphics: {:#x} (rva: {:#x})", impl::convar_graphics, impl::convar_graphics - impl::game_assembly);
			//}
			//else
			//{
			//	//std::println("ConVar_Graphics signature not found");
			//}
			impl::convar_graphics = impl::game_assembly + 0xE601938;
		}

		cache::create_instance();
	}

	inline std::vector<std::string> dump_component_names(uptr game_object)
	{
		std::vector<std::string> names;

		if (!game_object)
			return names;

		uptr list = memory::read(game_object + 0x30);

		// theres only ever like 17 components to an entity
		for (i32 i = 0; i < 20; i++)
		{
			uptr component = memory::read(list + (0x10 * i + 0x8));
			if (!component)
				continue;

			uptr unk1 = memory::read(component + 0x28);
			if (!unk1)
				continue;

			uptr name_ptr = memory::read(unk1);
			std::string name = memory::read_string(memory::read(name_ptr + 0x10));

			if (!name.empty())
			{
				names.push_back(name);
				std::println("[{:#x}] component: {} -> {}", game_object, i, name);
			}
		}

		return names;
	}

	inline std::string get_prefab_name(uptr entity)
	{
		uptr name_buffer = memory::chain_read<uptr>(entity, { 0x10, 0x30, 0x60 });
		if (!name_buffer)
			return "invalid";

		return memory::read_string(name_buffer);
	}

	void update_instance()
	{
		create_instance();
		if (!impl::created_instance)
			return;

		impl::tod_sky = memory::chain_read<uptr>(game::impl::game_assembly, { offsets::TOD_Sky::TOD_Sky_C, 0xb8, 0x20, 0x10, 0x20 });

		//uptr lol = memory::read(game::impl::game_assembly + offsets::TOD_Sky::TOD_Sky_C);
		//std::println("{:#x}", lol);

		static i32 invalid_count = 0;
		if (impl::local_player && impl::local_player->base_address && !g_should_refresh_cache)
		{
			auto origin = impl::local_player->origin;
			if ((origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f) || impl::local_player->bones->empty()) {
				invalid_count++;
			}
		}

		if (invalid_count > 10)
		{
			g_should_refresh_cache = true;
			std::println("refreshing cache from game instance");
			invalid_count = 0;
		}

		if (g_should_refresh_cache)
			return;

		uptr typeinfo = memory::read(impl::game_assembly + offsets::main_camera::typeinfo);
		if (!typeinfo) return;

		uptr static_fields = memory::read(typeinfo + offsets::main_camera::static_fields);
		if (!static_fields) return;

		uptr instance = memory::read(static_fields + offsets::main_camera::instance);
		if (!instance) return;

		uptr camera_obj = memory::read(instance + offsets::main_camera::buffer);
		if (!camera_obj) return;

		impl::view_matrix = memory::read<glm::mat4>(camera_obj + offsets::main_camera::view_matrix);

		features::aimbot::on_tick();

		if (impl::local_player && impl::local_player->base_address)
		{
			//uptr list_comp = memory::read(impl::game_assembly + offsets::ListComponent_Projectile::ListComponent_C);
			//if (list_comp)
			//{
			//
			//	//std::println("{:#x}", list_comp);
			//}

			//uptr player_model = memory::read(impl::local_player->base_address + offsets::BasePlayer::playerModel);
			//if (player_model)
			//{
			//	f32 skin_color = memory::read<f32>(player_model + 0x194);
			//	std::println("{}", skin_color);
			//	f32 lol = app::get_value<gui::Slider<float>>(xs("Pawjob"), xs("Visuals"), xs("Local"), xs("Skin color intensity"));
			//	memory::write<f32>(player_model + 0x194, lol);
			//}

			/*uptr seated_entity = memory::read(impl::local_player->base_address + 0x540);
			if (seated_entity)
			{
				uptr parent_mounted_entity = memory::read(seated_entity + 0x78);
				if (parent_mounted_entity)
				{
					std::string prefab = get_prefab_name(parent_mounted_entity);
					std::println("{:#x} {}", parent_mounted_entity, prefab);
				}
			}*/
		}

		//if (impl::local_player) {
		//	static i32 flags = 0;
		//	uptr model_state = memory::read(impl::local_player->base_address + 0x3a0);
		//	if (model_state)
		//	{
		//		flags = memory::read<i32>(model_state + 0x68);
		//		//std::println("{}", flags);
		//		if (flags != 4)
		//			memory::write<i32>(model_state + 0x68, 4);
		//	}

		//	uptr base_movement = memory::read(impl::local_player->base_address + offsets::BasePlayer::baseMovement);
		//	if (base_movement)
		//	{
		//		if (flags == 0)
		//		{
		//			memory::write<f32>(base_movement + 0x54, 9999.0f);
		//			memory::write<f32>(base_movement + 0x100, 1.0f);

		//			u32 f108 = decryption::encrypt_base_movement_float_108(1.0f);
		//			memory::write<u32>(base_movement + 0x108, f108);
		//			flags = 4;
		//		}
		//	}
		//}

		/*if (game::impl::local_player && game::impl::local_player->base_address)
		{

		// playerMovement
		//
		// 0x3C - VelocityX
		// 0x44 - VelocityY
		// 0x54 - VelocityZ? (used for shoot in air?)



		// held item
		//
		// current clip @ 0x20
		// current quantity @ 0x70
		// recoil @ 0x380



		uptr player_movement = memory::read(game::impl::local_player->base_address + offsets::BasePlayer::baseMovement);
		if (player_movement)
		{
			// chain TOD_SKY, 0xb8, 0x88, 0x10, 0x20

			uptr tod_sky_final = memory::chain_read<uptr>(impl::game_assembly, { offsets::TOD_Sky::TOD_Sky_C, 0xb8, 0x88, 0x10, 0x20 });

			struct unity_color
			{
				float r{};
				float g{};
				float b{};
				float a{};
			};

			if (tod_sky_final)
			{
				// fog color = tod_sky + 0x15c
				// sky color = tod_sky + 0x184
				// (also?) sky color = tod_sky + 0x1d4

				//memory::write<unity_color>(tod_sky_final + 0x15c, unity_color{ 1.0f, 0.0f, 1.0f, 1.0f });
				//memory::write<unity_color>(tod_sky_final + 0x184, unity_color{ 1.0f, 0.0f, 1.0f, 1.0f });
				//memory::write<unity_color>(tod_sky_final + 0x1d4, unity_color{ 1.0f, 0.0f, 1.0f, 1.0f });
				//memory::write<uptr>(tod_sky_final + 0x10, 1);

				//memory::write<unity_color>(tod_sky_final + 0x1d4, unity_color{ 1.0f, 0.0f, 1.0f, 1.0f });

				//std::println("{:#x}", tod_sky_copy);

				//std::println("------------------------------");
				/*uptr start = 0xe4;
				i32 i = app::get_value<gui::Slider<int>>("Pawjob", "Visuals", "Local", "Test");
				unity_color color = memory::read<unity_color>(tod_sky_final + start + ((sizeof(unity_color) + 0x4) * i));
				std::println("current index: {} - {:#x}", i, start + ((sizeof(unity_color) + 0x4) * i));
				memory::write<unity_color>(tod_sky_final + start + ((sizeof(unity_color) + 0x4) * i), unity_color{ 1.0f, 0.0f, 1.0f, color.a });

				//memory::write<float>(tod_sky_final + 0x18, -std::numeric_limits<float>::max());
				//memory::write<float>(tod_sky_final + 0x1c, -std::numeric_limits<float>::max());
				//for (int i = 0; i < 14; i++)
				//{

				//	unity_color color = memory::read<unity_color>(tod_sky_final + start + ((sizeof(unity_color) + 0x4) * i));
				//	//memory::write<unity_color>(tod_sky_final + start + ((sizeof(unity_color) + 0x4) * i), unity_color{ 1.0f, 0.0f, 1.0f, color.a });
				//	//std::println("({:#x}) - Color: {} - R: {} G: {} B: {} A: {}", start + (sizeof(unity_color) * i), i, color.r, color.g, color.b, color.a);
				//}
				//std::println("------------------------------");



		uptr ambient = memory::read(tod_sky_final + offsets::TOD_Sky::ambient);
		if (ambient)
		{
			// UpdateInterval
			//memory::write<float>(ambient + 0x18, std::numeric_limits<float>::max());
			//memory::write<float>(ambient + 0xa0, 18.0f);
		}

		{
			// rayleigh multiplier (0x10)
			// mie multiplier (0x14)
			// brightness (0x18)
			// contrast (0x1c)
			// night brightness (0x20
			// night contrast (0x24)
		}

	}*/
	}

	bool is_in_game()
	{
		return impl::tod_sky != 0 && !g_should_refresh_cache;
	}
}