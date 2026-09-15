#pragma once
#include <base/animation/animation.hpp>
#include <base/object/object.hpp>
#include <functional>
#include <string>

namespace gui
{
	class Button : public Object
	{
	public:
		Button(std::string_view label_name);

		void render() override;
		void dispatch_event(Event& e) override;

		std::string label{};
		std::function<void()> on_press{};

	private:
		bool is_hovered{ false };
		Animation hover_anim{};
		Animation press_anim{};
	};
}
