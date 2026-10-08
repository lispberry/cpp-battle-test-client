#pragma once

#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/World.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace sw
{
	struct Health;

	/// Applies the game rules: the only writer of the World. Performs one unit's turn at a time:
	///   1. each effect runs through its pipeline (a hit: HitAttempt -> damage -> HitTaken), and the effects the
	///      handlers return run right after the step that produced them (depth-first, at most 16 deep);
	///   2. the log gets one UNIT_ATTACKED per (attacker, target) with the total damage of the action;
	///   3. units killed by the action leave the map (UNIT_DIED, in creation order). They stay in the registry, dead,
	///      until the Simulation removes them at the end of the round.
	///
	/// A short-lived view over the parts it works on, made for one round; use it directly only to test the rules.
	///
	/// \code
	/// Executor executor(world, random, log);
	/// executor.execute(std::move(effects), round);    // after each unit's turn that returned effects
	/// executor.endRound(round);                       // after every unit's turn
	/// \endcode
	class Executor
	{
	public:
		/// Works on `world`, draws from `random` for the handlers' rolls, writes to `log`. All three must outlive it.
		Executor(World& world, RandomSource& random, LogSink& log);

		/// Performs `effects` as one action of `round`: each effect and what it causes, then the log, then the deaths.
		/// Hits and damage on a unit that is gone, at 0 hp or without Health do nothing; a move that is no longer
		/// possible is dropped.
		void execute(Effects effects, Round round);

		/// Sends RoundEnd to every living unit (poison ticks here), as one action.
		void endRound(Round round);

	private:
		void run(Effect& effect, std::size_t depth);
		void runAll(Effects effects, std::size_t depth);

		[[nodiscard]]
		Context context(UnitId self) const;

		Effects apply(const HitEffect& hit);
		Effects apply(const AttackEffect& attack);
		Effects apply(const MoveEffect& move);
		Effects apply(ApplyEffect& application);
		Effects apply(const UnapplyEffect& removal);
		Effects apply(const AbilityEffect& ability);
		Effects apply(ReportEffect& report);
		Effects apply(const ChangeEffect& change);
		static Effects apply(const ExpireEffect& expire);

		Effects damage(
				UnitId attacker, UnitId target, Damage amount, std::optional<Ability> ability, const TagSet& tags);
		Health* livingHealth(UnitId id);
		void tally(UnitId attacker, UnitId target, Damage amount, Hp hpBefore);

		void flush();
		void buryKilled();

		std::reference_wrapper<World> _world;
		std::reference_wrapper<RandomSource> _random;
		std::reference_wrapper<LogSink> _log;
		Round _round;
		// The log of the action in progress, written by flush.
		std::vector<Record> _records;
		std::vector<UnitId> _killed;
	};
}
