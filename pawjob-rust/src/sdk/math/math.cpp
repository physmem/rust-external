#include "math.hpp"
#include <app/winapp.hpp>
#include <game/game.hpp>

#define IA 16807
#define IM 2147483647
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB)
#define AM (1.0/IM)
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

namespace math
{
	bool world_to_screen(const glm::vec3& position, glm::vec2* out)
	{
		glm::mat4 vm = glm::transpose(game::impl::view_matrix);
		glm::vec2 size = winapp::impl::window_size;

		const glm::vec3 trans = { vm[3][0], vm[3][1], vm[3][2] };
		const glm::vec3 right = { vm[0][0], vm[0][1], vm[0][2] };
		const glm::vec3 up = { vm[1][0], vm[1][1], vm[1][2] };

		float w = glm::dot(trans, position) + vm[3][3];

		if (w < 0.001f)
			return false;

		float x = glm::dot(right, position) + vm[0][3];
		float y = glm::dot(up, position) + vm[1][3];

		out->x = (size.x / 2.0f) * (1.0f + x / w);
		out->y = (size.y / 2.0f) * (1.0f - y / w);

		return true;
	}

	glm::vec3 calc_angle(const glm::vec3& src, const glm::vec3& dst)
	{
		glm::vec3 delta = dst - src;

		float hyp = std::sqrt(delta.x * delta.x + delta.z * delta.z);
		glm::vec3 angles{};
		angles.x = -std::atan2(delta.y, hyp) * (180.0f / std::numbers::pi_v<float>);
		angles.y = std::atan2(delta.x, delta.z) * (180.0f / std::numbers::pi_v<float>);
		angles.z = 0.0f;

		return angles;
	}

	std::vector<glm::vec2> subdivide_arc(const std::vector<glm::vec2>& poly, float corner_fraction, int segments_per_arc)
	{
		if (poly.size() < 3)
			return poly;

		std::vector<glm::vec2> vec_out;
		vec_out.reserve(poly.size() * (segments_per_arc + 1));

		constexpr float eps = std::numeric_limits<float>::epsilon();
		const int n = static_cast<int>(poly.size());

		for (int i = 0; i < n; ++i)
		{
			const glm::vec2& p = poly[(i - 1 + n) % n];
			const glm::vec2& v = poly[i];
			const glm::vec2& q = poly[(i + 1) % n];

			glm::vec2 vec_previous_dir = { v.x - p.x, v.y - p.y };
			glm::vec2 vec_next_dir = { q.x - v.x, q.y - v.y };

			float len_prev = std::sqrt(vec_previous_dir.x * vec_previous_dir.x + vec_previous_dir.y * vec_previous_dir.y);
			float len_next = std::sqrt(vec_next_dir.x * vec_next_dir.x + vec_next_dir.y * vec_next_dir.y);

			if (len_prev < eps || len_next < eps)
			{
				vec_out.push_back(v);
				continue;
			}

			vec_previous_dir.x /= len_prev;
			vec_previous_dir.y /= len_prev;
			vec_next_dir.x /= len_next;
			vec_next_dir.y /= len_next;

			float fl_previous = std::min(len_prev * corner_fraction, len_prev - eps);
			float fl_next = std::min(len_next * corner_fraction, len_next - eps);

			glm::vec2 vec_a = { v.x - vec_previous_dir.x * fl_previous, v.y - vec_previous_dir.y * fl_previous };
			glm::vec2 vec_b = { v.x + vec_next_dir.x * fl_next, v.y + vec_next_dir.y * fl_next };

			vec_out.push_back(vec_a);

			for (int s = 1; s < segments_per_arc; ++s)
			{
				float t = static_cast<float>(s) / static_cast<float>(segments_per_arc);
				float u = 1.0f - t;

				glm::vec2 vec_bezier{};
				vec_bezier.x = (u * u * vec_a.x) + (2.0f * u * t * v.x) + (t * t * vec_b.x);
				vec_bezier.y = (u * u * vec_a.y) + (2.0f * u * t * v.y) + (t * t * vec_b.y);

				vec_out.push_back(vec_bezier);
			}

			vec_out.push_back(vec_b);
		}

		return vec_out;
	}


