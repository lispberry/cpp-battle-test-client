// Unit queries against the obvious loop: within / attackableBy / take select exactly the units a naive scan of the
// creation order selects, in that order, and pickRandom picks one of them.

#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Adaptors.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/UnitRef.hpp>
#include <Support/Arbitrary.hpp>
#include <Support/Battle.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Target.hpp>

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

namespace
{
	using sw::UnitRef;

	struct Probe
	{
		sw::fuzz::Bounded<std::size_t, 0, 15> caster;
		sw::Range range;
		sw::Attack attack;
		sw::fuzz::Bounded<std::size_t, 0, 8> limit;
	};
	SW_REFLECT(Probe, (), (caster, range, attack, limit))

	std::vector<sw::UnitId> ids(const sw::Units& units)
	{
		std::vector<sw::UnitId> result;
		for (const UnitRef unit : units)
		{
			result.push_back(unit.id());
		}
		return result;
	}

	void queriesAgreeWithNaiveScans(const sw::fuzz::Battlefield& battlefield, const Probe& probe)
	{
		sw::fuzz::Battle battle(battlefield);
		const sw::World& world = battle.simulation().world();
		const auto order = world.units().order();
		if (order.empty())
		{
			return;
		}
		const UnitRef self(world, order[probe.caster.value % order.size()]);
		const sw::Units others(world, self.id());

		std::vector<sw::UnitId> within;
		std::vector<sw::UnitId> attackable;
		for (const sw::UnitId id : order)
		{
			const UnitRef unit(world, id);
			if (id == self.id())
			{
				continue;
			}
			if (probe.range.contains(sw::distance(self, unit)))
			{
				within.push_back(id);
			}
			if (unit.isAttackableBy(self, probe.attack))
			{
				attackable.push_back(id);
			}
		}

		sw::fuzz::expect(ids(others | sw::within(self, probe.range)) == within, "within matches a scan");
		sw::fuzz::expect(
				ids(others | sw::attackableBy(self, probe.attack)) == attackable, "attackableBy matches a scan");
		std::vector<sw::UnitId> limited(
				within.begin(),
				within.begin() + static_cast<std::ptrdiff_t>(std::min(probe.limit.value, within.size())));
		sw::fuzz::expect(
				ids(others | sw::within(self, probe.range) | sw::take(probe.limit.value)) == limited,
				"take keeps the first n");
		sw::fuzz::expect((others | sw::within(self, probe.range) | sw::count()) == within.size(), "count counts");

		const std::expected<UnitRef, sw::Idle> picked
				= others | sw::within(self, probe.range) | sw::pickRandom(battle.random());
		sw::fuzz::expect(picked.has_value() == !within.empty(), "pickRandom finds a unit iff there is one");
		if (picked)
		{
			sw::fuzz::expect(std::ranges::find(within, picked->id()) != within.end(), "pickRandom picks a match");
		}
	}
}

SW_FUZZ_TARGET(queriesAgreeWithNaiveScans)
