#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Simulation/SpawnOrder.hpp>

#include <cstdint>

namespace sw::swordsman
{
	struct SpawnSwordsmanCommand
	{
		static constexpr const char* Name = "SPAWN_SWORDSMAN";

		uint32_t unitId{};
		uint32_t x{};
		uint32_t y{};
		uint32_t hp{};
		uint32_t strength{};
		uint32_t chance{};
		uint32_t rending{};
	};
	SW_REFLECT(SpawnSwordsmanCommand, (), (unitId, x, y, hp, strength, chance, rending))

	[[nodiscard]]
	SpawnOrder spawnSwordsman(const SpawnSwordsmanCommand& command);

	void registerSwordsman(Commands& commands);
}
