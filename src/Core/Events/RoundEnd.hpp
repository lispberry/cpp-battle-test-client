#pragma once

#include <Core/Events/Context.hpp>

namespace sw
{
	/// Sent to every living unit at the end of each round, after all turns. `self()` is that unit. Every handler is
	/// asked; the effects they return all happen.
	///
	/// An applied component returns `effect::expire()` to be removed once its handler returns.
	///
	/// \code
	/// Effects Burn::on(const RoundEnd& end)
	/// {
	///     auto effects = effect::attack(source, end.self().id(), damage);
	///     left = Rounds{left.get() - 1};
	///     if (left.get() == 0)
	///         effects |= effect::expire();               // removed after this round end
	///     return effects;
	/// }
	/// \endcode
	class RoundEnd : public Context
	{
	public:
		/// Every handler is asked; all the effects happen.
		static constexpr bool FirstAnswerWins = false;

		explicit RoundEnd(const Context& context);
	};
}
