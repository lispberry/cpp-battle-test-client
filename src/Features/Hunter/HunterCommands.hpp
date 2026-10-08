#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Simulation/SpawnOrder.hpp>

#include <cstdint>

namespace sw::hunter
{
	struct SpawnHunterCommand
	{
		static constexpr const char* Name = "SPAWN_HUNTER";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
		uint32_t hp{};
		uint32_t agility{};
		uint32_t strength{};
		uint32_t range{};
		uint32_t chance{};
		uint32_t poison{};
	};
	SW_REFLECT(SpawnHunterCommand, (), (unitId, x, y, hp, agility, strength, range, chance, poison))

	[[nodiscard]]
	SpawnOrder spawnHunter(const SpawnHunterCommand& command);

	void registerHunter(Commands& commands);
}
