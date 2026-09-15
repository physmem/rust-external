#pragma once
#include <base/object/object.hpp>
#include <base/animation/animation.hpp>
#include <string>

namespace gui
{
    class TextInput final : public Object
    {
    public:
        TextInput(std::string_view label_name, std::string_view placeholder = "", bool password = false);

        void render() override;
        void dispatch_event(Event& e) override;
        void update_layout() override;

        std::string storage{};
        std::string m_default_value{};

        void reset_to_default() override {
            storage = m_default_value;
        }
        std::string placeholder_text{};
        bool is_password{ false };
        bool show_label{ true };

    private:
        std::string label{};
        bool focused{ false };
        int cursor_pos{ 0 };
        int selection_start{ -1 };
        float blink_timer{ 0.0f };

        bool is_hovered{ false };
        Animation hover_anim{};

        void clear_selection();
        float get_box_height() const { return 26.0f; }
    };
}
