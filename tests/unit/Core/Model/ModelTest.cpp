#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Attacks.hpp>
#include <Support/ScriptedRandom.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <numeric>
#include <string_view>
#include <vector>

using namespace sw;

TEST_CASE("Hp minus Damage saturates at zero")
{
	CHECK(Hp{10} - Damage{3} == Hp{7});
	CHECK(Hp{3} - Damage{3} == Hp{0});
	CHECK(Hp{3} - Damage{5} == Hp{0});
}

TEST_CASE("Damage arithmetic and rounds")
{
	CHECK(Damage{2} + Damage{3} == Damage{5});
	CHECK(Damage{2} * 3 == Damage{6});
	CHECK(next(Round{4}) == Round{5});
}

TEST_CASE("rolls succeeds when the draw is at most the chance")
{
	sw::test::ScriptedRandom random{250, 251};
	CHECK(rolls(Chance{250}, random));
	CHECK_FALSE(rolls(Chance{250}, random));
}

TEST_CASE("Placement::covers its footprint only")
{
	const Placement big{.origin = {.x = 2, .y = 3}, .size = {.width = 2, .height = 2}};
	CHECK(big.covers({.x = 2, .y = 3}));
	CHECK(big.covers({.x = 3, .y = 4}));
	CHECK_FALSE(big.covers({.x = 1, .y = 3}));
	CHECK_FALSE(big.covers({.x = 4, .y = 3}));
	CHECK_FALSE(big.covers({.x = 2, .y = 2}));
	CHECK_FALSE(big.covers({.x = 2, .y = 5}));
}

TEST_CASE("cells lists a footprint row by row")
{
	CHECK(cells({.origin = {.x = 1, .y = 2}, .size = {.width = 2, .height = 2}})
		  == std::vector<Position>{{1, 2}, {2, 2}, {1, 3}, {2, 3}});
	CHECK(cells({.origin = {.x = 4, .y = 4}}) == std::vector<Position>{{4, 4}});
}

TEST_CASE("distance is Chebyshev between cells and the gap between footprints")
{
	CHECK(distance(Position{2, 2}, Position{5, 3}) == Distance{3});
	CHECK(distance(Position{5, 3}, Position{2, 2}) == Distance{3});
	CHECK(distance(Position{4, 4}, Position{4, 4}) == Distance{0});
	const Placement tower{.origin = {.x = 2, .y = 2}, .size = {.width = 2, .height = 2}};
	CHECK(distance(tower, Placement{.origin = {.x = 4, .y = 3}}) == Distance{1});
	CHECK(distance(Placement{.origin = {.x = 0, .y = 0}}, tower) == Distance{2});
	CHECK(distance(tower, Placement{.origin = {.x = 3, .y = 3}}) == Distance{0});
}

TEST_CASE("Positions are equal only when both coordinates are")
{
	CHECK(Position{1, 2} == Position{1, 2});
	CHECK_FALSE(Position{1, 2} == Position{3, 2});
	CHECK_FALSE(Position{1, 2} == Position{1, 3});
}

TEST_CASE("stepToward moves one cell along each axis that differs")
{
	CHECK(stepToward(Position{5, 5}, Position{8, 2}) == Position{6, 4});
	CHECK(stepToward(Position{5, 5}, Position{2, 8}) == Position{4, 6});
	CHECK(stepToward(Position{5, 5}, Position{5, 9}) == Position{5, 6});
	CHECK(stepToward(Position{5, 5}, Position{5, 5}) == Position{5, 5});
}

TEST_CASE("Range")
{
	const Range range{.min = Distance{2}, .max = Distance{4}};
	CHECK_FALSE(range.contains(Distance{1}));
	CHECK(range.contains(Distance{2}));
	CHECK(range.contains(Distance{4}));
	CHECK_FALSE(range.contains(Distance{5}));
	const Range shrunk = range.shrunk(Distance{3});
	CHECK(shrunk.min == Distance{0});
	CHECK(shrunk.max == Distance{1});
}

TEST_CASE("describe(WorldError)")
{
	CHECK(describe(WorldError::NoMap) == std::string_view{"the map has not been created"});
	CHECK(describe(WorldError::MapAlreadyCreated) == std::string_view{"the map has already been created"});
	CHECK(describe(WorldError::MapTooLarge) == std::string_view{"the map is too large (at most 1000 cells per side)"});
	CHECK(describe(WorldError::OutOfBounds) == std::string_view{"position is outside the map"});
	CHECK(describe(WorldError::CellOccupied) == std::string_view{"cell is occupied"});
	CHECK(describe(WorldError::DuplicateId) == std::string_view{"unit id is already taken"});
	CHECK(describe(WorldError::UnknownUnit) == std::string_view{"no such unit"});
	CHECK(describe(WorldError::MissingComponent) == std::string_view{"the unit cannot do that"});
	CHECK(describe(static_cast<WorldError>(200)) == std::string_view{"unknown error"});
}
