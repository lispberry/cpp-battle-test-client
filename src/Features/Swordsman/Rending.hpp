#pragma once

#include <Core/Base/Reflect.hpp>
#include <Core/Events/Effects.hpp>
#include <Core/Events/HitAttempt.hpp>
#include <Core/Events/Tags.hpp>
#include <Core/Model/Types.hpp>
#include <Features/Common/Tags.hpp>

namespace sw::swordsman
{
	// With probability chance/1000, a melee hit becomes "Rending": `damage` instead of the normal damage, dealt by the
	// ability so that other mechanics (the Hunter's poison) can react to it.
	struct Rending
	{
		static constexpr Ability Name{"rending"};
		using Tags = TagList<common::Wound>;

		Chance chance;
		Damage damage;

		[[nodiscard]]
		Effects on(const HitAttempt& attempt) const;
	};
	SW_REFLECT(Rending, (), (chance, damage))
}
