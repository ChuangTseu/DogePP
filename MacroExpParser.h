#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>
#include <memory>


#include "ErrorLogging.h"
#include "EnumCTTI.h"
#include "StringUtils.h"
#include "PreProcessorUtils.h"

#include "Tokens.h"
#include "Expressions.h"
#include "Parselets.h"
#include "BaseParser.h"

class MacroExpParser : public Parser
{
public:
	MacroExpParser(TokenCIt tokenIt);
	~MacroExpParser();

	void Postfix(ETokenType eTokenType, int precedence);
	void Prefix(ETokenType eTokenType, int precedence);
	void InfixLeft(ETokenType eTokenType, int precedence);
	void InfixRight(ETokenType eTokenType, int precedence);
};

