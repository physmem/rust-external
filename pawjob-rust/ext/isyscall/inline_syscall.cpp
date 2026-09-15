#include "inline_syscall.hpp"
#include "process.hpp"

#include <portable_executable/image.hpp>
#include <asmjit/asmjit.h>
#include <print>
#include <optional>

#include "initial_syscall.hpp"
#include "jit.hpp"
#include "portable_executable/file.hpp"

namespace
{
	crt::unordered_map_t<fnv1a::value_type, syscall_t>* syscalls = nullptr;
}

void* allocate_rwx_memory(std::uint64_t size)
{
	void* base_address = nullptr;

	ISYSCALL(NtAllocateVirtualMemory)(process::current_handle(), &base_address, nullptr, &size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	return base_address;
}

void deallocate_memory(void* ptr)
{
	std::uint64_t size = 0;

	ISYSCALL(NtFreeVirtualMemory)(process::current_handle(), &ptr, &size, MEM_RELEASE);
}

crt::optional_t<std::uint32_t> resolve_syscall_id(const std::uint8_t* const address)
{
	const std::uint16_t syscall_instruction = *reinterpret_cast<const std::uint16_t*>(&address[0x12]);

	if (syscall_instruction != 0x50F)
	{
		return { };
	}

	return *reinterpret_cast<const std::uint32_t*>(&address[4]);
}

syscall_t::~syscall_t()
{
	this->reset();
}

void syscall_t::reset()
{
	if (this->stub_)
	{
		deallocate_memory(stub_);

		stub_ = nullptr;
	}

	id_ = 0;
}

syscall_t& syscall_t::find(const fnv1a::value_type hash)
{
	return (*syscalls)[hash];
}

crt::vector_t<std::uint8_t> compile_syscall_stub(const std::uint32_t syscall_id)
{
	const auto assemble_fn =
		[syscall_id](asmjit::x86::Assembler& assembler) -> void
		{
			assembler.mov(asmjit::x86::eax, asmjit::imm(syscall_id));
			assembler.mov(asmjit::x86::r10, asmjit::x86::rcx);
			assembler.syscall();
			assembler.ret();
		};

	return jit::compile_code(assemble_fn);
}

bool syscall_t::load_stub()
{
	return load_stub(allocate_rwx_memory);
}

bool syscall_t::load_stub(const crt::function_t<void*(std::uint64_t)>& allocation_fn)
{
	stub_ = jit::alloc_and_compile_code(
		[this](asmjit::x86::Assembler& assembler) -> void
		{
			assembler.mov(asmjit::x86::eax, asmjit::imm(id_));
			assembler.mov(asmjit::x86::r10, asmjit::x86::rcx);
			assembler.syscall();
			assembler.ret();
		},
		allocation_fn
	);

	return stub_ != nullptr;
}

void syscall_t::unload_stub()
{
	return unload_stub(deallocate_memory);
}

void syscall_t::unload_stub(const crt::function_t<void(void*)>& deallocation_fn)
{
	if (this->stub_)
	{
		deallocation_fn(stub_);

		stub_ = nullptr;
	}
}

bool isyscall::load()
{
	syscalls = new crt::unordered_map_t<fnv1a::value_type, syscall_t>();

	const auto [ntdll_path, ntdll_base_address] = process::find_module(xs(L"ntdll.dll"));

	if (!ntdll_base_address)
	{
		return false;
	}

	portable_executable::file_t ntdll_file(crt::wstring_view_t{ ntdll_path });

	if (!ntdll_file.load())
	{
		return false;
	}

	const auto ntdll = ntdll_file.image();
	constexpr fnv1a::value_type nt_allocate_virtual_memory_hash = fnv1a::hash_string_literal("NtAllocateVirtualMemory");

	//std::println("loading syscalls");
	for (const auto& [name, address] : ntdll->exports())
	{
		const auto syscall_id = resolve_syscall_id(address);

		if (!syscall_id)
		{
			//std::println("failed to parse {}", name);
			continue;
		}

		const fnv1a::value_type hash = fnv1a::hash_ranged_object(name);

		if (hash == nt_allocate_virtual_memory_hash)
		{
			const auto persistent_ntdll = reinterpret_cast<const portable_executable::image_t*>(ntdll_base_address);

			(*syscalls)[hash] = set_up_nt_allocate_virtual_memory(persistent_ntdll, *syscall_id);
		}
		else
		{
			(*syscalls)[hash] = syscall_t(*syscall_id);
		}

		//std::println("pushed syscall {}", name);
	}

	// make sure NtFreeVirtualMemory stub is always allocated, as it is required to unload the other stubs
	deallocate_memory(nullptr);

	return true;
}

void isyscall::unload()
{
	constexpr fnv1a::value_type nt_free_virtual_memory_hash = fnv1a::hash_string_literal("NtFreeVirtualMemory");

	if (!syscalls || !syscalls->contains(nt_free_virtual_memory_hash))
	{
		return;
	}

	for (auto& [hash, syscall] : *syscalls)
	{
		if (hash != nt_free_virtual_memory_hash)
		{
			syscall.unload_stub();
		}
	}

	auto& nt_free_virtual_memory = (*syscalls)[nt_free_virtual_memory_hash];

	destroy_nt_free_virtual_memory(nt_free_virtual_memory);

	delete syscalls;
	syscalls = nullptr;
}
