#pragma once

#include <map>
#include <list>
#include <memory>
#include <vector>

#include "Precedence.h"
#include "Tokens.h"
#include "Expressions.h"

typedef std::vector<Token>::const_iterator TokenCIt;

class PrefixParselet;
class InfixParselet;

class Parser {
public:
	Parser(TokenCIt tokenIt) : m_tokens(tokenIt) {}

	void Register(ETokenType eTokenType, std::unique_ptr<const PrefixParselet> parselet);

	void Register(ETokenType eTokenType, std::unique_ptr<const InfixParselet> parselet);

	std::unique_ptr<const Expression> ParseExpression(int precedence = LOWEST_PRECEDENCE);

	Token Consume();
	Token ConsumeExpected(ETokenType eExpected);

	bool Match(ETokenType eExpected);

private:
	Token LookAhead(int distance);

	int GetPrecedence();

private:
	TokenCIt m_tokens;

	std::list<Token> m_readTokens;

	std::map<ETokenType, std::unique_ptr<const PrefixParselet>> m_prefixParseletsMap;
	std::map<ETokenType, std::unique_ptr<const InfixParselet>> m_infixParseletsMap;
};

