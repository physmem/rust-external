#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <string>
#include <vector>

namespace gui
{
    class Container;

    class SubTabBar : public Object
    {
    public:
        class TabPage : public Object
        {
        public:
            TabPage(std::string_view name);
            Container* add_container(std::string_view label_name);
            Container* add_full_container(std::string_view label_name);

            SubTabBar* add_subtab_bar(std::string_view label_name = "");
            SubTabBar* add_full_subtab_bar(std::string_view label_name = "");

        private:
            void ensure_columns();
            Object* m_column_wrapper{};
            Object* m_left_column{};
            Object* m_right_column{};
            int m_child_count = 0;
        };

        struct Tab {
            std::string name;
            std::string icon;
            TabPage* page{};
            Animation hover_anim{};
            Animation active_anim{};
            bool is_hovered{ false };
        };

        SubTabBar();

        void render() override;
        void dispatch_event(Event& e) override;

        TabPage* add_tab(std::string_view name, std::string_view icon = "");

    private:
        std::vector<Tab> m_tabs{};
        int m_active_tab{ 0 };
        Object* m_next_tab{ nullptr };
        int m_next_index{ -1 };
        Animation m_fade_anim{ 1.0f };
    };
}
