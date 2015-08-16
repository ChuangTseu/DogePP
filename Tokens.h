#pragma once

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
	X4(NAME			, ""	,	-1	,	-1	) \
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
	- 1,
#define X4(_1, _2, xPrefixPrecedence, _4) xPrefixPrecedence,
	X_LIST_OF_DoubleChar_ETokenType
#undef X4
	- 1,
#define X4(_1, _2, xPrefixPrecedence, _4) xPrefixPrecedence,
	X_LIST_OF_OtherType_ETokenType
#undef X4
	- 1
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
