#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Executor.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/World/GridMap.hpp>
#include <Core/World/Map.hpp>
#include <Core/World/World.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace sw
{
	Simulation::Simulation(std::unique_ptr<RandomSource> random, std::unique_ptr<LogSink> log) :
			_random(std::move(random)),
			_log(std::move(log))
	{
		if (_random == nullptr || _log == nullptr)
		{
			throw std::invalid_argument("a simulation needs a random source and a log");
		}
	}

	std::expected<void, WorldError> Simulation::createMap(const uint32_t width, const uint32_t height)
	{
		if (_world)
		{
			return std::unexpected(WorldError::MapAlreadyCreated);
		}
		if (width == 0 || height == 0)
		{
			return std::unexpected(WorldError::MapEmpty);
		}
		if (width > Map::MaxSide || height > Map::MaxSide)
		{
			return std::unexpected(WorldError::MapTooLarge);
		}

		_world.emplace(std::make_unique<GridMap>(width, height));
		report(MapCreated{.width = width, .height = height});
		return {};
	}

	std::expected<void, WorldError> Simulation::spawn(SpawnOrder order)
	{
		if (!_world)
		{
			return std::unexpected(WorldError::NoMap);
		}
		World& world = *_world;

		const auto id = order.unit->id();
		if (world.units().contains(id))
		{
			return std::unexpected(WorldError::DuplicateId);
		}

		const Placement oneCell{.origin = order.position, .size = {.width = 1, .height = 1}, .layer = Layer::Ground};
		PlacementQuery query{.placement = oneCell};
		order.unit->dispatch(query);
		const Placement& placement = query.placement;
		if (!world.map().contains(placement))
		{
			return std::unexpected(WorldError::OutOfBounds);
		}
		if (!world.map().canPlace(id, placement))
		{
			return std::unexpected(WorldError::CellOccupied);
		}

		world.map().place(id, placement);
		report(UnitSpawned{
				.unitId = id.get(),
				.unitType = std::string(order.unit->name().get()),
				.x = order.position.x,
				.y = order.position.y,
		});
		world.units().insert(std::move(order.unit));
		return {};
	}

	void Simulation::report(const Record& record)
	{
		_log->write(_round, record);
	}

	void Simulation::step()
	{
		if (!_world)
		{
			return;
		}
		World& world = *_world;
		_round = next(_round);

		// The order does not change during the round: units that die stay in the registry, dead, until its end.
		bool acted = false;
		for (const UnitId id : world.units().order())
		{
			acted = takeTurn(world, id) || acted;
		}

		Executor(world, *_random, *_log).endRound(_round);
		removeDead(world);

		_lastRoundActive = acted || effectsPending(world);
	}

	bool Simulation::isFinished() const
	{
		return !_world || (_round.get() > 0 && !_lastRoundActive);
	}

	void Simulation::run()
	{
		while (!isFinished())
		{
			step();
		}
	}

	bool Simulation::hasMap() const
	{
		return _world.has_value();
	}

	const World& Simulation::world() const
	{
		if (!_world)
		{
			throw std::logic_error("the world is created by CREATE_MAP");
		}
		return *_world;
	}

	Round Simulation::round() const
	{
		return _round;
	}

	RandomSource& Simulation::randomSource() const
	{
		return *_random;
	}

	Effects Simulation::decide(const UnitId unit, const Round round)
	{
		if (!_world)
		{
			throw std::logic_error("the world is created by CREATE_MAP");
		}
		return _world->units().find(unit)->dispatch(Turn(*_world, unit, round, *_random));
	}

	void Simulation::perform(Effects effects)
	{
		if (!_world)
		{
			throw std::logic_error("the world is created by CREATE_MAP");
		}
		Executor(*_world, *_random, *_log).execute(std::move(effects), _round);
	}

	bool Simulation::takeTurn(World& world, const UnitId id)
	{
		if (world.units().find(id)->isDead())
		{
			return false;  // killed earlier this round
		}

		auto effects = decide(id, _round);
		if (effects.empty())
		{
			return false;  // idle: nothing to perform
		}

		Executor(world, *_random, *_log).execute(std::move(effects), _round);
		return true;
	}

	void Simulation::removeDead(World& world)
	{
		for (const UnitId id : world.units().order())
		{
			const bool unreported = world.units().find(id)->isDead() && world.map().placement(id).has_value();
			if (unreported)
			{
				report(UnitDied{.unitId = id.get()});
				world.map().remove(id);
			}
		}
		world.units().removeIf([](const AnyUnit& unit) { return unit.isDead(); });
	}

	bool Simulation::effectsPending(const World& world)
	{
		return std::ranges::any_of(
				world.units().order(),
				[&world](const UnitId id) { return !world.units().find(id)->applied().empty(); });
	}
}
