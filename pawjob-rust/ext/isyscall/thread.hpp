#pragma once
#include "intrin.hpp"

#include <Windows.h>
#include <winternl.h>

namespace thread
{
	using handle_t = HANDLE;
	using teb_t = TEB;
	using context_t = CONTEXT;

	inline handle_t current_handle()
	{
		return reinterpret_cast<handle_t>(-2);
	}

	inline teb_t* current_teb()
	{
		return reinterpret_cast<teb_t*>(__readgsqword(0x30));
	}
}
