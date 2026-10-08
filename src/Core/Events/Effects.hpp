#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Events/Event.hpp>
#include <Core/Events/Events.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Tags.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Log/Records.hpp>
#include <Core/Model/Geometry.hpp>
#include <Core/Model/Types.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace sw
{
	class AnyUnit;

	/// A hit through the hit pipeline: HitAttempt on the attacker (an ability may take it over), damage, HitTaken on
	/// the target.
	struct HitEffect
	{
		AttackKind kind;
		UnitId attacker;
		UnitId target;
		Damage damage;
	};

	/// Plain damage: no HitAttempt, so no ability can take it over. `ability` is what dealt it and `tags` the kinds of
	/// damage it is, for the target's HitTaken handlers; nullopt and no tags for damage no ability dealt (poison
	/// ticking).
	struct AttackEffect
	{
		UnitId attacker;
		UnitId target;
		Damage damage;
		std::optional<Ability> ability;
		TagSet tags;
	};

	/// One step of `unit` to the neighbouring cell `to`. Dropped if the step is no longer possible when it happens.
	struct MoveEffect
	{
		UnitId unit;
		Position to;
	};

	/// A component applied to `target` during the battle, by `source`.
	struct ApplyEffect
	{
		UnitId target;
		std::unique_ptr<Hooks> component;
		UnitId source;
	};

	/// Removes every component `source` applied to `target`.
	struct UnapplyEffect
	{
		UnitId target;
		UnitId source;
	};

	/// `unit` used `ability`: logged as UNIT_ABILITY_USED, with no effect of its own.
	struct AbilityEffect
	{
		UnitId unit;
		Ability ability;
	};

	/// A log record written as is, in order with the other records of the action (MARCH_ENDED).
	struct ReportEffect
	{
		Record record;
	};

	/// A change to the state of `unit`'s own components, such as a finished order. Made through an effect because
	/// deciding never changes the world; skipped if the unit is gone.
	struct ChangeEffect
	{
		UnitId unit;
		std::function<void(AnyUnit&)> change;
	};

	/// Removes the applied component whose handler returned it. Nothing for a kit component, which stays for the
	/// unit's whole life.
	struct ExpireEffect
	{};

	/// One change to the world. The Executor performs it after the handler that returned it.
	using Effect = std::variant<
			HitEffect,
			AttackEffect,
			MoveEffect,
			ApplyEffect,
			UnapplyEffect,
			AbilityEffect,
			ReportEffect,
			ChangeEffect,
			ExpireEffect>;

	/// The changes a handler causes, in order: what a unit does on its turn, what a component does in answer to an
	/// event. Built with the functions in namespace `effect` and joined with `|`; nothing happens until the Executor
	/// performs them, so deciding never changes the world. Move-only: it owns the components it applies.
	///
	/// \code
	/// Effects PoisonArrows::on(const HitAttempt& attempt) const
	/// {
	///     if (attempt.kind() != common::Ranged || !attempt.roll(chance))
	///         return {};                                 // no effects: the plain hit lands
	///     return effect::useAbility(attempt.attacker(), Name)
	///          | effect::apply(attempt.target(), Poison{attempt.attacker(), total}, attempt.attacker());
	/// }
	/// \endcode
	class Effects
	{
	public:
		Effects() = default;

		/// One effect; the functions in namespace `effect` make them.
		explicit Effects(Effect effect);

		/// `left`, then `right`.
		[[nodiscard]]
		friend Effects operator|(Effects left, Effects right)
		{
			left |= std::move(right);
			return left;
		}

		/// Appends `other`.
		Effects& operator|=(Effects other);

		[[nodiscard]]
		bool empty() const;

		[[nodiscard]]
		std::size_t size() const;

		[[nodiscard]]
		const Effect& operator[](std::size_t index) const;

		/// Moves the effects out and leaves this empty.
		[[nodiscard]]
		std::vector<Effect> take();

		/// Removes the ExpireEffects, and says whether there was one. The engine's, for the applied component that
		/// returned them.
		bool removeExpire();

	private:
		std::vector<Effect> _effects;
	};

	namespace detail
	{
		// A question is still open while its answer can change: a refused attack stays refused.
		template <std::copyable A>
		constexpr bool open(const A& /*answer*/)
		{
			return true;
		}

		template <std::copyable A>
		constexpr bool open(const std::optional<A>& answer)
		{
			return answer.has_value();
		}
	}

	/// Runs `component`'s handler of `event`, if it has one; no effects otherwise. A handler with the wrong shape is a
	/// compile error rather than a handler that never runs.
	template <ClassType C, EventType E>
	[[nodiscard]]
	Effects dispatchTo(C& component, const E& event)
	{
		if constexpr (Handles<C, E>)
		{
			return component.on(event);
		}
		else
		{
			static_assert(!Declares<C, E>, "an event handler is `Effects on(const E&)`");
			return {};
		}
	}

	/// Runs `component`'s handler of `query` from the answer so far, if it has one and the question is still open.
	template <ClassType C, QueryType Q>
	void dispatchTo(const C& component, Q& query)
	{
		if constexpr (Answers<C, Q>)
		{
			if (detail::open(query.answer()))
			{
				query.answer() = component.on(std::as_const(query));
			}
		}
		else
		{
			static_assert(!Declares<std::remove_const_t<C>, Q>, "a question handler is `Q::Answer on(const Q&) const`");
		}
	}

	namespace detail
	{
		// `C` itself, or what it wraps: a decorator declares `using Wraps = Inner;`.
		template <ClassType C>
		bool isOrWraps(const std::type_index wanted)
		{
			if constexpr (requires { typename C::Wraps; })
			{
				return wanted == std::type_index(typeid(C)) || isOrWraps<typename C::Wraps>(wanted);
			}
			else
			{
				return wanted == std::type_index(typeid(C));
			}
		}
	}

	/// A component that can be applied to a unit during the battle: it handles at least one event or answers at least
	/// one question (a poison, a shield, a picked-up bow).
	///
	/// \code
	/// static_assert(Applicable<hunter::Poison>);              // HitTaken, RoundEnd
	/// static_assert(Applicable<common::Timed<common::Invulnerable>>);   // Targeted, RoundEnd
	/// static_assert(!Applicable<Health>);                     // data only
	/// \endcode
	template <class C>
	concept Applicable = Component<C>
						 && (Handles<C, Turn> || Handles<C, HitAttempt> || Handles<C, HitTaken> || Handles<C, RoundEnd>
							 || Answers<C, Targeted> || Answers<C, PlacementQuery> || Answers<C, SpeedQuery>);

	/// Holds an applied component of type `C` and passes it every event and question. Feature code never names it:
	/// `effect::apply` wraps the plain component.
	///
	/// \code
	/// template <Applicable C>
	/// Effects effect::apply(const UnitId target, C component, const UnitId source)
	/// {
	///     return Effects{ApplyEffect{target, std::make_unique<EffectOf<C>>(std::move(component)), source}};
	/// }
	/// \endcode
	template <Applicable C>
	class EffectOf final : public Hooks
	{
	public:
		explicit EffectOf(C component) :
				_component(std::move(component))
		{}

		[[nodiscard]]
		Effects dispatch(const Turn& turn) override
		{
			return dispatchTo(_component, turn);
		}

		[[nodiscard]]
		Effects dispatch(const HitAttempt& attempt) override
		{
			return dispatchTo(_component, attempt);
		}

		[[nodiscard]]
		Effects dispatch(const HitTaken& taken) override
		{
			return dispatchTo(_component, taken);
		}

		[[nodiscard]]
		Effects dispatch(const RoundEnd& end) override
		{
			return dispatchTo(_component, end);
		}

		void dispatch(Targeted& targeted) const override
		{
			dispatchTo(_component, targeted);
		}

		void dispatch(PlacementQuery& placement) const override
		{
			dispatchTo(_component, placement);
		}

		void dispatch(SpeedQuery& speed) const override
		{
			dispatchTo(_component, speed);
		}

		[[nodiscard]]
		std::type_index type() const override
		{
			return typeid(C);
		}

		[[nodiscard]]
		bool holds(const std::type_index wanted) const override
		{
			return detail::isOrWraps<C>(wanted);
		}

	private:
		C _component;
	};

	namespace detail
	{
		// Calls `apply` on the unit's component `C`, if it has one. One type per component, so every effect::change of
		// `C` shares the check.
		template <Component C>
		struct ChangeOf
		{
			std::function<void(C&)> apply;

			template <ClassType U>
			void operator()(U& owner) const
			{
				if (C* component = owner.template get<C>(); component != nullptr)
				{
					apply(*component);
				}
			}
		};
	}

	/// A component that implements an ability: its `Name` is what UNIT_ABILITY_USED prints and what the damage it deals
	/// carries; it may list the kinds of that damage in `Tags` (a TagList).
	///
	/// \code
	/// static_assert(AbilityComponent<swordsman::Rending>);   // Name "rending", Tags TagList<common::Wound>
	/// \endcode
	template <class A>
	concept AbilityComponent = Component<A> && requires {
		{ A::Name } -> std::convertible_to<Ability>;
	};

	namespace detail
	{
		// The ability's tags at run time; none if it declares no Tags.
		template <AbilityComponent A>
		TagSet tagsOf()
		{
			if constexpr (requires { typename A::Tags; })
			{
				return TagSet(typename A::Tags{});
			}
			else
			{
				return {};
			}
		}
	}

	/// The effects a handler returns, one function per kind of change. Join them with `|`:
	/// `return effect::useAbility(attacker, Name) | effect::attack(attacker, target, damage, Name);`.
	namespace effect
	{
		/// A hit through the hit pipeline (HitEffect).
		[[nodiscard]]
		Effects hit(AttackKind kind, UnitId attacker, UnitId target, Damage damage);

		/// Plain damage that `ability` dealt, or no ability (AttackEffect). Counts as `attacker`'s in the log even if
		/// `attacker` is dead.
		[[nodiscard]]
		Effects attack(UnitId attacker, UnitId target, Damage damage, std::optional<Ability> ability = std::nullopt);

		/// Plain damage dealt by `ability`, a component: the damage carries its Name and Tags.
		template <AbilityComponent A>
		[[nodiscard]]
		Effects attack(const UnitId attacker, const UnitId target, const Damage damage, const A& /*ability*/)
		{
			return Effects{AttackEffect{
					.attacker = attacker,
					.target = target,
					.damage = damage,
					.ability = A::Name,
					.tags = detail::tagsOf<A>(),
			}};
		}

		/// One step of `unit` to the neighbouring cell `to`.
		[[nodiscard]]
		Effects moveTo(UnitId unit, Position to);

		/// Applies `component` to `target` on behalf of `source`; it takes part in events after the target's kit and
		/// the components applied before it.
		template <Applicable C>
		[[nodiscard]]
		Effects apply(const UnitId target, C component, const UnitId source)
		{
			return Effects{ApplyEffect{
					.target = target,
					.component = std::make_unique<EffectOf<C>>(std::move(component)),
					.source = source,
			}};
		}

		/// Removes every component `source` applied to `target`.
		[[nodiscard]]
		Effects unapply(UnitId target, UnitId source);

		/// Removes the applied component whose handler returns it, such as a poison that ran out.
		[[nodiscard]]
		Effects expire();

		/// `unit` used `ability`: logged as UNIT_ABILITY_USED.
		[[nodiscard]]
		Effects useAbility(UnitId unit, Ability ability);

		/// `unit` used `ability`, a component: logged with its Name.
		template <AbilityComponent A>
		[[nodiscard]]
		Effects useAbility(const UnitId unit, const A& /*ability*/)
		{
			return useAbility(unit, A::Name);
		}

		/// A log record written as is (MARCH_ENDED).
		[[nodiscard]]
		Effects report(Record record);

		/// Calls `modify` on `unit`'s component `C` when the effect happens; nothing if the unit has no `C`.
		///
		/// \code
		/// return effect::change<common::March>(self, [](common::March& march) { march.target.reset(); });
		/// \endcode
		template <Component C, std::invocable<C&> F>
		[[nodiscard]]
		Effects change(const UnitId unit, F modify)
		{
			return Effects{ChangeEffect{.unit = unit, .change = detail::ChangeOf<C>{.apply = std::move(modify)}}};
		}
	}
}
