#include "system.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "../hook/hook.hpp"
#include "../hypercall.hpp"

#include "../portable_executable/image.hpp"

#include <print>
#include <string_encryption.hpp>

#include <Windows.h>
#include <intrin.h>
#include <print>
#include <vector>
#include <winternl.h>

// EPROCESS offsets per Windows build range.
// Source: https://www.vergiliusproject.com/kernels/x64
// DirectoryTableBase is always KPROCESS+0x28 (EPROCESS+0x28) across all versions.
//
//                                                      DTB    Exit    PID     Links   Base    Peb     SeAudit
static constexpr eprocess_offsets_t eprocess_win10_1507 = { 0x028, 0x670, 0x2E8, 0x2F0, 0x3C0, 0x3F8, 0x460 }; // Build 10240
static constexpr eprocess_offsets_t eprocess_win10_1511 = { 0x028, 0x678, 0x2E8, 0x2F0, 0x3C0, 0x3F8, 0x468 }; // Build 10586
static constexpr eprocess_offsets_t eprocess_win10_1607 = { 0x028, 0x688, 0x2E8, 0x2F0, 0x3C0, 0x3F8, 0x468 }; // Build 14393
static constexpr eprocess_offsets_t eprocess_win10_1703 = { 0x028, 0x690, 0x2E0, 0x2E8, 0x3C0, 0x3F8, 0x468 }; // Build 15063
static constexpr eprocess_offsets_t eprocess_win10_1709 = { 0x028, 0x690, 0x2E0, 0x2E8, 0x3C0, 0x3F8, 0x468 }; // Build 16299 (also 1803=17134, 1809=17763)
static constexpr eprocess_offsets_t eprocess_win10_1903 = { 0x028, 0x6C0, 0x2E8, 0x2F0, 0x3C8, 0x3F8, 0x468 }; // Build 18362 (also 1909=18363)
static constexpr eprocess_offsets_t eprocess_win10_2004 = { 0x028, 0x840, 0x440, 0x448, 0x520, 0x550, 0x5C0 }; // Build 19041-19045 (also Win11 21H2-23H2)
static constexpr eprocess_offsets_t eprocess_win11_24H2 = { 0x028, 0x5C0, 0x1D0, 0x1D8, 0x2B0, 0x2E0, 0x350 }; // Build 26100+ (24H2, 25H2)

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread,
	std::uint8_t* previous_enabled_state);

std::vector<std::uint8_t> dump_kernel_module(std::uint64_t module_base_address)
{
	constexpr std::uint64_t headers_size = 0x1000;

	std::vector<std::uint8_t> headers(headers_size);

	std::uint64_t bytes_read = hypercall::read_guest_virtual_memory(headers.data(), module_base_address, sys::current_cr3, headers_size);

	if (bytes_read != headers_size)
	{
		return {};
	}

	std::uint16_t magic = *reinterpret_cast<std::uint16_t*>(headers.data());

	if (magic != 0x5a4d)
	{
		return {};
	}

	const portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(headers.data());

	std::vector<std::uint8_t> image_buffer(image->nt_headers()->optional_header.size_of_image);

	memcpy(image_buffer.data(), headers.data(), 0x1000);

	for (const auto& current_section : image->sections())
	{
		std::uint64_t read_offset = current_section.virtual_address;
		std::uint64_t read_size = current_section.virtual_size;

		hypercall::read_guest_virtual_memory(image_buffer.data() + read_offset, module_base_address + read_offset, sys::current_cr3, read_size);
	}

	return image_buffer;
}

std::uint64_t find_kernel_detour_holder_base_address(portable_executable::image_t* ntoskrnl, std::uint64_t ntoskrnl_base_address)
{
	for (const auto& current_section : ntoskrnl->sections())
	{
		std::string_view current_section_name(current_section.name);

		if (current_section_name.contains("Pad") == true && current_section.characteristics.mem_execute == 1)
		{
			return ntoskrnl_base_address + current_section.virtual_address;
		}
	}

	return 0;
}

