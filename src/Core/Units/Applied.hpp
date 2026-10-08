#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Events.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Model/Types.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace sw
{
	/// The components applied to a unit during the battle (a poison, a timed shield, a picked-up bow), in the order
	/// they were applied, each with the unit that applied it. They take part in every event and question after the
	/// unit's kit, except the turn, which asks them first and newest first: what a unit picked up last decides. Feature
	/// code applies them with `effect::apply` and removes them with `effect::unapply` or `effect::expire`.
	///
	/// \code
	/// Applied& applied = unit.applied();
	/// applied.add(std::make_unique<EffectOf<hunter::Poison>>(hunter::Poison{source, Damage{10}}), source);
	/// applied.has<hunter::Poison>();                     // true
	/// Effects effects = applied.dispatch(roundEnd);      // a portion; the poison expires after the last one
	/// \endcode
	class Applied
	{
	public:
		/// Appends `component`, applied by `source`.
		void add(std::unique_ptr<Hooks> component, UnitId source);

		/// Removes every component `source` applied.
		void remove(UnitId source);

		/// True if no component is applied. The battle is not over while some unit still carries one.
		[[nodiscard]]
		bool empty() const;

		/// True if a component of type `C` is applied, on its own or inside a decorator that `Wraps` it:
		/// `has<common::Invulnerable>()` is true for a `Timed<common::Invulnerable>`.
		template <Component C>
		[[nodiscard]]
		bool has() const
		{
			return has(typeid(C));
		}

		/// has<C>, by the component's type.
		[[nodiscard]]
		bool has(std::type_index type) const;

		/// Asks the applied components about `event` (see the event's FirstAnswerWins) and removes those that expire.
		template <EventType E>
		[[nodiscard]]
		Effects dispatch(const E& event)
		{
			return ask(
					{.firstAnswerWins = E::FirstAnswerWins, .newestFirst = std::is_same_v<E, Turn>},
					[&event](Hooks& component) { return component.dispatch(event); });
		}

		/// Asks the applied components `query`, each from the answer so far.
		template <QueryType Q>
		void dispatch(Q& query) const
		{
			for (const Entry& entry : _entries)
			{
				entry.component->dispatch(query);
			}
		}

	private:
		struct Entry
		{
			std::unique_ptr<Hooks> component;
			UnitId source;
		};

		// How the answers of one event combine.
		struct Asking
		{
			bool firstAnswerWins;
			bool newestFirst;
		};

		// Asks each component with `dispatch`, combines the answers, removes the components that expire. One function
		// for every event, so its rules are written (and tested) once.
		[[nodiscard]]
		Effects ask(Asking asking, const std::function<Effects(Hooks&)>& dispatch);

		void removeAt(std::vector<std::size_t> indices);

		std::vector<Entry> _entries;
	};
}
