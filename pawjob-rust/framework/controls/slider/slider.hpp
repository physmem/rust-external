#pragma once
#include <base/animation/animation.hpp>
#include <base/object/object.hpp>
#include <functional>
#include <string>

namespace gui
{
	template <typename T>
	class Slider final : public Object
	{
	public:
		Slider(std::string_view label, T min, T max, T init_value, std::string_view suffix = "", int precision = 0, T step = static_cast<T>(1));

		void render() override;
		void dispatch_event(Event& e) override;
		float get_preferred_width() override;

		std::function<void(T)> on_change{};
		std::function<std::string(T)> format_value{};
		T value{};
		T m_default_value{};

		void reset_to_default() override {
			value = m_default_value;
			if (on_change) on_change(value);
		}

		std::string label{};
		std::string suffix{};

		T min{};
		T max{};
		T step{};
		int precision{};

	private:

		bool is_dragging{ false };
		bool is_hovered{ false };
		Animation hover_anim{};
		Animation percentage_anim{};

		std::string get_value_string();
	};
}
