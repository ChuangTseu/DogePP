#include "ExpParser.h"

void Parser::Register(ETokenType eTokenType, const InfixParselet* parselet)
{
	m_infixParseletsMap.insert(std::make_pair(eTokenType, parselet));
}

void Parser::Register(ETokenType eTokenType, const PrefixParselet* parselet)
{
	m_prefixParseletsMap.insert(std::make_pair(eTokenType, parselet));
}

const Expression* Parser::ParseExpression(int precedence)
{
	Token token = Consume();

	const auto itPrefixParselet = m_prefixParseletsMap.find(token.m_type);
	DOGE_ASSERT_MESSAGE(itPrefixParselet != m_prefixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
	const PrefixParselet* prefix = itPrefixParselet->second;

	const Expression* left = prefix->Parse(*this, token);

	while (precedence < GetPrecedence()) {
		token = Consume();

		const auto itInfixParselet = m_infixParseletsMap.find(token.m_type);
		DOGE_ASSERT_MESSAGE(itInfixParselet != m_infixParseletsMap.cend(), "Could not parse '%s'.\n", token.m_text.c_str());
		const InfixParselet* infix = itInfixParselet->second;

		left = infix->Parse(*this, left, token);
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
		const InfixParselet* parselet = itInfixParselet->second;
		return parselet->GetPrecedence();
	}

	return 0;
}


ExpParser::ExpParser(TokenCIt tokenIt) : Parser(tokenIt)
{
	Register(NAME, new NameParselet());
	Register(QUESTION, new ConditionalParselet());
	Register(LEFT_PAREN, new GroupParselet());
	Register(LEFT_PAREN, new CallParselet());

	// Register the simple operator parselets.
	Prefix(PLUS, Precedence::PREFIX);
	Prefix(MINUS, Precedence::PREFIX);
	Prefix(TILDE, Precedence::PREFIX);
	Prefix(BANG, Precedence::PREFIX);

	// For kicks, we'll make "!" both prefix and postfix, kind of like ++.
	Postfix(BANG, Precedence::POSTFIX);

	InfixLeft(PLUS, Precedence::SUM);
	InfixLeft(MINUS, Precedence::SUM);
	InfixLeft(ASTERISK, Precedence::PRODUCT);
	InfixLeft(SLASH, Precedence::PRODUCT);
	InfixLeft(CARET, Precedence::EXPONENT);
}

ExpParser::~ExpParser()
{
}

void ExpParser::Postfix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, new PostfixOperatorParselet(precedence));
}

void ExpParser::Prefix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, new PrefixOperatorParselet(precedence));
}

void ExpParser::InfixLeft(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, new BinaryOperatorParselet(precedence, false));
}

void ExpParser::InfixRight(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, new BinaryOperatorParselet(precedence, true));
}
