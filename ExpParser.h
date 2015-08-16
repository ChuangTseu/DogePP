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

// X4(TokenName, "SzToken", PrefixPrecedence, InfixPrecedence)

#define X_LIST_OF_SingleChar_ETokenType \
	X4(NOT			, "!"	,	3	,	-1	) \
	X4(BIT_NOT		, "~"	,	3	,	-1	) \
	X4(MUL			, "*"	,	-1	,	5	) \
	X4(DIV			, "/"	,	-1	,	5	) \
	X4(MOD			, "%"	,	-1	,	5	) \
	X4(PLUS			, "+"	,	3	,	6	) \
	X4(MINUS		, "-"	,	3	,	6	) \
	X4(BIT_AND		, "&"	,	-1	,	10	) \
	X4(BIT_XOR		, "^"	,	-1	,	11	) \
	X4(BIT_OR		, "|"	,	-1	,	12	) \
	X4(GT			, ">"	,	-1	,	8	) \
	X4(LT			, "<"	,	-1	,	8	) \
	X4(COMMA		, ","	,	-1	,	18	) \
	X4(LEFT_PAREN	, "("	,	-1	,	-1	) \
	X4(RIGHT_PAREN	, ")"	,	-1	,	-1	) \
	X4(QUESTION		, "?"	,	-1	,	-1	) \
	X4(COLON		, ":"	,	-1	,	-1	)

#define X_LIST_OF_DoubleChar_ETokenType \
	X4(NEQ			, "!="	,	-1	,	9	) \
	X4(EQ			, "=="	,	-1	,	9	) \
	X4(GE			, ">="	,	-1	,	8	) \
	X4(LE			, "<="	,	-1	,	8	) \
	X4(AND			, "&&"	,	-1	,	13	) \
	X4(OR			, "||"	,	-1	,	14	) \
	X4(BIT_LSHIFT	, "<<"	,	-1	,	7	) \
	X4(BIT_RSHIFT	, ">>"	,	-1	,	7	)

#define X_LIST_OF_OtherType_ETokenType \
	X4(NUMBER		, ""	,	-1	,	-1	) \
	X4(EOL			, ""	,	-1	,	-1	)

enum class ETokenType {

#define X4(xEnum, _2, _3, _4) xEnum,
	X_LIST_OF_SingleChar_ETokenType
#undef X4

	SINGLE_CHAR_END,

#define X4(xEnum, _2, _3, _4) xEnum,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4

	DOUBLE_CHAR_END,

#define X4(xEnum, _2, _3, _4) xEnum,
	X_LIST_OF_OtherType_ETokenType
#undef X4

	OTHER_TYPE_END,

	TOTAL_END,

	SINGLE_CHAR_START = 0,
	SINGLE_CHAR_COUNT = SINGLE_CHAR_END - SINGLE_CHAR_START,

	DOUBLE_CHAR_START = SINGLE_CHAR_END + 1,
	DOUBLE_CHAR_COUNT = DOUBLE_CHAR_END - DOUBLE_CHAR_START,

	OTHER_TYPE_START = DOUBLE_CHAR_END + 1,
	OTHER_TYPE_COUNT = OTHER_TYPE_END - OTHER_TYPE_START,

	TOTAL_COUNT = SINGLE_CHAR_COUNT + DOUBLE_CHAR_COUNT + OTHER_TYPE_COUNT,

	UNKNOWN = -1
};

inline ETokenType szNameToETokenType(const char* szName)
{
#define X4(xEnum, _2, _3, _4) if (szEq(szName, #xEnum)) return ETokenType::xEnum;
	X_LIST_OF_SingleChar_ETokenType
#undef X4

		if (szEq(szName, "SINGLE_CHAR_END")) return ETokenType::SINGLE_CHAR_END;

#define X4(xEnum, _2, _3, _4) if (szEq(szName, #xEnum)) return ETokenType::xEnum;
	X_LIST_OF_DoubleChar_ETokenType
#undef X4

		if (szEq(szName, "DOUBLE_CHAR_END")) return ETokenType::DOUBLE_CHAR_END;

#define X4(xEnum, _2, _3, _4) if (szEq(szName, #xEnum)) return ETokenType::xEnum;
	X_LIST_OF_OtherType_ETokenType
#undef X4

		if (szEq(szName, "OTHER_TYPE_END")) return ETokenType::OTHER_TYPE_END;

	return ETokenType::UNKNOWN;
}