std::unordered_map<std::string, std::uint64_t>* parse_module_exports(const portable_executable::image_t* image, const std::string& module_name,
	const std::uint64_t module_base_address)
{
	auto exports = new std::unordered_map<std::string, std::uint64_t>();

	for (const auto& current_export : image->exports())
	{
		std::string current_export_name = module_name + "!" + current_export.name;

		std::uint64_t delta = reinterpret_cast<std::uint64_t>(current_export.address) - image->as<std::uint64_t>();

		(*exports)[current_export_name] = module_base_address + delta;
	}

	return exports;
}

void add_module_to_list(const std::string& module_name, const std::vector<std::uint8_t>& module_dump, const std::uint64_t module_base_address,
	const std::uint32_t module_size)
{
	sys::kernel_module_t kernel_module = {};

	const portable_executable::image_t* image = reinterpret_cast<const portable_executable::image_t*>(module_dump.data());

	kernel_module.exports = parse_module_exports(image, module_name, module_base_address);
	kernel_module.base_address = module_base_address;
	kernel_module.size = module_size;

	if (sys::kernel::modules_list)
		(*sys::kernel::modules_list)[module_name] = kernel_module;
}

void erase_unused_modules(const std::unordered_map<std::string, sys::kernel_module_t>& modules_not_found)
{
	for (const auto& [module_name, module_info] : modules_not_found)
	{
		if (sys::kernel::modules_list)
			sys::kernel::modules_list->erase(module_name);
	}
}

// requires SeDebugPriviledge, use PsLoadedModulesList instead unless if using before ntoskrnl.exe is parsed
std::vector<rtl_process_module_information_t> get_loaded_modules_priviledged()
{
	std::uint32_t size_of_information = 0;

	sys::user::query_system_information(11, nullptr, 0, &size_of_information);

	if (size_of_information == 0)
	{
		return {};
	}

	std::vector<std::uint8_t> buffer(size_of_information);

	std::uint32_t status = sys::user::query_system_information(11, buffer.data(), size_of_information, &size_of_information);

	if (NT_SUCCESS(status) == false)
	{
		return {};
	}

	rtl_process_modules_t* process_modules = reinterpret_cast<rtl_process_modules_t*>(buffer.data());

	rtl_process_module_information_t* start = &process_modules->modules[0];
	rtl_process_module_information_t* end = start + process_modules->module_count;

	return { start, end };
}

template <class t>
t read_kernel_virtual_memory(std::uint64_t address)
{
	t buffer = t();

	hypercall::read_guest_virtual_memory(&buffer, address, sys::current_cr3, sizeof(t));

	return buffer;
}

std::wstring read_unicode_string(std::uint64_t address)
{
	std::uint16_t length = read_kernel_virtual_memory<std::uint16_t>(address);

	if (length == 0)
	{
		return {};
	}

	std::uint64_t buffer_address = read_kernel_virtual_memory<std::uint64_t>(address + 8);

	std::wstring string(length / 2, L'\0');

	hypercall::read_guest_virtual_memory(string.data(), buffer_address, sys::current_cr3, length);

	return string;
}

std::uint64_t get_ps_loaded_module_list()
{
	const std::string ntoskrnl_name = "ntoskrnl.exe";

	if (sys::kernel::modules_list->contains(ntoskrnl_name) == 0)
	{
		return 0;
	}

	sys::kernel_module_t& ntoskrnl = (*sys::kernel::modules_list)[ntoskrnl_name];

	const std::string ps_loaded_module_list_name = "ntoskrnl.exe!PsLoadedModuleList";

	if (!ntoskrnl.exports)
		return 0;

	return (*ntoskrnl.exports)[ps_loaded_module_list_name];
}

