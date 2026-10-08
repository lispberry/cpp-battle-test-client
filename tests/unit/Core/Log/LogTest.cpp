#include <Core/Log/Records.hpp>
#include <Core/Log/TextLogPrinter.hpp>
#include <Core/Model/Types.hpp>

#include <doctest/doctest.h>
#include <sstream>

using namespace sw;

TEST_CASE("TextLogPrinter writes the fixed output format")
{
	std::ostringstream output;
	TextLogPrinter printer(output);
	printer.write(Round{0}, MapCreated{.width = 10, .height = 10});
	printer.write(Round{3}, UnitSpawned{.unitId = 1, .unitType = "hunter", .x = 2, .y = 2});
	printer.write(Round{12}, UnitAttacked{.attackerUnitId = 1, .targetUnitId = 2, .damage = 5, .targetHp = 0});
	CHECK(output.str()
		  == "[0] MAP_CREATED width=10 height=10 \n"
			 "[3] UNIT_SPAWNED unitId=1 unitType=hunter x=2 y=2 \n"
			 "[12] UNIT_ATTACKED attackerUnitId=1 targetUnitId=2 damage=5 targetHp=0 \n");
}
