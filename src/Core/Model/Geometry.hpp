#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Base/StrongType.hpp>
#include <Core/Model/Types.hpp>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <vector>

namespace sw
{
	/// A cell of the map; `{0, 0}` is the top-left corner.
	struct Position
	{
		uint32_t x{};
		uint32_t y{};

		// Not defaulted: the defaulted == short-circuits, and older llvm-cov reports its branches as half covered.
		friend bool operator==(const Position& left, const Position& right)
		{
			return std::tie(left.x, left.y) == std::tie(right.x, right.y);
		}
	};

	/// A unit's footprint in cells; 1×1 by default.
	struct Size
	{
		uint32_t width{1};
		uint32_t height{1};
	};

	/// Ground units occupy cells; air units fly over them and never block anyone.
	enum class Layer : uint8_t
	{
		Ground,
		Air,
	};

	/// Where a unit is: the top-left cell of its footprint, the footprint, and its layer.
	///
	/// \code
	/// const Placement tower{.origin = {.x = 2, .y = 3}, .size = {.width = 2, .height = 2}};
	/// const bool inside = tower.covers({.x = 3, .y = 4});            // true: it covers x 2..3, y 3..4
	/// const auto gap = distance(tower, Placement{.origin = {.x = 4, .y = 3}});   // 1
	/// \endcode
	struct Placement
	{
		Position origin;
		Size size;
		Layer layer{Layer::Ground};

		/// Whether `cell` is inside the footprint (regardless of layer).
		[[nodiscard]]
		bool covers(Position cell) const;
	};
	SW_REFLECT(Placement, (), (origin, size, layer))

	/// The cells `placement` covers, row by row from its origin. Layers are ignored.
	[[nodiscard]]
	std::vector<Position> cells(const Placement& placement);

	/// Chebyshev distance between cells: 1 for any of the 8 neighbours.
	[[nodiscard]]
	Distance distance(Position from, Position to);

	/// Gap between two footprints in cells: 0 when they overlap, 1 when they touch. Layers are ignored.
	[[nodiscard]]
	Distance distance(const Placement& from, const Placement& to);

	/// The neighbour of `from` one step closer to `to` (diagonals included); `from` itself when they are equal.
	///
	/// \code
	/// const auto next = stepToward({.x = 5, .y = 5}, {.x = 8, .y = 2});   // {6, 4}
	/// \endcode
	[[nodiscard]]
	Position stepToward(Position from, Position to);

	/// An inclusive interval of distances, e.g. how near and how far an attack reaches.
	///
	/// \code
	/// const Range bow{.min = Distance{2}, .max = Distance{4}};
	/// const bool reaches = bow.contains(Distance{1});         // false: too close
	/// const auto closer = bow.shrunk(Distance{3});    // {0, 1}
	/// \endcode
	struct Range
	{
		Distance min;
		Distance max;

		/// Whether `min <= distance <= max`.
		[[nodiscard]]
		bool contains(Distance distance) const;

		/// Both ends reduced by `amount`, never below zero.
		[[nodiscard]]
		Range shrunk(Distance amount) const;
	};

	/// What kind of attack it is, by name, e.g. "melee". Abilities and defences react to some kinds only (rending to
	/// melee, flying against it). Core knows no kind: each is declared where it is defined (`common::Melee`).
	using AttackKind = StrongType<std::string_view, struct AttackKindTag>;

	/// How an attacker reaches its target: the kind of attack and the distances it covers.
	///
	/// \code
	/// const Attack bow{.kind = common::Ranged, .range = {.min = Distance{2}, .max = Distance{5}}};
	/// auto targets = turn.units() | attackableBy(turn.self(), bow);
	/// \endcode
	struct Attack
	{
		AttackKind kind;
		Range range;
	};
	SW_REFLECT(Attack, (), (kind, range))
}