static const char* LUT_ETokenType_szName[EASINT(ETokenType::TOTAL_END)] =
{
#define X4(xEnum, _2, _3, _4) #xEnum,
	X_LIST_OF_SingleChar_ETokenType
#undef X4
	"SINGLE_CHAR_END",
#define X4(xEnum, _2, _3, _4) #xEnum,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4
	"DOUBLE_CHAR_END",
#define X4(xEnum, _2, _3, _4) #xEnum,
	X_LIST_OF_OtherType_ETokenType
#undef X4
	"OTHER_TYPE_END"
};

inline const char* ETokenTypeToSzName(ETokenType eTokenType)
{
	DOGE_ASSERT(EASINT(eTokenType) >= 0 && EASINT(eTokenType) < EASINT(ETokenType::TOTAL_END));
	return LUT_ETokenType_szName[EASINT(eTokenType)];
}

DECLARE_ENUM_TEMPLATE_CTTI(ETokenType)

static const char* LUT_ETokenType_OperatorChar[EASINT(ETokenType::TOTAL_END)] =
{
#define X4(_1, xSzOp, _3, _4) xSzOp,
	X_LIST_OF_SingleChar_ETokenType
#undef X4

	"",

#define X4(_1, xSzOp, _3, _4) xSzOp,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4

	"",

#define X4(_1, xSzOp, _3, _4) xSzOp,
	X_LIST_OF_OtherType_ETokenType
#undef X4

	""
};

inline ETokenType ETokenTypeFromSingleCharOperator(const char* szOp)
{
#define X4(xEnum, xSzOp, _3, _4) if (szEq(szOp, xSzOp)) return ETokenType::xEnum;
		X_LIST_OF_SingleChar_ETokenType
#undef X4

		return ETokenType::UNKNOWN;
}

inline ETokenType ETokenTypeFromDoubleCharOperator(const char* szOp)
{
#define X4(xEnum, xSzOp, _3, _4) if (szEq(szOp, xSzOp)) return ETokenType::xEnum;
		X_LIST_OF_DoubleChar_ETokenType
#undef X4

		return ETokenType::UNKNOWN;
}

static const int LUT_ETokenType_PrefixPrecedence[EASINT(ETokenType::TOTAL_END)] = 
{
#define X4(_1, _2, xPrefixPrecedence, _4) xPrefixPrecedence,
	X_LIST_OF_SingleChar_ETokenType
#undef X4
	-1,
#define X4(_1, _2, xPrefixPrecedence, _4) xPrefixPrecedence,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4
	-1,
#define X4(_1, _2, xPrefixPrecedence, _4) xPrefixPrecedence,
	X_LIST_OF_OtherType_ETokenType
#undef X4
	-1
};

inline int GetOperatorPrefixPrecedence(ETokenType eOp) {
	const int iOp = EASINT(eOp);
	DOGE_ASSERT(iOp >= 0 && iOp < EASINT(ETokenType::TOTAL_END));
	int precedence = LUT_ETokenType_PrefixPrecedence[iOp];
	DOGE_ASSERT_MESSAGE(precedence >= 0, "Operator %s has no prefix precendence.\n", EnumToSzName(eOp));

	return precedence;
}

static const int LUT_ETokenType_InfixPrecedence[EASINT(ETokenType::TOTAL_END)] =
{
#define X4(_1, _2, _3, xInfixPrecedence) xInfixPrecedence,
	X_LIST_OF_SingleChar_ETokenType
#undef X4
	- 1,
#define X4(_1, _2, _3, xInfixPrecedence) xInfixPrecedence,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4
	- 1,
#define X4(_1, _2, _3, xInfixPrecedence) xInfixPrecedence,
	X_LIST_OF_OtherType_ETokenType
#undef X4
	- 1
};

