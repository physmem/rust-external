#pragma once
#include "system_def.hpp"
#include <optional>
#include <string>
#include <unordered_map>

namespace sys
{
    struct kernel_module_t;
    struct user_process_t;

    std::uint8_t set_up();
    std::uint8_t init_eprocess_offsets();
    void clean_up();

    namespace kernel
    {
        std::uint8_t parse_modules();

        inline std::unordered_map<std::string, kernel_module_t>* modules_list = new std::unordered_map<std::string, kernel_module_t>();
    } // namespace kernel

    namespace user
    {
        std::uint32_t query_system_information(std::int32_t information_class, void *information_out, std::uint32_t information_size,
                                               std::uint32_t *returned_size);

        std::uint32_t adjust_privilege(std::uint32_t privilege, std::uint8_t enable, std::uint8_t current_thread_only,
                                       std::uint8_t *previous_enabled_state);
        std::uint8_t set_debug_privilege(std::uint8_t state, std::uint8_t *previous_state);

        void *allocate_locked_memory(std::uint64_t size, std::uint32_t protection);
        std::uint8_t free_memory(void *address);

        std::string to_string(const std::wstring &wstring);

        std::uint8_t parse_processes();

        inline std::unordered_map<std::string, user_process_t>* processes_list = new std::unordered_map<std::string, user_process_t>();
    } // namespace user

    namespace fs
    {
        std::uint8_t exists(std::string_view path);

    } // namespace fs

    struct kernel_module_t
    {
        std::unordered_map<std::string, std::uint64_t>* exports = nullptr;

        std::uint64_t base_address = 0;
        std::uint32_t size = 0;

        kernel_module_t() = default;
        ~kernel_module_t() { if (exports) delete exports; exports = nullptr; }

        kernel_module_t(const kernel_module_t& other) : base_address(other.base_address), size(other.size) {
            if (other.exports) exports = new std::unordered_map<std::string, std::uint64_t>(*other.exports);
        }
        kernel_module_t& operator=(const kernel_module_t& other) {
            if (this == &other) return *this;
            if (exports) delete exports;
            base_address = other.base_address;
            size = other.size;
            if (other.exports) exports = new std::unordered_map<std::string, std::uint64_t>(*other.exports);
            return *this;
        }
        kernel_module_t(kernel_module_t&& other) noexcept : exports(other.exports), base_address(other.base_address), size(other.size) {
            other.exports = nullptr;
        }
        kernel_module_t& operator=(kernel_module_t&& other) noexcept {
            if (this == &other) return *this;
            if (exports) delete exports;
            exports = other.exports;
            base_address = other.base_address;
            size = other.size;
            other.exports = nullptr;
            return *this;
        }
    };

    struct user_process_t
    {
        std::uint64_t cr3;
        std::uint64_t bruteforced_cr3;
        std::uint64_t base_address;
        std::uint64_t pid;
        std::uint64_t peb;
        std::uint64_t eprocess;
    };

    inline std::uint64_t current_cr3 = 0;
    inline eprocess_offsets_t eprocess_offsets = {};
} // namespace sys
