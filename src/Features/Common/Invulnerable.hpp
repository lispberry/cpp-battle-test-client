#pragma once

#include <Core/Events/Targeted.hpp>
#include <Core/Model/Geometry.hpp>

#include <optional>

namespace sw::common
{
	struct Invulnerable
	{
		[[nodiscard]]
		std::optional<Attack> on(const Targeted& /*targeted*/) const
		{
			return std::nullopt;
		}
	};
}
