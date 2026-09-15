#include "kv3-parser.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

#define INRANGE(x,a,b)      (x >= a && x <= b) 
#define getBits( x )        (INRANGE(x,'0','9') ? (x - '0') : ((x&(~0x20)) - 'A' + 0xa))
#define get_byte( x )       (getBits(x[0]) << 4 | getBits(x[1]))

template <typename Ty>
vector<Ty> bytes_to_vec(const string& bytes)
{
	const auto num_bytes = bytes.size() / 3;
	const auto num_elements = num_bytes / sizeof(Ty);

	vector<Ty> vec;
	vec.resize(num_elements + 1);

	const char* p1 = bytes.c_str();
	uint8_t* p2 = reinterpret_cast<uint8_t*>(vec.data());
	while (*p1 != '\0')
	{
		if (*p1 == ' ')
		{
			++p1;
		}
		else
		{
			*p2++ = get_byte(p1);
			p1 += 2;
		}
	}

	return vec;
}

typedef struct Vector3 {
	float x, y, z;
};
typedef struct Triangle {
	Vector3 p1, p2, p3;
};
typedef struct Edge {
	uint8_t next, twin, origin, face;
};

vector<string> get_vphys_files() {
	vector<string> vphys_files;
	for (const auto& entry : fs::directory_iterator(".")) {
		if (entry.path().extension() == ".vphys") {
			vphys_files.push_back(entry.path().string());
		}
	}
	return vphys_files;
}

vector<int> get_collision_attribute_indices(c_kv3_parser parser) {
	vector<int> indices;
	int index = 0;
	while (true) {
		string index_str = to_string(index);
		string collision_group_string = parser.get_value("m_collisionAttributes[" + index_str + "].m_CollisionGroupString");
		if (collision_group_string != "") {
			if (collision_group_string == "\"default\"" || collision_group_string == "\"Default\"") {
				indices.push_back(index);
			}
		}
		else {
			break;
		}
		index++;
	}
	return indices;

}