#pragma once
#include <cstdint>

struct rtl_process_module_information_t
{
    std::uint64_t section;
    std::uint64_t mapped_base;
    std::uint64_t image_base;
    std::uint32_t image_size;
    std::uint32_t flags;
    std::uint16_t load_order_index;
    std::uint16_t init_order_index;
    std::uint16_t load_count;
    std::uint16_t offset_to_file_name;
    std::uint8_t full_path_name[256];
};

struct rtl_process_modules_t
{
    std::uint32_t module_count;
    rtl_process_module_information_t modules[1];
};

struct eprocess_offsets_t
{
    std::uint32_t directory_table_base;
    std::uint32_t exit_time;
    std::uint32_t unique_process_id;
    std::uint32_t active_process_links;
    std::uint32_t section_base_address;
    std::uint32_t peb;
    std::uint32_t se_audit_process_creation_info;
};
