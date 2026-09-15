#include "../utils/utils.hpp"
#include "memory.hpp"

#include <TlHelp32.h>

#include <chrono>
#include <thread>
#include <vector>

#include "../globals.hpp"
#include <isyscall/inline_syscall.hpp>
#include <print>
#include "../utils/vmp.hpp"

#include "../game/game.hpp"
#include "hypervisor/hypercall.hpp"
#include "hypervisor/system/system.hpp"
#include <portable_executable/image.hpp>

namespace memory
{
	static uint64_t find_mmpfn_database()
	{
		static uint64_t cached = 0;
		if (cached)
			return cached;

		const std::string ntoskrnlName = ("ntoskrnl.exe");
		if (!sys::kernel::modules_list || !sys::kernel::modules_list->contains(ntoskrnlName))
			return 0;

		auto& ntoskrnl = (*sys::kernel::modules_list)[ntoskrnlName];
		const std::string exportName = ("ntoskrnl.exe!MmGetVirtualForPhysical");

		if (!ntoskrnl.exports)
			return 0;

		auto it = ntoskrnl.exports->find(exportName);
		if (it == ntoskrnl.exports->end())
			return 0;

		uint64_t funcAddr = it->second;

		uint8_t code[0x30]{};
		if (hypercall::read_guest_virtual_memory(code, funcAddr, sys::current_cr3, sizeof(code)) != sizeof(code))
			return 0;

		// Pattern from MmGetVirtualForPhysical:
		// mov rax, rcx; shr rax, 0Ch; lea rdx, [rax+rax*2]; add rdx, rdx; mov rax, <MmPfnDatabase>
		static const uint8_t pattern[] = { 0x48, 0x8B, 0xC1, 0x48, 0xC1, 0xE8, 0x0C, 0x48, 0x8D, 0x14, 0x40, 0x48, 0x03, 0xD2, 0x48, 0xB8 };

		for (size_t i = 0; i + sizeof(pattern) + 8 <= sizeof(code); i++)
		{
			if (memcmp(code + i, pattern, sizeof(pattern)) == 0)
			{
				uint64_t pfnDb = 0;
				memcpy(&pfnDb, code + i + sizeof(pattern), sizeof(pfnDb));
				pfnDb &= ~0xFFFULL;
				cached = pfnDb;
				std::println("pfn db: {:#x}", cached);
				return cached;
			}
		}

		return 0;
	}

	uint64_t bruteforce_cr3(uint64_t base_address)
	{
		uint64_t mmPfnDatabase = find_mmpfn_database();
		if (!mmPfnDatabase)
		{
			return 0;
		}

		MEMORYSTATUSEX memInfo = { sizeof(memInfo) };
		if (!GlobalMemoryStatusEx(&memInfo))
			return 0;

		uint64_t maxPfn = (memInfo.ullTotalPhys + 0xFFF) >> 12;

		constexpr uint64_t MMPFN_SIZE = 0x30;
		constexpr uint64_t PTE_FRAME_OFFSET = 0x28;
		constexpr uint64_t PTE_FRAME_MASK = 0xFFFFFFFFFFULL;
		constexpr int BATCH_SIZE = 512;

		std::vector<uint8_t> pfnBatch(BATCH_SIZE * MMPFN_SIZE);

		for (uint64_t pfnBase = 1; pfnBase < maxPfn; pfnBase += BATCH_SIZE)
		{
			uint64_t count = (maxPfn - pfnBase < BATCH_SIZE) ? (maxPfn - pfnBase) : BATCH_SIZE;
			uint64_t readSize = count * MMPFN_SIZE;
			uint64_t readAddr = mmPfnDatabase + pfnBase * MMPFN_SIZE;

			if (hypercall::read_guest_virtual_memory(pfnBatch.data(), readAddr, sys::current_cr3, readSize) != readSize)
				continue;

			for (uint64_t i = 0; i < count; i++)
			{
				uint64_t pfn = pfnBase + i;

				uint64_t u4 = 0;
				memcpy(&u4, pfnBatch.data() + i * MMPFN_SIZE + PTE_FRAME_OFFSET, sizeof(u4));
				uint64_t pteFrame = u4 & PTE_FRAME_MASK;

				if (pteFrame != pfn)
					continue;

				uint64_t candidateCr3 = pfn << 12;

				uint16_t magic = 0;
				if (hypercall::read_guest_virtual_memory(&magic, base_address, candidateCr3, sizeof(magic)) == sizeof(magic) &&
					magic == IMAGE_DOS_SIGNATURE)
				{
					return candidateCr3;
				}
			}
		}

		return 0;
	}

