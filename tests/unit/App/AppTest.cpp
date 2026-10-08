#include <App/Application.hpp>
#include <App/Options.hpp>

#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <print>
#include <span>
#include <sstream>
#include <string>

using namespace sw;

namespace
{
	std::optional<Options> parse(const std::initializer_list<const char*> arguments)
	{
		return parseOptions(std::span(arguments.begin(), arguments.size()));
	}
}

TEST_CASE("parseOptions")
{
	CHECK_FALSE(parse({"sw"}));
	CHECK(parse({"sw", "a.txt"})->scenarioPath == "a.txt");
	CHECK_FALSE(parse({"sw", "a.txt"})->seed);
	CHECK(parse({"sw", "a.txt", "--seed", "42"})->seed == 42U);
	CHECK_FALSE(parse({"sw", "a.txt", "--seed"}));
	CHECK_FALSE(parse({"sw", "a.txt", "--sed", "42"}));
	CHECK_FALSE(parse({"sw", "a.txt", "--seed", "x"}));
	CHECK_FALSE(parse({"sw", "a.txt", "--seed", "42x"}));
}

TEST_CASE("runScenario prints the battle, or the first scenario error")
{
	std::istringstream scenario("CREATE_MAP 4 4\nSPAWN_SWORDSMAN 1 0 0 10 1 0 5\nMARCH 1 1 0\n");
	std::ostringstream output;
	std::ostringstream errors;
	CHECK(runScenario(scenario, 42, output, errors) == EXIT_SUCCESS);
	CHECK(output.str()
		  == "[0] MAP_CREATED width=4 height=4 \n"
			 "[0] UNIT_SPAWNED unitId=1 unitType=swordsman x=0 y=0 \n"
			 "[0] MARCH_STARTED unitId=1 x=0 y=0 targetX=1 targetY=0 \n"
			 "[1] UNIT_MOVED unitId=1 x=1 y=0 \n"
			 "[1] MARCH_ENDED unitId=1 x=1 y=0 \n");
	CHECK(errors.str().empty());

	std::istringstream broken("CREATE_MAP 4 4\nTELEPORT 1\n");
	std::ostringstream brokenErrors;
	CHECK(runScenario(broken, 42, output, brokenErrors) == EXIT_FAILURE);
	CHECK(brokenErrors.str() == "Error: line 2: unknown command TELEPORT\n");
}

TEST_CASE("run checks its arguments and the scenario file")
{
	std::ostringstream output;
	std::ostringstream errors;
	CHECK(run(std::array<const char*, 1>{"sw"}, output, errors) == EXIT_FAILURE);
	CHECK(errors.str() == std::format("{}\n", Usage));

	errors.str({});
	CHECK(run(std::array<const char*, 2>{"sw", "/no/such/file.txt"}, output, errors) == EXIT_FAILURE);
	CHECK(errors.str() == "Error: File not found - /no/such/file.txt\n");

	const std::filesystem::path path = std::filesystem::temp_directory_path() / "sw_app_test_scenario.txt";
	{
		std::ofstream file(path);
		std::print(file, "CREATE_MAP 2 2\n");
	}
	const std::string text = path.string();

	errors.str({});
	output.str({});
	CHECK(run(std::array<const char*, 4>{"sw", text.c_str(), "--seed", "7"}, output, errors) == EXIT_SUCCESS);
	CHECK(output.str() == "[0] MAP_CREATED width=2 height=2 \n");
	CHECK(errors.str().empty());

	errors.str({});
	CHECK(run(std::array<const char*, 2>{"sw", text.c_str()}, output, errors) == EXIT_SUCCESS);
	CHECK(errors.str().starts_with("seed: "));
	std::filesystem::remove(path);
}
