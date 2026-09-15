#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <thread>

#include <glm/glm.hpp>

namespace winapp
{
	void on_frame();

	bool create_window_instance();

	void destroy_window_instance();

	LRESULT __stdcall wnd_proc(HWND hwnd, std::uint32_t msg, WPARAM wparam, LPARAM lparam);

	LRESULT __stdcall ll_keyboard_proc(int code, WPARAM wparam, LPARAM lparam);
	LRESULT __stdcall ll_mouse_proc(int code, WPARAM wparam, LPARAM lparam);

	namespace impl
	{
		inline HWND window_hwnd{ nullptr };
		inline WNDCLASSEXW wnd_class{};

		inline std::uint32_t resize_width{ 0 };
		inline std::uint32_t resize_height{ 0 };

		inline bool present_with_virtual_sync{ true };

		inline HHOOK kb_hook{ nullptr };
		inline HHOOK mouse_hook{ nullptr };
		inline std::thread trigger_thread_handle{};

		inline glm::vec2 window_size{};
	}
}