#pragma once

#include "color.hpp"

namespace gui::style
{
	inline constexpr float window_rounding = 8.0f;
	inline constexpr float tab_padding = 15.0f;
	inline constexpr float tab_header_height = 42.0f;
	inline constexpr float footer_height = 34.0f;
	inline constexpr float padding = 10.0f;
	inline constexpr float container_header_height = 35.0f;
	inline constexpr float container_rounding = 4.0f;
	inline constexpr float watermark_rounding = 6.0f;
	inline constexpr float widget_rounding = 3.0f;
	inline constexpr float scrollbar_width = 3.0f;

	namespace colors
	{
		inline const Color window_bg{ 20, 20, 20 };
		inline const Color window_border{ 20, 20, 20, 200 };
		inline const Color window_shadow{ 5, 5, 5 };

		inline const Color tab_bg{ 12, 12, 12 };

		inline const Color control_bg{ 14, 14, 14 };
		inline const Color hovered_control_bg{ 18, 18, 18 };

		inline const Color separator{ 50, 50, 50 };
		inline Color accent{ 255, 85, 200 };

		inline const Color text_active{ 255, 255, 255 };
		inline const Color text_hover{ 180, 180, 180 };
		inline const Color text_inactive{ 100, 100, 100 };
	}
}
