#include "hook_disassembly.hpp"

// Simplified instruction length decoder for x86-64.
// Handles common instruction prefixes, REX, VEX, ModR/M, SIB, and displacement/immediate bytes.
// This covers the vast majority of kernel function prologues.
namespace
{
	std::uint8_t get_instruction_length(const std::uint8_t* code)
	{
		const std::uint8_t* p = code;

		// Legacy prefixes
		bool has_operand_override = false;
		bool has_address_override = false;

		while (true)
		{
			switch (*p)
			{
			case 0x26: case 0x2E: case 0x36: case 0x3E:
			case 0x64: case 0x65: case 0xF0: case 0xF2: case 0xF3:
				p++;
				continue;
			case 0x66:
				has_operand_override = true;
				p++;
				continue;
			case 0x67:
				has_address_override = true;
				p++;
				continue;
			default:
				break;
			}
			break;
		}

		// REX prefix
		bool rex_w = false;
		if ((*p & 0xF0) == 0x40)
		{
			rex_w = (*p & 0x08) != 0;
			p++;
		}

		// Opcode
		bool is_two_byte = false;
		std::uint8_t opcode = *p++;

		if (opcode == 0x0F)
		{
			is_two_byte = true;
			opcode = *p++;
		}

		// Determine if ModR/M is needed and immediate size
		bool has_modrm = false;
		std::uint8_t imm_size = 0;

		if (is_two_byte)
		{
			// Most 0F xx instructions have ModR/M
			has_modrm = true;

			// Conditional jumps (0F 80-8F) have rel32 immediate, no ModR/M
			if (opcode >= 0x80 && opcode <= 0x8F)
			{
				has_modrm = false;
				imm_size = 4;
			}
			// SETcc (0F 90-9F) have ModR/M, no immediate
			// CMOVcc (0F 40-4F) have ModR/M, no immediate
			// MOVZX/MOVSX etc have ModR/M, no immediate
		}
		else
		{
			// Single byte opcode analysis
			switch (opcode)
			{
			// ret
			case 0xC3:
				return static_cast<std::uint8_t>(p - code);

			// ret imm16
			case 0xC2:
				return static_cast<std::uint8_t>(p - code + 2);

			// int3
			case 0xCC:
				return static_cast<std::uint8_t>(p - code);

			// nop
			case 0x90:
				return static_cast<std::uint8_t>(p - code);

			// push/pop reg (50-5F)
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
			case 0x58: case 0x59: case 0x5A: case 0x5B:
			case 0x5C: case 0x5D: case 0x5E: case 0x5F:
				return static_cast<std::uint8_t>(p - code);

			// short jmp
			case 0xEB:
				return static_cast<std::uint8_t>(p - code + 1);

			// near jmp
			case 0xE9:
				return static_cast<std::uint8_t>(p - code + 4);

			// near call
			case 0xE8:
				return static_cast<std::uint8_t>(p - code + 4);

			// short conditional jumps (70-7F)
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			case 0x78: case 0x79: case 0x7A: case 0x7B:
			case 0x7C: case 0x7D: case 0x7E: case 0x7F:
				return static_cast<std::uint8_t>(p - code + 1);

			// mov reg, imm (B0-BF)
			case 0xB0: case 0xB1: case 0xB2: case 0xB3:
			case 0xB4: case 0xB5: case 0xB6: case 0xB7:
				return static_cast<std::uint8_t>(p - code + 1); // 8-bit immediate

			case 0xB8: case 0xB9: case 0xBA: case 0xBB:
			case 0xBC: case 0xBD: case 0xBE: case 0xBF:
				if (rex_w)
					return static_cast<std::uint8_t>(p - code + 8); // 64-bit immediate
				return static_cast<std::uint8_t>(p - code + 4); // 32-bit immediate

			// xchg eax, reg
			case 0x91: case 0x92: case 0x93:
			case 0x94: case 0x95: case 0x96: case 0x97:
				return static_cast<std::uint8_t>(p - code);

			// Group with imm8
			case 0x6A: // push imm8
				return static_cast<std::uint8_t>(p - code + 1);

			// push imm32
			case 0x68:
				return static_cast<std::uint8_t>(p - code + 4);

			// test al, imm8
			case 0xA8:
				return static_cast<std::uint8_t>(p - code + 1);

			// test eax, imm32
			case 0xA9:
				if (has_operand_override)
					return static_cast<std::uint8_t>(p - code + 2);
				return static_cast<std::uint8_t>(p - code + 4);

			// Opcodes with ModR/M + no immediate
			case 0x00: case 0x01: case 0x02: case 0x03: // add
			case 0x08: case 0x09: case 0x0A: case 0x0B: // or
			case 0x10: case 0x11: case 0x12: case 0x13: // adc
			case 0x18: case 0x19: case 0x1A: case 0x1B: // sbb
			case 0x20: case 0x21: case 0x22: case 0x23: // and
			case 0x28: case 0x29: case 0x2A: case 0x2B: // sub
			case 0x30: case 0x31: case 0x32: case 0x33: // xor
			case 0x38: case 0x39: case 0x3A: case 0x3B: // cmp
			case 0x84: case 0x85: // test
			case 0x86: case 0x87: // xchg
			case 0x88: case 0x89: case 0x8A: case 0x8B: // mov
			case 0x8D: // lea
			case 0x8E: case 0x8F: // mov seg / pop
			case 0xFF: // inc/dec/call/jmp/push group
			case 0xFE: // inc/dec group
			case 0xD1: case 0xD3: // shift group (by 1 / by CL)
				has_modrm = true;
				imm_size = 0;
				break;

			// Opcodes with ModR/M + imm8
			case 0x04: case 0x0C: case 0x14: case 0x1C:
			case 0x24: case 0x2C: case 0x34: case 0x3C: // alu al, imm8
				return static_cast<std::uint8_t>(p - code + 1);

			case 0x80: case 0x82: case 0x83: case 0xC0: case 0xC1: case 0xC6:
				has_modrm = true;
				imm_size = 1;
				break;

			// Opcodes with ModR/M + imm16/32
			case 0x05: case 0x0D: case 0x15: case 0x1D:
			case 0x25: case 0x2D: case 0x35: case 0x3D: // alu eax, imm32
				if (has_operand_override)
					return static_cast<std::uint8_t>(p - code + 2);
				return static_cast<std::uint8_t>(p - code + 4);

			case 0x81: case 0xC7:
				has_modrm = true;
				imm_size = has_operand_override ? 2 : 4;
				break;

			case 0x69: // imul r, r/m, imm32
				has_modrm = true;
				imm_size = has_operand_override ? 2 : 4;
				break;

			case 0x6B: // imul r, r/m, imm8
				has_modrm = true;
				imm_size = 1;
				break;

			default:
				// Unknown opcode — conservative fallback: assume 1 byte
				return static_cast<std::uint8_t>(p - code);
			}
		}

		// Process ModR/M
		if (has_modrm)
		{
			std::uint8_t modrm = *p++;
			std::uint8_t mod = (modrm >> 6) & 3;
			std::uint8_t rm = modrm & 7;

			// Check for SIB byte
			if (mod != 3 && rm == 4)
			{
				p++; // SIB byte
			}

			// Displacement
			if (mod == 0 && rm == 5)
			{
				p += 4; // disp32 (RIP-relative in 64-bit)
			}
			else if (mod == 1)
			{
				p += 1; // disp8
			}
			else if (mod == 2)
			{
				p += 4; // disp32
			}
		}

		// Immediate
		p += imm_size;

		return static_cast<std::uint8_t>(p - code);
	}
}

std::pair<std::vector<std::uint8_t>, std::uint64_t> hook_disasm::get_routine_aligned_bytes(
	std::uint8_t* routine, const std::uint64_t minimum_size, const std::uint64_t routine_runtime_address)
{
	(void)routine_runtime_address;

	std::vector<std::uint8_t> aligned_bytes;
	std::uint64_t total_original_bytes = 0;

	while (total_original_bytes < minimum_size)
	{
		const std::uint8_t length = get_instruction_length(routine + total_original_bytes);

		if (length == 0)
		{
			// Failed to decode — abort
			return {{}, 0};
		}

		// Copy raw instruction bytes
		for (std::uint8_t i = 0; i < length; i++)
		{
			aligned_bytes.push_back(routine[total_original_bytes + i]);
		}

		total_original_bytes += length;
	}

	return {aligned_bytes, total_original_bytes};
}
