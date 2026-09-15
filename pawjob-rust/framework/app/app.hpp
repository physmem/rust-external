#include "../window/window.hpp"
#include <array>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>

namespace app
{
    inline std::vector<std::shared_ptr<gui::Window>> windows;
    inline ID3D11Device* device = nullptr;

    inline std::recursive_mutex gui_mutex;

    void setup();
    void render();
    void render_watermark();
    bool on_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    gui::Object* find_widget_recursive(gui::Object* current, std::span<const std::string_view> paths, std::size_t depth);

    template <typename T, typename... Args>
    T* get_widget(const std::string_view& window_id, const Args &...path_ids)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            return nullptr;
        }
        else
        {
            std::lock_guard lock(gui_mutex);

            std::array<std::string_view, sizeof...(Args)> paths = { std::string_view(path_ids)... };
            for (auto& window : windows)
            {
                if (window->m_name == window_id)
                {
                    gui::Object* found = find_widget_recursive(window.get(), paths, 0);
                    return dynamic_cast<T*>(found);
                }
            }

            return nullptr;
        }
    }

    template <typename T, typename... Args>
    auto& get_value(std::string_view window_id, Args... path_ids)
    {
        if (T* widget = get_widget<T>(window_id, path_ids...))
            return widget->value;

        static decltype(T::value) default_val{};
        return default_val;
    }
}
