#pragma once

#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Health.hpp>
#include <Core/Units/Unit.hpp>
#include <Features/Common/Body.hpp>
#include <Features/Common/Flying.hpp>
#include <Features/Common/Invulnerable.hpp>
#include <Features/Common/March.hpp>

#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sw::test
{
	struct DummyKit
	{
		Health health{Hp{10}};
	};
	SW_REFLECT(DummyKit, (), (health))

	// A unit that never acts.
	class Dummy final : public Unit<Dummy, DummyKit>
	{
	public:
		static constexpr UnitName Name{"dummy"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& /*turn*/) const
		{
			return {};
		}
	};

	// A component whose turn is whatever the test wants.
	struct Script
	{
		std::function<Effects(const Turn&)> act;
	};

	// The effects of type `E` among `effects`, in order: what a test checks after a turn or a handler.
	template <class E>
	std::vector<E> only(const Effects& effects)
	{
		std::vector<E> found;
		for (std::size_t index = 0; index < effects.size(); ++index)
		{
			if (const auto* effect = std::get_if<E>(&effects[index]))
			{
				found.push_back(*effect);
			}
		}
		return found;
	}

	// A script that always acts with the effects `body` returns.
	inline Script acts(std::function<Effects(const Turn&)> body)
	{
		return {std::move(body)};
	}

	struct ScriptedKit
	{
		Health health{Hp{10}};
		Script script;
	};
	SW_REFLECT(ScriptedKit, (), (health, script))

	// A unit whose turn is scripted by the test.
	class Scripted final : public Unit<Scripted, ScriptedKit>
	{
	public:
		static constexpr UnitName Name{"scripted"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			return kit().script.act(turn);
		}
	};

	// Logs every handler call as "<label>:<event>" into a shared list, to check dispatch order and stop rules. Answers
	// every question with the answer so far.
	struct Probe
	{
		std::shared_ptr<std::vector<std::string>> calls;
		std::string label;

		[[nodiscard]]
		std::optional<Attack> on(const Targeted& targeted) const
		{
			calls->push_back(std::format("{}:targeted", label));
			return targeted.exposure;
		}

		[[nodiscard]]
		Placement on(const PlacementQuery& query) const
		{
			calls->push_back(std::format("{}:placement", label));
			return query.placement;
		}

		[[nodiscard]]
		Speed on(const SpeedQuery& query) const
		{
			calls->push_back(std::format("{}:speed", label));
			return query.speed;
		}

		[[nodiscard]]
		Effects on(const HitAttempt& /*event*/) const
		{
			calls->push_back(std::format("{}:hitAttempt", label));
			return {};
		}

		[[nodiscard]]
		Effects on(const HitTaken& /*event*/) const
		{
			calls->push_back(std::format("{}:hitTaken", label));
			return {};
		}

		[[nodiscard]]
		Effects on(const RoundEnd& /*event*/) const
		{
			calls->push_back(std::format("{}:roundEnd", label));
			return {};
		}
	};

	struct ProbedKit
	{
		Probe first;
		Health health{Hp{10}};
		Probe second;
		Script script;
	};
	SW_REFLECT(ProbedKit, (), (first, health, second, script))

	// A unit made of two probes, a body, and a health; acts through its script.
	class Probed final : public Unit<Probed, ProbedKit>
	{
	public:
		static constexpr UnitName Name{"probed"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			if (!kit().script.act)
			{
				return {};
			}
			return kit().script.act(turn);
		}
	};

	struct FortressKit
	{
		Health health{Hp{10}};
		common::Body body{.width = 2, .height = 2};
		common::Invulnerable invulnerable;
	};
	SW_REFLECT(FortressKit, (), (health, body, invulnerable))

	// A 2x2 invulnerable unit (the Tower of the spec's plans) and a flying one (the Raven).
	class Fortress final : public Unit<Fortress, FortressKit>
	{
	public:
		static constexpr UnitName Name{"fortress"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& /*turn*/) const
		{
			return {};
		}
	};

	struct StatueKit
	{
		common::Body body;
	};
	SW_REFLECT(StatueKit, (), (body))

	// No Health: cannot be attacked or killed.
	class Statue final : public Unit<Statue, StatueKit>
	{
	public:
		static constexpr UnitName Name{"statue"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& /*turn*/) const
		{
			return {};
		}
	};

	struct FlyerKit
	{
		Health health{Hp{10}};
		common::Flying flying;
		common::March march{.speed = Speed{2}, .target = std::nullopt};
	};
	SW_REFLECT(FlyerKit, (), (health, flying, march))

	class Flyer final : public Unit<Flyer, FlyerKit>
	{
	public:
		static constexpr UnitName Name{"flyer"};

		using Unit::Unit;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			return kit().march.on(turn);
		}
	};
}
