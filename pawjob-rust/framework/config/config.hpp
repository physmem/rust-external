#pragma once
#include <cstdint>
#include <string>

namespace gui { class Listbox; }

namespace config
{
	void init();
	std::string get_path();

	void reset();

	void load_by_name(const std::string& name);
	void save_by_name(const std::string& name);

	void refresh_file_list(gui::Listbox* listbox);
}
