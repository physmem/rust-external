#pragma once
#include <algorithm>
#include <cstdint>
#include <format>
#include <intrin.h>
#include <string>
#include <type_traits>

union Color;
struct Hsv
{
    float h, s, v = 0.0f;

    Color to_color();
};

union Color
{
    Color() : r(255), g(255), b(255), a(255) {}

    Color(std::uint32_t col) : packed(_byteswap_ulong(col)) {}

    template<typename T>
        requires std::is_integral_v<T>
    constexpr Color(const T& r, const T& g, const T& b, const T& a = 255) : r(r), g(g), b(b), a(a)
    {
    }

    template<typename T, typename T2>
        requires std::is_integral_v<T2>
    constexpr Color(const T& col, const T2& a = 255) : r(col.r), g(col.g), b(col.b), a(a)
    {
    }

    template<typename T, typename T2, typename T3, typename T4>
        requires std::is_integral_v<T>&& std::is_integral_v<T2>&& std::is_integral_v<T3>&& std::is_integral_v<T4>
    constexpr Color(const T& r, const T2& g, const T3& b, const T4& a = 255) : r(r), g(g), b(b), a(a)
    {
    }

    template<typename T, typename T2>
        requires std::is_integral_v<T>&& std::is_floating_point_v<T2>
    constexpr Color(const T& r, const T& g, const T& b, const T2& a = 1.f) : r(r), g(g), b(b), a(static_cast<std::uint8_t>(a * 255.f))
    {
    }

    template<typename T, typename T2>
        requires std::is_floating_point_v<T2>
    constexpr Color(const T& col, const T2& a = 1.f) : r(col.r), g(col.g), b(col.b), a(static_cast<std::uint8_t>(a * 255.f))
    {
    }

    constexpr operator unsigned int() const
    {
        return packed;
    }

    Color& operator=(const Color& other)
    {
        packed = other.packed;
        return *this;
    }

    std::uint32_t packed;

    struct {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
        std::uint8_t a;
    };

    template<typename T>
        requires std::is_integral_v<T>
    Color operator+(const T& val) const
    {
        return Color(
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(r) + static_cast<int>(val), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(g) + static_cast<int>(val), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(b) + static_cast<int>(val), 0, 255)),
            a
        );
    }

    template<typename T>
        requires std::is_integral_v<T>
    Color operator-(const T& val) const
    {
        return Color(
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(r) - static_cast<int>(val), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(g) - static_cast<int>(val), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(b) - static_cast<int>(val), 0, 255)),
            a
        );
    }

    Color scale_alpha(float alpha) const
    {
        return Color(r, g, b, static_cast<uint8_t>(std::clamp((a / 255.0f) * alpha * 255.0f, 0.0f, 255.0f)));
    }

    Color override_alpha(float alpha) const
    {
        return Color(r, g, b, static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f)));
    }

    float scalable_alpha() const
    {
        return static_cast<float>(a) / 255.0f;
    }

    Color lerp(const Color& to, float fraction) const
    {
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        // std::lerp has bound checks we don't need here, this is faster
        return Color(static_cast<int>((to.r - r) * fraction + r), static_cast<int>((to.g - g) * fraction + g),
            static_cast<int>((to.b - b) * fraction + b), static_cast<int>((to.a - a) * fraction + a));
    }

    Color invert() const
    {
        Color res = *this;
        res.packed ^= 0x00FFFFFF;
        return res;
    }

    Hsv to_hsv() const;

    std::string to_hex() const
    {
        return std::format("#{:02x}{:02x}{:02x}{:02x}", r, g, b, a);
    }

    static Color from_hex(const std::string& hex)
    {
        if (hex.size() == 7 || hex.size() == 9) // #RRGGBB or #RRGGBBAA
        {
            std::uint32_t col = std::stoul(hex.substr(1), nullptr, 16);
            if (hex.size() == 9) // #RRGGBBAA
            {
                return Color(col);
            }
            else // #RRGGBB
            {
                return Color(col | 0xFF000000);
            }
        }
        return Color::white();
    }

    static Color from_hsv(float h, float s, float v)
    {
        return Hsv{ h, s, v }.to_color();
    }

    constexpr static Color white()
    {
        return { 255, 255, 255 };
    }

    constexpr static Color black()
    {
        return { 0, 0, 0 };
    }

    constexpr static Color red()
    {
        return { 255, 0, 0 };
    }

    constexpr static Color green()
    {
        return { 0, 255, 0 };
    }

    constexpr static Color blue()
    {
        return { 0, 0, 255 };
    }

    constexpr static Color yellow()
    {
        return { 255, 255, 0 };
    }

    constexpr static Color pink()
    {
        return { 255, 0, 255 };
    }

    constexpr static Color cyan()
    {
        return { 0, 255, 255 };
    }
};
