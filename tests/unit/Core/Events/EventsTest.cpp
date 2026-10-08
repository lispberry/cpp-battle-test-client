#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Event.hpp>
#include <Core/Events/Events.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/UnitRef.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Common/Invulnerable.hpp>
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
	struct Refuser
	{
		int* calls;

		[[nodiscard]]
		std::optional<Attack> on(const Targeted& /*targeted*/) const
		{
			++*calls;
			return std::nullopt;
		}
	};

	struct NoHandlers
	{};

	// Declares handlers, but of the wrong shape: dispatchTo turns them into compile errors.
	struct Mutable
	{
		std::optional<Attack> on(const Targeted& targeted)
		{
			return targeted.exposure;
		}
	};

	struct WrongAnswer
	{
		[[nodiscard]]
		bool on(const RoundEnd& /*end*/) const
		{
			return true;
		}
	};

	static_assert(Answers<Refuser, Targeted>);
	static_assert(!Answers<NoHandlers, Targeted>);
	static_assert(!Answers<Mutable, Targeted>, "a question handler must be const");
	static_assert(Declares<Mutable, Targeted>);
	static_assert(!Handles<WrongAnswer, RoundEnd>, "an event handler returns Effects");
	static_assert(Declares<WrongAnswer, RoundEnd>);
	static_assert(Applicable<test::Probe>);
	static_assert(!Applicable<NoHandlers>);
	static_assert(Turn::FirstAnswerWins && HitAttempt::FirstAnswerWins);
	static_assert(!HitTaken::FirstAnswerWins && !RoundEnd::FirstAnswerWins);
	static_assert(EventType<RoundEnd> && !EventType<SpeedQuery>);
	static_assert(QueryType<SpeedQuery> && !QueryType<RoundEnd>);
}

TEST_CASE("A question is answered from the answer so far, and stays closed once refused")
{
	int calls = 0;
	const Refuser refuser{&calls};
	Targeted targeted{.attacker = UnitId{1}, .exposure = common::melee()};
	CHECK(targeted.attack().kind == common::Melee);
	dispatchTo(refuser, targeted);
	dispatchTo(refuser, targeted);
	CHECK(calls == 1);
	CHECK_FALSE(targeted.exposure.has_value());
	dispatchTo(NoHandlers{}, targeted);

	SpeedQuery speed{.speed = Speed{2}};
	dispatchTo(NoHandlers{}, speed);
	CHECK(speed.answer() == Speed{2});
	PlacementQuery placement{.placement = {.origin = {.x = 1, .y = 2}}};
	CHECK(placement.answer().origin == Position{1, 2});
}

TEST_CASE("Events carry the context: the unit they happen to, the round, the battlefield and the dice")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	world.spawn<test::Dummy>(2, {.x = 1, .y = 0});
	world.random().script({10});
	const Context context(world.world(), UnitId{1}, Round{4}, world.random());

	const HitAttempt attempt(
			context, {.kind = common::Ranged, .attacker = UnitId{1}, .target = UnitId{2}, .damage = Damage{3}});
	CHECK(attempt.self().id() == UnitId{1});
	CHECK(attempt.round() == Round{4});
	CHECK(attempt.kind() == common::Ranged);
	CHECK(attempt.attacker() == UnitId{1});
	CHECK(attempt.target() == UnitId{2});
	CHECK(attempt.damage() == Damage{3});
	CHECK(attempt.roll(Chance{10}));
	CHECK(&attempt.randomSource() == &world.random());
	CHECK(attempt.map().width() == 10);
	CHECK((attempt.units() | count()) == 1);

	const HitTaken taken(
			context, {.attacker = UnitId{2}, .target = UnitId{1}, .damage = Damage{3}, .ability = Ability{"a"}});
	CHECK(taken.attacker() == UnitId{2});
	CHECK(taken.target() == UnitId{1});
	CHECK(taken.damage() == Damage{3});
	CHECK(taken.ability() == Ability{"a"});

	const RoundEnd end(context);
	CHECK(end.self().id() == UnitId{1});
}

TEST_CASE("EffectOf passes every event and question to the component")
{
	test::TestWorld world;
	world.spawn<test::Dummy>(1, {.x = 0, .y = 0});
	const Context context(world.world(), UnitId{1}, Round{1}, world.random());
	auto calls = std::make_shared<std::vector<std::string>>();
	EffectOf<test::Probe> effect(test::Probe{.calls = calls, .label = "e"});
	const Hooks& constant = effect;

	Targeted targeted{.attacker = UnitId{1}, .exposure = common::melee()};
	constant.dispatch(targeted);
	PlacementQuery placement;
	constant.dispatch(placement);
	SpeedQuery speed;
	constant.dispatch(speed);
	CHECK(effect.dispatch(HitAttempt(context, {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
	CHECK(effect.dispatch(HitTaken(context, {.attacker = UnitId{1}, .target = UnitId{2}})).empty());
	CHECK(effect.dispatch(RoundEnd(context)).empty());
	CHECK(effect.dispatch(Turn(world.world(), UnitId{1}, Round{1}, world.random())).empty());

	CHECK(*calls
		  == std::vector<std::string>{
				  "e:targeted", "e:placement", "e:speed", "e:hitAttempt", "e:hitTaken", "e:roundEnd"});
	CHECK(effect.type() == std::type_index(typeid(test::Probe)));
	CHECK(effect.holds(typeid(test::Probe)));
	CHECK_FALSE(effect.holds(typeid(common::Invulnerable)));
}

TEST_CASE("Effects keep every kind of effect in order, joined with |")
{
	auto effects = effect::hit(common::Ranged, UnitId{1}, UnitId{2}, Damage{3})
				   | effect::attack(UnitId{1}, UnitId{2}, Damage{2}, Ability{"a"})
				   | effect::moveTo(UnitId{1}, {.x = 4, .y = 5})
				   | effect::apply(
						   UnitId{2},
						   test::Probe{.calls = std::make_shared<std::vector<std::string>>(), .label = "p"},
						   UnitId{1})
				   | effect::unapply(UnitId{2}, UnitId{1}) | effect::useAbility(UnitId{1}, Ability{"shout"})
				   | effect::report(UnitDied{.unitId = 9});
	effects |= effect::expire();

	REQUIRE(effects.size() == 8);
	CHECK(std::get<HitEffect>(effects[0]).kind == common::Ranged);
	CHECK(std::get<AttackEffect>(effects[1]).ability == Ability{"a"});
	CHECK(std::get<MoveEffect>(effects[2]).to == Position{4, 5});
	CHECK(std::get<ApplyEffect>(effects[3]).target == UnitId{2});
	CHECK(std::get<ApplyEffect>(effects[3]).source == UnitId{1});
	CHECK(std::get<ApplyEffect>(effects[3]).component->type() == std::type_index(typeid(test::Probe)));
	CHECK(std::get<UnapplyEffect>(effects[4]).source == UnitId{1});
	CHECK(std::get<AbilityEffect>(effects[5]).ability == Ability{"shout"});
	CHECK(std::holds_alternative<ReportEffect>(effects[6]));
	CHECK(std::holds_alternative<ExpireEffect>(effects[7]));

	CHECK(effects.removeExpire());
	CHECK_FALSE(effects.removeExpire());
	CHECK(effects.take().size() == 7);
	CHECK(effects.empty());
}
