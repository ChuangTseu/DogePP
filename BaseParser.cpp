#include "BaseParser.h"

#include "Parselets.h"

void Parser::Register(ETokenType eTokenType, std::unique_ptr<const InfixParselet> parselet)
{
	m_infixParseletsMap.insert(std::make_pair(eTokenType, std::move(parselet)));
}

void Parser::Register(ETokenType eTokenType, std::unique_ptr<const PrefixParselet> parselet)
{
	m_prefixParseletsMap.insert(std::make_pair(eTokenType, std::move(parselet)));
}

std::unique_ptr<const Expression> Parser::ParseExpression(int precedence)
{
	Token token = Consume();

	const auto itPrefixParselet = m_prefixParseletsMap.find(token.m_type);
	DOGE_ASSERT_MESSAGE(itPrefixParselet != m_prefixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
	const PrefixParselet* prefix = itPrefixParselet->second.get();

	std::unique_ptr<const Expression> left = prefix->Parse(*this, token);

	while (precedence > GetPrecedence()) {
		token = Consume();

		const auto itInfixParselet = m_infixParseletsMap.find(token.m_type);
		DOGE_ASSERT_MESSAGE(itInfixParselet != m_infixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
		const InfixParselet* infix = itInfixParselet->second.get();

		left = infix->Parse(*this, std::move(left), token);
	}

	return left;
}

Token Parser::Consume()
{
	// Make sure we've read the token.
	LookAhead(0);

	Token token = m_readTokens.front();
	m_readTokens.pop_front();
	return token;
}

Token Parser::ConsumeExpected(ETokenType eExpected)
{
	Token token = LookAhead(0);
	DOGE_ASSERT_MESSAGE(token.m_type == eExpected, "Expected token '%s' and found '%s'.\n", EnumToSzName(eExpected), EnumToSzName(token.m_type));

	return Consume();
}

bool Parser::Match(ETokenType eExpected)
{
	Token token = LookAhead(0);
	if (token.m_type != eExpected) {
		return false;
	}

	Consume();
	return true;
}

Token Parser::LookAhead(int distance)
{
	// Read in as many as needed.
	while (distance >= static_cast<int>(m_readTokens.size())) {
		m_readTokens.push_back(*m_tokens++);
	}

	// Get the queued token.
	return m_readTokens.back();
}

int Parser::GetPrecedence()
{
	const auto itInfixParselet = m_infixParseletsMap.find(LookAhead(0).m_type);
	if (itInfixParselet != m_infixParseletsMap.cend())
	{
		const InfixParselet* parselet = itInfixParselet->second.get();
		return parselet->GetPrecedence();
	}

	return LOWEST_PRECEDENCE;
}

