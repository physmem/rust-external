#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <string>
#include <vector>

namespace gui
{
    class MultiDropdown : public Object
    {
    public:
        MultiDropdown(std::string_view label, int init_value, const std::vector<std::string>& items);

        void render() override;
        void render_overlay() override;
        void on_event(Event& e) override;
        void dispatch_event(Event& e) override;

        std::function<void(int)> on_change{};
        int value{};
        int m_default_value{};

        void reset_to_default() override {
            value = m_default_value;
            if (on_change) on_change(value);
        }

    private:
        std::string label{};
        std::vector<std::string> items{};

        bool is_open{ false };
        bool is_hovered{ false };

        Animation open_anim{};
        Animation hover_anim{};
        std::vector<Animation> item_anims{};
        std::vector<bool> item_hovers{};

        float get_box_height() const { return 26.0f; }
        float get_item_height() const { return get_box_height(); }
    };
}
