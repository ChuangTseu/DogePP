// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <map>
#include <fstream>
#include <memory>
#include <streambuf>
#include <algorithm>
#include <memory>

#include "EnumCTTI.h"

#include "PreProcessorUtils.h"
#include "Config.h"
#include "StringUtils.h"
#include "Macro.h"
#include "FileUtils.h"
#include "StringContext.h"
#include "CharUtils.h"
#include "Match.h"
#include "ErrorLogging.h"
#include "MacroExpParser.h"

bool bKeepComment = true;

#define VEC_SIZE_T(vec) decltype(vec.size())

#define X_LIST_OF_Directive \
	X(include) \
	X(define) \
	X(undef) \
	X(pragma) \
	X(ifdef) \
	X(ifndef) \
	X(if) \
	X(else) \
	X(elif) \
	X(endif)

enum EDirective {

#define X(xDir) e_directive_##xDir,
	X_LIST_OF_Directive
#undef X

	e_directive_COUNT,

	e_directive_UNKNOWN = -1
};

bool isConditionnalDirective(EDirective eDirective)
{
	return (eDirective >= e_directive_ifdef && eDirective <= e_directive_endif);
}

static const char* LUT_EDirective_szName[e_directive_COUNT] =
{
#define X(xDir) #xDir,
	X_LIST_OF_Directive
#undef X
};

EDirective szNameToEDirective(const char* szName)
{
#define X(xDir) if (szEq(szName, #xDir)) return e_directive_##xDir;
	X_LIST_OF_Directive
#undef X

		return e_directive_UNKNOWN;
}

const char* EDirectiveToSzName(EDirective eDirective)
{
	DOGE_ASSERT(eDirective >= 0 && eDirective < e_directive_COUNT);
	return LUT_EDirective_szName[eDirective];
}

DECLARE_ENUM_TEMPLATE_CTTI(EDirective)

enum {
	IGNORE_ML_COMMENT = 1,
	IGNORE_SL_COMMENT = 2,
	INSIDE_LITERAL = 4
};

bool skipCtxHeadToNextValid(StrCTX& ctx);
void preprocess_file_string(std::string& strBuffer);
void preprocess_if_directive_string(std::string& strBuffer);
bool retrieveDirectiveType(StrCTX& ctx, EDirective& outEDirective, StrSizeT& outDirectiveStartPos);
std::unique_ptr<const Expression> EvaluateStringExpression(std::string strExp);
char ConsumeChar(StrCTX& ctx, uint32_t ignoreField = 0);
char LookChar(StrCTX& ctx, uint32_t ignoreField = 0);
SubStrLoc GetSubLocUntilChar(StrCTX& ctx, char c, uint32_t ignoreField);
SubStrLoc GetSubLocUntilSz(StrCTX& ctx, const char* sz, uint32_t ignoreField);

#if 0
char pickNextChar(StrCTX& ctx)
{
	DOGE_ASSERT(skipCtxHeadToNextValid(ctx));
	if (ctx.numNeededCommentSubtitutions > 0)
	{
		--ctx.numNeededCommentSubtitutions;
		//return ' ';
		return ctx.HeadChar();
	}
	else
	{
		return ctx.HeadChar();
	}
}
#endif

bool skipEntireMultiLineComment(StrCTX& ctx)
{
	while (!ctx.IsEnded() && !isMultiLineCommentEnd(ctx.HeadCStr()))
	{
		*(*(std::string::iterator*)&ctx.head) = 'o';
		ctx.Advance(1);
	}

	if (ctx.IsEnded())
	{
		return false; // No EOL found
	}

	ctx.Advance(COMMENT_ML_END_LENGTH);

	return true;
}

SubStrLoc GetMultiLineCommentSubLoc(StrCTX& ctx)
{
	SubStrLoc mlCommentSubLoc = GetSubLocUntilSz(ctx, "*/", IGNORE_ML_COMMENT | IGNORE_SL_COMMENT);

	return mlCommentSubLoc;

	/*
	mlCommentSubLoc.m_start = ctx.CurrentPos();
	mlCommentSubLoc.m_end = mlCommentSubLoc.m_start;

	const char* cstr = ctx.HeadCStr();

	while (*cstr && !isMultiLineCommentEnd(cstr))
	{
		++cstr;
		++mlCommentSubLoc.m_end;
	}

	DOGE_ASSERT_MESSAGE(! *cstr,
		"Error at multi line comment starting line %d. Could not get multiline comment end, unexpected EOL.\n", ctx.lineNumber);

	mlCommentSubLoc.m_end += COMMENT_ML_END_LENGTH;

	return mlCommentSubLoc;
	*/
}

SubStrLoc GetSingleLineCommentSubLoc(StrCTX& ctx)
{
	SubStrLoc slCommentSubLoc = GetSubLocUntilChar(ctx, '\n', IGNORE_ML_COMMENT | IGNORE_SL_COMMENT);

	return slCommentSubLoc;

	/*
	slCommentSubLoc.m_start = ctx.CurrentPos();
	slCommentSubLoc.m_end = slCommentSubLoc.m_start;

	const char* cstr = ctx.HeadCStr();

	StrSizeT lrLength;
	while (*cstr && !isLineReturn(cstr, &lrLength))
	{
		if (*cstr == '\\' && isLineReturn(cstr + 1, &lrLength))
		{
			cstr += 1 + lrLength;
			slCommentSubLoc.m_end += 1 + lrLength;
		}
		else
		{
			++cstr;
			++slCommentSubLoc.m_end;
		}
	}

	DOGE_ASSERT_MESSAGE(!*cstr,
		"Error at multi line comment starting line %d. Could not get multiline comment end, unexpected EOL.\n", ctx.lineNumber);

	slCommentSubLoc.m_end += lrLength;

	return slCommentSubLoc;
	*/
}

bool skipEntireLine(StrCTX& ctx)
{
	StrSizeT lastLRLength;
	while (!ctx.IsEnded() && !isLineReturn(ctx.HeadCStr(), &lastLRLength))
	{
		*(*(std::string::iterator*)&ctx.head) = '-';
		ctx.Advance(1);
	}

	if (ctx.IsEnded())
	{
		return false; // No EOL found
	}

	ctx.Advance(lastLRLength);

	return true;
}

