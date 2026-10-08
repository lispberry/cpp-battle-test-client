#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Applied.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/Flying.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/Timed.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>
#include <Support/TestWorld.hpp>

#include <doctest/doctest.h>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <variant>
#include <vector>

using namespace sw;

namespace
{
	struct OpenKit
	{};
	SW_REFLECT(OpenKit, (), ())

	// Not final: does not satisfy the unit contract. Never created: that would instantiate the destructor of
	// Unit<Open, OpenKit>, whose static_assert rejects the class at compile time.
	class Open : public Unit<Open, OpenKit>
	{
	public:
		[[maybe_unused]]
		static constexpr UnitName Name{"open"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const;
	};

	// An acting component that either hits `marker` or, when `idle`, returns no effects.
	struct Option
	{
		UnitId marker;
		bool idle{false};

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			if (idle)
			{
				return {};
			}
			return effect::hit(common::Melee, turn.self().id(), marker, Damage{1});
		}
	};

	struct OptionsKit
	{
		Health health{Hp{5}};
		Option first;
		Option second;
	};
	SW_REFLECT(OptionsKit, (), (health, first, second))

	// A unit without a turn rule of its own: it uses the kit's order.
	class Chooser final : public Unit<Chooser, OptionsKit>
	{
	public:
		static constexpr UnitName Name{"chooser"};

		using Unit::Unit;
	};

	static_assert(Acting<Option>);
	static_assert(!Acting<Health>);
	static_assert(UnitType<Chooser>);
	static_assert(!DecidesItself<Chooser>);
	static_assert(DecidesItself<test::Dummy>);
	static_assert(UnitType<test::Dummy>);
	static_assert(UnitType<test::Probed>);
	static_assert(!UnitType<Open>);
	static_assert(ComponentSet<test::ProbedKit>);
	static_assert(Applicable<test::Probe>);
	static_assert(!Applicable<Health>);

	std::shared_ptr<std::vector<std::string>> newCalls()
	{
		return std::make_shared<std::vector<std::string>>();
	}

	std::unique_ptr<AnyUnit> probed(const std::shared_ptr<std::vector<std::string>>& calls)
	{
		return makeUnit<test::Probed>(
				UnitId{1},
				{.first = {.calls = calls, .label = "first"},
				 .health = {Hp{5}},
				 .second = {.calls = calls, .label = "second"},
				 .script = {}});
	}
}

TEST_CASE("UnitOf exposes the unit's identity and behaviour")
{
	const std::unique_ptr<AnyUnit> unit = probed(newCalls());
	CHECK(unit->id() == UnitId{1});
	CHECK(unit->name() == UnitName{"probed"});
	CHECK(unit->type() == std::type_index(typeid(test::Probed)));
	CHECK(unit->holds(typeid(test::Probed)));
	CHECK_FALSE(unit->holds(typeid(Health)));
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	CHECK(unit->dispatch(Turn(world.world(), UnitId{1}, Round{1}, world.random())).empty());
}

TEST_CASE("A unit without a turn rule takes the first acting component, in kit order, that returns effects")
{
	test::TestWorld world;
	const auto marker = [&](const OptionsKit& kit)
	{
		world.spawn<Chooser>(1, {.x = 0, .y = 0}, kit);
		return world.decide(1);
	};
	SUBCASE("the first acts")
	{
		const auto decision = marker({.first = {.marker = UnitId{7}}, .second = {.marker = UnitId{8}}});
		CHECK(std::get<HitEffect>(decision[0]).target == UnitId{7});
	}
	SUBCASE("the first is idle, the second acts")
	{
		const auto decision = marker({.first = {.marker = UnitId{7}, .idle = true}, .second = {.marker = UnitId{8}}});
		CHECK(std::get<HitEffect>(decision[0]).target == UnitId{8});
	}
	SUBCASE("none acts: no effects, the unit stays idle")
	{
		const auto decision
				= marker({.first = {.marker = UnitId{7}, .idle = true}, .second = {.marker = UnitId{8}, .idle = true}});
		CHECK(decision.empty());
	}
}

