#pragma once

#include <Core/Base/Concepts.hpp>

#include <boost/describe/bases.hpp>
#include <boost/describe/class.hpp>
#include <boost/describe/members.hpp>
#include <boost/describe/modifiers.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

/// Declares the bases and fields of `Type` for sw::reflect, at namespace scope right after the type, in its namespace.
/// The lists are parenthesised and may be empty. Every base and every field must be listed: sw::reflect checks it.
///
/// \code
/// struct Rending
/// {
///     Chance chance;
///     Damage damage;
/// };
/// SW_REFLECT(Rending, (), (chance, damage))
/// \endcode
#define SW_REFLECT(Type, Bases, Fields) BOOST_DESCRIBE_STRUCT(Type, Bases, Fields)

/// Reflection of the types declared with SW_REFLECT: their fields in declaration order, with names. The engine walks a
/// unit's kit (events, the turn), a scenario command (parsing) and a log record (printing) through it. Feature code
/// only declares types; the backend (Boost.Describe today, C++26 reflection later) stays behind this header.
namespace sw::reflect
{
	namespace detail
	{
		template <ClassType T>
		using Fields = boost::describe::describe_members<T, boost::describe::mod_public>;

		template <ClassType T>
		using Bases = boost::describe::describe_bases<T, boost::describe::mod_public>;

		// Converts to any field type; only used unevaluated, to count an aggregate's initialisers.
		struct AnyField
		{
			template <std::destructible F>
			// NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor, misc-explicit-constructor): counting needs it implicit
			operator F() const;
		};

		template <Aggregate T, std::size_t... Index>
		consteval bool initialisableWith(std::index_sequence<Index...> /*initialisers*/)
		{
			return requires { T{(void(Index), AnyField{})...}; };
		}

		// How many initialisers the aggregate takes: one per base, then one per field.
		template <Aggregate T, std::size_t Count = 0>
		consteval std::size_t initialisers()
		{
			if constexpr (Count < 64 && initialisableWith<T>(std::make_index_sequence<Count + 1>{}))
			{
				return initialisers<T, Count + 1>();
			}
			else
			{
				return Count;
			}
		}
	}

	/// A type declared with SW_REFLECT whose description lists every base and field. An aggregate is checked against
	/// its initialisers, so a field left out of the declaration is a compile error, not a field that is silently
	/// skipped.
	///
	/// \code
	/// struct Rending { Chance chance; Damage damage; };
	/// SW_REFLECT(Rending, (), (chance, damage))
	/// static_assert(reflect::Described<Rending>);
	/// \endcode
	template <class T>
	concept Described = boost::describe::has_describe_members<std::remove_cv_t<T>>::value
						&& boost::describe::has_describe_bases<std::remove_cv_t<T>>::value
						&& (!Aggregate<std::remove_cv_t<T>>
							|| detail::initialisers<std::remove_cv_t<T>>()
									   == boost::mp11::mp_size<detail::Bases<std::remove_cv_t<T>>>::value
												  + boost::mp11::mp_size<detail::Fields<std::remove_cv_t<T>>>::value);

	/// The number of fields of `T`, bases excluded.
	template <Described T>
	inline constexpr std::size_t fieldCount = boost::mp11::mp_size<detail::Fields<T>>::value;

	/// The type of the field at `Index`, in declaration order.
	template <Described T, std::size_t Index>
	using FieldType = std::remove_cvref_t<
			decltype(std::declval<T&>().*boost::mp11::mp_at_c<detail::Fields<T>, Index>::pointer)>;

	/// Calls `visit(field)` for each field of `value`, in declaration order. `value` may be const.
	template <Described T, ClassType Visit>
	constexpr void forEachField(T& value, Visit visit)
	{
		boost::mp11::mp_for_each<detail::Fields<std::remove_cv_t<T>>>(
				[&value, &visit]<ClassType Field>(Field /*descriptor*/) { visit(value.*Field::pointer); });
	}

	/// Calls `visit(name, field)` for each field of `value`, in declaration order, with the field's name as declared.
	template <Described T, ClassType Visit>
	constexpr void forEachNamedField(T& value, Visit visit)
	{
		boost::mp11::mp_for_each<detail::Fields<std::remove_cv_t<T>>>(
				[&value, &visit]<ClassType Field>(Field /*descriptor*/)
				{ visit(std::string_view(Field::name), value.*Field::pointer); });
	}
}