void skipHSpaces(StrCTX& ctx)
{
	while (isHSpace(LookChar(ctx)))
	{
		ConsumeChar(ctx);
	}
}

bool skipMultiScopeCharEnclosed(StrCTX& ctx, char c_open, char c_close)
{
	DOGE_ASSERT_MESSAGE(c_open != c_close, "Error. Cannot handle multiple scopes when opening and closing sequence are the same.\n");

	skipHSpaces(ctx);

	int pileCount = 0;

	if (ConsumeChar(ctx) != c_open)
	{
		return false;
	}

	++pileCount;

	while (!ctx.IsEnded())
	{
		char c = ConsumeChar(ctx);

		if (c == '\n')
		{
			return false;
		}

		if (c == c_open) {
			++pileCount;
		}
		else if (c == c_close)
		{
			--pileCount;
		}

		if (pileCount == 0)
		{
			return true;
		}
	}

	return false;
}

bool advanceToNextConditionDirective(StrCTX& ctx)
{
	char c;
	while (!ctx.IsEnded() && ((c = LookChar(ctx)) != '#'))
	{
		ConsumeChar(ctx);
	}

	if (ctx.IsEnded())
	{
		return false; // Unexpected EOF
	}
	else
	{
		StrSizeT directiveStartPos;
		EDirective eDirective;
		if (!retrieveDirectiveType(ctx, eDirective, directiveStartPos))
		{
			DOGE_ASSERT_ALWAYS_MESSAGE("Error retrieving directive type.\n");
		}

		if (isConditionnalDirective(eDirective))
		{
			ctx.SetHead(directiveStartPos);
			ctx.bInLineStart = true; // Was valid if retrieveDirectiveType() succeeded, need to restore it since restorinhg manually the head position
			return true;
		}
		else
		{
			return advanceToNextConditionDirective(ctx);
		}
	}
}

bool advanceToEndOfLiteral(StrCTX& ctx, char litEndChar)
{
	SubStrLoc literalSubLoc = GetSubLocUntilChar(ctx, litEndChar, IGNORE_ML_COMMENT | IGNORE_SL_COMMENT | INSIDE_LITERAL);

	ctx.SetHead(literalSubLoc.m_end);

	return true;

	/*
	char c;
	while (!ctx.IsEnded() && ((c = LookChar(ctx)) != '\n') && c != litEndChar)
	{
		ConsumeChar(ctx);

		if (c == '\\')
		{
			ctx.Advance(1); // Ignore escaped character
		}
	}

	if (ctx.IsEnded())
	{
		return false; // Unexpected EOF
	}
	else if (ctx.HeadChar() == '\n')
	{
		return false; // Unexpected EOL
	}
	else
	{
		ctx.Advance(1); // Skip literal end symbol
		return true;
	}
	*/
}

#if 0
bool skipCtxHeadToNextValid(StrCTX& ctx)
{
	if (ctx.IsEnded())
	{
		return false; // An error for now, should be checked before picking
	}

	if (isMultiLineCommentStart(ctx.HeadCStr()))
	{
		ctx.Advance(COMMENT_ML_START_LENGTH);
		if (!skipEntireMultiLineComment(ctx))
		{
			DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Could not correctly skip commented section (multiline comment).\n", ctx.lineNumber);
		}

		++ctx.numNeededCommentSubtitutions;

		return skipCtxHeadToNextValid(ctx);
	}

	if (isSingleLineCommentStart(ctx.HeadCStr()))
	{
		ctx.Advance(COMMENT_SL_LENGTH);
		if (!skipEntireLine(ctx))
		{
			DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Could not correctly skip commented line (singleline comment).\n", ctx.lineNumber);
		}

		++ctx.numNeededCommentSubtitutions;

		return skipCtxHeadToNextValid(ctx);
	}

	StrSizeT lastLRLength;
	if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1, &lastLRLength)) // Skip line break
	{
		ctx.Erase({ ctx.CurrentPos(), ctx.CurrentPos() + 1 + lastLRLength }, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
		//ctx.Advance(1 + LR_LENGTH);
		return skipCtxHeadToNextValid(ctx);
	}

	if (ctx.HeadChar() == '\r')
	{
		ctx.Advance(1);
		if (ctx.HeadChar() != '\n')
		{
			return false; // Allow \r only if followed by \n, and skip \r
		}
	}

	return true;
}
#endif

char LookChar(StrCTX& ctx, uint32_t ignoreField)
{
	DOGE_ASSERT_MESSAGE(!ctx.IsEnded(),
		"Error at line %d. Could not consume char, unexpected EOL.\n", ctx.lineNumber); // An error for now, should be checked before consuming

	if (!(ignoreField & IGNORE_ML_COMMENT) && isMultiLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetMultiLineCommentSubLoc(ctx);

		if (bKeepComment)
		{
			ctx.SetHead(mlCommentSubLoc.m_end);
			ctx.InsertHere(" ", false);
		}
		else
		{
			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
		}

		return ' '; // Default separator
	}

	if (!(ignoreField & IGNORE_SL_COMMENT) && isSingleLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetSingleLineCommentSubLoc(ctx);

		if (bKeepComment)
		{
			ctx.SetHead(mlCommentSubLoc.m_end);
		}
		else
		{
			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
		}

		return ' '; // Default separator
	}

	StrSizeT lastLRLength;
	if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1, &lastLRLength)) // Skip line break
	{
		ctx.Erase({ ctx.CurrentPos(), ctx.CurrentPos() + 1 + lastLRLength }, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
		return LookChar(ctx, ignoreField);
	}

	if ((ignoreField & INSIDE_LITERAL) && ctx.HeadChar() == '\\')
	{
		ctx.AdvanceHead();
		ctx.AdvanceHead();

		return LookChar(ctx, ignoreField);
	}

	if (ctx.HeadChar() == '\r')
	{
		ctx.AdvanceHead();
		DOGE_ASSERT_MESSAGE(ctx.HeadChar() == '\n',
			"Error at line %d. Unsupported single ControlReturn without following LineReturn symbol.\n", ctx.lineNumber);

		return '\n';
	}

	return ctx.HeadChar();
}

