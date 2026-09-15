#pragma once
#include <cstdint>

#include <intrin.h>

namespace intrin
{
	inline std::uint64_t read_gs_qword(const std::uint32_t offset)
	{
		return __readgsqword(offset);
	}
}
