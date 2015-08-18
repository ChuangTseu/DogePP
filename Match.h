#pragma once

#include "PreProcessorUtils.h"
#include "StringUtils.h"
#include "Config.h"

inline bool isHSpace(char c)
{
	return c == ' ' || c == '\t';
}

inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

inline bool isValidInNumberChar(char c)
{
	return isDigit(c) || c == 'x';
}

inline bool isLiteralDelimiterChar(char c)
{
	return c == '"' || c == '\'';
}

inline bool isLineReturn(const char* sz, StrSizeT* pOutLRLength = NULL)
{
//#if LR_TYPE_LF
//	return szEqN(sz, "\n", LR_LENGTH);
//#elif LR_TYPE_CRLF
//	return szEqN(sz, "\r\n", LR_LENGTH);
//#endif

	if (szEqN(sz, "\n", 1))
	{
		if (pOutLRLength) *pOutLRLength = 1;
		return true;
	}

	if (szEqN(sz, "\r\n", 2))
	{
		if (pOutLRLength) *pOutLRLength = 2;
		return true;
	}

	return false;
}

#define COMMENT_ML_START_LENGTH 2

inline bool isMultiLineCommentStart(const char* sz)
{
	return szEqN(sz, "/*", COMMENT_ML_START_LENGTH);
}

#define COMMENT_ML_END_LENGTH 2

inline bool isMultiLineCommentEnd(const char* sz)
{
	return szEqN(sz, "*/", COMMENT_ML_END_LENGTH);
}

#define COMMENT_SL_LENGTH 2

inline bool isSingleLineCommentStart(const char* sz)
{
	return szEqN(sz, "//", COMMENT_SL_LENGTH);
}

static const size_t LUT_charColAdvance[256] =
{
#define S1(i) ((i == '\t') ? 4 : 1)
	S256(0)
#undef S1
};

#define BETWEEN_INCL(val, start, end) (val >= start && val <= end)

static const bool LUT_validIdFirstChar[256] =
{
#define S1(i) (BETWEEN_INCL(i, 'a', 'z') || BETWEEN_INCL(i, 'A', 'Z') || (i == '_') || (i == '$'))
	S256(0)
#undef S1
};

static const bool LUT_validIdBodyChar[256] =
{
#define S1(i) (BETWEEN_INCL(i, 'a', 'z') || BETWEEN_INCL(i, 'A', 'Z') || BETWEEN_INCL(i, '0', '9') || (i == '_') || (i == '$'))
	S256(0)
#undef S1
};


