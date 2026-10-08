#pragma once

#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/World.hpp>

#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sw
{
	/// A question asked of one unit: copyable and callable as `bool(const UnitRef&)`.
	///
	/// \code
	/// const auto isHunter = [](const UnitRef& unit) { return unit.name() == UnitName{"hunter"}; };
	/// static_assert(UnitPredicate<decltype(isHunter)>);
	/// const auto hunters = turn.units() | where(isHunter);
	/// \endcode
	template <class P>
	concept UnitPredicate = std::copy_constructible<P> && std::predicate<const P&, const UnitRef&>;

	/// The adaptor `where` returns: a predicate waiting to be applied to Units with `|`.
	///
	/// \code
	/// const auto filter = where([](const UnitRef& unit) { return unit.has<Health>(); });
	/// const auto mortal = turn.units() | filter;
	/// \endcode
	template <UnitPredicate P>
	struct Where
	{
		P predicate;
	};

	/// The adaptor `take` returns: a limit on the number of units yielded.
	struct Take
	{
		std::size_t count;
	};

	/// A lazy selection of living units in creation order, piped like a std::ranges view: adaptors narrow it,
	/// a terminal answers a question about it, and a range-for walks it.
	///
	/// `|` with an adaptor only records it; nothing is evaluated until iteration or a terminal (`any`, `none`,
	/// `count`, `first`, `pickRandom`). `|` returns a new Units, so one selection can start several pipelines.
	///
	/// \code
	/// const auto self = turn.self();
	/// const bool engaged = turn.units() | adjacentTo(self) | any();
	/// const auto target = turn.units() | attackableBy(self, common::melee()) | pickRandom(turn);
	/// for (const UnitRef unit : turn.units() | with<Health>() | take(3))
	/// {
	///     actions.attack(self.id(), unit.id(), Damage{1});
	/// }
	/// \endcode
	class Units
	{
	public:
		/// Input iterator over the accepted units; ends at std::default_sentinel.
		class Iterator
		{
		public:
			using value_type = UnitRef;
			using difference_type = std::ptrdiff_t;

			Iterator() = default;

			[[nodiscard]]
			UnitRef operator*() const;

			Iterator& operator++();

			Iterator operator++(int);

			[[nodiscard]]
			bool operator==(std::default_sentinel_t /*end*/) const;

		private:
			friend class Units;

			Iterator(const Units& units, std::size_t index);

			// Moves to the next unit every filter accepts. Not called once the limit is reached, so filters run only
			// for units that are yielded or skipped on the way.
			void skipRejected();

			[[nodiscard]]
			bool limitReached() const;

			const Units* _units{nullptr};
			std::size_t _index{0};
			std::size_t _yielded{0};
		};

		/// Every living unit of `world` except `excluded` (the unit whose turn it is). Inside a handler, use
		/// `turn.units()` instead.
		Units(const World& world, std::optional<UnitId> excluded);

		/// The first accepted unit; runs the filters up to it.
		[[nodiscard]]
		Iterator begin() const;

		[[nodiscard]]
		static std::default_sentinel_t end();

		template <UnitPredicate P>
		friend Units operator|(Units units, Where<P> where)
		{
			units._filters.emplace_back(std::move(where.predicate));
			return units;
		}

		friend Units operator|(Units units, Take take);

	private:
		[[nodiscard]]
		bool accepts(UnitId id) const;

		const World* _world;
		std::span<const UnitId> _order;
		std::optional<UnitId> _excluded;
		std::vector<std::function<bool(const UnitRef&)>> _filters;
		std::optional<std::size_t> _limit;
	};

	/// Keeps the units satisfying `predicate`: the building block of every game adaptor. Wrap it in a named
	/// function to add a word to the query vocabulary.
	///
	/// \code
	/// inline auto fragile(const Hp threshold)
	/// {
	///     return where([threshold](const UnitRef& unit)
	///     {
	///         const auto* health = unit.get<Health>();
	///         return health != nullptr && health->hp <= threshold;
	///     });
	/// }
	/// const auto prey = turn.units() | adjacentTo(turn.self()) | fragile(Hp{5}) | first();
	/// \endcode
	template <UnitPredicate P>
	[[nodiscard]]
	Where<P> where(P predicate)
	{
		return Where<P>{std::move(predicate)};
	}

	/// At most `limit` units; chained limits keep the smallest.
	///
	/// \code
	/// const auto twoNeighbours = turn.units() | adjacentTo(turn.self()) | take(2);
	/// const auto stillTwo = twoNeighbours | take(5);
	/// \endcode
	[[nodiscard]]
	Take take(std::size_t limit);

	/// The type of the `any` terminal.
	struct Any
	{};

	/// The type of the `none` terminal.
	struct None
	{};

	/// The type of the `count` terminal.
	struct Count
	{};

	/// The type of the `first` terminal.
	struct First
	{};

	/// The type `pickRandom` returns: the terminal with the source to draw from.
	struct PickRandom
	{
		RandomSource* random;
	};

	/// Terminal: `true` if the selection has at least one unit. Stops at the first one.
	///
	/// \code
	/// const bool engaged = turn.units() | adjacentTo(turn.self()) | any();
	/// \endcode
	[[nodiscard]]
	constexpr Any any() noexcept
	{
		return {};
	}

	/// Terminal: `true` if the selection is empty.
	[[nodiscard]]
	constexpr None none() noexcept
	{
		return {};
	}

	/// Terminal: the number of units in the selection, as std::size_t.
	[[nodiscard]]
	constexpr Count count() noexcept
	{
		return {};
	}

	/// Terminal: the first unit in creation order, or Idle::NoTarget if there is none.
	///
	/// \code
	/// const auto neighbour
	///         = turn.units() | adjacentTo(turn.self()) | first();
	/// \endcode
	[[nodiscard]]
	constexpr First first() noexcept
	{
		return {};
	}

	/// Terminal: one unit chosen uniformly in a single pass without storage (reservoir sampling: `random.oneIn(k)`
	/// for the k-th unit from the second on), or Idle::NoTarget without drawing if the selection is empty. Inside a handler, `pickRandom(turn)` passes the event's source.
	///
	/// \code
	/// const auto target
	///         = turn.units() | attackableBy(turn.self(), common::melee())
	///           | pickRandom(turn.randomSource());
	/// \endcode
	[[nodiscard]]
	PickRandom pickRandom(RandomSource& random);

	/// pickRandom with the battle's randomness, from a turn or any other event: `... | pickRandom(turn)`.
	[[nodiscard]]
	PickRandom pickRandom(const Context& context);

	[[nodiscard]]
	bool operator|(const Units& units, Any /*terminal*/);

	[[nodiscard]]
	bool operator|(const Units& units, None /*terminal*/);

	[[nodiscard]]
	std::size_t operator|(const Units& units, Count /*terminal*/);

	[[nodiscard]]
	std::expected<UnitRef, Idle> operator|(const Units& units, First /*terminal*/);

	[[nodiscard]]
	std::expected<UnitRef, Idle> operator|(const Units& units, PickRandom terminal);
}
