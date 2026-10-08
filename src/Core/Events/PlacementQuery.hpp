#pragma once

#include <Core/Model/Geometry.hpp>

namespace sw
{
	/// Asked of a unit when it is placed on the map: which cells does it take, and on which layer? Starts at one
	/// ground cell at the spawn position; each handler returns the placement as it sees it, from the placement so far.
	///
	/// \code
	/// struct Body                                        // common::Body
	/// {
	///     uint32_t width{1};
	///     uint32_t height{1};
	///     Placement on(const PlacementQuery& query) const
	///     {
	///         return {.origin = query.placement.origin, .size = {.width = width, .height = height},
	///                 .layer = query.placement.layer};
	///     }
	/// };
	/// \endcode
	struct PlacementQuery
	{
		/// What a handler returns.
		using Answer = Placement;

		/// The placement so far.
		Placement placement;

		/// The answer so far, for the engine to fill in.
		[[nodiscard]]
		Answer& answer()
		{
			return placement;
		}
	};
}
