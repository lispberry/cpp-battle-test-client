#pragma once

#include <concepts>
#include <type_traits>

namespace sw
{
	/// A class type. Works on incomplete types too, so it can constrain a CRTP argument or a tag struct.
	///
	/// \code
	/// template <ClassType Tag>
	/// class StrongType;                                  // StrongType<int, struct HpTag> — the tag is incomplete
	/// \endcode
	template <class T>
	concept ClassType = std::is_class_v<T>;

	/// An aggregate: initialisable with designated initializers. Scenario commands, log records and unit kits are
	/// aggregates, declared with SW_REFLECT so that sw::reflect can walk their fields.
	///
	/// \code
	/// struct SpawnHunterCommand { uint32_t unitId; uint32_t x; uint32_t y; };
	/// static_assert(Aggregate<SpawnHunterCommand>);
	/// \endcode
	template <class T>
	concept Aggregate = std::is_aggregate_v<T>;

	/// A reusable building block of a unit, in its kit or applied during the battle: a copyable value type with data
	/// and, optionally, handlers (`on`) of events and questions.
	///
	/// \code
	/// struct Health { Hp hp; };                          // data only
	/// struct Invulnerable                                // answers a question
	/// {
	///     std::optional<Attack> on(const Targeted&) const { return std::nullopt; }
	/// };
	/// static_assert(Component<Health> && Component<Invulnerable>);
	/// \endcode
	template <class T>
	concept Component = ClassType<T> && std::copy_constructible<T>;
}
