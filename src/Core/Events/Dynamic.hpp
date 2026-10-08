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

#include <functional>
#include <optional>

namespace sw
{
	/// A component whose handlers are functions chosen at run time: behaviour that is not worth a type of its own, or
	/// that comes from a script. Each function is optional; without one, an event causes no effects and a question
	/// keeps the answer so far. The signatures are the engine's, so a dynamic handler is as typesafe as a C++ one.
	///
	/// \code
	/// // Shoots the first unit in sight for two rounds, then stops.
	/// return effect::apply(target, Timed<Dynamic>{.component = {.onTurn = [](const Turn& turn) -> Effects {
	///     const auto prey = turn.units() | first();
	///     return prey ? effect::hit(common::Ranged, turn.self().id(), prey->id(), Damage{1}) : Effects{};
	/// }}, .left = Rounds{2}}, source);
	/// \endcode
	struct Dynamic
	{
		std::function<Effects(const Turn&)> onTurn;
		std::function<Effects(const HitAttempt&)> onHitAttempt;
		std::function<Effects(const HitTaken&)> onHitTaken;
		std::function<Effects(const RoundEnd&)> onRoundEnd;
		std::function<std::optional<Attack>(const Targeted&)> onTargeted;
		std::function<Placement(const PlacementQuery&)> onPlacement;
		std::function<Speed(const SpeedQuery&)> onSpeed;

		[[nodiscard]]
		Effects on(const Turn& turn) const
		{
			return onTurn ? onTurn(turn) : Effects{};
		}

		[[nodiscard]]
		Effects on(const HitAttempt& attempt) const
		{
			return onHitAttempt ? onHitAttempt(attempt) : Effects{};
		}

		[[nodiscard]]
		Effects on(const HitTaken& taken) const
		{
			return onHitTaken ? onHitTaken(taken) : Effects{};
		}

		[[nodiscard]]
		Effects on(const RoundEnd& end) const
		{
			return onRoundEnd ? onRoundEnd(end) : Effects{};
		}

		[[nodiscard]]
		std::optional<Attack> on(const Targeted& targeted) const
		{
			return onTargeted ? onTargeted(targeted) : targeted.exposure;
		}

		[[nodiscard]]
		Placement on(const PlacementQuery& query) const
		{
			return onPlacement ? onPlacement(query) : query.placement;
		}

		[[nodiscard]]
		Speed on(const SpeedQuery& query) const
		{
			return onSpeed ? onSpeed(query) : query.speed;
		}
	};
}
