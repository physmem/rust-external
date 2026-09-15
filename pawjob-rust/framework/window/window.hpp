#pragma once

#include "../base/object/object.hpp"
#include <base/animation/animation.hpp>
#include <glm/vec2.hpp>
#include <string_view>

namespace gui
{
	enum class WindowFlags : uint32_t
	{
		None = 0,
		NoTitleBar = 1 << 0,
		NoResize = 1 << 1,

		AlignCenter = 1 << 2,
		AlignTopRight = 1 << 3,
		AlignBottomLeft = 1 << 4,
	};

	inline WindowFlags operator|(WindowFlags a, WindowFlags b) {
		return static_cast<WindowFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline bool operator&(WindowFlags a, WindowFlags b) {
		return static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
	}

	class Window final : public Object
	{
	public:
		Window(std::string title, glm::vec2 init_pos, glm::vec2 init_size, WindowFlags flags = WindowFlags::None);
		~Window() override;

		void render() override;
		void dispatch_event(Event& e) override;

		void calculate_layout();

		glm::vec2 get_absolute_position() override
		{
			return root_position + glm::vec2(get_x(), get_y());
		}

		std::string window_title{};
		std::string window_title_show{};
		std::string window_tld{};
		bool m_opened{ true };

	private:
		void apply_flags(WindowFlags flags);

		WindowFlags window_flags{};

		glm::vec2 root_position{};
		glm::vec2 root_size{};

		bool is_dragging{ false };
		glm::vec2 drag_offset{};

		Animation m_fade_anim{ 1.0f };
	};
}
