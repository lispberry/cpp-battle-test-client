#include <Core/Model/Types.hpp>
#include <Core/Units/AnyUnit.hpp>
#include <Core/World/UnitRegistry.hpp>

#include <memory>
#include <span>
#include <utility>

namespace sw
{
	void UnitRegistry::insert(std::unique_ptr<AnyUnit> unit)
	{
		const auto id = unit->id();
		_order.push_back(id);
		_units.emplace(id, Entry{.unit = std::move(unit), .rank = _inserted++});
	}

	bool UnitRegistry::contains(const UnitId id) const
	{
		return _units.contains(id);
	}

	AnyUnit* UnitRegistry::find(const UnitId id)
	{
		const auto found = _units.find(id);
		return found == _units.end() ? nullptr : found->second.unit.get();
	}

	const AnyUnit* UnitRegistry::find(const UnitId id) const
	{
		const auto found = _units.find(id);
		return found == _units.end() ? nullptr : found->second.unit.get();
	}

	std::span<const UnitId> UnitRegistry::order() const
	{
		return _order;
	}

	bool UnitRegistry::createdBefore(const UnitId left, const UnitId right) const
	{
		return _units.at(left).rank < _units.at(right).rank;
	}
}
