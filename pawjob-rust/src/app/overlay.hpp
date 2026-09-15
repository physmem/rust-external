#pragma once
#include <Windows.h>

#if defined(STABLE)
#define HIJACK_OVERLAY
#else
#define HIJACK_OVERLAY
#endif

namespace Overlay {
	bool FindOverlay();
	extern HWND overlay;
	extern WNDPROC original_wndproc;
}
