#pragma once

#include <Core/Base/MersenneRandom.hpp>
#include <Core/Log/LogSink.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Simulation/Simulation.hpp>
#include <Core/Units/UnitOf.hpp>
#include <Features/Hunter/Hunter.hpp>
#include <Features/Swordsman/Swordsman.hpp>
#include <Support/GameObjects.hpp>
#include <Support/Target.hpp>

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace sw::fuzz
{
	// Keeps the battle log, to check it after the battle.
	class LogCapture final : public LogSink
	{
	public:
		void write(const Round round, const Record& record) override
		{
			_records.emplace_back(round, record);
		}

		[[nodiscard]]
		const std::vector<std::pair<Round, Record>>& records() const
		{
			return _records;
		}

	private:
		std::vector<std::pair<Round, Record>> _records;
	};

	// A simulation set up from a fuzzed Battlefield: the map, then every deployment that fits (ids 1, 2, ...;
	// positions wrapped into the map; deployments on taken cells are skipped).
	class Battle
	{
	public:
		explicit Battle(const Battlefield& battlefield) :
				Battle(battlefield, std::make_unique<LogCapture>())
		{}

		[[nodiscard]]
		Simulation& simulation()
		{
			return _simulation;
		}

		[[nodiscard]]
		const LogCapture& log() const
		{
			return *_log;
		}

		[[nodiscard]]
		RandomSource& random()
		{
			return _simulation.randomSource();
		}

	private:
		// The simulation owns its log; the battle keeps a typed handle to read it.
		Battle(const Battlefield& battlefield, std::unique_ptr<LogCapture> log) :
				_log(log.get()),
				_simulation(std::make_unique<MersenneRandom>(battlefield.seed), std::move(log))
		{
			expect(_simulation.createMap(battlefield.map.width, battlefield.map.height).has_value(),
				   "fuzzed maps are within the limit");
			uint32_t id = 0;
			for (const Deployment& deployment : battlefield.units)
			{
				const Position position{
						.x = deployment.position.x % battlefield.map.width,
						.y = deployment.position.y % battlefield.map.height,
				};
				const UnitId unitId{++id};
				std::visit(
						[this, unitId, position]<class Kit>(const Kit& kit)
						{ (void)_simulation.spawn({.position = position, .unit = make(unitId, kit)}); },
						deployment.unit);
			}
		}

		static std::unique_ptr<AnyUnit> make(const UnitId id, const swordsman::SwordsmanKit& kit)
		{
			return makeUnit<swordsman::Swordsman>(id, kit);
		}

		static std::unique_ptr<AnyUnit> make(const UnitId id, const hunter::HunterKit& kit)
		{
			return makeUnit<hunter::Hunter>(id, kit);
		}

		LogCapture* _log;
		Simulation _simulation;
	};
}
