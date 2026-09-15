#pragma once

#include <glm/vec2.hpp>
#include <cstdint>

namespace gui
{
    enum class EventType
    {
        None = 0,
        MouseMove,
        MouseButton,
        MouseScroll,
        KeyPress,
        KeyRelease,
        KeyChar,
        MouseEnter,
        MouseLeave,
    };

    class Event
    {
    public:
        virtual ~Event() = default;
        virtual EventType get_type() const = 0;
        bool handled{};
    };

    class MouseMoveEvent : public Event
    {
    public:
        MouseMoveEvent(const glm::vec2& position) : position(position) {}
        EventType get_type() const override { return EventType::MouseMove; }
        glm::vec2 position{};
    };

    enum class MouseButton
    {
        Left,
        Right,
        Middle,
        X1,
        X2
    };

    struct MouseState
    {
        glm::vec2 position{};
        MouseButton button{};
        bool pressed{};
    };

    class MouseButtonEvent : public Event
    {
    public:
        MouseButtonEvent(MouseButton button, bool pressed, const glm::vec2& position) 
            : button(button), pressed(pressed), position(position) {}
        EventType get_type() const override { return EventType::MouseButton; }
        MouseButton button{};
        bool pressed{};
        glm::vec2 position{};
    };

    class MouseScrollEvent : public Event
    {
    public:
        MouseScrollEvent(float offset) : offset(offset) {}
        EventType get_type() const override { return EventType::MouseScroll; }
        float offset{};
    };

    class KeyPressEvent : public Event
    {
    public:
        KeyPressEvent(std::int32_t key) : key(key) {}
        EventType get_type() const override { return EventType::KeyPress; }
        std::int32_t key{};
    };

    class KeyReleaseEvent : public Event
    {
    public:
        KeyReleaseEvent(std::int32_t key) : key(key) {}
        EventType get_type() const override { return EventType::KeyRelease; }
        std::int32_t key{};
    };

    class KeyCharEvent : public Event
    {
    public:
        KeyCharEvent(std::int32_t key) : key(key) {}
        EventType get_type() const override { return EventType::KeyChar; }
        std::int32_t key{};
    };

    class MouseEnterEvent : public Event
    {
    public:
        EventType get_type() const override { return EventType::MouseEnter; }
    };

    class MouseLeaveEvent : public Event
    {
    public:
        EventType get_type() const override { return EventType::MouseLeave; }
    };
}
