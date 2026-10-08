// Poison, isolated from battles: over its five round ends it deals exactly its total, plus one extra portion for
// each round in which the target took a Rending hit; every tick is the hunter's.

#include <Core/Base/MersenneRandom.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/Tags.hpp>
#include <Core/Model/Types.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/World.hpp>
#include <Features/Common/Tags.hpp>
#include <Features/Hunter/Poison.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Target.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace
{
	using namespace sw;

	// The poison damage in `effects`, all of it the hunter's.
	uint32_t dealt(sw::Effects effects)
	{
		uint32_t total = 0;
		for (const sw::Effect& effect : effects.take())
		{
			const auto& attack = std::get<sw::AttackEffect>(effect);
			sw::fuzz::expect(attack.attacker == UnitId{1}, "poison damage is the hunter's");
			total += attack.damage.get();
		}
		return total;
	}

	void poisonDealsItsTotal(const Damage total, const std::array<bool, 5>& rended, const bool plainHits)
	{
		const std::vector<Damage> portions = sw::hunter::split(total, sw::hunter::Poison::Duration);
		uint32_t expected = 0;
		for (std::size_t i = 0; i < portions.size(); ++i)
		{
			expected += portions[i].get() * (rended.at(i) ? 2 : 1);
		}

		// The poison reads only its own unit and the round: an empty battlefield is enough.
		const sw::World world(std::make_unique<sw::GridMap>(1, 1));
		sw::MersenneRandom random(0);
		uint32_t dealtTotal = 0;
		sw::hunter::Poison poison(UnitId{1}, total);
		bool expired = false;
		for (uint32_t round = 1; round <= 5; ++round)
		{
			sw::fuzz::expect(!expired, "poison lasts five rounds");
			const sw::Context context(world, UnitId{2}, Round{round}, random);
			if (plainHits)
			{
				sw::fuzz::expect(
						poison.on(sw::HitTaken(context, {.attacker = UnitId{3}, .target = UnitId{2}})).empty(),
						"a hit causes nothing");
			}
			if (rended.at(round - 1))
			{
				sw::fuzz::expect(
						poison.on(sw::HitTaken(
										  context,
										  {.attacker = UnitId{3},
										   .target = UnitId{2},
										   .ability = sw::Ability{"rending"},
										   .tags = sw::TagSet(sw::TagList<sw::common::Wound>{})}))
								.empty(),
						"a rending hit causes nothing");
			}
			auto ticked = poison.on(sw::RoundEnd(context));
			expired = ticked.removeExpire();
			dealtTotal += dealt(std::move(ticked));
		}
		sw::fuzz::expect(expired, "poison expires after its last portion");
		sw::fuzz::expect(dealtTotal == expected, "poison deals its total, doubled in rending rounds");
	}
}

SW_FUZZ_TARGET(poisonDealsItsTotal)
