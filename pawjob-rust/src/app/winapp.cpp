#include "../globals.hpp"
#include "winapp.hpp"

#include <print>

#include "../utils/vmp.hpp"
#include "backends/directx11/directx11.hpp"
#include "overlay.hpp"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "../gui/gui.hpp"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "../memory/memory.hpp"

#include <chrono>
#include <thread>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include <app/app.hpp>
#include <game/cache/cache.hpp>
#include <game/game.hpp>
#include <render/render.hpp>
#include <string_encryption.hpp>
#include <window/window.hpp>

#include <game/features/misc/misc.hpp>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace winapp
{
	void sync_window_with_game()
	{
		if (!memory::impl::game_hwnd)
			return;

		RECT client_rect{};
		if (!::GetClientRect(memory::impl::game_hwnd, &client_rect))
			return;

		POINT origin{};
		::ClientToScreen(memory::impl::game_hwnd, &origin);

		const auto w = static_cast<std::uint32_t>(client_rect.right - client_rect.left);
		const auto h = static_cast<std::uint32_t>(client_rect.bottom - client_rect.top);

		static RECT last_rect{};
		const RECT new_rect{ origin.x, origin.y, static_cast<LONG>(w), static_cast<LONG>(h) };

		if (new_rect.left != last_rect.left || new_rect.top != last_rect.top ||
			new_rect.right != last_rect.right || new_rect.bottom != last_rect.bottom)
		{
			last_rect = new_rect;

			::SetWindowPos(impl::window_hwnd, HWND_TOPMOST, origin.x, origin.y,
				static_cast<int>(w), static_cast<int>(h), SWP_NOACTIVATE | SWP_NOREDRAW);

			backends::directx11::cleanup_render_target();
			backends::directx11::impl::swap_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
			backends::directx11::create_render_target();

			ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
		}
	}

	void trigger_thread()
	{
		while (!g_should_terminate)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void on_frame()
	{
		while (!g_should_terminate)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			sync_window_with_game();

			game::update_instance();
			cache::update();
			features::misc::on_tick();

			// @Ichigo - TODO: update this so that the window is automatically focused and not focused on toggle so it
			// feels more seamless
			{
				static bool last_is_open = false;
				if (gui::is_open != last_is_open)
				{
					last_is_open = gui::is_open;
					LONG_PTR style = ::GetWindowLongPtrW(impl::window_hwnd, GWL_EXSTYLE);
					if (gui::is_open)
					{
						style &= ~WS_EX_TRANSPARENT;

						if (!impl::mouse_hook)
							impl::mouse_hook = ::SetWindowsHookExW(WH_MOUSE_LL, ll_mouse_proc, ::GetModuleHandleW(nullptr), 0);
					}
					else
					{
						style |= WS_EX_TRANSPARENT;

						if (impl::mouse_hook)
						{
							::UnhookWindowsHookEx(impl::mouse_hook);
							impl::mouse_hook = nullptr;
						}
					}
					::SetWindowLongPtrW(impl::window_hwnd, GWL_EXSTYLE, style);
				}
			}

			// listen for winapi messages
			MSG message{};
			while (::PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
			{
				::TranslateMessage(&message);
				::DispatchMessageW(&message);

				if (message.message == WM_QUIT)
					g_should_terminate = true;
			};

			if (g_should_terminate)
				break;

			// skip rendering entirely while the swapchain is occluded
			if (backends::directx11::impl::swap_chain_occluded)
			{
				if (backends::directx11::impl::swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}
				backends::directx11::impl::swap_chain_occluded = false;
			}

			// handle swapchain resize from WM_SIZE
			if (impl::resize_width != 0 && impl::resize_height != 0)
			{
				backends::directx11::cleanup_render_target();
				backends::directx11::impl::swap_chain->ResizeBuffers(0, impl::resize_width, impl::resize_height, DXGI_FORMAT_UNKNOWN, 0);
				impl::resize_width = impl::resize_height = 0;
				backends::directx11::create_render_target();
			}

			// push the new frame ready to render
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			if (gui::is_open)
			{
				ImGuiIO& io = ImGui::GetIO();

#ifdef HIJACK_OVERLAY
				POINT p;
				::GetCursorPos(&p);
				::ScreenToClient(impl::window_hwnd, &p);
				io.MousePos = ImVec2((float)p.x, (float)p.y);

				static POINT last_p = { -1, -1 };
				if (p.x != last_p.x || p.y != last_p.y)
				{
					app::on_wndproc(impl::window_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
					ImGui_ImplWin32_WndProcHandler(impl::window_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
					last_p = p;
				}

				static bool last_l_state = false;
				static bool last_r_state = false;
				static bool last_m_state = false;
				static bool last_x1_state = false;
				static bool last_x2_state = false;

				auto handle_button = [&](int vk, UINT down_msg, UINT up_msg, int io_idx, bool& last_state, WPARAM wparam = 0) {
					bool current_state = (::GetAsyncKeyState(vk) & 0x8000) != 0;

					if (current_state != last_state)
					{
						UINT msg = current_state ? down_msg : up_msg;
						LPARAM lparam = MAKELPARAM(p.x, p.y);

						app::on_wndproc(impl::window_hwnd, msg, wparam, lparam);
						ImGui_ImplWin32_WndProcHandler(impl::window_hwnd, msg, wparam, lparam);

						last_state = current_state;
					}
					};

				handle_button(VK_LBUTTON, WM_LBUTTONDOWN, WM_LBUTTONUP, 0, last_l_state);
				handle_button(VK_RBUTTON, WM_RBUTTONDOWN, WM_RBUTTONUP, 1, last_r_state);
				handle_button(VK_MBUTTON, WM_MBUTTONDOWN, WM_MBUTTONUP, 2, last_m_state);
				handle_button(VK_XBUTTON1, WM_XBUTTONDOWN, WM_XBUTTONUP, 3, last_x1_state, MAKEWPARAM(0, XBUTTON1));
				handle_button(VK_XBUTTON2, WM_XBUTTONDOWN, WM_XBUTTONUP, 4, last_x2_state, MAKEWPARAM(0, XBUTTON2));
#else
				if (g_menu_3d_enabled)
				{
					static bool last_l_state = false;
					static bool last_r_state = false;
					static bool last_m_state = false;
					static bool last_x1_state = false;
					static bool last_x2_state = false;

					auto handle_button = [&](int vk, UINT down_msg, UINT up_msg, int io_idx, bool& last_state, WPARAM wparam = 0) {
						bool current_state = (::GetAsyncKeyState(vk) & 0x8000) != 0;

						if (current_state != last_state)
						{
							UINT msg = current_state ? down_msg : up_msg;
							app::on_wndproc(impl::window_hwnd, msg, wparam, 0);

							if (io_idx >= 0 && io_idx < 5)
								io.MouseDown[io_idx] = current_state;

							last_state = current_state;
						}
						};

					handle_button(VK_LBUTTON, WM_LBUTTONDOWN, WM_LBUTTONUP, 0, last_l_state);
					handle_button(VK_RBUTTON, WM_RBUTTONDOWN, WM_RBUTTONUP, 1, last_r_state);
					handle_button(VK_MBUTTON, WM_MBUTTONDOWN, WM_MBUTTONUP, 2, last_m_state);
					handle_button(VK_XBUTTON1, WM_XBUTTONDOWN, WM_XBUTTONUP, 3, last_x1_state, MAKEWPARAM(0, XBUTTON1));
					handle_button(VK_XBUTTON2, WM_XBUTTONDOWN, WM_XBUTTONUP, 4, last_x2_state, MAKEWPARAM(0, XBUTTON2));
				}
#endif
			}

			ImGui::NewFrame();

			/*bool& particles_enabled = gui::app::get_value<gui::Checkbox>(CHEAT, xs("Visuals"), xs("Particles"), xs("Enabled"));
			if (particles_enabled)
			{
				particles::Settings settings{};
				settings.primary_color = gui::app::get_value<gui::ColorPicker>(CHEAT, xs("Visuals"), xs("Particles"), xs("Enabled"), xs("Primary color"));
				settings.secondary_color = gui::app::get_value<gui::ColorPicker>(CHEAT, xs("Visuals"), xs("Particles"), xs("Enabled"), xs("Secondary color"));
				settings.size_scale = gui::app::get_value<gui::Slider<float>>(CHEAT, xs("Visuals"), xs("Particles"), xs("Enabled"), xs("Particle options"), xs("Size scale"));
				settings.speed_scale = gui::app::get_value<gui::Slider<float>>(CHEAT, xs("Visuals"), xs("Particles"), xs("Enabled"), xs("Particle options"), xs("Movement speed"));

				particles::update(game::impl::view_angles, game::impl::view_matrix, settings);
				particles::render();
			}*/

			// render stuffs
			gui::on_render();

			// push all of the ImDrawData into a shader and render it onto our render target
			ImGui::Render();
			const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			backends::directx11::impl::device_context->OMSetRenderTargets(1, &backends::directx11::impl::render_target, nullptr);
			backends::directx11::impl::device_context->ClearRenderTargetView(backends::directx11::impl::render_target, clear_color_with_alpha);

			backends::directx11::impl::device_context->OMSetBlendState(backends::directx11::impl::blend_state, nullptr, 0xFFFFFFFF);

			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			HRESULT hr = backends::directx11::impl::swap_chain->Present(impl::present_with_virtual_sync ? 1 : 0, 0);
			backends::directx11::impl::swap_chain_occluded = (hr == DXGI_STATUS_OCCLUDED);
		}
	}

	bool create_window_instance()
	{
		VMP_START("create_window_instance");
		// we could make imgui dpi aware here but im not concerned for that in this enviroment

#ifdef HIJACK_OVERLAY
		if (!Overlay::FindOverlay())
		{
			VMP_END;
			return false;
		}

		impl::window_hwnd = Overlay::overlay;
#else
		// @Ichigo - setup the base window
		impl::wnd_class = { .cbSize{sizeof(impl::wnd_class)},
							   .style{CS_CLASSDC | CS_OWNDC},
							   .lpfnWndProc{wnd_proc},
							   .cbClsExtra{0},
							   .cbWndExtra{0},
							   .hInstance{GetModuleHandleA(nullptr)},
							   .hIcon{nullptr},
							   .hCursor{nullptr},
							   .hbrBackground{nullptr},
							   .lpszMenuName{L"prim-ext"},
							   .lpszClassName{L"UI"}
		};

		::RegisterClassExW(&impl::wnd_class);

		// get target window rect
		RECT rect{};
		::GetWindowRect(memory::impl::game_hwnd, &rect);

		// create our amazing window
		impl::window_hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, impl::wnd_class.lpszClassName, L"UI", WS_POPUP,
			rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, impl::wnd_class.hInstance, nullptr);

		if (!impl::window_hwnd)
		{
			std::println("failed to create new window (GetLastError: {})", GetLastError());
			VMP_END;
			return false;
		}

		MARGINS margins = { -1, -1, -1, -1 };
		::DwmExtendFrameIntoClientArea(impl::window_hwnd, &margins);
		::SetLayeredWindowAttributes(impl::window_hwnd, 0, 255, LWA_ALPHA);
#endif

		if (!backends::directx11::create_device(impl::window_hwnd))
		{
			backends::directx11::destroy_device();
#ifndef HIJACK_OVERLAY
			::UnregisterClassW(impl::wnd_class.lpszClassName, impl::wnd_class.hInstance);
#endif
			VMP_END;
			return false;
		}

		std::println("created directx device");

#ifndef HIJACK_OVERLAY
		::ShowWindow(impl::window_hwnd, SW_SHOWDEFAULT);
		::UpdateWindow(impl::window_hwnd);
#endif

		// install global low-level keyboard hook so WndProc receives key events
		// even when the game window has focus
		impl::kb_hook = ::SetWindowsHookExW(WH_KEYBOARD_LL, ll_keyboard_proc, ::GetModuleHandleW(nullptr), 0);
		if (!impl::kb_hook)
			std::println("failed to install keyboard hook (GetLastError: {})", ::GetLastError());

		// @Ichigo - Mouse hook is now installed dynamically when menu opens to prevent lag

		// setup imgui context
		ImGui::CreateContext();

		// remove the imgui.ini file that gets created usually
		ImGui::GetIO().IniFilename = nullptr;

		// setup imgui platform and rendering backend
		ImGui_ImplWin32_Init(impl::window_hwnd);
		ImGui_ImplDX11_Init(backends::directx11::impl::device, backends::directx11::impl::device_context);

		app::device = backends::directx11::impl::device;
		render::setup();
		app::setup();

		impl::trigger_thread_handle = std::thread(trigger_thread);

		// finally render!
		on_frame();

		VMP_END;
		return true;
	}
	void destroy_window_instance()
	{
		//particles::cleanup_resources();

		if (impl::kb_hook)
		{
			::UnhookWindowsHookEx(impl::kb_hook);
			impl::kb_hook = nullptr;
		}

		if (impl::mouse_hook)
		{
			::UnhookWindowsHookEx(impl::mouse_hook);
			impl::mouse_hook = nullptr;
		}

		// destroy imgui
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		// destory our device, swapchain, context & render target then destroy our window
		backends::directx11::destroy_device();

		if (impl::trigger_thread_handle.joinable())
			impl::trigger_thread_handle.join();

#ifndef HIJACK_OVERLAY
		::DestroyWindow(impl::window_hwnd);
		::UnregisterClassW(impl::wnd_class.lpszClassName, impl::wnd_class.hInstance);
#endif

		std::println("goodbye!");
	}

	LRESULT __stdcall ll_keyboard_proc(int code, WPARAM wparam, LPARAM lparam)
	{
		if (code == HC_ACTION && impl::window_hwnd)
		{
			const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);

			const UINT msg = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)
				? WM_KEYDOWN : WM_KEYUP;

			const auto l_param = static_cast<LPARAM>(1u | (kb->scanCode << 16u)
				| ((kb->flags & LLKHF_EXTENDED) ? (1u << 24u) : 0u)
				| (msg == WM_KEYUP ? (0xC0000000u) : 0u));

#ifdef HIJACK_OVERLAY
			app::on_wndproc(impl::window_hwnd, msg, static_cast<WPARAM>(kb->vkCode), l_param);

			if (gui::is_open)
			{
				ImGui_ImplWin32_WndProcHandler(impl::window_hwnd, msg, static_cast<WPARAM>(kb->vkCode), l_param);
			}
#endif

#ifndef HIJACK_OVERLAY
			if (::GetForegroundWindow() != impl::window_hwnd)
			{
				::PostMessageW(impl::window_hwnd, msg,
					static_cast<WPARAM>(kb->vkCode),
					l_param);
			}
#else
			::PostMessageW(impl::window_hwnd, msg,
				static_cast<WPARAM>(kb->vkCode),
				l_param);
#endif
		}

		return ::CallNextHookEx(impl::kb_hook, code, wparam, lparam);
	}

	LRESULT __stdcall ll_mouse_proc(int code, WPARAM wparam, LPARAM lparam)
	{
		if (code == HC_ACTION && wparam == WM_MOUSEWHEEL && impl::window_hwnd)
		{
			const auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
			const int delta = GET_WHEEL_DELTA_WPARAM(ms->mouseData);

#ifdef HIJACK_OVERLAY
			if (gui::is_open)
			{
				app::on_wndproc(impl::window_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(ms->pt.x, ms->pt.y));
				ImGui_ImplWin32_WndProcHandler(impl::window_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(ms->pt.x, ms->pt.y));
			}
#endif

#ifndef HIJACK_OVERLAY
			::PostMessageW(impl::window_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(ms->pt.x, ms->pt.y));
#else
			::PostMessageW(impl::window_hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, delta), MAKELPARAM(ms->pt.x, ms->pt.y));
#endif
		}

		return ::CallNextHookEx(impl::mouse_hook, code, wparam, lparam);
	}


	LRESULT __stdcall wnd_proc(HWND hwnd, std::uint32_t msg, WPARAM wparam, LPARAM lparam)
	{
		app::on_wndproc(hwnd, msg, wparam, lparam);

		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
			return true;

		switch (msg)
		{
		case WM_SIZE:
			if (wparam == SIZE_MINIMIZED)
				return 0;

			impl::resize_width = static_cast<std::uint32_t>(LOWORD(lparam));
			impl::resize_height = static_cast<std::uint32_t>(HIWORD(lparam));
			return 0;
		case WM_SYSCOMMAND:
			// alt menu can fuck off :D
			if ((wparam & 0xfff0) == SC_KEYMENU)
				return 0;
			break;

		case WM_DESTROY:
#ifdef HIJACK_OVERLAY
			g_should_terminate = true;
#else
			::PostQuitMessage(0);
#endif
			break;
		}

		return ::DefWindowProcW(hwnd, msg, wparam, lparam);
	}
}