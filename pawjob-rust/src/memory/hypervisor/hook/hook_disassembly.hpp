#pragma once
#include <cstdint>
#include <vector>

union parted_address_t
{
	struct
	{
		std::uint32_t low_part;
		std::uint32_t high_part;
	} u;

	std::uint64_t value;
};

namespace hook_disasm
{
	// Returns pair<aligned_original_bytes, total_original_bytes_consumed>
	// Simplified version: copies raw bytes without RIP-relative instruction relocation.
	// Safe for standard function prologues where the first 14+ bytes contain no
	// RIP-relative addressing (mov rdi,rsp / push rbp / sub rsp,XX / etc).
	std::pair<std::vector<std::uint8_t>, std::uint64_t> get_routine_aligned_bytes(
		std::uint8_t* routine, std::uint64_t minimum_size, std::uint64_t routine_runtime_address);
}
