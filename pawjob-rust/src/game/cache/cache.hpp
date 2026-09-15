#pragma once
#include <mutex>
#include <unordered_map>

namespace rust
{
	class Entity;
}

namespace cache
{
	bool create_instance();

	// gathers base entities
	void gather();

	// constantly updates entities
	void update();

	inline std::mutex cache_mut = {};
	inline std::unordered_map<std::uintptr_t, rust::Entity*>* entity_list = {};

	template <typename T, typename Func>
	void for_each_entity(Func callback)
	{
		std::unique_lock lock(cache_mut);

		if (!entity_list)
			return;

		for (const auto& pair : *entity_list)
		{
			T* entity = dynamic_cast<T*>(pair.second);
			if (!entity)
				continue;

			callback(entity);
		}
	}

	rust::Entity* get_local_player();
}