char ConsumeChar(StrCTX& ctx, uint32_t ignoreField) 
{
	DOGE_ASSERT_MESSAGE(!ctx.IsEnded(), 
		"Error at line %d. Could not consume char, unexpected EOL.\n", ctx.lineNumber); // An error for now, should be checked before consuming

	if (!(ignoreField & IGNORE_ML_COMMENT) && isMultiLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetMultiLineCommentSubLoc(ctx);

		if (bKeepComment)
		{
			ctx.SetHead(mlCommentSubLoc.m_end);
		}
		else
		{
			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_END);
		}

		return ' '; // Default separator
	}

	if (!(ignoreField & IGNORE_SL_COMMENT) && isSingleLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetSingleLineCommentSubLoc(ctx);

		if (bKeepComment)
		{
			ctx.SetHead(mlCommentSubLoc.m_end);
		}
		else
		{
			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_END);
		}

		return ' '; // Default separator
	}

	StrSizeT lastLRLength;
	if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1, &lastLRLength)) // Skip line break
	{
		ctx.Erase({ ctx.CurrentPos(), ctx.CurrentPos() + 1 + lastLRLength }, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
		return ConsumeChar(ctx);
	}

	if ((ignoreField & INSIDE_LITERAL) && ctx.HeadChar() == '\\')
	{
		ctx.AdvanceHead();
		ctx.AdvanceHead();

		return ConsumeChar(ctx, ignoreField);
	}

	if (ctx.HeadChar() == '\r')
	{
		ctx.AdvanceHead();
		DOGE_ASSERT_MESSAGE(ctx.HeadChar() == '\n',
			"Error at line %d. Unsupported single ControlReturn without following LineReturn symbol.\n", ctx.lineNumber);

		ctx.AdvanceHead();
		return '\n';
	}

	char consumedChar = ctx.HeadChar();
	ctx.AdvanceHead();

	return consumedChar;
}

bool advanceToNextLineExpectHSpaceOnly(StrCTX& ctx)
{
	char c;
	while (!ctx.IsEnded() && ((c = LookChar(ctx)) != '\n'))
	{
		if (!isHSpace(c))
		{
			return false; // Encountered non hspace character
		}
		ConsumeChar(ctx);
	}

	if (ctx.IsEnded())
	{
		return false; // Unexpected EOL
	}
	else
	{
		ConsumeChar(ctx); // Skip EOL symbol
		return true;
	}
}

bool retrieveIdentifier(StrCTX& ctx, std::string& outStrId)
{
	outStrId.clear();

	char c = LookChar(ctx);
	if (LUT_validIdFirstChar[c])
	{
		outStrId += c;
		ConsumeChar(ctx);
	}
	else
	{
		return false;
	}

	while (LUT_validIdBodyChar[(c = LookChar(ctx))])
	{
		outStrId += c;
		ConsumeChar(ctx);
	}

	return true;
}

bool getNextIdentifierLocation(StrCTX& ctx, SubStrLoc& outLocation, bool& isEOS)
{
	isEOS = false;

	while (!ctx.IsEnded() && !LUT_validIdFirstChar[ctx.HeadChar()])
	{
		ctx.Advance(1);
	}

	if (ctx.IsEnded())
	{
		isEOS = true;
		return true; // Not exactly an error, just EOS
	}

	if (LUT_validIdBodyChar[*(ctx.head - 1)])
	{
		return false; // Found identifier with start not correctly separated
	}

	StrSizeT idStartPos = ctx.CurrentPos();

	ctx.Advance(1);
	while (!ctx.IsEnded() && LUT_validIdBodyChar[ctx.HeadChar()])
	{
		ctx.Advance(1);
	}

	outLocation.m_start = idStartPos;
	outLocation.m_end = ctx.CurrentPos();

	return true;
}

bool retrieveDirectiveType(StrCTX& ctx, EDirective& outEDirective, StrSizeT& outDirectiveStartPos)
{
	if (!ctx.bInLineStart)
	{
		DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Character # can only happen at start of line content.\n", ctx.lineNumber);
	}

	StrSizeT directiveStartPos = ctx.CurrentPos();

	DOGE_ASSERT(ctx.HeadChar() == '#');

	ctx.Advance(1);

	std::string strId;
	if (!retrieveIdentifier(ctx, strId))
	{
		DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Invalid identifier.\n", ctx.lineNumber);
	}

	printf("Found # identifier #%s.\n", strId.c_str());

	EDirective eDirective = szNameToEnum<EDirective>(strId.c_str());
	if (eDirective == e_directive_UNKNOWN)
	{
		DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Unknown directive #%s.\n", ctx.lineNumber, strId.c_str());
	}

	outEDirective = eDirective;
	outDirectiveStartPos = directiveStartPos;

	return true;
}

bool getCharEnclosedString(StrCTX& ctx, char c_open, char c_close,
	std::string& strOutEnclosed)
{
	if (ConsumeChar(ctx) != c_open)
	{
		return false;
	}

	strOutEnclosed.clear();

	char c;
	while (!ctx.IsEnded() && ((c = LookChar(ctx)) != c_close))
	{
		strOutEnclosed += c;
		ConsumeChar(ctx);
	}

	if (ctx.IsEnded())
	{
		return false;
	}

	ConsumeChar(ctx);

	return true;
}

