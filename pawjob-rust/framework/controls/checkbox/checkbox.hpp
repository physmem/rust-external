#pragma once
#include <base/animation/animation.hpp>
#include <base/object/object.hpp>
#include <controls/color_picker/color_picker.hpp>
#include <functional>
#include <string>

namespace gui
{
	class ColorPicker;

	class Checkbox final : public Object
	{
	public:
		Checkbox(std::string_view label_name, bool init_value = false);

		void render() override;
		void dispatch_event(Event& e) override;
		float get_preferred_width() override;

		std::string label{};
		bool value{};
		bool m_default_value{};

		void reset_to_default() override {
			value = m_default_value;
			if (on_change) on_change(value);
		}

		Animation active_anim{};
		Animation hover_anim{};

		std::function<void(bool)> on_change;

	private:
		bool is_hovered = false;
	};
}
