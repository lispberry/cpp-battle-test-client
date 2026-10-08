#include <Core/Events/SpeedQuery.hpp>
#include <Core/Events/Turn.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/World/World.hpp>

namespace sw
{
	Speed Turn::speed(const Speed base) const
	{
		SpeedQuery query{.speed = base};
		world().units().find(selfId())->dispatch(query);
		return query.speed;
	}
}
