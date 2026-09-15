#include "initial_syscall.hpp"
#include "inline_syscall.hpp"
#include "process.hpp"
#include <string_encryption.hpp>

syscall_t set_up_nt_allocate_virtual_memory(const portable_executable::image_t* const ntdll, const std::uint32_t syscall_id)
{
	syscall_t syscall(syscall_id);

	syscall.load_stub(
		[ntdll, syscall_id](std::uint64_t size) -> void*
		{
			void* base_address = nullptr;

			do_initial_syscall(ntdll, syscall_id, process::current_handle(), &base_address, nullptr, &size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

			return base_address;
		}
	);

	return syscall;
}

void destroy_nt_free_virtual_memory(syscall_t& syscall)
{
	const auto [ntdll_path, ntdll_base_address] = process::find_module(xs(L"ntdll.dll"));

	if (!ntdll_base_address)
	{
		return;
	}

	const auto ntdll = reinterpret_cast<const portable_executable::image_t*>(ntdll_base_address);

	const std::uint32_t syscall_id = syscall.id();

	syscall.unload_stub(
		[ntdll, syscall_id](void* ptr) -> void
		{
			std::uint64_t size = 0;

			do_initial_syscall(ntdll, syscall_id, process::current_handle(), &ptr, &size, MEM_RELEASE);
		}
	);
}
