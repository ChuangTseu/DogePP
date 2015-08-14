#pragma once

#include <vector>
#include <map>

#include "StringUtils.h"

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
};

extern std::map<std::string, Macro> g_macros;

bool gMacroIsUnique(const std::string& strKey);
bool gMacroInsert(const std::string& strKey, Macro macro);
bool gMacroIsDefined(const std::string& strKey);
