#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Event.hpp>
#include <Core/Events/Events.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Model/Types.hpp>

#include <concepts>
#include <utility>

namespace sw::common
{
	// Any component as an applied one that lasts `left` round ends, counting the end of the round it was applied in,
	// then removes itself. Forwards every event and question the wrapped component handles, and `has<C>()` sees
	// through it: `has<Invulnerable>()` is true for a timed shield.
	//   // Shields the caster until the end of the next round.
	//   return effect::apply(turn.self().id(), Timed<Invulnerable>{.component = {}, .left = Rounds{2}}, turn.self().id());
	template <Component C>
	struct Timed
	{
		using Wraps = C;

		C component;
		Rounds left;

		template <QueryType Q>
			requires Answers<C, Q>
		[[nodiscard]]
		Q::Answer on(const Q& query) const
		{
			return component.on(query);
		}

		template <EventType E>
			requires(Handles<C, E> && !std::same_as<E, RoundEnd>)
		[[nodiscard]]
		Effects on(const E& event)
		{
			return component.on(event);
		}

		// Forwards to the component's RoundEnd handler if it has one, then counts `left` down and expires at zero.
		[[nodiscard]]
		Effects on(const RoundEnd& end)
		{
			Effects effects;
			if constexpr (Handles<C, RoundEnd>)
			{
				effects = component.on(end);
			}
			if (left.get() <= 1)
			{
				left = Rounds{0};
				return std::move(effects) | effect::expire();
			}
			left = Rounds{left.get() - 1};
			return effects;
		}
	};
}
