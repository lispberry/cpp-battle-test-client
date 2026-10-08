#include <Core/IO/Commands.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Features/Swordsman/Swordsman.hpp>
#include <Features/Swordsman/SwordsmanCommands.hpp>

#include <optional>

namespace sw::swordsman
{
	SpawnOrder spawnSwordsman(const SpawnSwordsmanCommand& command)
	{
		return {
				.position = {.x = command.x, .y = command.y},
				.unit = makeUnit<Swordsman>(
						UnitId{command.unitId},
						SwordsmanKit{
								.health = {Hp{command.hp}},
								.sword = {Damage{command.strength}},
								.rending = {.chance = Chance{command.chance}, .damage = Damage{command.rending}},
								.march = {.speed = Speed{1}, .target = std::nullopt},
						}),
		};
	}

	void registerSwordsman(Commands& commands)
	{
		commands.spawn<SpawnSwordsmanCommand>(&spawnSwordsman);
	}
}
