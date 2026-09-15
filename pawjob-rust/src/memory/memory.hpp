#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <sdk/globals.hpp>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#define MEM_PAD(SIZE) \
private: \
	char _MEM_CONCATENATE(pad_0, __COUNTER__)[SIZE]; \
public:

#define _MEM_INTERNAL_CONCATENATE(LEFT, RIGHT) LEFT##RIGHT
#define _MEM_CONCATENATE(LEFT, RIGHT) _MEM_INTERNAL_CONCATENATE(LEFT, RIGHT)

namespace memory
{
	void create_instance(std::string_view target_process);

	bool read_memory_raw(const std::uintptr_t& address, void* buffer, std::size_t size);
	bool write_memory_raw(const std::uintptr_t& address, void* buffer, std::size_t size);

	std::uintptr_t get_base_address();
	std::string get_game_directory();

	std::pair<std::uintptr_t, std::size_t> get_module_easy(std::wstring_view name);
	std::pair<std::uintptr_t, std::size_t> get_module(std::wstring_view name);

	template<typename T = std::uintptr_t>
	T read(const std::uintptr_t& address)
	{
		T buffer{};
		read_memory_raw(address, &buffer, sizeof(T));
		return buffer;
	}

	template<typename T>
	bool write(const std::uintptr_t& address, const T& value)
	{
		return write_memory_raw(address, (void*)&value, sizeof(T));
	}

	__forceinline std::string read_string(const std::uintptr_t& address)
	{
		char buffer[MAX_PATH]{};
		read_memory_raw(address, &buffer, sizeof(buffer));
		return std::string(buffer);
	}

	template <std::size_t size_of_char = 1>
	__forceinline std::wstring read_wstring(std::uintptr_t src, std::size_t size)
	{
		std::wstring buffer(size, '\0');
		read_memory_raw(src, buffer.data(), size * size_of_char);
		return buffer;
	}

	template<typename T>
	T chain_read(const uptr& address, std::vector<uptr> offsets)
	{
		uptr value = address;
		for (int i = 0; i < offsets.size() - 1; i++)
		{
			const uptr& offset = offsets[i];
			value = read<uptr>(value + offset);
		}
		return read<T>(value + offsets[offsets.size() - 1]);
	}

	std::uintptr_t pattern_scan(std::uintptr_t module_base, const std::string& signature, uintptr_t start_from = 0);

	std::uintptr_t resolve_lea(std::uintptr_t lea_address);

	void on_frame();

	namespace impl
	{
		inline i32 process_id = 0;
		inline HWND game_hwnd = nullptr;

		inline uptr base_address = 0;
		inline uptr process_peb = 0;
		inline uptr cr3 = 0;

		inline std::string game_directory{};
	}
}