std::uint8_t sys::kernel::parse_modules()
{
	const std::uint64_t ps_loaded_module_list = get_ps_loaded_module_list();

	if (ps_loaded_module_list == 0)
	{
		std::println("can't locate PsLoadedModuleList");

		return 0;
	}

	if (!modules_list)
		return 0;

	std::unordered_map<std::string, kernel_module_t> modules_not_found = *modules_list;

	const std::uint64_t start_entry = ps_loaded_module_list;

	std::uint64_t current_entry = read_kernel_virtual_memory<std::uint64_t>(start_entry); // flink

	while (current_entry != start_entry)
	{
		kernel_module_t kernel_module = {};

		std::uint64_t module_base_address = read_kernel_virtual_memory<std::uint64_t>(current_entry + 0x30); // DllBase
		std::uint32_t module_size = read_kernel_virtual_memory<std::uint32_t>(current_entry + 0x40);         // SizeOfImage
		std::string module_name = user::to_string(read_unicode_string(current_entry + 0x58));                // BaseDllName

		// current_entry must not be accessed after this point in this iteration
		current_entry = read_kernel_virtual_memory<std::uint64_t>(current_entry); // flink

		if (modules_list->contains(module_name) == true)
		{
			modules_not_found.erase(module_name);

			const kernel_module_t already_present_module = (*modules_list)[module_name];

			if (already_present_module.base_address == module_base_address && already_present_module.size == module_size)
			{
				continue;
			}
		}

		std::vector<std::uint8_t> module_dump = dump_kernel_module(module_base_address);

		if (module_dump.empty() == true)
		{
			continue;
		}

		add_module_to_list(module_name, module_dump, module_base_address, module_size);
	}

	erase_unused_modules(modules_not_found);

	return 1;
}

void fix_dump(std::vector<std::uint8_t>& buffer)
{
	portable_executable::image_t* image = reinterpret_cast<portable_executable::image_t*>(buffer.data());

	for (auto& current_section : image->sections())
	{
		current_section.pointer_to_raw_data = current_section.virtual_address;
		current_section.size_of_raw_data = current_section.virtual_size;
	}
}

struct ntoskrnl_information_t
{
	std::uint64_t base_address;
	std::uint32_t size;

	std::vector<std::uint8_t> dump;
};

std::optional<ntoskrnl_information_t> load_ntoskrnl_information()
{
	std::uint8_t desired_privilege_state = 1;
	std::uint8_t previous_privilege_state = 0;

	if (sys::user::set_debug_privilege(desired_privilege_state, &previous_privilege_state) == 0)
	{
		std::println("unable to acquire necessary privilege");

		return std::nullopt;
	}

	const std::vector<rtl_process_module_information_t> loaded_modules = get_loaded_modules_priviledged();

	sys::user::set_debug_privilege(previous_privilege_state, &desired_privilege_state);

	for (const rtl_process_module_information_t& current_module : loaded_modules)
	{
		std::string_view current_module_name = reinterpret_cast<const char*>(current_module.full_path_name + current_module.offset_to_file_name);

		if (current_module_name == "ntoskrnl.exe")
		{
			std::vector<std::uint8_t> ntoskrnl_dump = dump_kernel_module(current_module.image_base);

			if (ntoskrnl_dump.empty() == true)
			{
				std::println("unable to dump ntoskrnl.exe");

				return std::nullopt;
			}

			ntoskrnl_information_t ntoskrnl_info = {};

			ntoskrnl_info.base_address = current_module.image_base;
			ntoskrnl_info.size = current_module.image_size;
			ntoskrnl_info.dump = ntoskrnl_dump;

			return ntoskrnl_info;
		}
	}

	return std::nullopt;
}

