#pragma once
#include "crt.hpp"
#include "intrin.hpp"

#include <Windows.h>
#include <winternl.h>

namespace process
{
	using handle_t = HANDLE;
	using peb_t = PEB;
	using peb_ldr_data_t = PEB_LDR_DATA;
	using ldr_data_entry_t = LDR_DATA_TABLE_ENTRY;
	using list_entry_t = LIST_ENTRY;
	using unicode_string_t = UNICODE_STRING;

	inline handle_t current_handle()
	{
		return reinterpret_cast<handle_t>(-1);
	}

	inline peb_t* current_peb()
	{
		return reinterpret_cast<peb_t*>(__readgsqword(0x60));
	}

	inline peb_ldr_data_t* current_ldr_data()
	{
		const peb_t* const peb = current_peb();

		return peb->Ldr;
	}

	inline ldr_data_entry_t* ldr_entry_from_memory_ordered_list(const list_entry_t* const entry)
	{
		return CONTAINING_RECORD(entry, ldr_data_entry_t, InMemoryOrderLinks);
	}

	inline ldr_data_entry_t* ldr_entry_from_load_ordered_list(const list_entry_t* const entry)
	{
		return const_cast<ldr_data_entry_t*>(reinterpret_cast<const ldr_data_entry_t*>(entry));
	}

	inline std::uint8_t* base_from_ldr_entry(const ldr_data_entry_t* const entry)
	{
		return static_cast<std::uint8_t*>(entry->DllBase);
	}

	struct module_t
	{
		crt::wstring_t path;
		std::uint8_t* base_address;
	};

	inline module_t find_module(const crt::wstring_view_t target_name)
	{
		const peb_ldr_data_t* const ldr_data = current_ldr_data();
		const list_entry_t* const list_head = &ldr_data->InMemoryOrderModuleList;

		for (const list_entry_t* entry = list_head->Flink; entry != list_head; entry = entry->Flink)
		{
			const ldr_data_entry_t* const current_module = ldr_entry_from_memory_ordered_list(entry);

			const unicode_string_t full_dll_name = current_module->FullDllName;

			if (!full_dll_name.Buffer || !full_dll_name.Length)
			{
				continue;
			}

			const std::uint64_t name_count = full_dll_name.Length / sizeof(wchar_t);
			const crt::wstring_view_t module_name_view(full_dll_name.Buffer, name_count);

			if (module_name_view.find(target_name) != crt::wstring_view_t::npos)
			{
				module_t module;

				module.path = crt::wstring_t(module_name_view);
				module.base_address = base_from_ldr_entry(current_module);

				return module;
			}
		}

		return { };
	}
}
