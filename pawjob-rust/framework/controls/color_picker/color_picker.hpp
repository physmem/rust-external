#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <utils/color.hpp>

namespace gui
{
    class ColorPicker final : public Object
    {
    public:
        ColorPicker(std::string_view name, Color color, bool show_alpha = true);

        void render() override;
        void render_overlay() override;
        void on_event(Event& e) override;
        void dispatch_event(Event& e) override;

        bool is_open{ false };
        bool show_alpha{ true };
        Color value{ Color::white() };
        Color m_default_value{ Color::white() };

        void reset_to_default() override {
            value = m_default_value;
            update_hsv_from_color();
        }
        
        Animation open_anim{};
        glm::vec2 last_box_pos{ 0.0f, 0.0f };

    private:
        void update_hsv_from_color();
        void update_color_from_hsv();

        Hsv hsv{};
        bool dragging_sv{ false };
        bool dragging_hue{ false };
        bool dragging_alpha{ false };

        static constexpr float picker_width = 220.0f;
        static constexpr float sv_height = 120.0f;
        static constexpr float slider_height = 12.0f;
    };
}
