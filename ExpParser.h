#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>

#include "ErrorLogging.h"
#include "EnumCTTI.h"
#include "StringUtils.h"

#define X_LIST_OF_SingleChar_ETokenType \
	X2(LEFT_PAREN	, "("	) \
	X2(RIGHT_PAREN	, ")"	) \
	X2(PLUS			, "+"	) \
	X2(MINUS		, "-"	) \
	X2(MUL			, "*"	) \
	X2(DIV			, "/"	) \
	X2(MOD			, "%"	) \
	X2(BIT_NOT		, "~"	) \
	X2(BIT_AND		, "&"	) \
	X2(BIT_OR		, "|"	) \
	X2(BIT_XOR		, "^"	) \
	X2(NOT			, "!"	) \
	X2(GT			, ">"	) \
	X2(LT			, "<"	) \
	X2(QUESTION		, "?"	) \
	X2(COLON		, ":"	)

#define X_LIST_OF_DoubleChar_ETokenType \
	X2(NEQ			, "!="	) \
	X2(EQ			, "=="	) \
	X2(GE			, ">="	) \
	X2(LE			, "<="	) \
	X2(AND			, "&&"	) \
	X2(OR			, "||"	) \
	X2(BIT_LSHIFT	, "<<"	) \
	X2(BIT_RSHIFT	, ">>"	)

#define X_LIST_OF_OtherType_ETokenType \
	X2(NUMBER		, ""	) \
	X2(EOL			, ""	)

enum class ETokenType {

#define X2(eEnum, _2) eEnum,
	X_LIST_OF_SingleChar_ETokenType
#undef X2

	SINGLE_CHAR_END,

#define X2(eEnum, _2) eEnum,
	X_LIST_OF_DoubleChar_ETokenType
#undef X2

	DOUBLE_CHAR_END,

#define X2(eEnum, _2) eEnum,
	X_LIST_OF_OtherType_ETokenType
#undef X2

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

#define EASINT(eEnum) (static_cast<int>(eEnum))

static const char* LUT_ETokenType_OperatorChar[EASINT(ETokenType::TOTAL_END)] =
{
#define X2(_1, operatorSz) operatorSz,
	X_LIST_OF_SingleChar_ETokenType
#undef X2

	"",

#define X2(_1, operatorSz) operatorSz,
	X_LIST_OF_DoubleChar_ETokenType
#undef X2

	"",

#define X2(_1, operatorSz) operatorSz,
	X_LIST_OF_OtherType_ETokenType
#undef X2

	""
};

inline ETokenType ETokenTypeFromSingleCharOperator(const char* szOp)
{
#define X2(eEnum, operatorSz) if (szEq(szOp, operatorSz)) return ETokenType::eEnum;
		X_LIST_OF_SingleChar_ETokenType
#undef X2

		return ETokenType::UNKNOWN;
}

inline ETokenType ETokenTypeFromDoubleCharOperator(const char* szOp)
{
#define X2(eEnum, operatorSz) if (szEq(szOp, operatorSz)) return ETokenType::eEnum;
		X_LIST_OF_DoubleChar_ETokenType
#undef X2

		return ETokenType::UNKNOWN;
}

static const char* LUT_ETokenType_szName[EASINT(ETokenType::TOTAL_END)] =
{
#define X2(eEnum, _2) #eEnum,
	X_LIST_OF_SingleChar_ETokenType
#undef X2

	"SINGLE_CHAR_END",

#define X2(eEnum, _2) #eEnum,
	X_LIST_OF_DoubleChar_ETokenType
#undef X2

	"DOUBLE_CHAR_END",

#define X2(eEnum, _2) #eEnum,
	X_LIST_OF_OtherType_ETokenType
#undef X2

	"OTHER_TYPE_END"
};

inline ETokenType szNameToETokenType(const char* szName)
{
#define X2(eEnum, _2) if (szEq(szName, #eEnum)) return ETokenType::eEnum;
	X_LIST_OF_SingleChar_ETokenType
#undef X2

	if (szEq(szName, "SINGLE_CHAR_END")) return ETokenType::SINGLE_CHAR_END;

#define X2(eEnum, _2) if (szEq(szName, #eEnum)) return ETokenType::eEnum;
			X_LIST_OF_DoubleChar_ETokenType
#undef X2

	if (szEq(szName, "DOUBLE_CHAR_END")) return ETokenType::DOUBLE_CHAR_END;

#define X2(eEnum, _2) if (szEq(szName, #eEnum)) return ETokenType::eEnum;
				X_LIST_OF_OtherType_ETokenType
#undef X2

	if (szEq(szName, "OTHER_TYPE_END")) return ETokenType::OTHER_TYPE_END;

	return ETokenType::UNKNOWN;
}

inline const char* ETokenTypeToSzName(ETokenType eTokenType)
{
	DOGE_ASSERT(EASINT(eTokenType) >= 0 && EASINT(eTokenType) < EASINT(ETokenType::SINGLE_CHAR_COUNT));
	return LUT_ETokenType_szName[EASINT(eTokenType)];
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
public:
	virtual std::string GetStringExpression() const = 0;

	virtual int Evaluate() const = 0;
};

class OperatorExpression : public Expression {
public:
	OperatorExpression(const Expression* left, ETokenType eOperator, const Expression* right) {
		m_left = left;
		m_eOperator = eOperator;
		m_right = right;
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

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

	std::string GetStringExpression() const;
	int Evaluate() const;

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

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	const Expression* m_condition;
	const Expression* m_thenArm;
	const Expression* m_elseArm;
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
	PostfixExpression(const Expression* left, ETokenType eOperator) {
		m_left = left;
		m_eOperator = eOperator;
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

private:
	const Expression* m_left;
	ETokenType  m_eOperator;
};

class PrefixExpression : public Expression {
public:
	PrefixExpression(ETokenType eOperator, const Expression* right) {
		m_eOperator = eOperator;
		m_right = right;
	}

	std::string GetStringExpression() const;
	int Evaluate() const;

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

class ConditionalParselet : public InfixParselet {
public:
	const Expression* Parse(Parser& parser, const Expression* left, const Token& token) const {
		const Expression* thenArm = parser.ParseExpression();
		parser.ConsumeExpected(ETokenType::COLON);
		const Expression* elseArm = parser.ParseExpression(Precedence::CONDITIONAL - 1);

		return new ConditionalExpression(left, thenArm, elseArm);
	}

	int GetPrecedence() const {
		// Value from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B
		return 15;
	}
};

class GroupParselet : public PrefixParselet {
public:
	const Expression* Parse(Parser& parser, const Token& token) const {
		const Expression* expression = parser.ParseExpression();
		parser.ConsumeExpected(ETokenType::RIGHT_PAREN);
		return expression;
	}
};

class NumberParselet : public PrefixParselet {
public:
	const Expression* Parse(Parser& parser, const Token& token) const {
		return new NumberExpression(token.m_text);
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

