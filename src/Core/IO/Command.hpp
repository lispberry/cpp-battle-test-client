#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Base/Reflect.hpp>

#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>

namespace sw
{
	namespace detail
	{
		template <reflect::Described C, std::size_t... Index>
		consteval bool allUnsigned(std::index_sequence<Index...> /*fields*/)
		{
			return (std::unsigned_integral<reflect::FieldType<C, Index>> && ...);
		}
	}

	/// A scenario command: an aggregate declared with SW_REFLECT, with a static `Name` (the line's first word), whose
	/// fields, all unsigned integers, are the line's arguments in declaration order. Register it with Commands::add.
	///
	/// \code
	/// struct MarchCommand                                     // MARCH 1 5 5
	/// {
	///     static constexpr const char* Name = "MARCH";
	///     uint32_t unitId{};
	///     uint32_t targetX{};
	///     uint32_t targetY{};
	/// };
	/// SW_REFLECT(MarchCommand, (), (unitId, targetX, targetY))
	/// static_assert(Command<MarchCommand>);
	/// \endcode
	template <class C>
	concept Command = Aggregate<C> && reflect::Described<C> && requires {
		{ C::Name } -> std::convertible_to<std::string_view>;
	} && detail::allUnsigned<C>(std::make_index_sequence<reflect::fieldCount<C>>{});
}
