#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/World/World.hpp>

#include <optional>
#include <typeindex>
#include <typeinfo>

namespace sw
{
	/// Read-only handle to a unit: what every query yields and what feature code inspects. Cheap to copy; two refs
	/// are equal when they name the same unit.
	///
	/// Only `id()` and `isAlive()` may be called once the unit is gone; everything else requires it to exist.
	///
	/// \code
	/// const auto self = turn.self();
	/// for (const UnitRef unit : turn.units() | adjacentTo(self))
	/// {
	///     const auto* health = unit.get<Health>();    // nullptr: no Health
	///     if (health != nullptr && unit.isAttackableBy(self, common::melee()))
	///     {
	///         std::println("{} {} has {} hp", unit.name().get(), unit.id().get(), health->hp.get());
	///     }
	/// }
	/// \endcode
	class UnitRef
	{
	public:
		/// A handle to unit `id` of `world`. The unit need not exist: `isAlive()` is then false.
		UnitRef(const World& world, UnitId id);

		[[nodiscard]]
		UnitId id() const;

		[[nodiscard]]
		UnitName name() const;

		[[nodiscard]]
		Placement placement() const;

		/// The top-left cell of the unit's footprint.
		[[nodiscard]]
		Position position() const;

		/// True if the unit exists and has Health above zero, or no Health at all.
		[[nodiscard]]
		bool isAlive() const;

		/// True if the unit has a component of type `C`: in its kit, or applied during the battle, on its own or inside a
		/// decorator (`Timed<C>`).
		template <Component C>
		[[nodiscard]]
		bool has() const
		{
			return unit().has<C>();
		}

		/// The unit's first kit component of type `C`, or nullptr. Applied components are not searched.
		template <Component C>
		[[nodiscard]]
		const C* get() const
		{
			return unit().get<C>();
		}

		/// `attack` as this unit's handlers let it through (a flyer shrinks the range), or nothing if one refused it.
		/// Asks a Targeted question; does not check the distance.
		[[nodiscard]]
		std::optional<Attack> exposureTo(UnitRef attacker, Attack attack) const;

		/// True if the unit has Health, its hooks let `attack` through, and `attacker` is within the resulting range.
		[[nodiscard]]
		bool isAttackableBy(UnitRef attacker, Attack attack) const;

		/// isAttackableBy with the attacker's placement `from` already looked up, for queries that ask it of many
		/// targets.
		[[nodiscard]]
		bool isAttackableBy(UnitRef attacker, const Placement& from, Attack attack) const;

		friend bool operator==(const UnitRef& left, const UnitRef& right)
		{
			return left._id == right._id;
		}

	private:
		[[nodiscard]]
		const AnyUnit& unit() const;

		const World* _world;
		UnitId _id;
	};

	/// Gap in cells between the footprints of `from` and `to`: 1 for neighbours (diagonals included), 0 if they
	/// overlap (a flyer above a ground unit).
	[[nodiscard]]
	Distance distance(const UnitRef& from, const UnitRef& to);
}
