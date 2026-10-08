#pragma once

#include <Core/Base/Reflect.hpp>

#include <cstdint>
#include <string>
#include <variant>

namespace sw
{
	// The battle log: one record per thing that happened, printed in the fixed output format
	// `[round] NAME field=value ...`. Field names and order are part of that format.

	/// The map was created: CREATE_MAP succeeded.
	struct MapCreated
	{
		static constexpr const char* Name = "MAP_CREATED";

		uint32_t width{};
		uint32_t height{};
	};
	SW_REFLECT(MapCreated, (), (width, height))

	/// A unit was placed on the map; `unitType` is its UnitName, e.g. "hunter".
	struct UnitSpawned
	{
		static constexpr const char* Name = "UNIT_SPAWNED";

		uint32_t unitId{};
		std::string unitType;
		uint32_t x{};
		uint32_t y{};
	};
	SW_REFLECT(UnitSpawned, (), (unitId, unitType, x, y))

	/// A unit was ordered to march from (`x`, `y`) to (`targetX`, `targetY`).
	struct MarchStarted
	{
		static constexpr const char* Name = "MARCH_STARTED";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
		uint32_t targetX{};
		uint32_t targetY{};
	};
	SW_REFLECT(MarchStarted, (), (unitId, x, y, targetX, targetY))

	/// A marching unit reached its target (`x`, `y`).
	struct MarchEnded
	{
		static constexpr const char* Name = "MARCH_ENDED";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
	};
	SW_REFLECT(MarchEnded, (), (unitId, x, y))

	/// A unit stepped to (`x`, `y`).
	struct UnitMoved
	{
		static constexpr const char* Name = "UNIT_MOVED";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
	};
	SW_REFLECT(UnitMoved, (), (unitId, x, y))

	/// A unit dropped to 0 hp and was removed from the map.
	struct UnitDied
	{
		static constexpr const char* Name = "UNIT_DIED";

		uint32_t unitId{};
	};
	SW_REFLECT(UnitDied, (), (unitId))

	/// The total damage one unit dealt to another in one action (or one round end), and the target's hp after it.
	struct UnitAttacked
	{
		static constexpr const char* Name = "UNIT_ATTACKED";

		uint32_t attackerUnitId{};
		uint32_t targetUnitId{};
		uint32_t damage{};
		uint32_t targetHp{};
	};
	SW_REFLECT(UnitAttacked, (), (attackerUnitId, targetUnitId, damage, targetHp))

	/// A unit used a named ability, e.g. "rending".
	struct UnitAbilityUsed
	{
		static constexpr const char* Name = "UNIT_ABILITY_USED";

		uint32_t abilityUnitId{};
		std::string abilityName;
	};
	SW_REFLECT(UnitAbilityUsed, (), (abilityUnitId, abilityName))

	/// Any battle-log record. Add a new record type to this list to make it printable.
	///
	/// \code
	/// return effect::report(MarchEnded{.unitId = turn.self().id().get(), .x = target.x, .y = target.y});
	/// // printed as: [7] MARCH_ENDED unitId=1 x=5 y=5
	/// \endcode
	using Record = std::variant<
			MapCreated,
			UnitSpawned,
			MarchStarted,
			MarchEnded,
			UnitMoved,
			UnitDied,
			UnitAttacked,
			UnitAbilityUsed>;
}
