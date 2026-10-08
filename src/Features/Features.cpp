#include <Core/IO/Commands.hpp>
#include <Features/Common/MarchCommand.hpp>
#include <Features/Features.hpp>
#include <Features/Hunter/HunterCommands.hpp>
#include <Features/Swordsman/SwordsmanCommands.hpp>

namespace sw
{
	void registerFeatures(Commands& commands)
	{
		common::registerCommon(commands);
		swordsman::registerSwordsman(commands);
		hunter::registerHunter(commands);
	}
}