//char LookAheadSzOnce(const char* sz);
//const char* AdvanceSzOnce(const char* sz);
//
//const char* AdvanceSzOnce(const char* sz)
//{
//	if (!*sz)
//	{
//		DOGE_ASSERT_ALWAYS_MESSAGE("Unexpected EOL.\n");
//	}
//
//	if (*sz == '\r')
//	{
//		DOGE_ASSERT_MESSAGE(*(sz + 1) == '\n', "Symbol ControlReturn can only be followed by a LineReturn symbol.\n");
//		sz += 2;
//	}
//	else if (*sz == '\\' && LookAheadSzOnce(sz + 1) == '\n')
//	{
//		sz = AdvanceSzOnce(sz + 1);
//	}
//	
//	if (isMultiLineCommentStart(ctx.HeadCStr()))
//	{
//		SubStrLoc mlCommentSubLoc = GetMultiLineCommentSubLoc(ctx);
//
//		if (bKeepComment)
//		{
//			ctx.SetHead(mlCommentSubLoc.m_end);
//		}
//		else
//		{
//			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_END);
//		}
//
//		return ' '; // Default separator
//	}
//
//	if (isSingleLineCommentStart(ctx.HeadCStr()))
//	{
//		SubStrLoc mlCommentSubLoc = GetSingleLineCommentSubLoc(ctx);
//
//		if (bKeepComment)
//		{
//			ctx.SetHead(mlCommentSubLoc.m_end);
//		}
//		else
//		{
//			ctx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_END);
//		}
//
//		return ' '; // Default separator
//	}
//}
//
//char LookAheadCtxOnce(StrCTX& ctx)
//{
//	if (ctx.IsEnded())
//	{
//		DOGE_ASSERT_ALWAYS_MESSAGE("Unexpected EOL.\n");
//	}
//
//	if (ctx.HeadChar() == '\r')
//	{
//		ctx.AdvanceHead();
//		DOGE_ASSERT_MESSAGE(ctx.HeadChar() == '\n', "Symbol ControlReturn can only be followed by a LineReturn symbol.\n");
//
//		return LookAheadSzOnce(sz);
//	}
//
//	if (*sz == '\\' && LookAheadSzOnce(sz + 1) == '\n')
//	{
//		sz = AdvanceSzOnce(sz);
//
//		return LookAheadSzOnce(sz);
//	}
//}
//
//char LookAheadSzOnce(const char* sz)
//{
//	if (!*sz)
//	{
//		DOGE_ASSERT_ALWAYS_MESSAGE("Unexpected EOL.\n");
//	}
//
//	if (*sz == '\r')
//	{
//		DOGE_ASSERT_MESSAGE(*(sz + 1) == '\n', "Symbol ControlReturn can only be followed by a LineReturn symbol.\n");
//		++sz;
//
//		return LookAheadSzOnce(sz);
//	}
//
//	if (*sz == '\\' && LookAheadSzOnce(sz + 1) == '\n')
//	{
//		sz = AdvanceSzOnce(sz);
//
//		return LookAheadSzOnce(sz);
//	}
//}

SubStrLoc GetSubLocUntilChar(StrCTX& ctx, char c, uint32_t ignoreField)
{
	SubStrLoc subLocUntil;
	subLocUntil.m_start = ctx.CurrentPos();

	char cdebug;
	while ((cdebug = ConsumeChar(ctx, ignoreField)) != c)
	{
		int breakpoint = -1;
	}

	subLocUntil.m_end = ctx.CurrentPos();

	ctx.SetHead(subLocUntil.m_start);

	return subLocUntil;
}

SubStrLoc GetSubLocUntilSz(StrCTX& ctx, const char* sz, uint32_t ignoreField)
{
	SubStrLoc subLocUntil;
	subLocUntil.m_start = ctx.CurrentPos();

	StrSizeT szLen = strlen(sz);
	while (!szEqN(ctx.HeadCStr(), sz, szLen))
	{
		ConsumeChar(ctx, ignoreField);
	}
	ctx.SetHead(ctx.CurrentPos() + szLen);

	subLocUntil.m_end = ctx.CurrentPos();

	ctx.SetHead(subLocUntil.m_start);

	return subLocUntil;
}

//
// This method might seem redundant when pick next char already auto skipping line breaks, but it allows for a faster string building (no more char by char)
bool retrieveMultiLinePPContent(StrCTX& ctx, std::string& strMultiLineContent)
{
	SubStrLoc mlContentSubLoc = GetSubLocUntilChar(ctx, '\n', 0);

	strMultiLineContent = mlContentSubLoc.ExtractSubStr(ctx.baseStr);

	ctx.SetHead(mlContentSubLoc.m_end);

	return true;

	/*
	StrCItT itMultiLineContentPartStart = ctx.head;
	StrCItT itMultiLineContentPartEnd;

	bool bLineBreakFound = false;

	strMultiLineContent.clear();

	while (!ctx.IsEnded() && !isLineReturn(ctx.HeadCStr()))
	{
		StrSizeT lastLRLength;
		if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1, &lastLRLength)) // Skip line break
		{
			itMultiLineContentPartEnd = ctx.head;
			strMultiLineContent.append(itMultiLineContentPartStart, itMultiLineContentPartEnd);

			ctx.Advance(1 + lastLRLength);
			itMultiLineContentPartStart = ctx.head;
		}
		else
		{
			DOGE_ASSERT(skipCtxHeadToNextValid(ctx));
			ctx.Advance(1);
		}
	}

	itMultiLineContentPartEnd = ctx.head;
	strMultiLineContent.append(itMultiLineContentPartStart, itMultiLineContentPartEnd);

	return true;
	*/
}

bool handleDirectiveContent_Include(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	char enclosing_c_open;
	char enclosing_c_close;

	bool bIsSytemInclude;

	char c = LookChar(ctx);
	if (c == '"')
	{
		enclosing_c_open = '"';
		enclosing_c_close = '"';
		bIsSytemInclude = false;
	}
	else if (c == '<')
	{
		enclosing_c_open = '<';
		enclosing_c_close = '>';
		bIsSytemInclude = true;
	}
	else
	{
		return false;
	}

	std::string strEnclosed;
	if (!getCharEnclosedString(ctx, enclosing_c_open, enclosing_c_close, strEnclosed))
	{
		return false;
	}

	std::string strIncludedFile;
	if (!readFileToString(strEnclosed.c_str(), strIncludedFile))
	{
		return false;
	}

	//preprocess_string(strIncludedFile);

	SubStrLoc replacedByIncludeZone{ directiveStartPos, ctx.CurrentPos() };
	ctx.SetHead(directiveStartPos);
	ctx.bInLineStart = true; // Start of file means start of line
	ctx.Replace(replacedByIncludeZone, strIncludedFile, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);

	return true;
}

