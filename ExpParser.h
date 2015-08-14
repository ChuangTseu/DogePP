#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>

#include "ErrorLogging.h"
#include "EnumCTTI.h"
#include "StringUtils.h"

#define X_LIST_OF_ETokenType \
	X(LEFT_PAREN	, '('	) \
	X(RIGHT_PAREN	, ')'	) \
	X(COMMA			, ','	) \
	X(ASSIGN		, '='	) \
	X(PLUS			, '+'	) \
	X(MINUS			, '-'	) \
	X(ASTERISK		, '*'	) \
	X(SLASH			, '/'	) \
	X(CARET			, '^'	) \
	X(TILDE			, '~'	) \
	X(BANG			, '!'	) \
	X(QUESTION		, '?'	) \
	X(COLON			, ':'	) \
	X(NAME			, '\0'	) \
	X(EOL			, '\0'	)

enum ETokenType {
#define X(eEnum, _2) eEnum,
	X_LIST_OF_ETokenType
#undef X

	ETokenType_COUNT,
	ETokenType_UNKNOWN = -1
};

static const char* LUT_ETokenType_szName[ETokenType_COUNT] =
{
#define X(eEnum, _2) #eEnum,
	X_LIST_OF_ETokenType
#undef X
};

ETokenType szNameToETokenType(const char* szName)
{
#define X(eEnum, _2) if (szEq(szName, #eEnum)) return eEnum;
	X_LIST_OF_ETokenType
#undef X

		return ETokenType_UNKNOWN;
}

const char* ETokenTypeToSzName(ETokenType eTokenType)
{
	DOGE_ASSERT(eTokenType >= 0 && eTokenType < ETokenType_COUNT);
	return LUT_ETokenType_szName[eTokenType];
}

DECLARE_ENUM_TEMPLATE_CTTI(ETokenType)

struct Token {

	ETokenType m_type;
	std::string m_text;
};

struct Precedence {
	// Ordered in increasing precedence.
	static const int ASSIGNMENT = 1;
	static const int CONDITIONAL = 2;
	static const int SUM = 3;
	static const int PRODUCT = 4;
	static const int EXPONENT = 5;
	static const int PREFIX = 6;
	static const int POSTFIX = 7;
	static const int CALL = 8;
};

class Expression {

};

class OperatorExpression : public Expression {
public:
	OperatorExpression(const Expression* left, ETokenType eOperator, const Expression* right) {
		m_left = left;
		m_eOperator = eOperator;
		m_right = right;
	}

	const Expression* m_left;
	ETokenType m_eOperator;
	const Expression* m_right;
};

class CallExpression : public Expression {
public:
	CallExpression(const Expression* function, std::list<const Expression*> args) {
		m_function = function;
		m_args = args;
	}

private:
	const Expression* m_function;
	std::list<const Expression*> m_args;
};

class ConditionalExpression : public Expression {
public:
	ConditionalExpression(const Expression* condition, const Expression* thenArm, const Expression* elseArm) {
		m_condition = condition;
		m_thenArm = thenArm;
		m_elseArm = elseArm;
	}

private:
	const Expression* m_condition;
	const Expression* m_thenArm;
	const Expression* m_elseArm;
};

class NameExpression : public Expression {
public:
	NameExpression(const std::string& name) {
		m_name = name;
	}

	std::string GetName() { return m_name; }

private:
	std::string m_name;
};

class PostfixExpression : public Expression {
public:
	PostfixExpression(const Expression* left, ETokenType eOperator) {
		mLeft = left;
		m_eOperator = eOperator;
	}

private:
	const Expression* mLeft;
	ETokenType  m_eOperator;
};

class PrefixExpression : public Expression {
public:
	PrefixExpression(ETokenType eOperator, const Expression* right) {
		m_eOperator = eOperator;
		m_right = right;
	}

private:
	ETokenType  m_eOperator;
	const Expression* m_right;
};

class PrefixParselet;
class InfixParselet;
class Parser;

typedef std::vector<Token>::const_iterator TokenCIt;

class Parser {
public:
	Parser(TokenCIt tokenIt) : m_tokens(tokenIt) {}

	void Register(ETokenType eTokenType, const PrefixParselet* parselet);

	void Register(ETokenType eTokenType, const InfixParselet* parselet);

