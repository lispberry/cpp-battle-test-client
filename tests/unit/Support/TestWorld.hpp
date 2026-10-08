#pragma once

#include <Core/Events/Context.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Simulation/SpawnOrder.hpp>
#include <Core/Units/Unit.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Core/World/UnitRef.hpp>
#include <Support/RecordingLog.hpp>
#include <Support/ScriptedRandom.hpp>

#include <cstdint>
#include <doctest/doctest.h>
#include <expected>
#include <memory>
#include <stdexcept>
#include <utility>

namespace sw::test
{
	// A battlefield for tests: a simulation with scripted randomness and a recorded battle log. The log only keeps
	// what happened after the last spawn (spawning itself is tested on Simulation directly).
	class TestWorld
	{
	public:
		explicit TestWorld(const uint32_t width = 10, const uint32_t height = 10) :
				TestWorld(std::make_unique<ScriptedRandom>(), std::make_unique<RecordingLog>(), width, height)
		{}

		[[nodiscard]]
		ScriptedRandom& random()
		{
			return *_random;
		}

		[[nodiscard]]
		RecordingLog& log()
		{
			return *_log;
		}

		[[nodiscard]]
		Simulation& simulation()
		{
			return _simulation;
		}

		[[nodiscard]]
		const World& world() const
		{
			return _simulation.world();
		}

		template <UnitType T>
		UnitRef spawn(const uint32_t id, const Position position, typename T::Kit kit = {})
		{
			const auto spawned
					= _simulation.spawn({.position = position, .unit = makeUnit<T>(UnitId{id}, std::move(kit))});
			if (!spawned)
			{
				throw std::logic_error("TestWorld::spawn failed");
			}
			_log->clear();
			return unit(id);
		}

		[[nodiscard]]
		UnitRef unit(const uint32_t id) const
		{
			return {_simulation.world(), UnitId{id}};
		}

		// The context of an event that happens to unit `self` in `round`, to call a handler directly.
		[[nodiscard]]
		Context context(const uint32_t self, const Round round = Round{1})
		{
			return {_simulation.world(), UnitId{self}, round, *_random};
		}

		// Runs only the unit's turn: the effects it returns, without performing them.
		[[nodiscard]]
		Effects decide(const uint32_t id)
		{
			return _simulation.decide(UnitId{id}, next(_simulation.round()));
		}

		void round()
		{
			_simulation.step();
		}

	private:
		// The simulation owns the random source and the log; the test keeps typed handles to script and read them.
		TestWorld(
				std::unique_ptr<ScriptedRandom> random,
				std::unique_ptr<RecordingLog> log,
				uint32_t width,
				uint32_t height) :
				_random(random.get()),
				_log(log.get()),
				_simulation(std::move(random), std::move(log))
		{
			if (!_simulation.createMap(width, height))
			{
				throw std::logic_error("TestWorld: invalid map size");
			}
			_log->clear();
		}

		ScriptedRandom* _random;
		RecordingLog* _log;
		Simulation _simulation;
	};
}