TEST_CASE("UnitOf passes events and questions to the kit in declaration order, then to applied components")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const Context context(world.world(), UnitId{1}, Round{1}, world.random());
	const auto calls = newCalls();
	const std::unique_ptr<AnyUnit> unit = probed(calls);
	unit->applied().add(
			std::make_unique<EffectOf<test::Probe>>(test::Probe{.calls = calls, .label = "applied"}), UnitId{9});
	const AnyUnit& constant = *unit;

	Targeted targeted{.attacker = UnitId{2}, .exposure = common::melee()};
	constant.dispatch(targeted);
	PlacementQuery placement;
	constant.dispatch(placement);
	SpeedQuery speed;
	constant.dispatch(speed);
	CHECK(unit->dispatch(HitAttempt(context, {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
	CHECK(unit->dispatch(HitTaken(context, {.attacker = UnitId{2}, .target = UnitId{1}})).empty());
	CHECK(unit->dispatch(RoundEnd(context)).empty());

	const std::vector<std::string> expected{
			"first:targeted",
			"second:targeted",
			"applied:targeted",
			"first:placement",
			"second:placement",
			"applied:placement",
			"first:speed",
			"second:speed",
			"applied:speed",
			"first:hitAttempt",
			"second:hitAttempt",
			"applied:hitAttempt",
			"first:hitTaken",
			"second:hitTaken",
			"applied:hitTaken",
			"first:roundEnd",
			"second:roundEnd",
			"applied:roundEnd"};
	CHECK(*calls == expected);
}

TEST_CASE("AnyUnit::get finds the first component of a type")
{
	const std::unique_ptr<AnyUnit> unit = probed(newCalls());
	const AnyUnit& constant = *unit;
	REQUIRE(constant.get<test::Probe>() != nullptr);
	CHECK(constant.get<test::Probe>()->label == "first");
	CHECK(constant.get<Health>()->hp == Hp{5});
	unit->get<Health>()->hp = Hp{2};
	CHECK(constant.get<Health>()->hp == Hp{2});
	CHECK(constant.get<common::Body>() == nullptr);
	CHECK(unit->get<common::Body>() == nullptr);
}

TEST_CASE("Applied components keep their order, see through decorators and drop expired ones")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const Context context(world.world(), UnitId{1}, Round{1}, world.random());
	const auto calls = newCalls();
	Applied applied;
	CHECK(applied.empty());
	applied.add(
			std::make_unique<EffectOf<common::Timed<test::Probe>>>(
					common::Timed<test::Probe>{.component = {.calls = calls, .label = "a"}, .left = Rounds{1}}),
			UnitId{7});
	applied.add(std::make_unique<EffectOf<test::Probe>>(test::Probe{.calls = calls, .label = "b"}), UnitId{8});
	CHECK_FALSE(applied.empty());
	CHECK(applied.has<common::Timed<test::Probe>>());
	CHECK(applied.has<test::Probe>());
	CHECK_FALSE(applied.has<Health>());

	CHECK(applied.dispatch(RoundEnd(context)).empty());
	CHECK(*calls == std::vector<std::string>{"a:roundEnd", "b:roundEnd"});
	CHECK_FALSE(applied.has<common::Timed<test::Probe>>());
	CHECK(applied.has<test::Probe>());

	applied.remove(UnitId{7});
	CHECK(applied.has<test::Probe>());
	applied.remove(UnitId{8});
	CHECK(applied.empty());
}

TEST_CASE("Applied components decide the turn before the kit, newest first")
{
	test::TestWorld world;
	world.spawn<Chooser>(1, {.x = 0, .y = 0}, {.first = {.marker = UnitId{7}}, .second = {.marker = UnitId{8}}});
	CHECK(std::get<HitEffect>(world.decide(1)[0]).target == UnitId{7});

	auto& simulation = world.simulation();
	simulation.perform(effect::apply(UnitId{1}, Option{.marker = UnitId{20}, .idle = true}, UnitId{3}));
	CHECK(std::get<HitEffect>(world.decide(1)[0]).target == UnitId{7});
	simulation.perform(
			effect::apply(UnitId{1}, Option{.marker = UnitId{21}}, UnitId{4})
			| effect::apply(UnitId{1}, Option{.marker = UnitId{22}}, UnitId{5}));
	CHECK(std::get<HitEffect>(world.decide(1)[0]).target == UnitId{22});
	simulation.perform(effect::unapply(UnitId{1}, UnitId{5}));
	CHECK(std::get<HitEffect>(world.decide(1)[0]).target == UnitId{21});
}
