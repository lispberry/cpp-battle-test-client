#include <Core/Base/RandomSource.hpp>
#include <Core/Events/Context.hpp>
#include <Core/Model/Errors.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Query/Units.hpp>
#include <Core/World/UnitRef.hpp>
#include <Core/World/World.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <optional>

namespace sw
{
	Units::Iterator::Iterator(const Units& units, const std::size_t index) :
			_units(&units),
			_index(index)
	{
		if (!limitReached())
		{
			skipRejected();
		}
	}

	UnitRef Units::Iterator::operator*() const
	{
		return {*_units->_world, _units->_order[_index]};
	}

	Units::Iterator& Units::Iterator::operator++()
	{
		++_index;
		++_yielded;
		if (!limitReached())
		{
			skipRejected();
		}
		return *this;
	}

	Units::Iterator Units::Iterator::operator++(int)
	{
		Iterator previous = *this;
		++*this;
		return previous;
	}

	bool Units::Iterator::operator==(std::default_sentinel_t /*end*/) const
	{
		return limitReached() || _index >= _units->_order.size();
	}

	bool Units::Iterator::limitReached() const
	{
		return _units->_limit && _yielded >= *_units->_limit;
	}

	void Units::Iterator::skipRejected()
	{
		while (_index < _units->_order.size() && !_units->accepts(_units->_order[_index]))
		{
			++_index;
		}
	}

	Units::Units(const World& world, const std::optional<UnitId> excluded) :
			_world(&world),
			_order(world.units().order()),
			_excluded(excluded)
	{}

	Units::Iterator Units::begin() const
	{
		return {*this, 0};
	}

	std::default_sentinel_t Units::end()
	{
		return std::default_sentinel;
	}

	Units operator|(Units units, const Take take)
	{
		units._limit = std::min(units._limit.value_or(take.count), take.count);
		return units;
	}

	bool Units::accepts(const UnitId id) const
	{
		if (id == _excluded)
		{
			return false;
		}
		const UnitRef unit(*_world, id);
		if (!unit.isAlive())
		{
			return false;
		}
		return std::ranges::all_of(
				_filters, [&unit](const std::function<bool(const UnitRef&)>& filter) { return filter(unit); });
	}

	Take take(const std::size_t limit)
	{
		return Take{limit};
	}

	PickRandom pickRandom(RandomSource& random)
	{
		return PickRandom{&random};
	}

	PickRandom pickRandom(const Context& context)
	{
		return pickRandom(context.randomSource());
	}

	bool operator|(const Units& units, Any /*terminal*/)
	{
		return units.begin() != std::default_sentinel;
	}

	bool operator|(const Units& units, None /*terminal*/)
	{
		return units.begin() == std::default_sentinel;
	}

	std::size_t operator|(const Units& units, Count /*terminal*/)
	{
		std::size_t result = 0;
		for (auto it = units.begin(); it != std::default_sentinel; ++it)
		{
			++result;
		}
		return result;
	}

	std::expected<UnitRef, Idle> operator|(const Units& units, First /*terminal*/)
	{
		const auto it = units.begin();
		if (it == std::default_sentinel)
		{
			return std::unexpected(Idle::NoTarget);
		}
		return *it;
	}

	std::expected<UnitRef, Idle> operator|(const Units& units, const PickRandom terminal)
	{
		// Reservoir sampling: one pass, no storage. The k-th accepted unit replaces the pick with probability 1/k,
		// which leaves every unit picked with probability 1/n.
		std::optional<UnitRef> picked;
		uint32_t seen = 0;
		for (const UnitRef unit : units)
		{
			++seen;
			if (seen == 1 || terminal.random->oneIn(seen))
			{
				picked = unit;
			}
		}
		if (!picked)
		{
			return std::unexpected(Idle::NoTarget);
		}
		return *picked;
	}
}
