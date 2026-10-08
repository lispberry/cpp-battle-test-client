#pragma once

#include <Core/Base/Concepts.hpp>
#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/Event.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/PlacementQuery.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Targeted.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/Units/Unit.hpp>

#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace sw
{
	namespace detail
	{
		// Combining answers is written once here, not in UnitOf's templates, so that its rules are one function each.

		// The effects of the first answer that has any, asking in order: the answers after it are not asked.
		[[nodiscard]]
		inline Effects firstAnswer(const std::vector<std::function<Effects()>>& answers)
		{
			for (const auto& answer : answers)
			{
				if (auto effects = answer(); !effects.empty())
				{
					return effects;
				}
			}
			return {};
		}

		// The effects of every answer, in order.
		[[nodiscard]]
		inline Effects allAnswers(const std::vector<std::function<Effects()>>& answers)
		{
			Effects all;
			for (const auto& answer : answers)
			{
				all |= answer();
			}
			return all;
		}

		// `candidate` if nothing was found yet and its type is the wanted one; `found` otherwise.
		[[nodiscard]]
		inline void* pick(void* found, const std::type_index wanted, const std::type_index type, void* candidate)
		{
			return found == nullptr && wanted == type ? candidate : found;
		}

		[[nodiscard]]
		inline const void* pick(
				const void* found, const std::type_index wanted, const std::type_index type, const void* candidate)
		{
			return found == nullptr && wanted == type ? candidate : found;
		}
	}

	/// Stores a unit class by value behind the AnyUnit interface. An event or a question reaches every component of
	/// `T::Kit` that handles it, in declaration order, and the unit's applied components (see Turn for the order, and
	/// each event's FirstAnswerWins for which answers count).
	///
	/// Prefer makeUnit to naming this class.
	///
	/// \code
	/// std::unique_ptr<AnyUnit> unit = std::make_unique<UnitOf<Swordsman>>(Swordsman{UnitId{1}, kit});
	/// // the same as makeUnit<Swordsman>(UnitId{1}, kit)
	/// \endcode
	template <UnitType T>
	class UnitOf final : public AnyUnit
	{
	public:
		explicit UnitOf(T unit) :
				_unit(std::move(unit))
		{}

		[[nodiscard]]
		UnitId id() const override
		{
			return _unit.id();
		}

		[[nodiscard]]
		UnitName name() const override
		{
			return T::Name;
		}

		[[nodiscard]]
		std::type_index type() const override
		{
			return typeid(T);
		}

		[[nodiscard]]
		Effects dispatch(const Turn& turn) override
		{
			return detail::firstAnswer({
					[this, &turn] { return applied().dispatch(turn); },
					[this, &turn] { return ownTurn(turn); },
			});
		}

		[[nodiscard]]
		Effects dispatch(const HitAttempt& attempt) override
		{
			return answer(attempt);
		}

		[[nodiscard]]
		Effects dispatch(const HitTaken& taken) override
		{
			return answer(taken);
		}

		[[nodiscard]]
		Effects dispatch(const RoundEnd& end) override
		{
			return answer(end);
		}

		void dispatch(Targeted& targeted) const override
		{
			ask(targeted);
		}

		void dispatch(PlacementQuery& placement) const override
		{
			ask(placement);
		}

		void dispatch(SpeedQuery& speed) const override
		{
			ask(speed);
		}

	protected:
		[[nodiscard]]
		const void* find(const std::type_index type) const override
		{
			return findIn(*this, type);
		}

		[[nodiscard]]
		void* find(const std::type_index type) override
		{
			return findIn(*this, type);
		}

	private:
		// The unit's own turn, once its applied components passed: its class's rule, or its kit in order.
		[[nodiscard]]
		Effects ownTurn(const Turn& turn) const
		{
			if constexpr (DecidesItself<T>)
			{
				return _unit.on(turn);
			}
			else
			{
				return _unit.kitTurn(turn);
			}
		}

		// An event other than the turn: the kit in declaration order, then the applied components.
		template <EventType E>
		[[nodiscard]]
		Effects answer(const E& event)
		{
			std::vector<std::function<Effects()>> answers;
			reflect::forEachField(
					_unit.kit(),
					[&answers, &event]<Component C>(C& component)
					{ answers.emplace_back([&component, &event] { return dispatchTo(component, event); }); });
			answers.emplace_back([this, &event] { return applied().dispatch(event); });
			if constexpr (E::FirstAnswerWins)
			{
				return detail::firstAnswer(answers);
			}
			else
			{
				return detail::allAnswers(answers);
			}
		}

		// A question: the kit in declaration order, then the applied components, each from the answer so far.
		template <QueryType Q>
		void ask(Q& query) const
		{
			reflect::forEachField(
					_unit.kit(), [&query]<Component C>(const C& component) { dispatchTo(component, query); });
			applied().dispatch(query);
		}

		// The first component of the requested type, or nullptr.
		template <std::derived_from<UnitOf> Self>
		static auto* findIn(Self& self, const std::type_index type)
		{
			using Pointer = std::conditional_t<std::is_const_v<Self>, const void*, void*>;
			Pointer found = nullptr;
			reflect::forEachField(
					self._unit.kit(),
					[&found, type]<ClassType C>(C& component)
					{ found = detail::pick(found, type, typeid(C), &component); });
			return found;
		}

		T _unit;
	};

	/// Creates a unit of class `T` from its `id` and `kit`, ready to be spawned. The id is not checked here:
	/// Simulation::spawn rejects a duplicate.
	///
	/// \code
	/// SpawnOrder order{
	///         .position = {.x = 2, .y = 3},
	///         .unit = makeUnit<Swordsman>(
	///                 UnitId{1},
	///                 {.health = {Hp{100}},
	///                  .sword = {Damage{5}},
	///                  .rending = {.chance = Chance{200}, .damage = Damage{12}},
	///                  .march = {.speed = Speed{1}, .target = std::nullopt}}),
	/// };
	/// \endcode
	template <UnitType T>
	[[nodiscard]]
	std::unique_ptr<AnyUnit> makeUnit(const UnitId id, typename T::Kit kit)
	{
		return std::make_unique<UnitOf<T>>(T{id, std::move(kit)});
	}
}
