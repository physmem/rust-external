#pragma once
#include <glm/glm.hpp>
#include <numbers>

constexpr float DEG2RAD(float deg) noexcept { return deg * (std::numbers::pi_v<float> / 180.0f); }
constexpr float RAD2DEG(float rad) noexcept { return rad * (180.0f / std::numbers::pi_v<float>); }

namespace math
{
	bool world_to_screen(const glm::vec3& position, glm::vec2* out);
	float random_float(float low, float high);
	glm::vec3 calc_angle(const glm::vec3& src, const glm::vec3& dst);

	std::vector<glm::vec2> subdivide_arc(const std::vector<glm::vec2>& poly, float corner_fraction = 0.12f, int segments_per_arc = 8);

	void angle_vectors(const glm::vec3& angles, glm::vec3* forward, glm::vec3* right = nullptr, glm::vec3* up = nullptr);
	glm::vec3 quaternion_to_euler(const glm::vec4& q);
	bool ray_plane_intersection(const glm::vec3& ray_origin, const glm::vec3& ray_dir, const glm::vec3& plane_origin, const glm::vec3& plane_normal, float* distance, glm::vec3* intersection);
}