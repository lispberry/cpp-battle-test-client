#include <Core/Base/Reflect.hpp>
#include <Core/Model/Types.hpp>

#include <concepts>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sw;

namespace reflect_test
{
	struct Tag
	{};

	struct Plain
	{
		uint32_t count{};
		Damage damage;
	};
	SW_REFLECT(Plain, (), (count, damage))

	struct Derived : Tag
	{
		Chance chance;
	};
	SW_REFLECT(Derived, (Tag), (chance))

	struct MissingField
	{
		uint32_t kept{};
		uint32_t forgotten{};
	};
	SW_REFLECT(MissingField, (), (kept))

	struct MissingBase : Tag
	{
		uint32_t value{};
	};
	SW_REFLECT(MissingBase, (), (value))

	struct NotDescribed
	{
		uint32_t value{};
	};
}

using namespace reflect_test;

static_assert(reflect::Described<Plain>);
static_assert(reflect::Described<const Plain>);
static_assert(reflect::Described<Derived>, "a base is fine once it is described");
static_assert(!reflect::Described<MissingField>, "a field left out of SW_REFLECT is caught");
static_assert(!reflect::Described<MissingBase>, "a base left out of SW_REFLECT is caught");
static_assert(!reflect::Described<NotDescribed>);
static_assert(reflect::fieldCount<Plain> == 2);
static_assert(reflect::fieldCount<Derived> == 1);
static_assert(std::is_same_v<reflect::FieldType<Plain, 1>, Damage>);

TEST_CASE("forEachNamedField visits the fields in declaration order, with their names")
{
	const Plain plain{.count = 3, .damage = Damage{7}};
	std::vector<std::string> seen;
	reflect::forEachNamedField(
			plain,
			[&seen](const std::string_view name, const std::copyable auto& /*value*/) { seen.emplace_back(name); });
	CHECK(seen == std::vector<std::string>{"count", "damage"});
}

TEST_CASE("forEachField can change the fields of a non-const value")
{
	Plain plain{};
	reflect::forEachField(
			plain,
			[]<std::copyable F>(F& field)
			{
				if constexpr (std::is_same_v<F, uint32_t>)
				{
					field = 5;
				}
				else
				{
					field = Damage{9};
				}
			});
	CHECK(plain.count == 5);
	CHECK(plain.damage == Damage{9});
}
