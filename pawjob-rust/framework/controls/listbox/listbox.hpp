#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <string>
#include <vector>

namespace gui
{
    class Listbox final : public Object
    {
    public:
        Listbox(std::string_view label_name, const std::vector<std::string>& opts, int max_visible = 8);

        void render() override;
        void dispatch_event(Event& e) override;
        void update_layout() override;

        std::vector<std::string> options{};
        int value{ 0 };
        bool show_label{ true };

        void refresh_options(const std::vector<std::string>& new_options);

    private:
        std::string label{};
        int max_items{ 8 };

        float scroll_offset{ 0.0f };
        float scroll_velocity{ 0.0f };
        bool is_hovered{ false };

        Animation hover_anim{};
        std::vector<bool> item_hovers{};
        std::vector<Animation> item_hover_anims{};
        std::vector<Animation> item_active_anims{};

        float get_item_height() const { return 24.0f; }
    };
}
