#pragma once
#include <portable_executable/image.hpp>
#include <string_encryption.hpp>
#include <cstdint>

class syscall_t;

extern "C" std::uint32_t do_initial_syscall_gadget(void* rcx);

// IMPORTANT: this is only for the STUB allocation for NtAllocateVirtualMemory and the STUB deallocation for NtFreeVirtualMemory
// all other syscalls will be done through dynamically allocated stubs
template <class... Arguments>
std::uint32_t do_initial_syscall(const portable_executable::image_t* const ntdll, const std::uint32_t syscall_id, void* const real_rcx_value, Arguments... arguments)
{
	struct
	{
		void* real_rcx_value;
		std::uint8_t* gadget;
		std::uint32_t syscall_id;
	} context;

	context.real_rcx_value = real_rcx_value;
	context.gadget = ntdll->signature_scan(xs("0F 05 C3 CD 2E C3"));
	context.syscall_id = syscall_id;

	using initial_syscall_fn_t = std::uint32_t(__fastcall*)(void*, Arguments...);

	const initial_syscall_fn_t initial_syscall_fn = reinterpret_cast<initial_syscall_fn_t>(do_initial_syscall_gadget);

	return initial_syscall_fn(&context, arguments...);
}

syscall_t set_up_nt_allocate_virtual_memory(const portable_executable::image_t* ntdll, std::uint32_t syscall_id);
void destroy_nt_free_virtual_memory(syscall_t& syscall);
