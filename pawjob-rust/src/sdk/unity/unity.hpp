#pragma once
#include <sdk/globals.hpp>
#include <vector>

namespace unity
{
	uptr get_component_by_id(uptr game_object, i32 index);
	void get_components_in_children_for_weapons(uptr game_object,
		std::vector<uptr>& gun_renderers);

	void get_components_in_children(uptr game_object,
		std::vector<uptr>& gun_renderers,
		std::vector<uptr>& hand_renderers,
		i32 depth = 0, bool weapon = false);
}