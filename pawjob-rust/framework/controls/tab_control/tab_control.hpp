#pragma once
#include <base/object/object.hpp>
#include "../tab/tab.hpp"
#include <vector>
#include <memory>
#include <string_view>

namespace gui
{
    class TabControl : public Object
    {
    public:
        TabControl();

        Tab* add_tab(std::string_view label, std::string_view icon = "");

        void render() override;
        void dispatch_event(Event& e) override;
        void for_each_logical_child(std::function<void(Object*)> callback) override;

    private:
        Object* body_container{};

        std::vector<std::shared_ptr<Tab>> tabs{};
        Tab* active_tab{};
        Tab* m_next_tab{};
        Animation m_fade_anim{ 1.0f };
    };
}
