#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <string>
#include <vector>

namespace gui
{
    class Container : public Object
    {
    public:
        struct Subtab
        {
            std::string name;
            std::string icon;
            Object* page{};
            Animation hover_anim{};
            Animation active_anim{};
            bool is_hovered{ false };
        };

        Container(std::string_view label_name);

        void render() override;
        void dispatch_event(Event& e) override;

        Object* add_subtab(std::string_view name, std::string_view icon = "");

        std::string label{};
        bool hide_subtab_labels{ false };

    private:
        std::vector<Subtab> subtabs{};
        int active_subtab{ 0 };

        Object* next_subtab{ nullptr };
        int next_index{ -1 };
        Animation fade_anim{ 1.0f };
    };
}
