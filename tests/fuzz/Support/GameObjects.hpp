#pragma once

#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Attacks.hpp>
#include <Features/Hunter/Hunter.hpp>
#include <Features/Swordsman/Swordsman.hpp>
#include <Support/Arbitrary.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <fuzzer/FuzzedDataProvider.h>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Adapters for game objects, with ranges that make battles meaningful (a unit with 0 hp or a 4-billion-cell map
// finds nothing interesting). Plain aggregates of these - unit kits, Placement, Attack, the structs below -
// get their adapter automatically (Arbitrary.hpp).
namespace sw::fuzz
{
	inline constexpr uint32_t MaxMapSide = 16;

	template <>
	struct Arbitrary<UnitId>
	{
		static UnitId generate(FuzzedDataProvider& input)
		{
			return UnitId{input.ConsumeIntegralInRange<uint32_t>(1, 32)};
		}
	};

	template <>
	struct Arbitrary<Hp>
	{
		static Hp generate(FuzzedDataProvider& input)
		{
			return Hp{input.ConsumeIntegralInRange<uint32_t>(1, 50)};
		}
	};

	// At least 1: with zero damage two units may legitimately fight forever.
	template <>
	struct Arbitrary<Damage>
	{
		static Damage generate(FuzzedDataProvider& input)
		{
			return Damage{input.ConsumeIntegralInRange<uint32_t>(1, 10)};
		}
	};

	template <>
	struct Arbitrary<Chance>
	{
		static Chance generate(FuzzedDataProvider& input)
		{
			return Chance{input.ConsumeIntegralInRange<uint32_t>(0, 1000)};
		}
	};

	template <>
	struct Arbitrary<Distance>
	{
		static Distance generate(FuzzedDataProvider& input)
		{
			return Distance{input.ConsumeIntegralInRange<uint32_t>(0, 8)};
		}
	};

	template <>
	struct Arbitrary<Speed>
	{
		static Speed generate(FuzzedDataProvider& input)
		{
			return Speed{input.ConsumeIntegralInRange<uint32_t>(1, 3)};
		}
	};

	template <>
	struct Arbitrary<Rounds>
	{
		static Rounds generate(FuzzedDataProvider& input)
		{
			return Rounds{input.ConsumeIntegralInRange<uint32_t>(1, 6)};
		}
	};

	template <>
	struct Arbitrary<Round>
	{
		static Round generate(FuzzedDataProvider& input)
		{
			return Round{input.ConsumeIntegralInRange<uint32_t>(1, 100)};
		}
	};

	// The abilities features use, so that reactions between features (poison and rending) are exercised.
	inline constexpr std::array<std::string_view, 3> KnownAbilities{"rending", "retaliation", "ember"};

	template <>
	struct Arbitrary<Ability>
	{
		static Ability generate(FuzzedDataProvider& input)
		{
			return Ability{input.PickValueInArray(KnownAbilities)};
		}
	};

	// Anywhere on the largest map; Battle wraps it into the actual one.
	template <>
	struct Arbitrary<Position>
	{
		static Position generate(FuzzedDataProvider& input)
		{
			const uint32_t x = input.ConsumeIntegralInRange<uint32_t>(0, MaxMapSide - 1);
			return {.x = x, .y = input.ConsumeIntegralInRange<uint32_t>(0, MaxMapSide - 1)};
		}
	};

	template <>
	struct Arbitrary<Size>
	{
		static Size generate(FuzzedDataProvider& input)
		{
			const uint32_t width = input.ConsumeIntegralInRange<uint32_t>(1, 3);
			return {.width = width, .height = input.ConsumeIntegralInRange<uint32_t>(1, 3)};
		}
	};

	template <>
	struct Arbitrary<Layer>
	{
		static Layer generate(FuzzedDataProvider& input)
		{
			return input.ConsumeBool() ? Layer::Air : Layer::Ground;
		}
	};

	template <>
	struct Arbitrary<AttackKind>
	{
		static AttackKind generate(FuzzedDataProvider& input)
		{
			return input.ConsumeBool() ? common::Ranged : common::Melee;
		}
	};

	template <>
	struct Arbitrary<Range>
	{
		static Range generate(FuzzedDataProvider& input)
		{
			const uint32_t min = input.ConsumeIntegralInRange<uint32_t>(0, 4);
			return {.min = Distance{min}, .max = Distance{min + input.ConsumeIntegralInRange<uint32_t>(0, 6)}};
		}
	};

