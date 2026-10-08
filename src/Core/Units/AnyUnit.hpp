#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Applied.hpp>
#include <Core/Units/Health.hpp>

#include <typeindex>
#include <typeinfo>

namespace sw
{
	/// A unit as the engine stores it: any unit class, type-erased by UnitOf, plus the components applied to it during
	/// the battle. Its Hooks pass an event or a question to the unit's kit in declaration order and to its applied
	/// components (see Turn and each event's FirstAnswerWins for the order). Create one with makeUnit; feature code reads units
	/// through UnitRef instead.
	///
	/// \code
	/// std::unique_ptr<AnyUnit> unit = makeUnit<Swordsman>(UnitId{1}, kit);
	/// unit->name();                                      // UnitName{"swordsman"}
	/// unit->get<Health>()->hp = Hp{0};                   // get() is nullptr if the unit has no Health
	/// PlacementQuery query{.placement = {.origin = position}};
	/// unit->dispatch(query);                             // a Body or Flying answers with the footprint and layer
	/// \endcode
	class AnyUnit : public Hooks
	{
	public:
		AnyUnit(const AnyUnit&) = delete;
		AnyUnit(AnyUnit&&) = delete;
		AnyUnit& operator=(const AnyUnit&) = delete;
		AnyUnit& operator=(AnyUnit&&) = delete;
		~AnyUnit() override = default;

		/// Unique in the battle.
		[[nodiscard]]
		virtual UnitId id() const = 0;

		/// The unit class's `Name`, e.g. "hunter".
		[[nodiscard]]
		virtual UnitName name() const = 0;

		/// The unit's first kit component of type `C`, or nullptr. Applied components are not searched.
		template <Component C>
		[[nodiscard]]
		const C* get() const
		{
			return static_cast<const C*>(find(typeid(C)));
		}

		template <Component C>
		[[nodiscard]]
		C* get()
		{
			return static_cast<C*>(find(typeid(C)));
		}

		/// True if the unit has a component of type `C`: in its kit, or applied during the battle, on its own or inside a
		/// decorator (`Timed<C>`).
		template <Component C>
		[[nodiscard]]
		bool has() const
		{
			return has(typeid(C));
		}

		/// has<C>, by the component's type.
		[[nodiscard]]
		bool has(const std::type_index type) const
		{
			return find(type) != nullptr || _applied.has(type);
		}

		/// True if the unit has Health and it is down to 0. A unit without Health never dies.
		[[nodiscard]]
		bool isDead() const
		{
			const auto* health = get<Health>();
			return health != nullptr && health->hp.get() == 0;
		}

		/// The components applied to the unit during the battle.
		[[nodiscard]]
		Applied& applied()
		{
			return _applied;
		}

		[[nodiscard]]
		const Applied& applied() const
		{
			return _applied;
		}

	protected:
		AnyUnit() = default;

		[[nodiscard]]
		virtual const void* find(std::type_index type) const = 0;

		[[nodiscard]]
		virtual void* find(std::type_index type) = 0;

	private:
		Applied _applied;
	};
}