std::uint8_t parse_ntoskrnl()
{
	std::optional<ntoskrnl_information_t> ntoskrnl_info = load_ntoskrnl_information();

	if (ntoskrnl_info.has_value() == 0)
	{
		std::println("unable to load ntoskrnl.exe's information");

		return 0;
	}

	std::vector<std::uint8_t>& ntoskrnl_dump = ntoskrnl_info->dump;

	portable_executable::image_t* ntoskrnl_image = reinterpret_cast<portable_executable::image_t*>(ntoskrnl_dump.data());

	add_module_to_list("ntoskrnl.exe", ntoskrnl_dump, ntoskrnl_info->base_address, ntoskrnl_info->size);

	// Initialize kernel hook detour holder from ntoskrnl's Pad section
	std::uint64_t detour_base = find_kernel_detour_holder_base_address(ntoskrnl_image, ntoskrnl_info->base_address);

	if (detour_base != 0)
	{
		hook::set_kernel_detour_holder_base(detour_base);
		std::println("kernel detour holder base: {:#x}", detour_base);
	}
	else
	{
		std::println("could not locate kernel detour holder (Pad section not found)");
	}

	return 1;
}

std::uint8_t sys::init_eprocess_offsets()
{
	OSVERSIONINFOEXW ovi = { sizeof(ovi) };

	using RtlGetVersionFn = NTSTATUS(NTAPI*)(PRTL_OSVERSIONINFOW);
	const auto RtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
		GetProcAddress(GetModuleHandleA(xs("ntdll.dll")), xs("RtlGetVersion")));

	if (!RtlGetVersion || !NT_SUCCESS(RtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&ovi))))
	{
		return 0;
	}

	const std::uint32_t build = ovi.dwBuildNumber;

	if (build >= 26100)          // Win11 24H2 / 25H2+
		eprocess_offsets = eprocess_win11_24H2;
	else if (build >= 19041)     // Win10 2004-22H2, Win11 21H2-23H2
		eprocess_offsets = eprocess_win10_2004;
	else if (build >= 18362)     // Win10 1903-1909
		eprocess_offsets = eprocess_win10_1903;
	else if (build >= 15063)     // Win10 1703-1809
		eprocess_offsets = (build >= 16299) ? eprocess_win10_1709 : eprocess_win10_1703;
	else if (build >= 14393)     // Win10 1607
		eprocess_offsets = eprocess_win10_1607;
	else if (build >= 10586)     // Win10 1511
		eprocess_offsets = eprocess_win10_1511;
	else if (build >= 10240)     // Win10 1507
		eprocess_offsets = eprocess_win10_1507;
	else
	{
#if !defined(STABLE)
		std::println(xs("unsupported Windows build: {}"), build);
#endif
		return 0;
	}
#if !defined(STABLE)
	std::println(xs("Windows build {} | EPROCESS offsets: PID={:#X} Links={:#X} Base={:#X} PEB={:#X}"),
		build, eprocess_offsets.unique_process_id, eprocess_offsets.active_process_links,
		eprocess_offsets.section_base_address, eprocess_offsets.peb);
#endif
	return 1;
}

std::uint8_t sys::set_up()
{
	if (init_eprocess_offsets() == 0)
	{
		return 0;
	}

	current_cr3 = hypercall::read_guest_cr3();

	if (current_cr3 == 0)
	{
		return 0;
	}

	if (parse_ntoskrnl() == 0)
	{
		return 0;
	}

	if (kernel::parse_modules() == 0)
	{
		return 0;
	}

	return 1;
}

void sys::clean_up()
{
	hook::clean_up();
}

std::uint32_t sys::user::query_system_information(std::int32_t information_class, void* information_out, std::uint32_t information_size,
	std::uint32_t* returned_size)
{
	return NtQuerySystemInformation(static_cast<SYSTEM_INFORMATION_CLASS>(information_class), information_out, information_size,
		reinterpret_cast<ULONG*>(returned_size));
}

std::uint32_t sys::user::adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only,
	std::uint8_t* previous_enabled_state)
{
	return RtlAdjustPrivilege(privilege, enable, current_thread_only, previous_enabled_state);
}

