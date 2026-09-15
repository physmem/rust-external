#pragma once
#include "crt.hpp"
#include "hash.hpp"

namespace isyscall
{
	bool load();
	void unload();
}

class syscall_t
{
public:
	class status_t
	{
	public:
		using value_type = std::uint32_t;

		constexpr static value_type length_mismatch = 0xC0000004;
		constexpr static value_type buffer_too_small = 0xC0000023;
		constexpr static value_type not_found = 0xC0000225;
		constexpr static value_type port_not_set = 0xC0000353;

		status_t(const value_type code)
				: code_(code) {}

		explicit operator bool() const noexcept
		{
			return code_ == 0;
		}

		[[nodiscard]] value_type code() const noexcept
		{
			return code_;
		}

	protected:
		value_type code_;
	};

	syscall_t() = default;

	explicit syscall_t(const std::uint32_t id) noexcept
		: id_(id),
		stub_(nullptr) {
	}

	syscall_t(const syscall_t&) = delete;
	syscall_t& operator=(const syscall_t&) = delete;

	syscall_t(syscall_t&& right) noexcept
		: id_(right.id_),
		stub_(right.stub_)
	{
		right.stub_ = nullptr;
	}

	syscall_t& operator=(syscall_t&& right) noexcept
	{
		if (this != &right)
		{
			reset();

			stub_ = right.stub_;
			id_ = right.id_;

			right.stub_ = nullptr;

			right.reset();
		}

		return *this;
	}

	~syscall_t();

	template <class... Arguments>
	status_t operator()(Arguments... arguments) noexcept
	{
		if (!stub_ && !load_stub())
		{
			return { status_t::not_found };
		}

		using syscall_fn_t = status_t::value_type(__fastcall*)(Arguments...);

		return { reinterpret_cast<syscall_fn_t>(stub_)(arguments...) };
	}

	[[nodiscard]] std::uint32_t id() const
	{
		return id_;
	}

	void reset();

	static syscall_t& find(fnv1a::value_type hash);

	template <fnv1a::value_type Hash>
	static syscall_t& find()
	{
		return find(Hash);
	}

	bool load_stub();
	bool load_stub(const crt::function_t<void*(std::uint64_t)>& allocation_fn);

	void unload_stub();
	void unload_stub(const crt::function_t<void(void*)>& deallocation_fn);

protected:
	std::uint32_t id_;
	std::uint8_t* stub_;
};

#define ISYSCALL(syscall_name) syscall_t::find<fnv1a::hash_string_literal(#syscall_name)>()
