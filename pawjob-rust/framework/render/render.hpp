#pragma once
#include "../base/hash/fnv.hpp"
#include <array>
#include <cstdint>
#include <glm/vec2.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#define IM_VEC2_CLASS_EXTRA                                                                                                                          \
	constexpr ImVec2( const glm::vec2& f ) : x( f.x ), y( f.y ) { }                                                                                     \
	operator glm::vec2( ) const                                                                                                                         \
	{                                                                                                                                                \
		return glm::vec2( x, y );                                                                                                                       \
	}    

#include <imgui.h>
#include <string_view>
#include <unordered_map>
#include <utils/color.hpp>
#include <vector>
#include <functional>
#include "texture/texture.hpp"

namespace render
{
	struct FontId
	{
		fnv1a_t hash{};
		std::uint8_t* data{};
		std::size_t data_size{};
		float size{};

		consteval FontId(const char* name, std::uint8_t* p_data, std::size_t p_data_size, float p_size)
			: hash(fnv::hash_const(name)), data(p_data), data_size(p_data_size), size(p_size) {
		}

		consteval FontId(const char* name, float p_size)
			: hash(fnv::hash_const(name)), data(nullptr), data_size(0), size(p_size) {
		}

		constexpr bool operator==(const FontId& other) const {
			return hash == other.hash;
		}
	};

	namespace Fonts
	{
		inline constexpr FontId NotoSans10px{ "NotoSans10px", 10.0f };
		inline constexpr FontId NotoSans12px{ "NotoSans12px", 12.0f };
		inline constexpr FontId NotoSans14px{ "NotoSans14px", 14.0f };
		inline constexpr FontId NotoSans16px{ "NotoSans16px", 16.0f };
		inline constexpr FontId NotoSans18px{ "NotoSans18px", 18.0f };
		inline constexpr FontId NotoSans20px{ "NotoSans20px", 20.0f };
		inline constexpr FontId Arial14px{ "Arial14px", 14.0f };
		inline constexpr FontId Pixelmix10px{ "Pixelmix10px", 10.0f };
		inline constexpr FontId RoundedElegance24px{ "RoundedElegance24px", 24.0f };
		inline constexpr FontId Icons16px{ "Icons16px", 16.0f };
		inline constexpr FontId Icons20px{ "Icons20px", 20.0f };
	}

	void setup();

	enum TextFlags
	{
		TextFlagsNone = (1 << 0),
		TextFlagsDropShadow = (1 << 1),
		TextFlagsOutline = (1 << 2),
		TextFlagsMAX = (1 << 3)
	};

	enum CornerFlags : std::int32_t
	{
		None = 0,
		TopLeft = 1 << 4,
		TopRight = 1 << 5,
		BottomLeft = 1 << 6,
		BottomRight = 1 << 7,
		All = TopLeft | TopRight | BottomLeft | BottomRight,
		Top = TopLeft | TopRight,
		Bottom = BottomLeft | BottomRight,
		Left = TopLeft | BottomLeft,
		Right = TopRight | BottomRight
	};

	typedef ImFont Font;
	inline ImDrawList* draw_list = nullptr;
	// 582x450 * 2
	inline texture_t menu_bg{};

	// 270x25 * 2
	inline texture_t container_header{};

	// 80x24 * 2
	inline texture_t watermark{};

	// 180x180
	inline texture_t steam_avatar{};

	namespace fonts
	{
		inline std::unordered_map<fnv1a_t, Font*> f = {};

		Font* create_font(const FontId& config, const std::string_view& font_name, const ImWchar* glyph_ranges = nullptr, bool anti_alias = true);
		Font* create_font(const FontId& config, const ImWchar* glyph_ranges = nullptr, bool anti_alias = true);
	}

	inline Font* get_font(const FontId& id)
	{
		auto it = fonts::f.find(id.hash);
		return it != fonts::f.end() ? it->second : nullptr;
	}

	void set_to_background();
	void set_to_foreground();

	void add_text(const FontId& font_id, const std::string_view& text, const glm::vec2& position, const Color& color, const TextFlags& text_flags = TextFlagsNone, const glm::vec2& align = glm::vec2{ 0.0f, 0.0f });
	glm::vec2 get_text_size(const FontId& font_id, const std::string_view& text);
	void add_rect(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& rounding = 0.0f, const float& thickness = 1.0f, const std::int32_t flags = 0);
	void add_rect_filled(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& rounding = 0.0f, const std::int32_t flags = 0);
	void add_line(const glm::vec2& from, const glm::vec2& to, const Color& color, const float& thickness = 1.0f);
	void add_polyline(const std::vector<glm::vec2>& points, const Color& color, const float& thickness = 1.0f, const bool& closed = false);
	void add_shadow_rect_fast(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& shadow_thick = 15.0f, const float& rounding = 0.0f);
	void add_shadow_rect(const glm::vec2& position, const glm::vec2& size, const Color& color, const float& shadow_thick = 15.0f, const float& rounding = 0.0f);
	void add_circle(const glm::vec2& position, const float& radius, const Color& color, const float& thickness = 1.0f);
	void add_circle_filled(const glm::vec2& position, const float& radius, const Color& color);
	void add_circle_shadow(const glm::vec2& position, const float& radius, const Color& color, const float& shadow_thick = 15.0f);
	void add_capsule(const glm::vec2& position, const glm::vec2& size, const Color& color);
	void add_checkmark(glm::vec2 position, const Color& color, float size, const float& progression = 1.0f);
	void add_shadow_poly(const std::vector<glm::vec2>& points, const Color& color, const float& shadow_thick = 15.0f, bool cut_background = false);
	void add_image(ID3D11ShaderResourceView* srv, const glm::vec2& position, const glm::vec2& size, const float& rounding = 0.0f, const Color& color = Color::white());

	void add_triangle(const glm::vec2& one, const glm::vec2& two, const glm::vec2& three, const Color& color, const float& thickness = 1.0f);
	void add_triangle_filled(const glm::vec2& one, const glm::vec2& two, const glm::vec2& three, const Color& color);

	enum class GradientType : int {
		Horizontal,
		Vertical
	};

	void add_rect_gradient(const glm::vec2& position, const glm::vec2& size, GradientType type, const Color& color1, const Color& color2, float rounding = 0.0f, int flags = 0);
	void gradient_items(const glm::vec2& position, const glm::vec2& size, const Color& first_color, const Color& second_color, const std::function< void() >& fn, const float& rotation = 0.0f);

	void push_clip_rect(const glm::vec2& position, const glm::vec2& size);
	void pop_clip_rect();

	void rotate_vertices(float rotation, std::function< void() > items_to_rotate);
	void modify_alpha(int start_idx, int end_idx, float alpha);
}

