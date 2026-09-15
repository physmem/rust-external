#include "encryption_dumper.hpp"
#include <memory/memory.hpp>
#include <print>

#include <Zydis/Zydis.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace encryption_dumper
{
	struct pattern_byte
	{
		u8 value;
		bool wildcard;
	};

	static std::vector<pattern_byte> parse_ida_pattern(const std::string& sig)
	{
		std::vector<pattern_byte> pattern;
		std::istringstream ss(sig);
		std::string token;
		while (ss >> token)
		{
			if (token == "?")
				pattern.push_back({ 0, true });
			else
				pattern.push_back({ static_cast<u8>(strtoul(token.c_str(), nullptr, 16)), false });
		}
		return pattern;
	}

	static bool match_pattern(const u8* data, const std::vector<pattern_byte>& pattern)
	{
		for (std::size_t i = 0; i < pattern.size(); i++)
		{
			if (!pattern[i].wildcard && data[i] != pattern[i].value)
				return false;
		}
		return true;
	}

	// End of pattern utilities

	struct decoded_insn
	{
		ZydisDecodedInstruction insn;
		ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
		u64 address;
	};

	static std::vector<decoded_insn> decode_bytes(const u8* data, std::size_t size, u64 base_addr, std::size_t max_insns = 300)
	{
		std::vector<decoded_insn> result;
		result.reserve(max_insns);

		ZydisDecoder decoder;
		ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

		std::size_t offset = 0;
		while (offset < size && result.size() < max_insns)
		{
			decoded_insn di{};
			di.address = base_addr + offset;

			auto status = ZydisDecoderDecodeFull(&decoder, data + offset, size - offset, &di.insn, di.operands);
			if (!ZYAN_SUCCESS(status))
			{
				offset++;
				continue;
			}

			result.push_back(di);
			offset += di.insn.length;

			// stop at ret
			if (di.insn.mnemonic == ZYDIS_MNEMONIC_RET)
				break;
		}

		return result;
	}

	static bool is_imm(const ZydisDecodedOperand& op) { return op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE; }
	static bool is_reg(const ZydisDecodedOperand& op) { return op.type == ZYDIS_OPERAND_TYPE_REGISTER; }
	static bool is_mem(const ZydisDecodedOperand& op) { return op.type == ZYDIS_OPERAND_TYPE_MEMORY; }
	static u32 get_imm32(const ZydisDecodedOperand& op) { return static_cast<u32>(op.imm.value.u); }
	static u8 get_imm8(const ZydisDecodedOperand& op) { return static_cast<u8>(op.imm.value.u); }

	decryption_info analyze_routine(uptr base, uptr address, const std::string& name)
	{
		decryption_info info{};
		info.name = name;
		info.found_at = address - base;

		constexpr std::size_t CODE_SIZE = 1024;
		u8 code[CODE_SIZE]{};

		if (!memory::read_memory_raw(address, code, CODE_SIZE))
		{
			std::println("[dumper] failed to read code at {:#x} for '{}'", address, name);
			return info;
		}

		auto instructions = decode_bytes(code, CODE_SIZE, address);
		if (instructions.empty())
		{
			std::println("[dumper] failed to decode instructions at {:#x}", address);
			return info;
		}

		// If the first instruction is a CALL or JMP, follow it to the actual routine
		if (instructions[0].insn.mnemonic == ZYDIS_MNEMONIC_CALL || instructions[0].insn.mnemonic == ZYDIS_MNEMONIC_JMP)
		{
			if (is_imm(instructions[0].operands[0]))
			{
				uptr target = address + instructions[0].insn.length + instructions[0].operands[0].imm.value.s;
				std::println("[dumper] following {} to target {:#x}",
					(instructions[0].insn.mnemonic == ZYDIS_MNEMONIC_CALL ? "CALL" : "JMP"), target);
				return analyze_routine(base, target, name);
			}
		}

		std::println("[dumper] analyzing routine '{}' at {:#x} ({} instructions)", name, address, instructions.size());

		bool pending_shl = false;
		bool pending_shr = false;
		u8 shl_amount = 0;
		u8 shr_amount = 0;

		for (size_t i = 0; i < instructions.size(); i++)
		{
			const auto& di = instructions[i];
			const auto& insn = di.insn;
			const auto& ops = di.operands;

			if (insn.mnemonic == ZYDIS_MNEMONIC_MOV && insn.operand_count >= 2 &&
				is_reg(ops[0]) && is_mem(ops[1]) &&
				ops[1].mem.disp.has_displacement && ops[1].mem.disp.value > 0 && ops[1].mem.disp.value < 0x1000)
			{
				if (info.read_offset == 0)
					info.read_offset = static_cast<u32>(ops[1].mem.disp.value);
			}

			if (insn.mnemonic == ZYDIS_MNEMONIC_MOV && is_reg(ops[0]) && is_reg(ops[1])) {
				continue;
			}

			// check for xor
			if (insn.mnemonic == ZYDIS_MNEMONIC_XOR && is_imm(ops[1])) {
				u32 val = get_imm32(ops[1]);
				if (val > 0xFF) info.steps.push_back({ decryption_step::OP_XOR, val });
			}
			// check for sub
			else if (insn.mnemonic == ZYDIS_MNEMONIC_SUB && is_imm(ops[1])) {
				u32 val = get_imm32(ops[1]);
				if (val > 0xFF) info.steps.push_back({ decryption_step::OP_SUB, val });
			}
			// check for add (sometimes used as sub with negative)
			else if (insn.mnemonic == ZYDIS_MNEMONIC_ADD && is_imm(ops[1])) {
				u32 val = get_imm32(ops[1]);
				if (val > 0x80000000) // negative
					info.steps.push_back({ decryption_step::OP_SUB, static_cast<u32>(-(i32)val) });
				else if (val > 0xFF)
					info.steps.push_back({ decryption_step::OP_ADD, val });
			}
			// check for rol
			else if (insn.mnemonic == ZYDIS_MNEMONIC_ROL && insn.operand_count >= 1) {
				u8 amount = (insn.operand_count >= 2 && is_imm(ops[1])) ? get_imm8(ops[1]) : 1;
				info.steps.push_back({ decryption_step::OP_ROL, amount });
			}
			// check for ror
			else if (insn.mnemonic == ZYDIS_MNEMONIC_ROR && insn.operand_count >= 1) {
				u8 amount = (insn.operand_count >= 2 && is_imm(ops[1])) ? get_imm8(ops[1]) : 1;
				info.steps.push_back({ decryption_step::OP_ROR, amount });
			}
			// SHL / SHR tracking for manual rotation
			else if (insn.mnemonic == ZYDIS_MNEMONIC_SHL && is_imm(ops[1])) {
				shl_amount = get_imm8(ops[1]);
				pending_shl = true;
			}
			else if (insn.mnemonic == ZYDIS_MNEMONIC_SHR && is_imm(ops[1])) {
				shr_amount = get_imm8(ops[1]);
				pending_shr = true;
			}
			else if (insn.mnemonic == ZYDIS_MNEMONIC_OR && is_reg(ops[0]) && is_reg(ops[1])) {
				if (pending_shl && pending_shr && (shl_amount + shr_amount == 32)) {
					info.steps.push_back({ decryption_step::OP_ROL, shl_amount });
				}
				pending_shl = false;
				pending_shr = false;
			}
			// check for ADD reg, reg (often used for SHL 1)
			else if (insn.mnemonic == ZYDIS_MNEMONIC_ADD && is_reg(ops[0]) && is_reg(ops[1]) && ops[0].reg.value == ops[1].reg.value) {
				shl_amount = 1;
				pending_shl = true;
			}
		}

		info.valid = !info.steps.empty();
		if (info.loop_count == 0 && info.valid) info.loop_count = 1;

		return info;
	}

	std::string step_to_string(const decryption_step& step)
	{
		switch (step.type)
		{
		case decryption_step::OP_ADD: return std::format("ADD {:#010x}", step.immediate);
		case decryption_step::OP_SUB: return std::format("SUB {:#010x}", step.immediate);
		case decryption_step::OP_XOR: return std::format("XOR {:#010x}", step.immediate);
		case decryption_step::OP_ROL: return std::format("ROL {}", step.immediate);
		case decryption_step::OP_ROR: return std::format("ROR {}", step.immediate);
		default: return "???";
		}
	}

	std::string generate_code(const decryption_info& info)
	{
		std::ostringstream ss;

		ss << "// Auto-dumped: " << info.name << "\n";
		ss << "// RVA: " << std::format("{:#x}", info.found_at) << "\n";
		ss << "// Steps: ";
		for (size_t i = 0; i < info.steps.size(); i++)
		{
			if (i > 0) ss << " -> ";
			ss << step_to_string(info.steps[i]);
		}
		ss << "\n\n";

		std::string clean_name = info.name;
		if (clean_name.starts_with("encrypt_")) clean_name = clean_name.substr(8);
		else if (clean_name.starts_with("decrypt_")) clean_name = clean_name.substr(8);

		ss << std::format("u32 encrypt_{}(f32 input_value) {{\n", clean_name);
		ss << "\tu32 val = *reinterpret_cast<u32*>(&input_value);\n";

		for (const auto& step : info.steps)
		{
			switch (step.type)
			{
			case decryption_step::OP_ADD:
				ss << std::format("\tval += {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_SUB:
				ss << std::format("\tval -= {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_XOR:
				ss << std::format("\tval ^= {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_ROL:
				ss << std::format("\tval = std::rotl(val, {});\n", step.immediate);
				break;
			case decryption_step::OP_ROR:
				ss << std::format("\tval = std::rotr(val, {});\n", step.immediate);
				break;
			}
		}

		ss << "\treturn val;\n";
		ss << "}\n\n";

		ss << std::format("u32 decrypt_{}(u32 encrypted) {{\n", clean_name);
		ss << "\tu32 val = encrypted;\n";

		for (int i = static_cast<int>(info.steps.size()) - 1; i >= 0; i--)
		{
			const auto& step = info.steps[i];
			switch (step.type)
			{
			case decryption_step::OP_ADD:
				ss << std::format("\tval -= {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_SUB:
				ss << std::format("\tval += {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_XOR:
				ss << std::format("\tval ^= {:#x};\n", step.immediate);
				break;
			case decryption_step::OP_ROL:
				ss << std::format("\tval = std::rotr(val, {});\n", step.immediate);
				break;
			case decryption_step::OP_ROR:
				ss << std::format("\tval = std::rotl(val, {});\n", step.immediate);
				break;
			}
		}

		ss << "\treturn val;\n";
		ss << "}\n";

		return ss.str();
	}

	static const struct {
		const char* name;
		const char* signature;
	} known_signatures[] = {
		{
			"convar_fov_encryption",
			"48 83 EC ? 45 33 C0 0F 29 74 24 ? 0F 57 C9 48 8B CA E8 ? ? ? ? 80 3D ? ? ? ? ? 0F 28 F0 75 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? F0 83 0C 24 ? 48 8D 0D ? ? ? ? E8 ? ? ? ? F0 83 0C 24 ? C6 05 ? ? ? ? ? F3 0F 10 0D"
		},
		{
			"convar_graphics",
			"48 8D 0D ? ? ? ? E8 ? ? ? ? F0 83 0C 24 ? C6 05 ? ? ? ? ? F3 0F 10 0D ? ? ? ? 0F 2F CE 77 ? F3 0F 10 0D ? ? ? ? 0F 2F F1 77 ? 0F 28 CE 48 C7 44 24 ? ? ? ? ? 48 8D 54 24 ? F3 0F 11 4C 24 ? 41 B8 ? ? ? ? 0F 1F 40"
		},
		{
			"base_movement_float_108",
			"81 ? 75 F7 EA 7A"
		},
	};

	void dump_all(uptr game_assembly_base)
	{
		std::ofstream out_file("decryptions_dump.txt");
		auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm tm_struct;
		localtime_s(&tm_struct, &now);

		out_file << "//      (\\_/)  \n";
		out_file << "//      ( •.•)  \n";
		out_file << "//      />❤<\n";
		out_file << "//   PAWJOB.LIFE\n";
		out_file << "// Pawjobs 4 Lyfe! >.<\n";
		out_file << "// ============================================================\n";
		out_file << "// Generated: " << std::put_time(&tm_struct, "%Y-%m-%d %H:%M:%S") << "\n";
		out_file << std::format("// GameAssembly base: {:#x}\n", game_assembly_base);
		out_file << "// ============================================================\n\n";

		int found_count = 0;
		std::set<uptr> dumped_addresses;

		for (const auto& [name, sig] : known_signatures)
		{
			std::println("[dumper] scanning for '{}' ...", name);

			uptr last_addr = 0;
			while (true)
			{
				uptr addr = memory::pattern_scan(game_assembly_base, sig, last_addr ? last_addr + 1 : 0);
				if (!addr)
				{
					if (last_addr == 0)
						std::println("[dumper] '{}' not found", name);
					break;
				}

				if (dumped_addresses.contains(addr))
				{
					last_addr = addr;
					continue;
				}

				auto info = analyze_routine(game_assembly_base, addr, name);

				if (!info.valid)
				{
					std::println("[dumper] '{}' found at {:#x} but failed to extract constants", name, addr);
					last_addr = addr;
					continue;
				}

				std::println("[dumper] '{}' found at {:#x} (RVA: {:#x})", name, addr, addr - game_assembly_base);

				found_count++;
				dumped_addresses.insert(addr);

				std::println("[dumper] --- {} ---", info.name);
				std::println("[dumper]   RVA: {:#x}", info.found_at);
				std::println("[dumper]   steps ({}):", info.steps.size());
				for (const auto& step : info.steps)
					std::println("[dumper]     {}", step_to_string(step));

				std::string code = generate_code(info);
				std::println("[dumper] generated code:\n{}", code);

				out_file << code << "\n";
				break; // found the correct one, move to next signature
			}
		}

		out_file.close();

		if (found_count > 0)
			std::println("[dumper] results written to decryptions_dump.txt");
		else
			std::println("[dumper] no routines found");

		std::println("[dumper] ======= done uwu =======");
	}
}
