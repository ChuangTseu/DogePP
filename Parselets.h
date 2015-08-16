#pragma once

#include <memory>
#include <vector>

#include "Tokens.h"
#include "Expressions.h"
#include "BaseParser.h"
#include "Precedence.h"


class PrefixParselet {
public:
	virtual std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const = 0;
};

class InfixParselet {
public:
	virtual std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const = 0;
	virtual int GetPrecedence() const = 0;
};

class DefinedCallParselet : public PrefixParselet {
	std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const
	{
		parser.ConsumeExpected(ETokenType::LEFT_PAREN);
		Token idToken = parser.ConsumeExpected(ETokenType::NAME);
		parser.ConsumeExpected(ETokenType::RIGHT_PAREN);

		return std::make_unique<DefinedCallExpression>(idToken.m_text);
	}

	int GetPrecedence() const {
		return HIGHEST_PRECEDENCE;
	}
};

class CallParselet : public InfixParselet {
	std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const
	{
		// Parse the comma-separated arguments until we hit, ")".
		std::vector<std::unique_ptr<const Expression>> args;

		// There may be no arguments at all.
		if (!parser.Match(ETokenType::RIGHT_PAREN)) {
			do {
				args.push_back(parser.ParseExpression());
			} while (parser.Match(ETokenType::COMMA));
			parser.ConsumeExpected(ETokenType::RIGHT_PAREN);
		}

		return std::make_unique<CallExpression>(std::move(left), std::move(args));
	}

	int GetPrecedence() const {
		return SpecialPrecedence::CALL;
	}
};

class BinaryOperatorParselet : public InfixParselet {
public:
	BinaryOperatorParselet(int precedence, bool bIsRight) :
		m_precedence(precedence), m_bIsRight(bIsRight)
	{
	}

	std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const
	{
		// To handle right-associative operators like "^", we allow a slightly
		// lower precedence when parsing the right-hand side. This will let a
		// parselet with the same precedence appear on the right, which will then
		// take *this* parselet's result as its left-hand argument.
		std::unique_ptr<const Expression> right = parser.ParseExpression(m_precedence + (m_bIsRight ? 1 : 0));

		return std::make_unique<OperatorExpression>(std::move(left), token.m_type, std::move(right));
	}

	int GetPrecedence() const {
		return m_precedence;
	}

private:
	int m_precedence;
	bool m_bIsRight;
};

class ConditionalParselet : public InfixParselet {
public:
	std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const
	{
		std::unique_ptr<const Expression> thenArm = parser.ParseExpression();
		parser.ConsumeExpected(ETokenType::COLON);
		std::unique_ptr<const Expression> elseArm = parser.ParseExpression(SpecialPrecedence::CONDITIONAL + 1);

		return std::make_unique<ConditionalExpression>(std::move(left), std::move(thenArm), std::move(elseArm));
	}

	int GetPrecedence() const {
		return SpecialPrecedence::CONDITIONAL;
	}
};

class GroupParselet : public PrefixParselet {
public:
	std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const
	{
		std::unique_ptr<const Expression> expression = parser.ParseExpression();
		parser.ConsumeExpected(ETokenType::RIGHT_PAREN);
		return expression;
	}
};

class NameParselet : public PrefixParselet {
public:
	std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const
	{
		return std::make_unique<NameExpression>(token.m_text);
	}
};

class NumberParselet : public PrefixParselet {
public:
	std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const
	{
		return std::make_unique<NumberExpression>(token.m_text);
	}
};

class PostfixOperatorParselet : public InfixParselet {
public:
	PostfixOperatorParselet(int precedence)
	{
		m_precedence = precedence;
	}

	std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const
	{
		return std::make_unique<PostfixExpression>(std::move(left), token.m_type);
	}

	int GetPrecedence() const
	{
		return m_precedence;
	}

private:
	int m_precedence;
};

class PrefixOperatorParselet : public PrefixParselet {
public:
	PrefixOperatorParselet(int precedence)
	{
		m_precedence = precedence;
	}

	std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const
	{
		// To handle right-associative operators like "^", we allow a slightly
		// lower precedence when parsing the right-hand side. This will let a
		// parselet with the same precedence appear on the right, which will then
		// take *this* parselet's result as its left-hand argument.
		std::unique_ptr<const Expression> right = parser.ParseExpression(m_precedence);

		return std::make_unique<PrefixExpression>(token.m_type, std::move(right));
	}

	int GetPrecedence() const
	{
		return m_precedence;
	}

private:
	int m_precedence;
};

