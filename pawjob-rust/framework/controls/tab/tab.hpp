#pragma once
#include <base/animation/animation.hpp>
#include <base/object/object.hpp>
#include <string>

namespace gui
{
	class Container;
	class SubTabBar;

	class Tab final : public Object
	{
	public:
		Tab(std::string_view label_name);

		void render() override;
		void on_event(Event& e) override;
		void dispatch_event(Event& e) override;
		void for_each_logical_child(std::function<void(Object*)> callback) override;

		glm::vec2 get_scroll_offset() const override { return { 0.0f, m_target_scroll }; }
		glm::vec2 get_child_origin() override;

		Container* add_container(std::string_view label_name);
		Container* add_full_container(std::string_view label_name);

		SubTabBar* add_subtab_bar(std::string_view label_name = "");
		SubTabBar* add_full_subtab_bar(std::string_view label_name = "");

		std::string label{};
		std::string icon{};
		Animation hover_anim{};
		Animation active_anim{};

		bool is_hovered = false;
		Animation m_scroll_anim{};
		float m_target_scroll = 0.0f;
		float m_content_height = 0.0f;
		bool m_show_scrollbar = false;

	private:
		void ensure_columns();

		Object* m_column_wrapper{};
		Object* m_left_column{};
		Object* m_right_column{};
		int m_child_count = 0;
	};
}
