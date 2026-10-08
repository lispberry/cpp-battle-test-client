#include <Core/World/Map.hpp>
#include <Core/World/World.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace sw
{
	World::World(std::unique_ptr<Map> map) :
			_map(std::move(map))
	{
		if (_map == nullptr)
		{
			throw std::invalid_argument("a world needs a map");
		}
	}
}
