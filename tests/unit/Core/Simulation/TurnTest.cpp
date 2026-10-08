#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/World/UnitRef.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <expected>

using namespace sw;

namespace
{
	struct Haste
	{
		[[nodiscard]]
		Speed on(const SpeedQuery& query) const
		{
			return Speed{query.speed.get() + 1};
		}
	};

	struct RunnerKit
	{
		Health health{Hp{5}};
		Haste haste;
	};
	SW_REFLECT(RunnerKit, (), (health, haste))

	class Runner final : public Unit<Runner, RunnerKit>
	{
	public:
		static constexpr UnitName Name{"runner"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& /*turn*/) const
		{
			return {};
		}
	};
}

TEST_CASE("Turn reads the world as it was when the turn started")
{
	test::TestWorld world;
	world.spawn<Runner>(1, {.x = 2, .y = 2});
	world.spawn<test::Dummy>(2, {.x = 3, .y = 3});
	world.random().script({5});
	const Turn turn(world.world(), UnitId{1}, Round{4}, world.random());

	CHECK(turn.self().id() == UnitId{1});
	CHECK(turn.round() == Round{4});
	CHECK((turn.units() | count()) == 1);
	CHECK(&turn.map() == &world.world().map());
	CHECK(&turn.randomSource() == &world.random());
	CHECK(turn.roll(Chance{5}));
	CHECK(turn.speed(Speed{1}) == Speed{2});
	CHECK((turn.units() | pickRandom(turn))->id() == UnitId{2});
	CHECK(world.unit(1).position() == Position{2, 2});
	CHECK(Runner(UnitId{3}, {}).on(turn).empty());
}
