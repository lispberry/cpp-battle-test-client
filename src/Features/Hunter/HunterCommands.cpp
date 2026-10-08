#include <Core/IO/Commands.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Features/Hunter/Hunter.hpp>
#include <Features/Hunter/HunterCommands.hpp>

#include <optional>

namespace sw::hunter
{
	SpawnOrder spawnHunter(const SpawnHunterCommand& command)
	{
		return {
				.position = {.x = command.x, .y = command.y},
				.unit = makeUnit<Hunter>(
						UnitId{command.unitId},
						HunterKit{
								.health = {Hp{command.hp}},
								.bow = {
										.range = {.min = Distance{2}, .max = Distance{command.range}},
										.damage = Damage{command.agility},
										.holdWhenEngaged = true,
								},
								.knife = {Damage{command.strength}},
								.poison
								= {.chance = Chance{command.chance}, .total = Damage{command.poison}},
							.march = {.speed = Speed{1}, .target = std::nullopt},
						}),
		};
	}

	void registerHunter(Commands& commands)
	{
		commands.spawn<SpawnHunterCommand>(&spawnHunter);
	}
}
