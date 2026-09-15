#pragma once
#include <deque>
#include <glm/glm.hpp>
#include <sdk/globals.hpp>
#include <shared_mutex>

namespace rust
{
	class Entity;
}

namespace features::aimbot
{
	inline std::shared_mutex debug_mut{};
	inline std::deque< std::string > debug_info{};
	inline std::deque< std::string > safe_debug_info{};

	struct ActiveConfig
	{
		bool enabled{ false };
		bool silent_aim{ false };
		bool distance_fov_scaling{ false };

		i32 field_of_view{ 30 };
		f32 smoothing{ 20.0f };
		f32 silent_hit_chance{ 100.0f };
		f32 max_distance{ 150.0f };

		bool skip_scientists{ false };
		bool skip_dwellers{ false };
		bool skip_sleepers{ false };
		bool skip_wounded{ false };
		bool skip_animals{ false };
	};

	ActiveConfig get_active_config();

	rust::Entity* get_best_target();

	void on_tick();

	void on_render();
}