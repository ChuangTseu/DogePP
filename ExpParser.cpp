#include "ExpParser.h"

ExpParser::ExpParser(TokenCIt tokenIt) : Parser(tokenIt)
{
	Register(ETokenType::NUMBER, std::make_unique<NumberParselet>());
	Register(ETokenType::QUESTION, std::make_unique<ConditionalParselet>());
	Register(ETokenType::LEFT_PAREN, std::make_unique<GroupParselet>());
	Register(ETokenType::LEFT_PAREN, std::make_unique<CallParselet>());

	// Register the simple operator parselets.

	// Precedence values from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B

	Prefix(ETokenType::PLUS, GetOperatorPrefixPrecedence(ETokenType::PLUS));
	Prefix(ETokenType::MINUS, GetOperatorPrefixPrecedence(ETokenType::MINUS));
	Prefix(ETokenType::BIT_NOT, GetOperatorPrefixPrecedence(ETokenType::BIT_NOT));
	Prefix(ETokenType::NOT, GetOperatorPrefixPrecedence(ETokenType::NOT));

	InfixLeft(ETokenType::MUL, GetOperatorInfixPrecedence(ETokenType::MUL));
	InfixLeft(ETokenType::DIV, GetOperatorInfixPrecedence(ETokenType::DIV));
	InfixLeft(ETokenType::MOD, GetOperatorInfixPrecedence(ETokenType::MOD));

	InfixLeft(ETokenType::PLUS, GetOperatorInfixPrecedence(ETokenType::PLUS));
	InfixLeft(ETokenType::MINUS, GetOperatorInfixPrecedence(ETokenType::MINUS));

	InfixLeft(ETokenType::BIT_LSHIFT, GetOperatorInfixPrecedence(ETokenType::BIT_LSHIFT));
	InfixLeft(ETokenType::BIT_RSHIFT, GetOperatorInfixPrecedence(ETokenType::BIT_RSHIFT));

	InfixLeft(ETokenType::LT, GetOperatorInfixPrecedence(ETokenType::LT));
	InfixLeft(ETokenType::LE, GetOperatorInfixPrecedence(ETokenType::LE));
	InfixLeft(ETokenType::GT, GetOperatorInfixPrecedence(ETokenType::GT));
	InfixLeft(ETokenType::GE, GetOperatorInfixPrecedence(ETokenType::GE));

	InfixLeft(ETokenType::EQ, GetOperatorInfixPrecedence(ETokenType::EQ));
	InfixLeft(ETokenType::NEQ, GetOperatorInfixPrecedence(ETokenType::NEQ));

	InfixLeft(ETokenType::BIT_AND, GetOperatorInfixPrecedence(ETokenType::BIT_AND));

	InfixLeft(ETokenType::BIT_XOR, GetOperatorInfixPrecedence(ETokenType::BIT_XOR));

	InfixLeft(ETokenType::BIT_OR, GetOperatorInfixPrecedence(ETokenType::BIT_OR));

	InfixLeft(ETokenType::AND, GetOperatorInfixPrecedence(ETokenType::AND));

	InfixLeft(ETokenType::OR, GetOperatorInfixPrecedence(ETokenType::OR));
}

ExpParser::~ExpParser()
{
}

void ExpParser::Postfix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<PostfixOperatorParselet>(precedence));
}

void ExpParser::Prefix(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<PrefixOperatorParselet>(precedence));
}

void ExpParser::InfixLeft(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<BinaryOperatorParselet>(precedence, false));
}

void ExpParser::InfixRight(ETokenType eTokenType, int precedence)
{
	Register(eTokenType, std::make_unique<BinaryOperatorParselet>(precedence, true));
}