bool decomposeString(const std::string& strIn, char sep, std::vector<std::string>& vOutStrDecomposed)
{
	StrSizeT oldAfterSepPos = 0;
	StrSizeT sepPos;
	
	vOutStrDecomposed.clear();

	while ((sepPos = strIn.find(sep, oldAfterSepPos)) != npos<StrSizeT>::value)
	{
		vOutStrDecomposed.push_back(strIn.substr(oldAfterSepPos, sepPos - oldAfterSepPos));
		oldAfterSepPos = sepPos + 1;
	}

	sepPos = strIn.length(); // Take remaining

	vOutStrDecomposed.push_back(strIn.substr(oldAfterSepPos, sepPos - oldAfterSepPos));

	return true;
}

void adjustSubStrLoc_CleanExtHSpaces(const std::string& strFrom, SubStrLoc& inoutLoc)
{
	while (inoutLoc.m_start < inoutLoc.m_end && isHSpace(strFrom[inoutLoc.m_start]))
	{
		++inoutLoc.m_start;
	}

	while (inoutLoc.m_end > inoutLoc.m_start + 1)
	{
		if (!isHSpace(strFrom[inoutLoc.m_end - 1]))
			break;
		--inoutLoc.m_end;
	}
}

bool decomposeStringCleanHSpaces(const std::string& strIn, char sep, std::vector<std::string>& vOutStrDecomposed)
{
	StrSizeT sepPos;

	SubStrLoc sepSubLoc;
	sepSubLoc.m_start = 0;

	vOutStrDecomposed.clear();

	if (strIn.empty())
	{
		return true; // Empty strings means 0 decomposed str
	}

	while ((sepPos = strIn.find(sep, sepSubLoc.m_start)) != npos<StrSizeT>::value)
	{
		sepSubLoc.m_end = sepPos;
		adjustSubStrLoc_CleanExtHSpaces(strIn, sepSubLoc);

		vOutStrDecomposed.push_back(sepSubLoc.ExtractSubStr(strIn));
		sepSubLoc.m_start = sepPos + 1;
	}

	sepSubLoc.m_end = strIn.length(); // Take remaining
	adjustSubStrLoc_CleanExtHSpaces(strIn, sepSubLoc);

	vOutStrDecomposed.push_back(sepSubLoc.ExtractSubStr(strIn));

	return true;
}

#define FOR_VEC(counter, vec) for (decltype(vec.size()) counter = 0; counter < vec.size(); ++counter)

bool parseFunctionMacroDefinition(StrCTX& ctx, Macro& outFnMacro)
{
	if (!(LookChar(ctx) == '('))
	{
		return false; // Args declaration needs opening parenthesis
	}

	std::string strMacroArgs;
	if (!getCharEnclosedString(ctx, '(', ')', strMacroArgs))
	{
		return false; // Args declaration not correctly terminated
	}

	std::vector<std::string> vStrArgs;
	decomposeStringCleanHSpaces(strMacroArgs, ',', vStrArgs);

	if (std::any_of(vStrArgs.cbegin(), vStrArgs.cend(), [](const std::string& str) { return str.empty(); }))
	{
		return false; // Empty parameter
	}

	for (VEC_SIZE_T(vStrArgs) nArg = 0; nArg < vStrArgs.size(); ++nArg)
	{ 
		const auto& strArg = vStrArgs[nArg];
		if (std::find(vStrArgs.cbegin() + nArg + 1, vStrArgs.cend(), strArg) != vStrArgs.cend())
		{
			return false; // Duplicate parameter
		}
	}

	outFnMacro.m_args.clear();
	outFnMacro.m_args.resize(vStrArgs.size());

	std::string strDefineContent;
	if (!retrieveMultiLinePPContent(ctx, strDefineContent))
	{
		return false;
	}

	if (vStrArgs.size() == 0) // No need to build the macro args
	{
		outFnMacro.m_strContent = strDefineContent;
		return true;
	}

	SubStrLoc idLoc;
	StrCTX defineContentCtx{ strDefineContent, strDefineContent.cbegin() };
	bool isEOS = false;

	while (!isEOS)
	{
		if (!getNextIdentifierLocation(defineContentCtx, idLoc, isEOS))
		{
			return false; // Error retrieving next identifier
		}

		if (!isEOS)
		{
			std::string strId(strDefineContent, idLoc.m_start, idLoc.Length());

			for (VEC_SIZE_T(vStrArgs) nArg = 0; nArg < vStrArgs.size(); ++nArg)
			{
				const auto& strArg = vStrArgs[nArg];
				if (strArg == strId)
				{
					outFnMacro.m_args[nArg].m_subLocations.push_back(idLoc);
					break;
				}
			}
		}		
	}

	for (VEC_SIZE_T(outFnMacro.m_args) nArg = 0; nArg < outFnMacro.m_args.size(); ++nArg)
	{
		const auto& macroArg = outFnMacro.m_args[nArg];

		for (VEC_SIZE_T(macroArg.m_subLocations) nSubLoc = 0; nSubLoc < macroArg.m_subLocations.size(); ++nSubLoc)
		{
			const auto& subLoc = macroArg.m_subLocations[nSubLoc];

			for (StrSizeT nStrPos = subLoc.m_start; nStrPos < subLoc.m_end; ++nStrPos)
			{
				strDefineContent[nStrPos] = toUpper(strDefineContent[nStrPos]);
			}
		}
	}

	outFnMacro.m_strContent = strDefineContent;

	return true;
}

