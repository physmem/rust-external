#pragma once
#include "../../base/object/object.hpp"
#include "../../base/animation/animation.hpp"
#include <string>

namespace gui
{
    class Popup final : public Object
    {
    public:
        Popup(std::string_view name);
        ~Popup() override;

        void render() override;
        void render_overlay() override;
        void on_event(Event& e) override;
        void dispatch_event(Event& e) override;

        void attach_child(std::shared_ptr<Object> child) override;
        void update_layout();

        glm::vec2 get_child_origin() override;

        bool is_open{ false };
        bool is_hovered{ false };
        bool hide_icon{ false };

        Animation open_anim{};
        Animation hover_anim{};

        float calculated_height{ 0.0f };
        float calculated_width{ 200.0f };
        glm::vec2 content_pos{ 0.0f, 0.0f };

    private:
        std::string label{};
        YGNodeRef content_node{};
        
        static constexpr float popup_width = 220.0f;
    };
}
