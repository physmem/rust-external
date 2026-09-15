#pragma once
#include <asmjit/asmjit.h>

#include "crt.hpp"

namespace jit
{
	inline crt::vector_t<std::uint8_t> compile_code(const crt::function_t<void(asmjit::x86::Assembler&)>& assemble_fn)
	{
		asmjit::JitRuntime runtime;
		asmjit::CodeHolder code_holder;

		code_holder.init(runtime.environment());
		asmjit::x86::Assembler assembler(&code_holder);

		assemble_fn(assembler);

		std::uint8_t* compiled_code = nullptr;

		if (runtime.add(&compiled_code, &code_holder))
		{
			return { };
		}

		return crt::vector_t<std::uint8_t>{ compiled_code, compiled_code + code_holder.codeSize() };
	}

	inline std::uint8_t* alloc_and_compile_code(const crt::function_t<void(asmjit::x86::Assembler&)>& assemble_fn, const crt::function_t<void*(std::uint64_t)>& allocation_fn)
	{
		const auto compiled_callback = compile_code(assemble_fn);

		if (compiled_callback.empty())
		{
			return nullptr;
		}

		const std::uint64_t buffer_size = compiled_callback.size();

		const auto buffer = static_cast<std::uint8_t*>(allocation_fn(buffer_size));

		if (!buffer)
		{
			return nullptr;
		}

		crt::copy_memory(buffer, compiled_callback.data(), buffer_size);

		return buffer;
	}
}
