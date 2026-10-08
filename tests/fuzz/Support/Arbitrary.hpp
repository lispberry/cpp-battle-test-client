#pragma once

#include <Core/Base/Reflect.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <fuzzer/FuzzedDataProvider.h>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sw::fuzz
{
	// How to make a T out of fuzzer bytes: specialise Arbitrary<T> with `static T generate(FuzzedDataProvider&)`.
	// The bytes are read with LLVM's FuzzedDataProvider; this layer only adds types on top of it.
	//
	// Provided: bool, integers, std::optional, std::vector, std::variant, std::pair, std::string, Bounded<...>, and
	// every aggregate declared with SW_REFLECT whose fields are all Fuzzable (generated field by field) - so a unit's
	// kit, a command or a test's own input struct needs only its SW_REFLECT line. Game types with meaningful
	// ranges (Hp, Position, map operations, battlefields, ...) are in GameObjects.hpp.
	template <class T>
	struct Arbitrary;

	template <class T>
	concept Fuzzable = requires(FuzzedDataProvider& input) {
		{ Arbitrary<T>::generate(input) } -> std::same_as<T>;
	};

	template <Fuzzable T>
	[[nodiscard]]
	T take(FuzzedDataProvider& input)
	{
		return Arbitrary<T>::generate(input);
	}

	// An integer restricted to [Min, Max], e.g. Bounded<std::size_t, 0, 8> for "how many".
	template <std::integral T, T Min, T Max>
	struct Bounded
	{
		T value{Min};
	};

	template <>
	struct Arbitrary<bool>
	{
		static bool generate(FuzzedDataProvider& input)
		{
			return input.ConsumeBool();
		}
	};

	template <std::integral T>
		requires(!std::same_as<T, bool>)
	struct Arbitrary<T>
	{
		static T generate(FuzzedDataProvider& input)
		{
			return input.ConsumeIntegral<T>();
		}
	};

	template <std::integral T, T Min, T Max>
	struct Arbitrary<Bounded<T, Min, Max>>
	{
		static Bounded<T, Min, Max> generate(FuzzedDataProvider& input)
		{
			return {input.ConsumeIntegralInRange(Min, Max)};
		}
	};

	template <>
	struct Arbitrary<std::string>
	{
		static constexpr std::size_t MaxLength = 64;

		static std::string generate(FuzzedDataProvider& input)
		{
			return input.ConsumeRandomLengthString(MaxLength);
		}
	};

	template <Fuzzable T>
	struct Arbitrary<std::optional<T>>
	{
		static std::optional<T> generate(FuzzedDataProvider& input)
		{
			if (!input.ConsumeBool())
			{
				return std::nullopt;
			}
			return take<T>(input);
		}
	};

	template <Fuzzable T>
	struct Arbitrary<std::vector<T>>
	{
		static constexpr std::size_t MaxSize = 16;

		static std::vector<T> generate(FuzzedDataProvider& input)
		{
			const std::size_t size = input.ConsumeIntegralInRange<std::size_t>(0, MaxSize);
			std::vector<T> result;
			result.reserve(size);
			for (std::size_t i = 0; i < size; ++i)
			{
				result.push_back(take<T>(input));
			}
			return result;
		}
	};

	template <Fuzzable T, std::size_t Size>
	struct Arbitrary<std::array<T, Size>>
	{
		static std::array<T, Size> generate(FuzzedDataProvider& input)
		{
			std::array<T, Size> result{};
			for (T& element : result)
			{
				element = take<T>(input);
			}
			return result;
		}
	};

	template <Fuzzable First, Fuzzable Second>
	struct Arbitrary<std::pair<First, Second>>
	{
		static std::pair<First, Second> generate(FuzzedDataProvider& input)
		{
			First first = take<First>(input);
			return {std::move(first), take<Second>(input)};
		}
	};

	template <Fuzzable... Alternatives>
	struct Arbitrary<std::variant<Alternatives...>>
	{
		using Variant = std::variant<Alternatives...>;

		static Variant generate(FuzzedDataProvider& input)
		{
			return make(input, input.ConsumeIntegralInRange<std::size_t>(0, sizeof...(Alternatives) - 1));
		}

	private:
		template <std::size_t Index = 0>
		static Variant make(FuzzedDataProvider& input, const std::size_t chosen)
		{
			if constexpr (Index + 1 < sizeof...(Alternatives))
			{
				if (chosen != Index)
				{
					return make<Index + 1>(input, chosen);
				}
			}
			return Variant(std::in_place_index<Index>, take<std::variant_alternative_t<Index, Variant>>(input));
		}
	};

	namespace detail
	{
		template <class T>
		concept DescribedAggregate = std::is_aggregate_v<T> && std::is_default_constructible_v<T>
									 && !std::is_polymorphic_v<T> && reflect::Described<T>;

		template <DescribedAggregate T, std::size_t... Index>
		consteval bool allFieldsFuzzable(std::index_sequence<Index...> /*fields*/)
		{
			return (Fuzzable<reflect::FieldType<T, Index>> && ...);
		}
	}

	// Any aggregate declared with SW_REFLECT whose fields are fuzzable: generated field by field, in declaration order.
	template <detail::DescribedAggregate T>
		requires(detail::allFieldsFuzzable<T>(std::make_index_sequence<reflect::fieldCount<T>>{}))
	struct Arbitrary<T>
	{
		static T generate(FuzzedDataProvider& input)
		{
			T value{};
			reflect::forEachField(
					value, [&input]<Fuzzable Field>(Field& field) { field = take<std::remove_cvref_t<Field>>(input); });
			return value;
		}
	};
}
