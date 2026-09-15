#pragma once
#include <atomic>
#include <thread>

inline std::atomic<bool> g_should_terminate = false;
inline bool g_menu_3d_enabled = false;
inline std::atomic<bool> g_should_refresh_cache = false;