#pragma once
#include <sdk/globals.hpp>
#include <string>
#include <vector>

namespace encryption_dumper
{
	struct decryption_step
	{
		enum op_type {
			OP_ADD,		// add reg, imm32
			OP_SUB,		// sub reg, imm32
			OP_XOR,		// xor reg, imm32
			OP_ROL,		// rotate left by N bits  (shl + shr + or)
			OP_ROR,		// rotate right by N bits (shr + shl + or)
		};

		op_type type;
		u32 immediate;
	};

	struct decryption_info
	{
		std::string name;
		u32 loop_count = 0;						// iteration count (typically 2)
		u32 read_offset = 0;					// offset from input ptr (e.g. 0x18)
		std::vector<decryption_step> steps;		// ordered operations per loop iteration
		bool valid = false;
		uptr found_at = 0;						// RVA where the routine was found
	};

	decryption_info analyze_routine(uptr game_assembly_base, uptr address, const std::string& name);

	std::vector<decryption_info> scan_for_routines(uptr game_assembly_base, uptr module_size);

	void dump_all(uptr game_assembly_base);

	std::string generate_code(const decryption_info& info);

	std::string step_to_string(const decryption_step& step);
}
