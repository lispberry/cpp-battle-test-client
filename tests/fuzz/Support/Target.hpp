#pragma once

#include <Support/Arbitrary.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fuzzer/FuzzedDataProvider.h>
#include <print>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace sw::fuzz
{
	// A parameter of a fuzz function: any Fuzzable type, by value or const reference.
	template <class P>
	concept FuzzParameter = Fuzzable<std::remove_cvref_t<P>>;

	// Decodes the bytes into the function's parameters (left to right) and calls it.
	template <FuzzParameter... Parameters>
	int run(void (*function)(Parameters...), const std::uint8_t* data, const std::size_t size)
	{
		FuzzedDataProvider input(data, size);
		// A braced initializer evaluates left to right, so parameters are decoded in declaration order.
		std::tuple<std::remove_cvref_t<Parameters>...> arguments{take<std::remove_cvref_t<Parameters>>(input)...};
		std::apply(function, arguments);
		return 0;
	}

	// A broken property: report it and crash, so libFuzzer (or the replay driver) saves and reports the input.
	inline void expect(const bool holds, const std::string_view property)
	{
		if (!holds)
		{
			std::println(stderr, "fuzz property violated: {}", property);
			std::abort();
		}
	}
}

// Turns a typed function into a libFuzzer target:
//
//     void battleKeepsTheWorldConsistent(const fuzz::Battlefield& battlefield) { ... }
//     SW_FUZZ_TARGET(battleKeepsTheWorldConsistent)
//
// One target per file: libFuzzer links one LLVMFuzzerTestOneInput per executable.
#define SW_FUZZ_TARGET(function)                                                                                       \
	extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);                                 \
	extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)                                  \
	{                                                                                                                  \
		return ::sw::fuzz::run(&(function), data, size);                                                               \
	}
