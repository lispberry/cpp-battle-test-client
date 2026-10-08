#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/IO/Commands.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Adaptors.hpp>
#include <Core/Query/Units.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Features/Blueprint/Blueprint.hpp>
#include <Features/Common/Attacks.hpp>

#include <optional>

namespace sw::blueprint
{
	Effects Blueprint::on(const Turn& turn) const
	{
		const auto self = turn.self();
		if (const auto weak = turn.units() | attackableBy(self, common::melee()) | fragile(Hp{3}) | pickRandom(turn))
		{
			return effect::hit(common::Melee, self.id(), weak->id(), kit().strike.damage);
		}
		return kitTurn(turn);
	}

	SpawnOrder spawnBlueprint(const SpawnBlueprintCommand& command)
	{
		return {
				.position = {.x = command.x, .y = command.y},
				.unit = makeUnit<Blueprint>(
						UnitId{command.unitId},
						{
								.health = {Hp{command.hp}},
								.body = {.width = 1, .height = 1},
								.ward = {Distance{3}},
								.strike = {Damage{2}},
								.volley = {.range = {.min = Distance{2}, .max = Distance{4}}, .damage = Damage{1}},
								.ember = {.chance = Chance{100}, .burn = Damage{1}, .rounds = Rounds{3}},
								.retaliation = {Damage{1}},
								.swiftness = {Speed{1}},
								.vigil = {Rounds{2}},
								.march = {.speed = Speed{1}, .target = std::nullopt},
						}),
		};
	}

	void registerBlueprint(Commands& commands)
	{
		commands.spawn<SpawnBlueprintCommand>(&spawnBlueprint);
	}
}
