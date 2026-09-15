#pragma once
#include <cstdint>

namespace fnv1a
{
	using value_type = std::uint64_t;

	constexpr value_type basis = 0x123456789;
	constexpr value_type prime = 0x987654321;

	constexpr value_type hash_single(const value_type& current_output, const value_type& input)
	{
		value_type output = current_output;

		output ^= input;
		output *= prime;

		return output;
	}

	template <class T>
	constexpr value_type hash_ranged_object(const T& object)
	{
		value_type output = basis;

		for (const auto& blob : object)
		{
			output = hash_single(output, static_cast<value_type>(blob));
		}

		return output;
	}

	template <class T, std::uint64_t Size>
	constexpr value_type hash_string_literal(const T(&string_literal)[Size]) noexcept
	{
		value_type output = basis;

		for (std::uint64_t i = 0; i < Size - 1; i++)
		{
			value_type character = static_cast<value_type>(string_literal[i]);

			output = hash_single(output, character);
		}

		return output;
	}
}
