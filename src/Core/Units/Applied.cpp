#include <Core/Events/Effects.hpp>
#include <Core/Events/Hooks.hpp>
#include <Core/Model/Types.hpp>
#include <Core/Units/Applied.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

namespace sw
{
	void Applied::add(std::unique_ptr<Hooks> component, const UnitId source)
	{
		_entries.push_back(Entry{.component = std::move(component), .source = source});
	}

	void Applied::remove(const UnitId source)
	{
		std::erase_if(_entries, [source](const Entry& entry) { return entry.source == source; });
	}

	bool Applied::empty() const
	{
		return _entries.empty();
	}

	bool Applied::has(const std::type_index type) const
	{
		return std::ranges::any_of(_entries, [type](const Entry& entry) { return entry.component->holds(type); });
	}

	Effects Applied::ask(const Asking asking, const std::function<Effects(Hooks&)>& dispatch)
	{
		Effects answer;
		std::vector<std::size_t> expired;
		for (std::size_t step = 0; step < _entries.size(); ++step)
		{
			const std::size_t index = asking.newestFirst ? _entries.size() - 1 - step : step;
			auto effects = dispatch(*_entries[index].component);
			if (effects.removeExpire())
			{
				expired.push_back(index);
			}
			if (asking.firstAnswerWins && !effects.empty())
			{
				answer = std::move(effects);
				break;
			}
			answer |= std::move(effects);
		}
		removeAt(std::move(expired));
		return answer;
	}

	void Applied::removeAt(std::vector<std::size_t> indices)
	{
		// From the back, so that each index still points at its entry.
		std::ranges::sort(indices, std::greater{});
		for (const std::size_t index : indices)
		{
			_entries.erase(std::next(_entries.begin(), static_cast<std::ptrdiff_t>(index)));
		}
	}
}
