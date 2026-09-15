#pragma once
#include <base/object/object.hpp>
#include <render/render.hpp>
#include <controls/color_picker/color_picker.hpp>
#include <string>

namespace gui
{
    class Label : public Object
    {
    public:
        Label(std::string_view text);

        void render() override;
        void dispatch_event(Event& e) override;

        std::string text{};

    private:
        bool is_hovered{ false };
    };
}
