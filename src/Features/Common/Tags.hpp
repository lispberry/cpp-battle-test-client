#pragma once

namespace sw::common
{
	// The kinds of damage that other features react to, declared once so that a feature can recognise them without
	// knowing which ability deals them. An ability lists its kinds in `using Tags = TagList<...>`.

	// Tearing damage (the swordsman's Rending); the hunter's poison doubles in a round in which its target takes it.
	struct Wound
	{};
}
