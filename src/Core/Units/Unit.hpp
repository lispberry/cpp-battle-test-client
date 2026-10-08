#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Event.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Types.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace sw
{
	namespace detail
	{
		template <reflect::Described S, std::size_t... Index>
		consteval bool allComponents(std::index_sequence<Index...> /*fields*/)
		{
			return (Component<reflect::FieldType<S, Index>> && ...);
		}
	}

	/// A unit's kit: an aggregate declared with SW_REFLECT (nothing virtual) whose every field is a component.
	/// sw::reflect walks the fields to dispatch events, so each component is declared exactly once. The order of the
	/// fields is the unit's priority: the default turn (Unit::kitTurn) asks the acting components from top to bottom.
	///
	/// \code
	/// struct FootmanKit { Health health; common::MeleeAttack sword; common::March march; };
	/// SW_REFLECT(FootmanKit, (), (health, sword, march))
	/// static_assert(ComponentSet<FootmanKit>);
	/// \endcode
	template <class S>
	concept ComponentSet = Aggregate<S> && reflect::Described<S> && !std::is_polymorphic_v<S>
						   && detail::allComponents<S>(std::make_index_sequence<reflect::fieldCount<S>>{});

	/// A component that can decide the unit's turn: `Effects on(const Turn&) const`. The default turn asks these in
	/// kit order; the other components (Health, hooks) are skipped.
	///
	/// \code
	/// static_assert(Acting<common::March>);
	/// static_assert(!Acting<Health>);
	/// \endcode
	template <class C>
	concept Acting = Handles<const C, Turn>;

	namespace detail
	{
		// Lets an acting component decide unless an earlier one already acted. A free function, so it is one function
		// per component type, not per unit.
		template <Component C>
		void tryAct(const C& component, const Turn& turn, Effects& decision)
		{
			static_assert(Acting<C> || !Declares<C, Turn>, "a turn handler is `Effects on(const Turn&) const`");
			if constexpr (Acting<C>)
			{
				if (decision.empty())
				{
					decision = component.on(turn);
				}
			}
		}
	}

	/// A unit class with a rule of its own: it declares `Effects on(const Turn&) const`, which the engine asks instead
	/// of the kit, and which may fall back to Unit::kitTurn.
	///
	/// \code
	/// static_assert(DecidesItself<blueprint::Blueprint>);
	/// static_assert(!DecidesItself<hunter::Hunter>);                // acts in kit order
	/// \endcode
	template <class T>
	concept DecidesItself = Handles<const T, Turn>;

	template <ClassType Derived, ComponentSet K>
	class Unit;

	/// The contract every unit class satisfies. `Unit<Derived, Kit>` checks it with a static_assert, so a broken unit
	/// fails to compile where it is defined; see Unit for a complete class.
	///
	/// A unit class is final and copyable, derives from `Unit<Self, Self::Kit>`, and has a
	/// `static constexpr UnitName Name` and a `(UnitId, Kit)` constructor (inherited with `using Unit::Unit`). It acts in
	/// kit order unless it declares `Effects on(const Turn&) const` (DecidesItself); a turn handler of another shape is
	/// an error, not ignored.
	///
	/// \code
	/// static_assert(UnitType<swordsman::Swordsman>);
	///
	/// template <UnitType T>
	/// UnitRef spawn(uint32_t id, Position position, typename T::Kit kit = {});
	/// \endcode
	template <class T>
	concept UnitType = ClassType<T> && std::is_final_v<T> && std::copy_constructible<T> && requires {
		typename T::Kit;
		requires std::derived_from<T, Unit<T, typename T::Kit>>;
		requires std::constructible_from<T, UnitId, typename T::Kit>;
		{ T::Name } -> std::convertible_to<UnitName>;
	} && (DecidesItself<T> || !Declares<T, Turn>);

	template <UnitType T>
	class UnitOf;

	/// Base of every unit class (CRTP): holds the unit's id and its kit, and checks the UnitType contract.
	///
	/// The derived class inherits the `(UnitId, Kit)` constructor and reads its components through `kit()`; the engine
	/// (UnitOf) walks the same kit to dispatch events. A turn only decides: it returns the effects, and the Executor
	/// performs them afterwards. By default the unit acts in kit order (kitTurn); a class with a rule of its own
	/// declares `Effects on(const Turn&) const`, and may still fall back to `kitTurn`.
	///
	/// \code
	/// struct FootmanKit
	/// {
	///     Health health;
	///     common::MeleeAttack sword;                              // strikes a neighbour if there is one,
	///     common::March march;                                    // marches otherwise
	/// };
	///
	/// class Footman final : public Unit<Footman, FootmanKit>
	/// {
	/// public:
	///     static constexpr UnitName Name{"footman"};
	///     using Unit::Unit;                                       // Footman(UnitId, FootmanKit)
	/// };
	/// \endcode
	template <ClassType Derived, ComponentSet K>
	class Unit
	{
	public:
		using Kit = K;

		// Public on purpose: an inherited constructor is only as accessible as this one is to the caller (makeUnit),
		// and the protected destructor already stops anyone from creating a bare Unit.

		/// Creates the unit `id` made of `kit`. Derived classes inherit it with `using Unit::Unit`.
		// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
		Unit(const UnitId id, Kit kit) :
				_id(id),
				_kit(std::move(kit))
		{}

		[[nodiscard]]
		UnitId id() const
		{
			return _id;
		}

	protected:
		Unit& operator=(const Unit&) = default;
		Unit& operator=(Unit&&) noexcept = default;

		~Unit()
		{
			static_assert(UnitType<Derived>, "a unit class must satisfy sw::UnitType, see Core/Units/Unit.hpp");
		}

		/// The default turn: the effects of the first acting component, in kit order, that returns any; no effects if
		/// none does.
		[[nodiscard]]
		Effects kitTurn(const Turn& turn) const
		{
			Effects decision;
			reflect::forEachField(
					_kit,
					[&turn, &decision]<Component C>(const C& component) { detail::tryAct(component, turn, decision); });
			return decision;
		}

		/// The unit's components, for its turn rule and the derived class's helpers.
		[[nodiscard]]
		Kit& kit()
		{
			return _kit;
		}

		[[nodiscard]]
		const Kit& kit() const
		{
			return _kit;
		}

	private:
		// Only Derived copies its base: `class Hunter : public Unit<Swordsman, SwordsmanKit>` cannot be copied.
		friend Derived;

		// The engine walks the kit to dispatch events to the components' hooks.
		template <UnitType T>
		friend class UnitOf;

		Unit(const Unit&) = default;
		Unit(Unit&&) noexcept = default;

		UnitId _id;
		Kit _kit;
	};
}
