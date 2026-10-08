#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/World/World.hpp>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

namespace sw
{
	/// The engine loop: owns the battle (its random source, its log and, once CREATE_MAP made it, its world) and
	/// advances it round by round:
	///   1. every living unit, in creation order, takes its Turn, and the Executor performs the effects it returns;
	///   2. RoundEnd reaches every unit (applied components such as poison tick);
	///   3. the dead are removed;
	///   4. the battle is over after a round in which nobody acted and no effect is pending.
	///
	/// A movable value: it owns everything it uses.
	///
	/// \code
	/// Simulation battle(std::make_unique<MersenneRandom>(seed), std::make_unique<TextLogPrinter>(std::cout));
	/// if (battle.createMap(10, 10))
	/// {
	///     const auto placed = battle.spawn({.position = {.x = 1, .y = 1}, .unit = makeUnit<Swordsman>(UnitId{1}, kit)});
	///     battle.run();                              // or step() until isFinished()
	/// }
	/// \endcode
	class Simulation
	{
	public:
		/// A battle with no world yet, drawing from `random` and writing its log to `log`. Throws std::invalid_argument
		/// if either is null.
		Simulation(std::unique_ptr<RandomSource> random, std::unique_ptr<LogSink> log);

		/// Creates the world on a `width` x `height` map and logs MAP_CREATED. Only once: WorldError::MapAlreadyCreated
		/// if there is a world, WorldError::MapEmpty for a zero side, WorldError::MapTooLarge above Map::MaxSide.
		[[nodiscard]]
		std::expected<void, WorldError> createMap(uint32_t width, uint32_t height);

		/// Places `order.unit` at `order.position` with the size and layer its PlacementQuery handlers give, and logs
		/// UNIT_SPAWNED. On NoMap, DuplicateId, OutOfBounds or CellOccupied nothing changes.
		[[nodiscard]]
		std::expected<void, WorldError> spawn(SpawnOrder order);

		/// A unit's component, mutable, for scenario commands such as MARCH that configure units: NoMap, UnknownUnit or
		/// MissingComponent if there is none.
		///
		/// \code
		/// const auto march = simulation.component<common::March>(UnitId{1});
		/// if (march)
		/// {
		///     (*march)->target = Position{.x = 5, .y = 5};
		/// }
		/// \endcode
		template <Component C>
		[[nodiscard]]
		std::expected<C*, WorldError> component(const UnitId id)
		{
			if (!_world)
			{
				return std::unexpected(WorldError::NoMap);
			}
			auto* unit = _world->units().find(id);
			if (unit == nullptr)
			{
				return std::unexpected(WorldError::UnknownUnit);
			}
			auto* found = unit->get<C>();
			if (found == nullptr)
			{
				return std::unexpected(WorldError::MissingComponent);
			}
			return found;
		}

		/// Writes `record` to the battle log at the current round (0 before the first step).
		void report(const Record& record);

		/// Runs one round: advances round(), lets every living unit act in creation order (each turn's effects are
		/// performed before the next unit's turn), ends the round, then removes the dead. Does nothing without a world.
		void step();

		/// `true` without a world, or after a round in which nobody acted and no effect was pending.
		[[nodiscard]]
		bool isFinished() const;

		void run();

		/// True once CREATE_MAP made the world.
		[[nodiscard]]
		bool hasMap() const;

		/// The battlefield, read-only: only the Executor changes it. Throws std::logic_error before CREATE_MAP.
		[[nodiscard]]
		const World& world() const;

		/// The round played last; 0 before the first step.
		[[nodiscard]]
		Round round() const;

		[[nodiscard]]
		RandomSource& randomSource() const;

		/// What `unit` would do on its turn in round `round`: the effects of its Turn, not performed. Empty if it would
		/// stay idle. Throws std::logic_error before CREATE_MAP.
		[[nodiscard]]
		Effects decide(UnitId unit, Round round);

		/// Performs `effects` from outside a handler (setup, tests), as one action of the current round: the way to
		/// apply a component to a unit, or to remove it, between rounds. Throws std::logic_error before CREATE_MAP.
		///
		/// \code
		/// simulation.perform(effect::apply(UnitId{1}, common::RangedAttack{.range = {2, 4}, .damage = Damage{3}}, UnitId{1}));
		/// \endcode
		void perform(Effects effects);

	private:
		// The unit's turn: it decides, and what it decided is performed. True if it acted.
		bool takeTurn(World& world, UnitId id);

		// Dead units leave the registry. One that died without being killed by an action (spawned at 0 hp) is reported
		// and taken off the map first.
		void removeDead(World& world);

		[[nodiscard]]
		static bool effectsPending(const World& world);

		std::unique_ptr<RandomSource> _random;
		std::unique_ptr<LogSink> _log;
		std::optional<World> _world;
		Round _round{0};
		bool _lastRoundActive{false};
	};
}