std::uint8_t sys::user::set_debug_privilege(const std::uint8_t state, std::uint8_t* previous_state)
{
	constexpr std::uint32_t debug_privilege_id = 20;

	std::uint32_t status = adjust_privilege(debug_privilege_id, state, 0, previous_state);

	return NT_SUCCESS(status);
}

void* sys::user::allocate_locked_memory(std::uint64_t size, std::uint32_t protection)
{
	void* allocation_base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protection);

	if (allocation_base == nullptr)
	{
		return nullptr;
	}

	std::int32_t lock_status = VirtualLock(allocation_base, size);

	if (lock_status == 0)
	{
		free_memory(allocation_base);

		return nullptr;
	}

	return allocation_base;
}

std::uint8_t sys::user::free_memory(void* address)
{
	std::int32_t free_status = VirtualFree(address, 0, MEM_RELEASE);

	return free_status != 0;
}

std::string sys::user::to_string(const std::wstring& wstring)
{
	if (wstring.empty() == 1)
	{
		return {};
	}

	std::string converted_string = {};

	std::ranges::transform(wstring, std::back_inserter(converted_string), [](wchar_t character) { return static_cast<char>(character); });

	return converted_string;
}

std::uint64_t get_ps_initial_system_process()
{
	const std::string ntoskrnl_name = "ntoskrnl.exe";

	if (sys::kernel::modules_list->contains(ntoskrnl_name) == 0)
	{
		return 0;
	}

	sys::kernel_module_t& ntoskrnl = (*sys::kernel::modules_list)[ntoskrnl_name];

	const std::string ps_initial_system_process_name = "ntoskrnl.exe!PsInitialSystemProcess";

	if (!ntoskrnl.exports)
		return 0;

	return (*ntoskrnl.exports)[ps_initial_system_process_name];
}

std::string truncate_process_path(const std::string& path)
{
	std::size_t pos = path.find_last_of("\\/");
	if (pos != std::string::npos)
		return path.substr(pos + 1);

	// no slash found, return the whole string
	return path;
}

std::uint8_t sys::user::parse_processes()
{
	const std::uint64_t ps_initial_system_process = get_ps_initial_system_process();

	if (ps_initial_system_process == 0)
	{
		return 0;
	}

	const auto& o = eprocess_offsets;

	std::uint64_t current_entry = read_kernel_virtual_memory<std::uint64_t>(ps_initial_system_process);
	const std::uint64_t start_entry = current_entry;

	constexpr int max_processes = 1024;
	int count = 0;

	do
	{
		if (++count > max_processes)
			break;

		// Advance to next entry via active_process_links.
		// Flink points to the next EPROCESS's active_process_links field.
		std::uint64_t flink = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.active_process_links);

		if (!flink)
			break;

		std::uint64_t next_entry = flink - o.active_process_links;

		// Skip exited processes
		const auto exit_time = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.exit_time);

		if (exit_time != 0)
		{
			current_entry = next_entry;
			continue;
		}

		user_process_t user_process = {};

		std::uint64_t cr3 = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.directory_table_base);
		std::uint64_t pid = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.unique_process_id);
		std::uint64_t base = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.section_base_address);
		std::uint64_t peb = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.peb);

		std::uint64_t image_file_name_ptr = read_kernel_virtual_memory<std::uint64_t>(current_entry + o.se_audit_process_creation_info);
		std::string image_path = user::to_string(read_unicode_string(image_file_name_ptr));

		std::string process_name = truncate_process_path(image_path);

		if (base != 0 && process_name.empty() == false && sys::user::processes_list)
		{
			user_process.cr3 = cr3;
			user_process.base_address = base;
			user_process.pid = pid;
			user_process.peb = peb;
			user_process.eprocess = current_entry;

			sys::user::processes_list->insert_or_assign(process_name, user_process);
		}

		current_entry = next_entry;
	} while (current_entry != start_entry);

	return 1;
}


std::uint8_t sys::fs::exists(std::string_view path)
{
	return std::filesystem::exists(path);
}