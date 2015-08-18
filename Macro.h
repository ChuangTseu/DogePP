#pragma once

#include <vector>
#include <map>

#include "StringUtils.h"
#include "StringContext.h"

enum EMacroType {
	e_macrotype_definition,
	e_macrotype_function
};

struct MacroArg {
	std::vector<SubStrLoc> m_subLocations;
};

struct Macro {
	//
	// Common Core Macro data
	std::string m_name; // Debug

	EMacroType m_eType;
	std::string m_strContent;

	//
	// Function Macro specific data
	std::vector<MacroArg> m_args;

	bool PatchString(std::vector<std::string> vStrArgs, std::string& outStr) const
	{
		DOGE_ASSERT(m_args.size() == vStrArgs.size());

		outStr = m_strContent;
		StrCTX tmpCtx(outStr, "");

		std::vector<MacroArg> tmpMacroArgs = m_args;

		for (MacroArg& macroArg : tmpMacroArgs)
			for (SubStrLoc& macroArgSubLoc : macroArg.m_subLocations)
				tmpCtx.RegisterLivePatchableCtxSubLoc(macroArgSubLoc);

		for (size_t nStrArg = 0; nStrArg < vStrArgs.size(); ++nStrArg)
		{
			const std::string& strArg = vStrArgs[nStrArg];
			for (const SubStrLoc& macroArgSubLoc : tmpMacroArgs[nStrArg].m_subLocations)
			{
				tmpCtx.UnregisterLivePatchableCtxSubLoc(macroArgSubLoc);
				tmpCtx.Replace(macroArgSubLoc, strArg, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
			}
		}

		return true;
	}
};

extern std::map<std::string, Macro> g_macros;

bool gMacroIsUnique(const std::string& strKey);
bool gMacroInsert(const std::string& strKey, Macro macro);
bool gMacroIsDefined(const std::string& strKey);
const Macro& gMacroGet(const std::string& strKey);
void gMacroUndefine(const std::string& strKey);