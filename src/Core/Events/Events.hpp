#pragma once

#include <concepts>

namespace sw
{
	class Turn;
	class HitAttempt;
	class HitTaken;
	class RoundEnd;
	struct Targeted;
	struct PlacementQuery;
	struct SpeedQuery;

	/// An event: something happens to a unit, and its handlers answer with the Effects it causes. A handler is
	/// `Effects on(const E&)`; it may change its own component. Each event says how its answers combine in
	/// `E::FirstAnswerWins`.
	///
	/// \code
	/// static_assert(EventType<RoundEnd> && !EventType<SpeedQuery>);
	/// \endcode
	template <class E>
	concept EventType = std::same_as<E, Turn> || std::same_as<E, HitAttempt> || std::same_as<E, HitTaken>
						|| std::same_as<E, RoundEnd>;

	/// A question: something is asked about a unit, and each handler returns the answer as it sees it, in the type the
	/// question is about (`Q::Answer`). A handler is `Q::Answer on(const Q&) const`.
	///
	/// \code
	/// static_assert(QueryType<SpeedQuery> && !QueryType<RoundEnd>);
	/// \endcode
	template <class Q>
	concept QueryType = std::same_as<Q, Targeted> || std::same_as<Q, PlacementQuery> || std::same_as<Q, SpeedQuery>;
}
