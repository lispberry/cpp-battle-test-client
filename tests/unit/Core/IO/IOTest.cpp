#include <Core/IO/Commands.hpp>
#include <Core/IO/CoreCommands.hpp>
#include <Core/IO/Scenario.hpp>
#include <Core/IO/ScenarioError.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Support/RecordingLog.hpp>
#include <Support/ScriptedRandom.hpp>
#include <Support/TestUnits.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <expected>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sw;

namespace io_test
{
	struct PlaceCommand
	{
		static constexpr const char* Name = "PLACE";

		uint32_t id{};
		uint32_t x{};
	};
	SW_REFLECT(PlaceCommand, (), (id, x))

	struct SpawnDummyCommand
	{
		static constexpr const char* Name = "SPAWN_DUMMY";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
	};
	SW_REFLECT(SpawnDummyCommand, (), (unitId, x, y))
}

using io_test::PlaceCommand;
using io_test::SpawnDummyCommand;

namespace
{
	SpawnOrder spawnDummy(const SpawnDummyCommand& command)
	{
		return {.position = {.x = command.x, .y = command.y},
				.unit = makeUnit<test::Dummy>(UnitId{command.unitId}, {})};
	}

	// A simulation with commands, and a PLACE command that remembers what it was given.
	struct Scenario
	{
		Simulation simulation{std::make_unique<test::ScriptedRandom>(), std::make_unique<test::RecordingLog>()};
		Commands commands;
		std::vector<PlaceCommand> placed;

		Scenario()
		{
			commands.add<PlaceCommand>(
					[this](Simulation& target, const PlaceCommand& place) -> CommandResult
					{
						CHECK(&target == &simulation);
						if (place.id == 0)
						{
							return std::unexpected(std::string("id must not be zero"));
						}
						placed.push_back(place);
						return {};
					});
		}
	};
}

TEST_CASE("Commands read a command's fields in declaration order")
{
	Scenario scenario;
	REQUIRE(scenario.commands.execute("PLACE 3 7", 1, scenario.simulation));
	REQUIRE(scenario.commands.execute("  PLACE\t4  8  ", 2, scenario.simulation));
	REQUIRE(scenario.placed.size() == 2);
	CHECK(scenario.placed[0].id == 3);
	CHECK(scenario.placed[0].x == 7);
	CHECK(scenario.placed[1].x == 8);
	CHECK(scenario.commands.execute("", 3, scenario.simulation));
	CHECK(scenario.commands.execute("   ", 4, scenario.simulation));
	CHECK(scenario.commands.execute("// PLACE 1 1", 5, scenario.simulation));
	CHECK(scenario.placed.size() == 2);
}

TEST_CASE("Commands report what is wrong with a line")
{
	Scenario scenario;
	const auto error = [&scenario](const std::string& line)
	{
		return scenario.commands.execute(line, 9, scenario.simulation).error();
	};

	CHECK(error("JUMP 1").kind == ScenarioErrorKind::UnknownCommand);
	CHECK(error("JUMP 1").detail == "JUMP");
	CHECK(error("JUMP 1").line == 9);
	CHECK(error("PLACE 1").kind == ScenarioErrorKind::MissingArgument);
	CHECK(error("PLACE 1").detail == "x");
	CHECK(error("PLACE 1").line == 9);
	CHECK(error("PLACE 1 x").kind == ScenarioErrorKind::BadNumber);
	CHECK(error("PLACE 1 x").detail == "x=x");
	CHECK(error("PLACE -1 2").kind == ScenarioErrorKind::BadNumber);
	CHECK(error("PLACE 1 2abc").kind == ScenarioErrorKind::BadNumber);
	CHECK(error("PLACE 1 2 3").kind == ScenarioErrorKind::TrailingInput);
	CHECK(error("PLACE 1 2 3").detail == "3");
	CHECK(error("PLACE 0 2").kind == ScenarioErrorKind::Rejected);
	CHECK(error("PLACE 0 2").detail == "id must not be zero");
	CHECK(error("PLACE 0 2").line == 9);
}

TEST_CASE("Commands refuse to register a command twice")
{
	Scenario scenario;
	CHECK_THROWS_AS(
			scenario.commands.add<PlaceCommand>([](Simulation&, const PlaceCommand&) -> CommandResult { return {}; }),
			std::invalid_argument);
}

TEST_CASE("spawn connects a SPAWN_ command to the simulation")
{
	Scenario scenario;
	REQUIRE(scenario.simulation.createMap(5, 5));
	scenario.commands.spawn<SpawnDummyCommand>(&spawnDummy);
	REQUIRE(scenario.commands.execute("SPAWN_DUMMY 1 2 3", 1, scenario.simulation));
	CHECK(scenario.simulation.world().units().contains(UnitId{1}));
	const auto duplicate = scenario.commands.execute("SPAWN_DUMMY 1 4 4", 2, scenario.simulation);
	REQUIRE_FALSE(duplicate);
	CHECK(duplicate.error().kind == ScenarioErrorKind::Rejected);
	CHECK(duplicate.error().detail == "unit id is already taken");
}

TEST_CASE("CREATE_MAP creates the map once")
{
	Scenario scenario;
	registerCoreCommands(scenario.commands);
	CHECK(createMap(scenario.simulation, {.width = 1001, .height = 6}).error()
		  == "the map is too large (at most 1000 cells per side)");
	REQUIRE(scenario.commands.execute("CREATE_MAP 6 6", 1, scenario.simulation));
	CHECK(scenario.simulation.world().map().width() == 6);
	CHECK(createMap(scenario.simulation, {.width = 4, .height = 4}).error() == "the map has already been created");
}

TEST_CASE("describe(ScenarioError)")
{
	CHECK(describe({.line = 3, .kind = ScenarioErrorKind::UnknownCommand, .detail = "JUMP"})
		  == "line 3: unknown command JUMP");
	CHECK(describe({.line = 1, .kind = ScenarioErrorKind::MissingArgument, .detail = "x"})
		  == "line 1: missing argument x");
	CHECK(describe({.line = 1, .kind = ScenarioErrorKind::BadNumber, .detail = "x=a"})
		  == "line 1: not a non-negative number x=a");
	CHECK(describe({.line = 1, .kind = ScenarioErrorKind::TrailingInput, .detail = "3"})
		  == "line 1: unexpected extra argument 3");
	CHECK(describe({.line = 1, .kind = ScenarioErrorKind::Rejected, .detail = "no"}) == "line 1: rejected no");
	CHECK(describe({.line = 1, .kind = static_cast<ScenarioErrorKind>(99), .detail = "?"}) == "line 1: error ?");
}

TEST_CASE("readScenario runs every line and stops at the first error")
{
	Scenario scenario;
	std::istringstream good("PLACE 1 1\n\n// comment\nPLACE 2 2\n");
	REQUIRE(readScenario(good, scenario.commands, scenario.simulation));
	CHECK(scenario.placed.size() == 2);

	std::istringstream bad("PLACE 3 3\nPLACE 4\nPLACE 5 5\n");
	const auto result = readScenario(bad, scenario.commands, scenario.simulation);
	REQUIRE_FALSE(result);
	CHECK(result.error().line == 2);
	CHECK(scenario.placed.size() == 3);
}
