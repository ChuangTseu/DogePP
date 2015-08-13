#include "Macro.h"

std::map<std::string, Macro> g_macros;

bool gMacroIsUnique(const std::string& strKey)
{
	return g_macros.find(strKey) != g_macros.end();
}

bool gMacroInsert(const std::string& strKey, Macro macro)
{
	bool bWasUnique = true;

	if (!gMacroIsUnique(strKey))
	{
		bWasUnique = false;
		fprintf(stderr, "Warning. Redefinition of macro '%s'.\n", strKey.c_str());
	}
	g_macros[strKey] = std::move(macro);

	return bWasUnique;
}

