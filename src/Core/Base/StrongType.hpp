#pragma once

#include <Core/Base/Concepts.hpp>

#include <compare>	// IWYU pragma: keep (required by the defaulted operator<=>)
#include <concepts>
#include <cstddef>
#include <functional>
#include <utility>

namespace sw
{
	/// A distinct type wrapping a value, so that e.g. a UnitId cannot be passed where an Hp is expected.
	///
	/// Only construction, access and comparison are provided. Arithmetic is deliberately left out:
	/// add it per type where it makes sense, instead of making every id addable.
	///
	/// \code
	/// using UnitId = StrongType<uint32_t, struct UnitIdTag>;
	/// using Hp = StrongType<uint32_t, struct HpTag>;
	///
	/// const UnitId id{7};
	/// const uint32_t raw = id.get();
	/// const bool same = id == UnitId{7};
	/// Hp hp = id;                                        // does not compile: different types
	/// \endcode
	template <std::semiregular T, ClassType Tag>
	class StrongType
	{
	public:
		using ValueType = T;

		constexpr StrongType() = default;

		/// Wraps `value`. Explicit, so that a raw number never becomes a UnitId by accident.
		constexpr explicit StrongType(T value) :
				_value(std::move(value))
		{}

		[[nodiscard]]
		constexpr const T& get() const noexcept
		{
			return _value;
		}

		[[nodiscard]]
		constexpr T& get() noexcept
		{
			return _value;
		}

		constexpr auto operator<=>(const StrongType&) const = default;

	private:
		T _value{};
	};
}

template <std::semiregular T, sw::ClassType Tag>
struct std::hash<sw::StrongType<T, Tag>>
{
	std::size_t operator()(const sw::StrongType<T, Tag>& value) const noexcept
	{
		return std::hash<T>{}(value.get());
	}
};
