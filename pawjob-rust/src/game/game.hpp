#pragma once
#include <glm/glm.hpp>

namespace rust
{
	class Entity;
}

namespace game
{
	void create_instance();

	void update_instance();

	bool is_in_game();

	namespace impl
	{
		inline bool created_instance = false;

		inline std::uintptr_t game_assembly = 0;
		inline std::uintptr_t convar_graphics = 0;

		// p2c in game check
		inline std::uintptr_t tod_sky = 0;

		inline glm::mat4x4 view_matrix{};
		inline glm::vec3 view_angles{};
		inline rust::Entity* local_player = nullptr;

		namespace scanned
		{
			inline std::uintptr_t entity_list = 0;
			inline std::uintptr_t view_matrix = 0;
			inline std::uintptr_t local_player_controller = 0;
			inline std::uintptr_t planted_c4 = 0;
			inline std::uintptr_t auto_accept_array = 0;
			inline std::uintptr_t global_vars = 0;
			inline std::uintptr_t network_game_client = 0;
			inline std::uintptr_t view_angles = 0;
		}
	}
}