	const Expression* ParseExpression(int precedence = 0);

	Token Consume();
	Token ConsumeExpected(ETokenType eExpected);

	bool Match(ETokenType eExpected);

private:
	Token LookAhead(int distance);

	int GetPrecedence();

private:
	TokenCIt m_tokens;

	std::list<Token> m_readTokens;

	std::map<ETokenType, const PrefixParselet*> m_prefixParseletsMap;
	std::map<ETokenType, const InfixParselet*> m_infixParseletsMap;
};

class PrefixParselet {
public:
	virtual const Expression* Parse(Parser& parser, const Token& token) const = 0;
};

class InfixParselet {
public:
	virtual const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const = 0;
	virtual int GetPrecedence() const = 0;
};

class BinaryOperatorParselet : public InfixParselet {
public:
	BinaryOperatorParselet(int precedence, bool bIsRight) {
		m_precedence = precedence;
		m_bIsRight = bIsRight;
	}

	const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const {
		// To handle right-associative operators like "^", we allow a slightly
		// lower precedence when parsing the right-hand side. This will let a
		// parselet with the same precedence appear on the right, which will then
		// take *this* parselet's result as its left-hand argument.
		const Expression* right = parser.ParseExpression(m_precedence - (m_bIsRight ? 1 : 0));

		return new OperatorExpression(left, token.m_type, right);
	}

	int GetPrecedence() const {
		return m_precedence;
	}

private:
	int m_precedence;
	bool m_bIsRight;
};

class CallParselet : public InfixParselet {
public:
	const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const {
		// Parse the comma-separated arguments until we hit, ")".
		std::list<const Expression*> args;

		// There may be no arguments at all.
		if (!parser.Match(RIGHT_PAREN)) {
			do {
				args.push_back(parser.ParseExpression());
			} while (parser.Match(COMMA));
			parser.ConsumeExpected(RIGHT_PAREN);
		}

		return new CallExpression(left, args);
	}

	int GetPrecedence() const {
		return Precedence::CALL;
	}
};

class ConditionalParselet : public InfixParselet {
public:
	const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const {
		const Expression* thenArm = parser.ParseExpression();
		parser.ConsumeExpected(COLON);
		const Expression* elseArm = parser.ParseExpression(Precedence::CONDITIONAL - 1);

		return new ConditionalExpression(left, thenArm, elseArm);
	}

	int GetPrecedence() const {
		return Precedence::CONDITIONAL;
	}
};

class GroupParselet : public PrefixParselet {
public:
	const Expression* Parse(Parser& parser, const Token& token) const {
		const Expression* expression = parser.ParseExpression();
		parser.ConsumeExpected(RIGHT_PAREN);
		return expression;
	}
};

class NameParselet : public PrefixParselet {
public:
	const Expression* Parse(Parser& parser, const Token& token) const {
		return new NameExpression(token.m_text);
	}
};

class PostfixOperatorParselet : public InfixParselet {
public:
	PostfixOperatorParselet(int precedence) {
		m_precedence = precedence;
	}

	const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const {
		return new PostfixExpression(left, token.m_type);
	}

	int GetPrecedence() const {
		return m_precedence;
	}

private:
	int m_precedence;
};

class PrefixOperatorParselet : public PrefixParselet {
public:
	PrefixOperatorParselet(int precedence) {
		m_precedence = precedence;
	}

	const Expression* Parse(Parser& parser, const Token& token) const {
		// To handle right-associative operators like "^", we allow a slightly
		// lower precedence when parsing the right-hand side. This will let a
		// parselet with the same precedence appear on the right, which will then
		// take *this* parselet's result as its left-hand argument.
		const Expression* right = parser.ParseExpression(m_precedence);

		return new PrefixExpression(token.m_type, right);
	}

	int GetPrecedence() const {
		return m_precedence;
	}

private:
	int m_precedence;
};

class ExpParser : public Parser
{
public:
	ExpParser(TokenCIt tokenIt);
	~ExpParser();

	void Postfix(ETokenType eTokenType, int precedence);
	void Prefix(ETokenType eTokenType, int precedence);
	void InfixLeft(ETokenType eTokenType, int precedence);
	void InfixRight(ETokenType eTokenType, int precedence);
};

