#include "utils.hpp"
#include <windows.h>

namespace utils
{
	std::string to_string(const std::wstring& input)
	{
		if (input.empty())
		{
			return { };
		}

		const std::int32_t count = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<std::int32_t>(input.size()), nullptr, 0, nullptr, nullptr);

		if (count <= 0)
		{
			return { };
		}

		std::string output(count, '\0');

		WideCharToMultiByte(CP_UTF8, 0, input.c_str(), static_cast<std::int32_t>(input.size()), output.data(), count, nullptr, nullptr);

		return output;
	}
}