inline int GetOperatorInfixPrecedence(ETokenType eOp) {
	const int iOp = EASINT(eOp);
	DOGE_ASSERT(iOp >= 0 && iOp < EASINT(ETokenType::TOTAL_END));
	int precedence = LUT_ETokenType_InfixPrecedence[iOp];
	DOGE_ASSERT_MESSAGE(precedence >= 0, "Operator %s has no infix precendence.\n", EnumToSzName(eOp));

	return precedence;
}

struct Token {

	ETokenType m_type;
	std::string m_text;
};

struct SpecialPrecedence {
	// Value from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B

	// Ordered in increasing precedence.
	static const int CONDITIONAL = 15;
	static const int CALL = 2;
};

class Expression {
public:
	virtual std::string GetStringExpression() const = 0;

	virtual int Evaluate() const = 0;
};

class OperatorExpression : public Expression {
public:
	OperatorExpression(std::unique_ptr<const Expression> left, ETokenType eOperator, std::unique_ptr<const Expression> right) :
		m_left(std::move(left)), m_eOperator(eOperator), m_right(std::move(right))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

	std::unique_ptr<const Expression> m_left;
	ETokenType m_eOperator;
	std::unique_ptr<const Expression> m_right;
};

class CallExpression : public Expression {
public:
	CallExpression(std::unique_ptr<const Expression> function, std::list<std::unique_ptr<const Expression>> args) :
		m_function(std::move(function)), m_args(std::move(args))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_function;
	std::list<std::unique_ptr<const Expression>> m_args;
};

class ConditionalExpression : public Expression {
public:
	ConditionalExpression(std::unique_ptr<const Expression> condition, std::unique_ptr<const Expression> thenArm, 
		std::unique_ptr<const Expression> elseArm) : 
		m_condition(std::move(condition)), m_thenArm(std::move(thenArm)), m_elseArm(std::move(elseArm))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_condition;
	std::unique_ptr<const Expression> m_thenArm;
	std::unique_ptr<const Expression> m_elseArm;
};

class NumberExpression : public Expression {
public:
	NumberExpression(const std::string& name) {
		m_name = name;
	}

	std::string GetName() { return m_name; }

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::string m_name;
};

class PostfixExpression : public Expression {
public:
	PostfixExpression(std::unique_ptr<const Expression> left, ETokenType eOperator) :
		m_left(std::move(left)), m_eOperator(eOperator)
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	std::unique_ptr<const Expression> m_left;
	ETokenType  m_eOperator;
};

class PrefixExpression : public Expression {
public:
	PrefixExpression(ETokenType eOperator, std::unique_ptr<const Expression> right) :
		m_eOperator(eOperator), m_right(std::move(right))
	{
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	ETokenType m_eOperator;
	std::unique_ptr<const Expression> m_right;
};

class PrefixParselet;
class InfixParselet;
class Parser;

typedef std::vector<Token>::const_iterator TokenCIt;

#define LOWEST_PRECEDENCE	(100)
#define HIGHEST_PRECEDENCE	(0)

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

class PrefixParselet {
public:
	virtual std::unique_ptr<const Expression> Parse(Parser& parser, const Token& token) const = 0;
};

class InfixParselet {
public:
	virtual std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const = 0;
	virtual int GetPrecedence() const = 0;
};

class CallParselet : public InfixParselet {
	std::unique_ptr<const Expression> Parse(Parser& parser, std::unique_ptr<const Expression> left, const Token& token) const
	{
		// Parse the comma-separated arguments until we hit, ")".
		std::list<std::unique_ptr<const Expression>> args;

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
		std::unique_ptr<const Expression> right =parser.ParseExpression(m_precedence);

		return std::make_unique<PrefixExpression>(token.m_type, std::move(right));
	}

	int GetPrecedence() const 
	{
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