bool handleDirectiveContent_Define(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strDefine;
	if (!retrieveIdentifier(ctx, strDefine))
	{
		return false;
	}

	Macro macro;
	macro.m_name = strDefine;

	char c = LookChar(ctx);
	if (c == '(')
	{
		macro.m_eType = e_macrotype_function;

		if (!parseFunctionMacroDefinition(ctx, macro))
		{
			return false;
		}

		//sprintf(g_sprintf_buffer, "<<< #define fn : '%s', args number : '%d', content : '%s' >>>", strDefine.c_str(), macro.m_args.size(), macro.m_strContent.c_str());
		//SubStrLoc replacedByTmpDefineInfoZone{ directiveStartPos, ctx.CurrentPos() };
		//ctx.Replace(replacedByTmpDefineInfoZone, std::string(g_sprintf_buffer), StrCTX::e_insidereplacedzonebehaviour_ASSERT);

		gMacroInsert(strDefine, std::move(macro));

		return true;
	}
	else if (isHSpace(c) || c == '\n')
	{
		macro.m_eType = e_macrotype_definition;

		skipHSpaces(ctx);

		if (LookChar(ctx) == '\n')
		{
			macro.m_strContent.clear(); // simple define with no content
		}
		else
		{
			if (!retrieveMultiLinePPContent(ctx, macro.m_strContent))
			{
				return false;
			}
		}

		//sprintf(g_sprintf_buffer, "<<< #define : '%s', content : '%s' >>>", strDefine.c_str(), macro.m_strContent.c_str());
		//SubStrLoc replacedByTmpDefineInfoZone{ directiveStartPos, ctx.CurrentPos() };
		//ctx.Replace(replacedByTmpDefineInfoZone, std::string(g_sprintf_buffer), StrCTX::e_insidereplacedzonebehaviour_ASSERT);

		gMacroInsert(strDefine, std::move(macro));

		return true;
	}
	else
	{
		return false; // Unauthorized character after defined identifier
	}
}

bool handleDirectiveContent_Undef(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strUndefine;
	if (!retrieveIdentifier(ctx, strUndefine))
	{
		return false;
	}

	if (!skipEntireLine(ctx))
	{
		return false;
	}

	gMacroUndefine(strUndefine);

	return true;
}

bool handleDirectiveContent_Pragma(StrCTX& ctx, StrSizeT directiveStartPos)
{
	skipHSpaces(ctx);

	std::string strPragmaId;
	if (!retrieveIdentifier(ctx, strPragmaId))
	{
		return false;
	}

	std::string strMultilineContent;
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent))
	{
		return false;
	}

	//sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	//SubStrLoc replacedByTmpPragmaInfoZone{ directiveStartPos, ctx.CurrentPos() };
	//ctx.Replace(replacedByTmpPragmaInfoZone, std::string(g_sprintf_buffer), StrCTX::e_insidereplacedzonebehaviour_ASSERT);

	return true;
}