	static long idum = 0;
	void SeedRandomNumberGenerator(long lSeed)
	{
		if (lSeed)
		{
			idum = lSeed;
		}
		else
		{
			idum = -time(NULL);
		}
		if (1000 < idum)
		{
			idum = -idum;
		}
		else if (-1000 < idum)
		{
			idum -= 22261048;
		}
	}

	long ran1(void)
	{
		int j;
		long k;
		static long iy = 0;
		static long iv[NTAB];

		if (idum <= 0 || !iy)
		{
			if (-(idum) < 1) idum = 1;
			else idum = -(idum);
			for (j = NTAB + 7; j >= 0; j--)
			{
				k = (idum) / IQ;
				idum = IA * (idum - k * IQ) - IR * k;
				if (idum < 0) idum += IM;
				if (j < NTAB) iv[j] = idum;
			}
			iy = iv[0];
		}
		k = (idum) / IQ;
		idum = IA * (idum - k * IQ) - IR * k;
		if (idum < 0) idum += IM;
		j = iy / NDIV;
		iy = iv[j];
		iv[j] = idum;

		return iy;
	}

	float fran1(void)
	{
		float temp = (float)AM * ran1();
		if (temp > RNMX) return (float)RNMX;
		else return temp;
	}

	float random_float(float low, float high)
	{
		if (idum == 0)
			SeedRandomNumberGenerator(0);

		float fl = fran1(); // float in [0,1)
		return (fl * (high - low)) + low; // float in [low,high)
	}

	void angle_vectors(const glm::vec3& angles, glm::vec3* forward, glm::vec3* right, glm::vec3* up)
	{
		float sp = std::sin(glm::radians(angles.x));
		float cp = std::cos(glm::radians(angles.x));
		float sy = std::sin(glm::radians(angles.y));
		float cy = std::cos(glm::radians(angles.y));

		if (forward) {
			forward->x = cp * sy;
			forward->y = -sp;
			forward->z = cp * cy;
		}

		if (right) {
			float sr = std::sin(glm::radians(angles.z));
			float cr = std::cos(glm::radians(angles.z));

			right->x = cy * cr + sy * sp * sr;
			right->y = cp * sr;
			right->z = -sy * cr + cy * sp * sr;
		}

		if (up) {
			float sr = std::sin(glm::radians(angles.z));
			float cr = std::cos(glm::radians(angles.z));

			up->x = -cy * sr + sy * sp * cr;
			up->y = cp * cr;
			up->z = sy * sr + cy * sp * cr;
		}
	}

	glm::vec3 quaternion_to_euler(const glm::vec4& q)
	{
		glm::vec3 euler;

		// Pitch (x-axis rotation)
		float sinp = 2.0f * (q.w * q.x - q.y * q.z);
		if (std::abs(sinp) >= 1)
			euler.x = std::copysign(std::numbers::pi_v<float> / 2.0f, sinp);
		else
			euler.x = std::asin(sinp);

		// Yaw (y-axis rotation)
		float siny_cosp = 2.0f * (q.w * q.y + q.z * q.x);
		float cosy_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
		euler.y = std::atan2(siny_cosp, cosy_cosp);

		// Roll (z-axis rotation)
		float sinr_cosp = 2.0f * (q.w * q.z + q.x * q.y);
		float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
		euler.z = std::atan2(sinr_cosp, cosr_cosp);

		return { RAD2DEG(euler.x), RAD2DEG(euler.y), RAD2DEG(euler.z) };
	}

	bool ray_plane_intersection(const glm::vec3& ray_origin, const glm::vec3& ray_dir, const glm::vec3& plane_origin, const glm::vec3& plane_normal, float* distance, glm::vec3* intersection)
	{
		float dot = glm::dot(plane_normal, ray_dir);
		if (std::abs(dot) < 0.0001f)
			return false;

		float t = glm::dot(plane_normal, plane_origin - ray_origin) / dot;
		if (t < 0)
			return false;

		if (distance) *distance = t;
		if (intersection) *intersection = ray_origin + ray_dir * t;
		return true;
	}
}