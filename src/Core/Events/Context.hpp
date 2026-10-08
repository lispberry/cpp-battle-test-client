#pragma once

#include <Core/Base/RandomSource.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	class World;
	class UnitRef;
	class Units;
	class Map;

	/// What every event shows its handlers: the unit it happens to (`self`), the round, a read-only view of the
	/// battlefield and the battle's randomness. Handlers decide from it and answer with Effects; nothing here changes
	/// the world. Built by the engine (Simulation, Executor) for each event.
	///
	/// Declared here, below the World, and defined with the simulation (Core/Simulation/Context.cpp): an event names
	/// the world without depending on it.
	class Context
	{
	public:
		Context(const World& world, UnitId self, Round round, RandomSource& random);

		/// The unit the event happens to: the one whose turn it is, the attacker of a HitAttempt, the target of a
		/// HitTaken.
		[[nodiscard]]
		UnitRef self() const;

		[[nodiscard]]
		Round round() const;

		/// The living units other than `self`, to query (`context.units() | adjacentTo(context.self())`).
		[[nodiscard]]
		Units units() const;

		[[nodiscard]]
		const Map& map() const;

		[[nodiscard]]
		RandomSource& randomSource() const;

		/// Rolls `chance` out of 1000 against the battle's randomness.
		[[nodiscard]]
		bool roll(Chance chance) const;

	protected:
		[[nodiscard]]
		const World& world() const;

		[[nodiscard]]
		UnitId selfId() const;

	private:
		const World* _world;
		UnitId _self;
		Round _round;
		RandomSource* _random;
	};
}
