#pragma once
#include "structures/memory_operation.hpp"
#include <cstdint>

enum class hypercall_type_t : std::uint64_t
{
	guest_physical_memory_operation,
	guest_virtual_memory_operation,
	translate_guest_virtual_address,
	read_guest_cr3,
	add_slat_code_hook,
	remove_slat_code_hook,
	hide_guest_physical_page,
	log_current_state,
	flush_logs,
	get_heap_free_page_count,
	get_last_invalid_cr3_data,
	set_search_for_cr3_data
};

#pragma warning(push)
#pragma warning(disable : 4201)

// saltys keys
//constexpr std::uint64_t hypercall_primary_key = 0x16CE;
//constexpr std::uint64_t hypercall_secondary_key = 0x19;

constexpr std::uint64_t hypercall_primary_key = 0xA057;
constexpr std::uint64_t hypercall_secondary_key = 0x79;

union hypercall_info_t
{
	std::uint64_t value;

	struct
	{
		std::uint64_t primary_key : 16;
		hypercall_type_t call_type : 4;
		std::uint64_t secondary_key : 7;
		std::uint64_t call_reserved_data : 37;
	};
};

union virt_memory_op_hypercall_info_t
{
	std::uint64_t value;

	struct
	{
		std::uint64_t primary_key : 16;
		hypercall_type_t call_type : 4;
		std::uint64_t secondary_key : 7;
		memory_operation_t memory_operation : 1;
		std::uint64_t
			address_of_page_directory : 36; // we will construct the other cr3 (aside from the caller process) involved in the operation from this
	};
};

#pragma warning(pop)