bool handleDirectiveContent_If(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strMultilineContent;
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent))
	{
		return false;
	}

	preprocess_if_directive_string(strMultilineContent);

	std::unique_ptr<const Expression> expression = EvaluateStringExpression(strMultilineContent);
	bool bYieldedValue = expression->Evaluate() ? true : false;

	std::string strDebugExpression = expression->GetStringExpression();

	printf("%s#if at pos %d yields %s\n", tabIndentStr(g_ifCtx.TopLevelCount()),
		directiveStartPos, bYieldedValue ? "'true'" : "'false'");

	bool bMustSkip;
	g_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Ifdef(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strTestedIdentifier;
	if (!retrieveIdentifier(ctx, strTestedIdentifier))
	{
		return false;
	}

	bool bYieldedValue = gMacroIsDefined(strTestedIdentifier);

	if (!advanceToNextLineExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after tested #ifdef identifier
	}

	printf("%s#ifdef at pos %d yields %s\n", tabIndentStr(g_ifCtx.TopLevelCount()),
		directiveStartPos, bYieldedValue ? "'true'" : "'false'");

	bool bMustSkip;
	g_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);
	
	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Ifndef(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strTestedIdentifier;
	if (!retrieveIdentifier(ctx, strTestedIdentifier))
	{
		return false;
	}

	bool bYieldedValue = !gMacroIsDefined(strTestedIdentifier);

	if (!advanceToNextLineExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after tested #ifdef identifier
	}

	printf("%s#ifndef at pos %d yields %s\n", tabIndentStr(g_ifCtx.TopLevelCount()),
		directiveStartPos, bYieldedValue ? "'true'" : "'false'");

	bool bMustSkip;
	g_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Elif(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!isHSpace(LookChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	std::string strMultilineContent;
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent))
	{
		return false;
	}

	preprocess_if_directive_string(strMultilineContent);

	std::unique_ptr<const Expression> expression = EvaluateStringExpression(strMultilineContent);
	bool bYieldedValue = expression->Evaluate() ? true : false;

	std::string strDebugExpression = expression->GetStringExpression();

	printf("%s#elif at pos %d yields %s\n", tabIndentStr(g_ifCtx.TopLevelCount()),
		directiveStartPos, bYieldedValue ? "'true'" : "'false'");

	bool bMustSkip;
	g_ifCtx.UpdateElseIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Else(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!advanceToNextLineExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after #else
	}

	bool bMustSkip;
	g_ifCtx.UpdateElse(ctx, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Endif(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!advanceToNextLineExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after #else
	}

	bool bMustSkip;
	g_ifCtx.UpdateEndif(ctx, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent(StrCTX& ctx, EDirective eDirective, StrSizeT directiveStartPos)
{
	switch (eDirective)
	{
	case e_directive_include:
		return handleDirectiveContent_Include(ctx, directiveStartPos);
	case e_directive_define:
		return handleDirectiveContent_Define(ctx, directiveStartPos);
	case e_directive_undef:
		return handleDirectiveContent_Undef(ctx, directiveStartPos);
	case e_directive_pragma:
		return handleDirectiveContent_Pragma(ctx, directiveStartPos);
	case e_directive_if:
		return handleDirectiveContent_If(ctx, directiveStartPos);
	case e_directive_ifdef:
		return handleDirectiveContent_Ifdef(ctx, directiveStartPos);
	case e_directive_ifndef:
		return handleDirectiveContent_Ifndef(ctx, directiveStartPos);
	case e_directive_elif:
		return handleDirectiveContent_Elif(ctx, directiveStartPos);
	case e_directive_else:
		return handleDirectiveContent_Else(ctx, directiveStartPos);
	case e_directive_endif:
		return handleDirectiveContent_Endif(ctx, directiveStartPos);
	default:
		return false;
	}

	return true;
}

std::vector<std::string> g_encounteredIdentifiers;

void preprocess_file_string(std::string& strBuffer)
{
	StrCTX ctx{ strBuffer, strBuffer.cbegin() };

	while (!ctx.IsEnded())
	{
		char c = LookChar(ctx);
		if (isHSpace(c))
		{
			ConsumeChar(ctx);
		}
		else if (c == '\n')
		{
			printf("Line return at line %d.\n", ctx.lineNumber);

			ConsumeChar(ctx);

			++ctx.lineNumber;
		}
		else if (c == '#') {
			StrSizeT directiveStartPos;
			EDirective eDirective;
			if (!retrieveDirectiveType(ctx, eDirective, directiveStartPos))
			{
				DOGE_ASSERT_ALWAYS_MESSAGE("Error retrieving directive type.\n");
			}

			if (!handleDirectiveContent(ctx, eDirective, directiveStartPos))
			{
				DOGE_ASSERT_ALWAYS_MESSAGE("Error at line %d. Invalid directive content.\n", ctx.lineNumber);
			}
		}
		else if (isLiteralDelimiterChar(c))
		{
			ConsumeChar(ctx);
			DOGE_ASSERT_MESSAGE(advanceToEndOfLiteral(ctx, c), "Error at line %d. Invalid literal termination.\n", ctx.lineNumber);
		}
		else if (LUT_validIdFirstChar[c])
		{
			std::string strId;
			SubStrLoc idSubLoc;
			idSubLoc.m_start = ctx.CurrentPos();
			DOGE_ASSERT_MESSAGE(retrieveIdentifier(ctx, strId), "Error at line %d. Could not retrieve identifier.\n", ctx.lineNumber);
			g_encounteredIdentifiers.push_back(strId);
			idSubLoc.m_end = ctx.CurrentPos();

			if (gMacroIsDefined(strId))
			{
				const Macro& macro = gMacroGet(strId);

				if (macro.m_eType == e_macrotype_definition) // Todo : support functions macro
				{
					ctx.SetHead(idSubLoc.m_start);
					ctx.Replace(idSubLoc, macro.m_strContent, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
				}
				else
				{
					std::string strMacroArgs;
					DOGE_ASSERT(getCharEnclosedString(ctx, '(', ')', strMacroArgs));
					std::vector<std::string> vStrArgs;
					decomposeStringCleanHSpaces(strMacroArgs, ',', vStrArgs);

					std::string strFnMacroPatchedContent;
					macro.PatchString(vStrArgs, strFnMacroPatchedContent);

					idSubLoc.m_end = ctx.CurrentPos();

					ctx.SetHead(idSubLoc.m_start);
					ctx.Replace(idSubLoc, strFnMacroPatchedContent, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
				}
			}
		}
		else
		{
			ConsumeChar(ctx);
		}
	}
}

void preprocess_if_directive_string(std::string& strBuffer)
{
	StrCTX ctx{ strBuffer, strBuffer.cbegin() };

	while (!ctx.IsEnded())
	{
		char c = LookChar(ctx);
		if (isHSpace(c))
		{
			ConsumeChar(ctx);
		}
		else if (c == '\n')
		{
			printf("Line return at line %d.\n", ctx.lineNumber);

			ConsumeChar(ctx);

			++ctx.lineNumber;
		}
		else if (isLiteralDelimiterChar(c))
		{
			ConsumeChar(ctx);
			DOGE_ASSERT_MESSAGE(advanceToEndOfLiteral(ctx, c), "Error at line %d. Invalid literal termination.\n", ctx.lineNumber);
		}
		else if (LUT_validIdFirstChar[c])
		{
			std::string strId;
			SubStrLoc idSubLoc;
			idSubLoc.m_start = ctx.CurrentPos();
			DOGE_ASSERT_MESSAGE(retrieveIdentifier(ctx, strId), "Error at line %d. Could not retrieve identifier.\n", ctx.lineNumber);
			g_encounteredIdentifiers.push_back(strId);
			idSubLoc.m_end = ctx.CurrentPos();

			if (strId == "defined")
			{
				skipMultiScopeCharEnclosed(ctx, '(', ')'); // We don't want the content to be altered inside the defined(...)
			}
			else
			{
				if (gMacroIsDefined(strId))
				{
					const Macro& macro = gMacroGet(strId);

					if (macro.m_eType == e_macrotype_definition) // Todo : support functions macro
					{
						ctx.SetHead(idSubLoc.m_start);
						ctx.Replace(idSubLoc, macro.m_strContent, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
					}
					else
					{
						std::string strMacroArgs;
						DOGE_ASSERT(getCharEnclosedString(ctx, '(', ')', strMacroArgs));
						std::vector<std::string> vStrArgs;
						decomposeStringCleanHSpaces(strMacroArgs, ',', vStrArgs);

						std::string strFnMacroPatchedContent;
						macro.PatchString(vStrArgs, strFnMacroPatchedContent);

						idSubLoc.m_end = ctx.CurrentPos();

						ctx.SetHead(idSubLoc.m_start);
						ctx.Replace(idSubLoc, strFnMacroPatchedContent, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
					}
				}
			}
		}
		else
		{
			ConsumeChar(ctx);
		}
	}
}

static char IndirectLUT_CharAsCStr[256 * 2];
int Init_CharAsCStr_Data() {
	for (int i = 0; i < 256; ++i)
		IndirectLUT_CharAsCStr[i << 1] = static_cast<char>(i);

	return 0;
}

const char* CharAsCStr(char c)
{
	static int dummyStaticInit = Init_CharAsCStr_Data();

	return &IndirectLUT_CharAsCStr[static_cast<int>(c) << 1];
}


template <class T>
bool CArrayIsPresent(const T carray[], size_t arraySize, T val)
{
	return std::find(carray, carray + arraySize, val) != (carray + arraySize);
}

std::unique_ptr<const Expression> EvaluateStringExpression(std::string strExp)
{
	StrCTX expStrCtx(strExp);

	static const char singleOnlyCharSymbol[] = "?:^~%*/()";
	static const char doubleMaybeCharSymbol[] = "!&|+-><=";
	static const char doubleOnlyCharSecondSymbol[] = "=&|<>";

	std::vector<Token> vParsedTokens;

	while (!expStrCtx.IsEnded())
	{
		char c = LookChar(expStrCtx);

		if (c == '\n')
		{
			break;
		}
		else if (isHSpace(c))
		{
			ConsumeChar(expStrCtx);
		}
		else if (isDigit(c))
		{
			StrSizeT numberStartPos = expStrCtx.CurrentPos();
			ConsumeChar(expStrCtx);
			while (!expStrCtx.IsEnded() && isValidInNumberChar(LookChar(expStrCtx)))
			{
				ConsumeChar(expStrCtx);
			}
			StrSizeT numberEndPos = expStrCtx.CurrentPos();
			StrSizeT suffixStartPos = expStrCtx.CurrentPos();
			while (!expStrCtx.IsEnded() && LUT_validIdBodyChar[LookChar(expStrCtx)])
			{
				ConsumeChar(expStrCtx);
			}
			StrSizeT suffixEndPos = expStrCtx.CurrentPos();

			SubStrLoc fullNumberSubLoc{ numberStartPos, suffixEndPos };

			int number;
			expStrCtx.ExecWithTmpSzSubCStr(fullNumberSubLoc, [&](const char* sz) {
				number = atoi(sz);
			});

			Token numberToken{ ETokenType::NUMBER, fullNumberSubLoc.ExtractSubStr(expStrCtx.baseStr) };
			vParsedTokens.push_back(numberToken);
		}
		else if (LUT_validIdFirstChar[c])
		{
			StrSizeT idStartPos = expStrCtx.CurrentPos();
			ConsumeChar(expStrCtx);
			while (!expStrCtx.IsEnded() && LUT_validIdFirstChar[LookChar(expStrCtx)])
			{
				ConsumeChar(expStrCtx);
			}
			StrSizeT idEndPos = expStrCtx.CurrentPos();

			SubStrLoc idSubLoc{ idStartPos, idEndPos };
			
			bool isDefinedFnCall;
			expStrCtx.ExecWithTmpSzSubCStr(idSubLoc, [&](const char* sz) {
				isDefinedFnCall = szEq("defined", sz);
			});

			if (isDefinedFnCall)
			{
				Token defineToken{ ETokenType::DEFINED, "" };
				vParsedTokens.push_back(defineToken);
			}
			else
			{
				Token nameToken{ ETokenType::NAME, idSubLoc.ExtractSubStr(expStrCtx.baseStr) };
				vParsedTokens.push_back(nameToken);
			}
		}
		else if (CArrayIsPresent(singleOnlyCharSymbol, DOGE_ARRAY_SIZE(singleOnlyCharSymbol), c))
		{
			ETokenType eOperator = ETokenTypeFromSingleCharOperator(CharAsCStr(c));
			DOGE_ASSERT(eOperator != ETokenType::UNKNOWN);

			vParsedTokens.push_back({ eOperator, "" });

			ConsumeChar(expStrCtx);
		}
		else if (CArrayIsPresent(doubleMaybeCharSymbol, DOGE_ARRAY_SIZE(doubleMaybeCharSymbol), c))
		{
			StrSizeT doubleCharOpStartPos = expStrCtx.CurrentPos();

			ConsumeChar(expStrCtx);

			// Handcrafted one symbol "lookahead" for those modest needs
			if (!expStrCtx.IsEnded() &&
				CArrayIsPresent(doubleOnlyCharSecondSymbol, DOGE_ARRAY_SIZE(doubleOnlyCharSecondSymbol), LookChar(expStrCtx)))
			{
				StrSizeT doubleCharOpEndPos = expStrCtx.CurrentPos() + 1;
				SubStrLoc doubleCharOpSubLoc{ doubleCharOpStartPos, doubleCharOpEndPos };

				ETokenType eOperator;
				expStrCtx.ExecWithTmpSzSubCStr(doubleCharOpSubLoc, [&](const char* sz) {
					eOperator = ETokenTypeFromDoubleCharOperator(sz);
				});
				DOGE_ASSERT(eOperator != ETokenType::UNKNOWN);

				vParsedTokens.push_back({ eOperator, "" });

				ConsumeChar(expStrCtx);
			}
			else
			{
				ETokenType eOperator = ETokenTypeFromSingleCharOperator(CharAsCStr(c));
				DOGE_ASSERT(eOperator != ETokenType::UNKNOWN);

				vParsedTokens.push_back({ eOperator, "" });
			}
		}
		else
		{
			DOGE_ASSERT_ALWAYS_MESSAGE("Unsupported expression character '%c'.\n", c);
		}
	}

	vParsedTokens.push_back({ ETokenType::EOL, "" });

	MacroExpParser expParser(vParsedTokens.cbegin());
	std::unique_ptr<const Expression> result = expParser.ParseExpression();

	return result;
}

int main(int argc, char* argv[])
{
	//std::string testExpressionString = "(-1+20)*1?10:3*4";
	//std::string testExpressionString = "(1 > 0 && 3 ? 5 : 6) * 10";
	//TestEvaluationStringExpression(testExpressionString);

	/*std::string strUserinput;
	while (getline(std::cin, strUserinput))
	{
		std::cout << "===> " << EvaluateStringExpression(strUserinput) << '\n';
	}

	return 0;*/

	std::string strFile;
	DOGE_ASSERT(readFileToString("testfile.txt", strFile));
	preprocess_file_string(strFile);

	std::cout << "===================== BOF =====================\n";
	std::cout << strFile;
	std::cout << "===================== EOF =====================\n\n";

	std::cout << "Global macros : \n";

	for (const auto& globalMacro : g_macros)
	{
		std::cout << "\t- " << globalMacro.second.m_name << '\n';
	}

	coutlr;

	for (const auto& globalStrId : g_encounteredIdentifiers)
	{
		//std::cout << "\t~ " << globalStrId << '\n';
	}

	coutlr;

	return 0;
}

