#pragma once
#include "math.hpp"
#include <glm/glm.hpp>

namespace valve
{
	struct Matrix3x4
	{
		Matrix3x4() = default;

		constexpr Matrix3x4(
			const float m00, const float m01, const float m02, const float m03,
			const float m10, const float m11, const float m12, const float m13,
			const float m20, const float m21, const float m22, const float m23)
		{
			mat[0][0] = m00;
			mat[0][1] = m01;
			mat[0][2] = m02;
			mat[0][3] = m03;
			mat[1][0] = m10;
			mat[1][1] = m11;
			mat[1][2] = m12;
			mat[1][3] = m13;
			mat[2][0] = m20;
			mat[2][1] = m21;
			mat[2][2] = m22;
			mat[2][3] = m23;
		}

		constexpr Matrix3x4(const glm::vec3& vecForward, const glm::vec3& vecLeft, const glm::vec3& vecUp, const glm::vec3& vecOrigin)
		{
			SetForward(vecForward);
			SetLeft(vecLeft);
			SetUp(vecUp);
			SetOrigin(vecOrigin);
		}

		[[nodiscard]] float* operator[](const int nIndex)
		{
			return mat[nIndex];
		}

		[[nodiscard]] const float* operator[](const int nIndex) const
		{
			return mat[nIndex];
		}

		glm::vec3 operator*(const glm::vec3& vec) const
		{
			glm::vec3 result;
			for (int i = 0; i < 3; i++)
			{
				result[i] = mat[i][0] * vec.x +
					mat[i][1] * vec.y +
					mat[i][2] * vec.z +
					mat[i][3];
			}
			return result;
		}

		Matrix3x4 operator+(const Matrix3x4& dataIn) const
		{
			Matrix3x4 result;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					result.mat[i][j] = mat[i][j] + dataIn[i][j];
				}
			}
			return result;
		}

		Matrix3x4 operator+(const Matrix3x4& dataIn)
		{
			Matrix3x4 result;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					result.mat[i][j] = mat[i][j] + dataIn[i][j];
				}
			}
			return result;
		}

		Matrix3x4 operator*(const Matrix3x4& dataIn)
		{
			Matrix3x4 result;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					result.mat[i][j] = mat[i][j] * dataIn[i][j];
				}
			}
			return result;
		}

		Matrix3x4 operator*(const float& dataIn)
		{
			Matrix3x4 result;
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					result.mat[i][j] = mat[i][j] * dataIn;
				}
			}
			return result;
		}

		constexpr void SetForward(const glm::vec3& vecForward)
		{
			mat[0][0] = vecForward.x;
			mat[1][0] = vecForward.y;
			mat[2][0] = vecForward.z;
		}

		constexpr void SetLeft(const glm::vec3& vecLeft)
		{
			mat[0][1] = vecLeft.x;
			mat[1][1] = vecLeft.y;
			mat[2][1] = vecLeft.z;
		}

		constexpr void SetUp(const glm::vec3& vecUp)
		{
			mat[0][2] = vecUp.x;
			mat[1][2] = vecUp.y;
			mat[2][2] = vecUp.z;
		}

		constexpr void SetOrigin(const glm::vec3& vecOrigin)
		{
			mat[0][3] = vecOrigin.x;
			mat[1][3] = vecOrigin.y;
			mat[2][3] = vecOrigin.z;
		}

		__forceinline float* Base()
		{
			return &mat[0][0];
		}

		__forceinline const float* Base() const
		{
			return &mat[0][0];
		}

		[[nodiscard]] constexpr glm::vec3 GetForward() const
		{
			return { mat[0][0], mat[1][0], mat[2][0] };
		}

		[[nodiscard]] constexpr glm::vec3 GetLeft() const
		{
			return { mat[0][1], mat[1][1], mat[2][1] };
		}

		[[nodiscard]] constexpr glm::vec3 GetUp() const
		{
			return { mat[0][2], mat[1][2], mat[2][2] };
		}

		[[nodiscard]] constexpr glm::vec3 GetOrigin() const
		{
			return { mat[0][3], mat[1][3], mat[2][3] };
		}

		[[nodiscard]] glm::vec3 GetRotation() const
		{
			glm::vec3 angles;

			const float forwardX = mat[0][0];
			const float forwardY = mat[1][0];
			const float forwardZ = mat[2][0];

			angles.x = RAD2DEG(std::atan2(forwardY, forwardX));

			const float forwardXY = std::sqrt(forwardX * forwardX + forwardY * forwardY);
			angles.y = RAD2DEG(std::atan2(-forwardZ, forwardXY));

			const float upX = mat[0][2];
			const float upY = mat[1][2];
			const float upZ = mat[2][2];

			const float leftX = upY * forwardZ - upZ * forwardY;
			const float leftY = upZ * forwardX - upX * forwardZ;

			angles.z = RAD2DEG(std::atan2(leftY, leftX));

			return angles;
		}

		constexpr void Invalidate()
		{
			for (auto& arrSubData : mat)
			{
				for (auto& flData : arrSubData)
					flData = std::numeric_limits<float>::infinity();
			}
		}

		[[nodiscard]] constexpr Matrix3x4 ConcatTransforms(const Matrix3x4& matOther) const
		{
			return {
				mat[0][0] * matOther.mat[0][0] + mat[0][1] * matOther.mat[1][0] + mat[0][2] * matOther.mat[2][0],
				mat[0][0] * matOther.mat[0][1] + mat[0][1] * matOther.mat[1][1] + mat[0][2] * matOther.mat[2][1],
				mat[0][0] * matOther.mat[0][2] + mat[0][1] * matOther.mat[1][2] + mat[0][2] * matOther.mat[2][2],
				mat[0][0] * matOther.mat[0][3] + mat[0][1] * matOther.mat[1][3] + mat[0][2] * matOther.mat[2][3] + mat[0][3],

				mat[1][0] * matOther.mat[0][0] + mat[1][1] * matOther.mat[1][0] + mat[1][2] * matOther.mat[2][0],
				mat[1][0] * matOther.mat[0][1] + mat[1][1] * matOther.mat[1][1] + mat[1][2] * matOther.mat[2][1],
				mat[1][0] * matOther.mat[0][2] + mat[1][1] * matOther.mat[1][2] + mat[1][2] * matOther.mat[2][2],
				mat[1][0] * matOther.mat[0][3] + mat[1][1] * matOther.mat[1][3] + mat[1][2] * matOther.mat[2][3] + mat[1][3],

				mat[2][0] * matOther.mat[0][0] + mat[2][1] * matOther.mat[1][0] + mat[2][2] * matOther.mat[2][0],
				mat[2][0] * matOther.mat[0][1] + mat[2][1] * matOther.mat[1][1] + mat[2][2] * matOther.mat[2][1],
				mat[2][0] * matOther.mat[0][2] + mat[2][1] * matOther.mat[1][2] + mat[2][2] * matOther.mat[2][2],
				mat[2][0] * matOther.mat[0][3] + mat[2][1] * matOther.mat[1][3] + mat[2][2] * matOther.mat[2][3] + mat[2][3]
			};
		}

		float mat[3][4] = {};
	};
}