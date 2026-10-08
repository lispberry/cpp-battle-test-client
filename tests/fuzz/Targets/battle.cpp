// Whole battles: any map, any mix of units with any stats and orders. After every round the world must be
// consistent; the battle must end; the log must never resurrect or heal anyone.

#include <Support/Battle.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Invariants.hpp>
#include <Support/Target.hpp>

namespace
{
	// Generous: a battle on a 16x16 map with 16 units and at most 50 hp each ends long before.
	constexpr int MaxRounds = 2000;

	void battleKeepsTheWorldConsistent(const sw::fuzz::Battlefield& battlefield)
	{
		sw::fuzz::Battle battle(battlefield);
		sw::Simulation& simulation = battle.simulation();
		int rounds = 0;
		while (!simulation.isFinished() && rounds < MaxRounds)
		{
			simulation.step();
			sw::fuzz::expectConsistent(simulation.world());
			++rounds;
		}
		sw::fuzz::expect(simulation.isFinished(), "every battle ends");
		sw::fuzz::expectLogConsistent(battle.log().records());
	}
}

SW_FUZZ_TARGET(battleKeepsTheWorldConsistent)
