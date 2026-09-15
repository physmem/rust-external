#pragma once
#include <unordered_map>
#include <functional>
#include <string>
#include <string_view>
#include <array>
#include <memory>
#include <optional>

namespace crt
{
	using string_t = std::string;
	using wstring_t = std::wstring;

	using string_view_t = std::string_view;
	using wstring_view_t = std::wstring_view;

	template <typename Key, typename Value>
	using unordered_map_t = std::unordered_map<Key, Value>;

	template <typename T, std::uint64_t Size>
	using array_t = std::array<T, Size>;

	template <typename T>
	using vector_t = std::vector<T>;

	template <typename T>
	using optional_t = std::optional<T>;

	template <typename T>
	using function_t = std::function<T>;

	template <typename T>
	using shared_ptr_t = std::shared_ptr<T>;

	inline void copy_memory(void* const destination, const void* const source, const uint64_t size)
	{
		memcpy(destination, source, size);
	}
}
