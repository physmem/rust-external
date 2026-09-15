#pragma once

#include <cstdint>
#include <cstring>
#include <string>

using fnv1a_t = std::uint64_t;

namespace fnv
{
    constexpr fnv1a_t ull_basis = 0xCBF29CE484222325ULL;
    constexpr fnv1a_t ull_prime = 0x100000001B3ULL;

    consteval fnv1a_t hash_const(const char* str, const fnv1a_t key = ull_basis) noexcept
    {
        return (str[0] == '\0') ? key : hash_const(&str[1], (key ^ static_cast<fnv1a_t>(str[0])) * ull_prime);
    }

    inline fnv1a_t hash(std::string_view str, fnv1a_t key = ull_basis) noexcept
    {
        for (std::size_t i = 0U; i < str.size(); ++i) {
            key ^= str[i];
            key *= ull_prime;
        }

        return key;
    }

    inline fnv1a_t hash(const char* str, fnv1a_t key = ull_basis) noexcept
    {
        return hash(std::string_view(str), key);
    }
} // namespace fnv
