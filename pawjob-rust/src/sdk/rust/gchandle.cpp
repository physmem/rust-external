#include "gchandle.hpp"
#include <game/game.hpp>
#include <memory/memory.hpp>
#include <sdk/offsets.hpp>

namespace gchandle
{
	uptr get_target(u32 gchandle)
	{
		u32 slot = get_handle_slot(gchandle);
		u32 type = get_handle_type(gchandle);

		handle_data gc_handles = memory::read<handle_data>(game::impl::game_assembly + (offsets::il2cpp::get_handle + type * sizeof(handle_data)));
		uptr gc_entry = memory::read<uptr>(reinterpret_cast<uptr>(gc_handles.entries + slot));

		if (gc_handles.type <= HANDLE_WEAK_TRACK) {
			gc_entry = ~gc_entry;
		}

		return gc_entry;
	}
}