	namespace
	{
		struct handle_data {
			unsigned long process_id;
			HWND window_handle;
		};

		BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam) {
			handle_data& data = *(handle_data*)lParam;
			unsigned long process_id = 0;
			GetWindowThreadProcessId(handle, &process_id);
			if (data.process_id != process_id || !IsWindowVisible(handle))
				return TRUE;
			data.window_handle = handle;
			return FALSE;
		}

		HWND find_main_window(unsigned long process_id) {
			handle_data data;
			data.process_id = process_id;
			data.window_handle = 0;
			EnumWindows(enum_windows_callback, (LPARAM)&data);
			return data.window_handle;
		}
	}

	void create_instance(std::string_view target_process)
	{
		VMP_START("create_instance");
		if (!isyscall::load())
		{
			std::println("failed to initialize isyscall");
			VMP_END;
			return;
		}

		if (!sys::set_up())
		{
			VMP_END;
			return;
		}

		sys::kernel::parse_modules();
		sys::user::parse_processes();

		std::println("searching for rust");

		while (impl::base_address == 0)
		{
			sys::user::parse_processes();

			if (!sys::user::processes_list)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			auto it = sys::user::processes_list->find(std::string(target_process));
			if (it == sys::user::processes_list->end())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			const auto& process = it->second;
			impl::base_address = process.base_address;
			impl::process_peb = process.peb;
			impl::process_id = process.pid;
		}

		std::println("found rust");

		while (impl::cr3 == 0)
		{
			impl::cr3 = bruteforce_cr3(impl::base_address);
		}

		while (impl::game_hwnd == nullptr)
		{
			impl::game_hwnd = find_main_window(impl::process_id);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (impl::process_peb)
		{
			uptr process_parameters = memory::read<uptr>(impl::process_peb + 0x20);
			if (process_parameters)
			{
				uptr string_buffer = memory::read<uptr>(process_parameters + 0x60 + 0x8);
				unsigned short string_len = memory::read<unsigned short>(process_parameters + 0x60);

				if (string_buffer && string_len)
				{
					std::wstring full_path = memory::read_wstring<2>(string_buffer, string_len / 2);
					size_t pos = full_path.find_last_of(L"\\/");
					if (pos != std::wstring::npos)
					{
						std::wstring dir = full_path.substr(0, pos);
						impl::game_directory = std::string(dir.begin(), dir.end());
						std::println("game directory: {}", impl::game_directory);
					}
				}
			}
		}

		std::println("found game. process id: {}, window: {:p}", impl::process_id, (void*)impl::game_hwnd);

		on_frame();
		VMP_END;
	}


	struct pattern_byte
	{
		uint8_t value;
		bool wildcard;
	};

	static std::vector<pattern_byte> parse_ida_pattern(const std::string& sig)
	{
		std::vector<pattern_byte> result;
		std::istringstream ss(sig);
		std::string token;
		while (ss >> token)
		{
			if (token == "?")
				result.push_back({ 0, true });
			else
				result.push_back({ static_cast<uint8_t>(strtoul(token.c_str(), nullptr, 16)), false });
		}
		return result;
	}

	std::uintptr_t pattern_scan(std::uintptr_t module_base, const std::string& signature, uintptr_t start_from)
	{
		auto pattern = parse_ida_pattern(signature);
		if (pattern.empty()) return 0;

		uint8_t header[0x1000]{};
		if (!read_memory_raw(module_base, header, sizeof(header)))
			return 0;

		auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(header);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;

		auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(header + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

		auto* sections = IMAGE_FIRST_SECTION(nt);

		for (WORD s = 0; s < nt->FileHeader.NumberOfSections; s++)
		{
			bool is_readable = (sections[s].Characteristics & IMAGE_SCN_MEM_READ) != 0;
			if (!is_readable)
				continue;

			uptr sec_rva = sections[s].VirtualAddress;
			uptr sec_size = sections[s].Misc.VirtualSize;

			constexpr size_t PAGE_SIZE = 0x1000;
			const size_t carry_size = pattern.size() - 1;
			std::vector<uint8_t> buffer(PAGE_SIZE + carry_size);
			size_t carried = 0;

			uptr start_offset = (start_from > (module_base + sec_rva)) ? (start_from - (module_base + sec_rva)) : 0;
			if (start_offset >= sec_size) continue;

			for (uptr offset = start_offset; offset < sec_size; offset += PAGE_SIZE)
			{
				size_t read_size = std::min<size_t>(PAGE_SIZE, sec_size - offset);
				uptr addr = module_base + sec_rva + offset;

				if (!read_memory_raw(addr, buffer.data() + carried, read_size))
				{
					carried = 0;
					continue;
				}

				size_t total = carried + read_size;

				for (size_t i = 0; i + pattern.size() <= total; i++)
				{
					bool found = true;
					for (size_t j = 0; j < pattern.size(); j++)
					{
						if (!pattern[j].wildcard && buffer[i + j] != pattern[j].value)
						{
							found = false;
							break;
						}
					}
					if (found)
						return addr - carried + i;
				}

				if (total >= carry_size)
				{
					memmove(buffer.data(), buffer.data() + total - carry_size, carry_size);
					carried = carry_size;
				}
				else
				{
					carried = total;
				}
			}
		}

		return 0;
	}

	std::uintptr_t resolve_lea(std::uintptr_t lea_address)
	{
		int32_t disp = read<int32_t>(lea_address + 3);
		return lea_address + 7 + disp;
	}

	void on_frame()
	{
	}

	bool read_memory_raw(const std::uintptr_t& address, void* buffer, std::size_t size)
	{
		std::size_t read_size = hypercall::read_guest_virtual_memory(reinterpret_cast<void*>(buffer), address, impl::cr3, size);
		return size == read_size;
	}

	bool write_memory_raw(const std::uintptr_t& address, void* buffer, std::size_t size)
	{
		std::size_t write_size = hypercall::write_guest_virtual_memory(reinterpret_cast<void*>(buffer), address, impl::cr3, size);
		return size == write_size;
	}

	std::uintptr_t get_base_address()
	{
		return impl::base_address;
	}

	std::string get_game_directory()
	{
		return impl::game_directory;
	}

	std::pair<std::uintptr_t, std::size_t> get_module_easy(std::wstring_view name)
	{
		if (!impl::process_id)
			return {};

		const HANDLE snap = ::CreateToolhelp32Snapshot(
			TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
			static_cast<DWORD>(impl::process_id));

		if (snap == INVALID_HANDLE_VALUE)
			return {};

		MODULEENTRY32W entry{};
		entry.dwSize = sizeof(entry);

		std::pair<std::uintptr_t, std::size_t> result{};
		for (BOOL ok = ::Module32FirstW(snap, &entry); ok; ok = ::Module32NextW(snap, &entry))
		{
			if (::_wcsicmp(entry.szModule, name.data()) == 0)
			{
				result = { reinterpret_cast<std::uintptr_t>(entry.modBaseAddr),
					static_cast<std::size_t>(entry.modBaseSize) };
				break;
			}
		}

		::CloseHandle(snap);
		return result;
	}


	std::pair<std::uintptr_t, std::size_t> get_module(std::wstring_view name)
	{
		if (!impl::process_id || !impl::process_peb || !impl::cr3)
			return {};

		uptr ldr = read<uptr>(impl::process_peb + 0x18);
		if (!ldr) return {};

		uptr list_head = ldr + 0x10;
		uptr current_entry = read<uptr>(list_head);

		while (current_entry != list_head)
		{
			uptr base = read<uptr>(current_entry + 0x30);
			uptr size = read<uptr>(current_entry + 0x40);

			uptr name_ptr = read<uptr>(current_entry + 0x58 + 8); // Buffer of BaseDllName
			unsigned short name_len = read<unsigned short>(current_entry + 0x58); // Length of BaseDllName

			if (name_ptr && name_len)
			{
				std::wstring name_buf = read_wstring<2>(name_ptr, name_len / 2);
				if (_wcsicmp(name_buf.c_str(), name.data()) == 0)
				{
					return { base, size };
				}
			}

			current_entry = read<uptr>(current_entry);
			if (!current_entry) break;
		}

		return {};
	}
}
