#pragma once

#if !defined(APP_BACKEND_USE_DX11)
#define APP_BACKEND_USE_DX11
#endif

#if defined(APP_BACKEND_USE_DX11)
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <atomic>
#include <array>

namespace winapp::backends::directx11
{
	bool create_device(HWND hwnd);

	bool create_render_target();

	void cleanup_render_target();

	bool create_blend_state();

	bool update_refresh_rate(HWND hwnd);

	void destroy_device();

	namespace impl
	{
		inline ID3D11Device* device{ nullptr };
		inline ID3D11DeviceContext* device_context{ nullptr };
		inline IDXGISwapChain1* swap_chain{ nullptr };

		inline ID3D11RenderTargetView* render_target{ nullptr };
		inline ID3D11BlendState* blend_state{ nullptr };

		inline IDCompositionDevice* dcomp_device{ nullptr };
		inline IDCompositionTarget* dcomp_target{ nullptr };
		inline IDCompositionVisual* dcomp_visual{ nullptr };

		inline std::atomic_bool swap_chain_occluded{ false };

		inline double refresh_rate{ 60.0 };
	}
}
#endif