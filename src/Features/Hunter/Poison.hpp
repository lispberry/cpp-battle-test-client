#pragma once

#include <Core/Events/Effects.hpp>
#include <Core/Events/HitTaken.hpp>
#include <Core/Events/RoundEnd.hpp>
#include <Core/Model/Types.hpp>

#include <optional>
#include <vector>

namespace sw::hunter
{
	// Splits `total` into `parts` portions whose sum is exactly `total`; the first `total % parts` portions get one more
	// (7 in 5 parts: 2 2 1 1 1). Requires `parts > 0`.
	[[nodiscard]]
	std::vector<Damage> split(Damage total, Rounds parts);

	// Applied by poisoned arrows: deals `total` damage over five round ends, attributed to the hunter.
	// A portion is doubled in a round in which the poisoned unit takes a Wound (the swordsman's Rending).
	class Poison
	{
	public:
		static constexpr Rounds Duration{5};

		Poison(UnitId source, Damage total);

		[[nodiscard]]
		Effects on(const HitTaken& taken);

		[[nodiscard]]
		Effects on(const RoundEnd& end);

	private:
		UnitId _source;
		std::vector<Damage> _portions;
		std::optional<Round> _amplifiedIn;
	};
}
