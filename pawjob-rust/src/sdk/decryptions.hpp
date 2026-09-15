#pragma once
#include "globals.hpp"
#include "rust/gchandle.hpp"

namespace decryption
{
	inline uptr client_entities(uptr a1) {

		uint64_t value = memory::read(a1 + 0x18);
		uint32_t* data = (uint32_t*)&value;
		uint32_t count = 2;

		do
		{
			uint32_t x = *data;
			uint32_t y = ((x << 16) | (x >> 16)) - 750462601;
			uint32_t result = (y << 12) | (y >> 20);
			*data = result;
			data++;
			--count;
		} while (count);

		return gchandle::get_target(value);
	}


	inline uptr entity_list(uptr a1) {

		uint64_t value = memory::read(a1 + 0x18);
		uint32_t* data = (uint32_t*)&value;
		uint32_t count = 2;

		do
		{
			uint32_t x = *data;
			uint32_t temp = ((x << 23) | (x >> 9)) - 516734799;
			uint32_t result = (temp << 8) | (temp >> 24);
			*data = result;
			data++;
			--count;
		} while (count);

		return gchandle::get_target(value);
	}

	inline uptr cl_active_item(uptr a1) {

		if (!a1) return 0;
		uint32_t* v = reinterpret_cast<uint32_t*>(&a1);
		uint32_t count = 2;

		do
		{
			uint32_t x = *v;
			*v = (((x - 757610148) ^ 0x0D38A8EE) + 881127734) ^ 0xE4CADF06;
			v++;
			--count;
		} while (count);


		return a1;
	}

	inline u32 encrypt_convar_fov_encryption(f32 input_value) {
		uint32_t v = *(uint32_t*)&input_value;
		v = ((v - 1110698532) ^ 0xB6AD3C74) + 1008482038;
		return v;
	}

	inline u32 decrypt_convar_fov_encryption(u32 encrypted) {
		u32 val = encrypted;
		val += 0x4fdf3546;
		val = std::rotr(val, 24);
		val ^= 0x731a6b75;
		return val;
	}

	inline u32 encrypt_base_movement_float_108(f32 input_value) {
		u32 val = *reinterpret_cast<u32*>(&input_value);
		val ^= 0x7aeaf775;
		return val;
	}
}