#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Model/Types.hpp>

namespace sw
{
	/// Hit points. A unit without Health cannot be attacked; a unit whose Health reaches 0 dies after the action.
	struct Health
	{
		Hp hp;
	};
	SW_REFLECT(Health, (), (hp))
}
