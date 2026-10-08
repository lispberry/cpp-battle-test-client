#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Events.hpp>

#include <concepts>
#include <type_traits>

namespace sw
{
	class Effects;

	/// Component `C` handles event `E` (one of Events): it declares `Effects on(const E&)`, const or not.
	///
	/// \code
	/// static_assert(Handles<swordsman::Rending, HitAttempt>);
	/// static_assert(!Handles<Health, RoundEnd>);           // data only
	/// \endcode
	template <class C, class E>
	concept Handles = ClassType<std::remove_const_t<C>> && EventType<E> && requires(C& component, const E& event) {
		{ component.on(event) } -> std::same_as<Effects>;
	};

	/// Component `C` answers question `Q` (one of Queries): it declares `Q::Answer on(const Q&) const`.
	///
	/// \code
	/// static_assert(Answers<common::Flying, Targeted>);    // std::optional<Attack> on(const Targeted&) const
	/// static_assert(Answers<common::Body, PlacementQuery>);
	/// \endcode
	template <class C, class Q>
	concept Answers
			= ClassType<std::remove_const_t<C>> && QueryType<Q> && requires(const C& component, const Q& query) {
				  { component.on(query) } -> std::same_as<typename Q::Answer>;
			  };

	/// `C` declares an `on` for event `E` or question `E`, whatever its signature. Used to turn a handler with the wrong
	/// return type or constness into a compile error instead of a handler that never runs.
	///
	/// \code
	/// struct Broken { void on(const RoundEnd&) const; };   // declares, but does not Handles<Broken, RoundEnd>
	/// static_assert(Declares<Broken, RoundEnd>);
	/// \endcode
	template <class C, class E>
	concept Declares = requires(C& component, const E& event) { component.on(event); };
}
