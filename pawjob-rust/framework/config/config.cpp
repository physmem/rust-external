#include "config.hpp"

#include <app/app.hpp>
#include <controls/checkbox/checkbox.hpp>
#include <controls/color_picker/color_picker.hpp>
#include <controls/dropdown/dropdown.hpp>
#include <controls/listbox/listbox.hpp>
#include <controls/multi_dropdown/multi_dropdown.hpp>
#include <controls/slider/slider.hpp>
#include <controls/text_input/text_input.hpp>

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace config
{
	static std::string build_path(gui::Object* obj)
	{
		std::string path;
		std::vector<std::string> parts;

		gui::Object* current = obj;
		while (current)
		{
			if (!current->m_name.empty())
				parts.push_back(current->m_name);
			current = current->parent;
		}

		for (auto it = parts.rbegin(); it != parts.rend(); ++it)
		{
			if (!path.empty()) path += ".";
			path += *it;
		}

		return path;
	}

	std::string get_path()
	{
		return "C:\\pawjob\\configs\\rust\\";
	}

	void init()
	{
		std::filesystem::create_directories(get_path());

		std::string default_path = get_path() + "default.cfg";
		if (!std::filesystem::exists(default_path))
		{
			save_by_name("default");
		}
		else {
			load_by_name("default");
		}
	}

	void refresh_file_list(gui::Listbox* listbox)
	{
		if (!listbox) return;

		std::vector<std::string> files;
		std::string dir = get_path();

		if (std::filesystem::exists(dir))
		{
			for (auto& entry : std::filesystem::directory_iterator(dir))
			{
				if (entry.path().extension() == ".cfg")
				{
					files.push_back(entry.path().stem().string());
				}
			}
		}

		std::sort(files.begin(), files.end());
		listbox->refresh_options(files);
	}

	void reset()
	{
		std::function<void(gui::Object*)> reset_recursive = [&](gui::Object* current)
			{
				if (!current->should_save) return;
				current->reset_to_default();

				for (auto& child : current->children)
					reset_recursive(child.get());
			};

		for (auto& window : app::windows)
			reset_recursive(window.get());
	}

	void load_by_name(const std::string& name)
	{
		std::ifstream file(get_path() + name + ".cfg");
		if (!file.is_open())
			return;

		nlohmann::json j;
		try { file >> j; }
		catch (...) { return; }

		std::function<void(gui::Object*)> load_recursive = [&](gui::Object* current)
			{
				if (!current->should_save) return;

				std::string path = build_path(current);

				if (!path.empty() && j.contains(path))
				{
					auto& widget_json = j[path];

					auto get_val = [&](const nlohmann::json& node) -> nlohmann::json
						{
							if (node.is_object() && node.contains("value"))
								return node["value"];
							return node;
						};

					auto val = get_val(widget_json);

					if (auto b = dynamic_cast<gui::Checkbox*>(current))
					{
						if (val.is_boolean()) b->value = val.get<bool>();
					}
					else if (auto sfd = dynamic_cast<gui::Slider<float>*>(current))
					{
						if (val.is_number()) sfd->value = val.get<float>();
					}
					else if (auto sid = dynamic_cast<gui::Slider<int>*>(current))
					{
						if (val.is_number()) sid->value = val.get<int>();
					}
					else if (auto d = dynamic_cast<gui::Dropdown*>(current))
					{
						if (val.is_number()) d->value = val.get<int>();
					}
					else if (auto m = dynamic_cast<gui::MultiDropdown*>(current))
					{
						if (val.is_number()) m->value = val.get<int>();
					}
					else if (auto t = dynamic_cast<gui::TextInput*>(current))
					{
						if (val.is_string()) t->storage = val.get<std::string>();
					}
					else if (auto c = dynamic_cast<gui::ColorPicker*>(current))
					{
						if (val.is_object() && val.contains("r"))
						{
							c->value = Color(
								val["r"].get<int>(),
								val["g"].get<int>(),
								val["b"].get<int>(),
								val["a"].get<int>()
							);
						}
					}
				}

				for (auto& child : current->children)
					load_recursive(child.get());
			};

		for (auto& window : app::windows)
			load_recursive(window.get());
	}

	void save_by_name(const std::string& name)
	{
		nlohmann::json j;

		std::function<void(gui::Object*)> save_recursive = [&](gui::Object* current)
			{
				if (!current->should_save) return;

				std::string path = build_path(current);
				bool has_value = false;
				nlohmann::json widget_json;

				if (auto b = dynamic_cast<gui::Checkbox*>(current))
				{
					widget_json["value"] = b->value;
					has_value = true;
				}
				else if (auto sfd = dynamic_cast<gui::Slider<float>*>(current))
				{
					widget_json["value"] = sfd->value;
					has_value = true;
				}
				else if (auto sid = dynamic_cast<gui::Slider<int>*>(current))
				{
					widget_json["value"] = sid->value;
					has_value = true;
				}
				else if (auto d = dynamic_cast<gui::Dropdown*>(current))
				{
					widget_json["value"] = d->value;
					has_value = true;
				}
				else if (auto m = dynamic_cast<gui::MultiDropdown*>(current))
				{
					widget_json["value"] = m->value;
					has_value = true;
				}
				else if (auto t = dynamic_cast<gui::TextInput*>(current))
				{
					widget_json["value"] = t->storage;
					has_value = true;
				}
				else if (auto c = dynamic_cast<gui::ColorPicker*>(current))
				{
					widget_json["value"] = {
						{"r", (int)c->value.r},
						{"g", (int)c->value.g},
						{"b", (int)c->value.b},
						{"a", (int)c->value.a}
					};
					has_value = true;
				}

				if (has_value && !path.empty())
				{
					j[path] = widget_json;
				}

				for (auto& child : current->children)
					save_recursive(child.get());
			};

		for (auto& window : app::windows)
			save_recursive(window.get());

		std::ofstream file(get_path() + name + ".cfg");
		if (file.is_open())
		{
			file << j.dump(4);
		}
	}
}
