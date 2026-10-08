#pragma once

#include <Core/Events/Events.hpp>

#include <typeindex>

namespace sw
{
	class Effects;

	/// Every event and question behind one interface: what the engine calls on a stored unit (AnyUnit) and on an
	/// applied component (EffectOf). An event returns the Effects it causes; a question gets its answer filled in.
	/// Feature code never implements it: a unit class or a plain component gets it from UnitOf / EffectOf.
	///
	/// \code
	/// std::unique_ptr<Hooks> poison = std::make_unique<EffectOf<hunter::Poison>>(hunter::Poison{source, total});
	/// Effects effects = poison->dispatch(roundEnd);       // runs Poison::on(const RoundEnd&)
	/// poison->type() == typeid(hunter::Poison);          // true
	/// \endcode
	class Hooks
	{
	public:
		virtual ~Hooks() = default;

		[[nodiscard]]
		virtual Effects dispatch(const Turn& turn) = 0;

		[[nodiscard]]
		virtual Effects dispatch(const HitAttempt& attempt) = 0;

		[[nodiscard]]
		virtual Effects dispatch(const HitTaken& taken) = 0;

		[[nodiscard]]
		virtual Effects dispatch(const RoundEnd& end) = 0;

		virtual void dispatch(Targeted& targeted) const = 0;

		virtual void dispatch(PlacementQuery& placement) const = 0;

		virtual void dispatch(SpeedQuery& speed) const = 0;

		/// The type behind the interface: the unit class, or the applied component, e.g. to tell whether a unit carries
		/// a Poison.
		[[nodiscard]]
		virtual std::type_index type() const = 0;

		/// True if the type behind the interface is `wanted`, or a decorator that wraps it (`Timed<Invulnerable>` holds
		/// an Invulnerable).
		[[nodiscard]]
		virtual bool holds(const std::type_index wanted) const
		{
			return wanted == type();
		}

	protected:
		Hooks() = default;
		Hooks(const Hooks&) = default;
		Hooks(Hooks&&) = default;
		Hooks& operator=(const Hooks&) = default;
		Hooks& operator=(Hooks&&) = default;
	};
}