	// --- map objects

	struct MapSize
	{
		uint32_t width{1};
		uint32_t height{1};
	};

	template <>
	struct Arbitrary<MapSize>
	{
		static MapSize generate(FuzzedDataProvider& input)
		{
			const uint32_t width = input.ConsumeIntegralInRange<uint32_t>(1, MaxMapSide);
			return {.width = width, .height = input.ConsumeIntegralInRange<uint32_t>(1, MaxMapSide)};
		}
	};

	// A small pool of ids, so that operations hit the same objects again.
	using ObjectId = Bounded<uint32_t, 1, 8>;

	struct Place
	{
		ObjectId object;
		Placement placement;
	};
	SW_REFLECT(Place, (), (object, placement))

	struct Move
	{
		ObjectId object;
		Position origin;
	};
	SW_REFLECT(Move, (), (object, origin))

	struct Remove
	{
		ObjectId object;
	};
	SW_REFLECT(Remove, (), (object))

	using MapOperation = std::variant<Place, Move, Remove>;

	// --- units and battles

	// Every unit kind of the game, with fuzzed components: adding a unit to the game is adding it here.
	using UnitBlueprint = std::variant<swordsman::SwordsmanKit, hunter::HunterKit>;

	struct Deployment
	{
		Position position;
		UnitBlueprint unit;
	};
	SW_REFLECT(Deployment, (), (position, unit))

	struct Battlefield
	{
		MapSize map;
		std::vector<Deployment> units;
		uint32_t seed{};
	};
	SW_REFLECT(Battlefield, (), (map, units, seed))

	// --- scenario text

	// Scenario text that mostly follows the grammar (so the parser gets past the first line and battles happen) but
	// mixes in unknown commands, garbage tokens, missing and extra arguments.
	struct ScenarioText
	{
		std::string text;
	};

	template <>
	struct Arbitrary<ScenarioText>
	{
		static ScenarioText generate(FuzzedDataProvider& input)
		{
			std::string text;
			const std::size_t lines = input.ConsumeIntegralInRange<std::size_t>(0, 24);
			for (std::size_t line = 0; line < lines; ++line)
			{
				text += generateLine(input, line == 0) + '\n';
			}
			return {text};
		}

	private:
		struct Shape
		{
			std::string_view name;
			std::size_t arguments;
		};

		static constexpr std::array<Shape, 4> Commands{{
				{.name = "CREATE_MAP", .arguments = 2},
				{.name = "SPAWN_SWORDSMAN", .arguments = 7},
				{.name = "SPAWN_HUNTER", .arguments = 9},
				{.name = "MARCH", .arguments = 3},
		}};

		static std::string generateLine(FuzzedDataProvider& input, const bool first)
		{
			const uint32_t kind = first && input.ConsumeBool() ? 0 : input.ConsumeIntegralInRange<uint32_t>(0, 9);
			if (kind == 9)
			{
				return input.ConsumeBool() ? std::format("// {}", input.ConsumeRandomLengthString(16)) : std::string{};
			}
			std::string line;
			std::size_t arguments = 0;
			if (kind == 8)
			{
				line = input.ConsumeRandomLengthString(12);
				arguments = input.ConsumeIntegralInRange<std::size_t>(0, 4);
			}
			else
			{
				const Shape& shape = Commands.at(kind % Commands.size());  // kinds 0..7 cover every command twice
				line = shape.name;
				arguments = shape.arguments;
				// Sometimes one argument too few or too many.
				const uint32_t skew = input.ConsumeIntegralInRange<uint32_t>(0, 15);
				arguments = skew == 0 && arguments > 0 ? arguments - 1 : (skew == 1 ? arguments + 1 : arguments);
			}
			for (std::size_t i = 0; i < arguments; ++i)
			{
				std::format_to(std::back_inserter(line), " {}", argument(input));
			}
			return line;
		}

		static std::string argument(FuzzedDataProvider& input)
		{
			switch (input.ConsumeIntegralInRange<uint32_t>(0, 15))
			{
				case 0: return std::to_string(input.ConsumeIntegralInRange<uint32_t>(0, UINT32_MAX));
				case 1: return std::format("-{}", input.ConsumeIntegralInRange<uint32_t>(1, 9));
				case 2: return input.ConsumeRandomLengthString(6);
				default: return std::to_string(input.ConsumeIntegralInRange<uint32_t>(0, 20));
			}
		}
	};
}
