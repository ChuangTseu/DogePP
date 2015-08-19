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

bool g_bKeepComment = true;

int g_iLastIncludedLinePos = 0;

enum class ESupportedLanguage {
	C,
	CPP,
	XML,
	HTML
};

enum ESupportedLanguage_cenum {
	e_lang_C,
	e_lang_CPP,
	e_lang_XML,
	e_lang_HTML
};

#define LANG_C		1
#define LANG_CPP	2
#define LANG_XML	3
#define LANG_HTML	4

#define PPED_LANG LANG_CPP

#if PPED_LANG == LANG_C

#define SUPPORT_ML_COMMENTS 1
const char* g_mlComment_szOpen = "/*";
const size_t g_mlComment_openLength = 2;
const char* g_mlComment_szClose = "*/";
const size_t g_mlComment_closeLength = 2;

#define SUPPORT_SL_COMMENTS 1
const char* g_slComment_szOpen = "//";
const size_t g_slComment_openLength = 2;

#elif PPED_LANG == LANG_CPP

#define SUPPORT_ML_COMMENTS 1
const char* g_mlComment_szOpen = "/*";
const size_t g_mlComment_openLength = 2;
const char* g_mlComment_szClose = "*/";
const size_t g_mlComment_closeLength = 2;

#define SUPPORT_SL_COMMENTS 1
const char* g_slComment_szOpen = "//";
const size_t g_slComment_openLength = 2;

#elif PPED_LANG == LANG_XML

#define SUPPORT_ML_COMMENTS 1
const char* g_mlComment_szOpen = "<!--";
const size_t g_mlComment_openLength = 3;
const char* g_mlComment_szClose = "-->";
const size_t g_mlComment_closeLength = 3;

#define SUPPORT_SL_COMMENTS 0

#elif PPED_LANG == LANG_HTML

#define SUPPORT_ML_COMMENTS 1
const char* g_mlComment_szOpen = "<!--";
const size_t g_mlComment_openLength = 3;
const char* g_mlComment_szClose = "-->";
const size_t g_mlComment_closeLength = 3;

#define SUPPORT_SL_COMMENTS 0

#else
#error "Unsupported language"
#endif

#define VEC_SIZE_T(vec) decltype(vec.size())

#define X_LIST_OF_Directive \
	X(include) \
	X(define) \
	X(undef) \
	X(error) \
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
	DOGE_DEBUG_ASSERT(eDirective >= 0 && eDirective < e_directive_COUNT);
	return LUT_EDirective_szName[eDirective];
}

DECLARE_ENUM_TEMPLATE_CTTI(EDirective)

enum {
	IGNORE_ML_COMMENT = 1,
	IGNORE_SL_COMMENT = 2,
	INSIDE_LITERAL = 4
};

bool skipCtxHeadToNextValid(StrCTX& ctx);
void preprocess_file_string(StrCTX& ctx, bool* bPerformedMacroSubstitution = NULL);
void preprocess_if_directive_string(std::string& strBuffer, bool* bPerformedMacroSubstitution = NULL);
bool retrieveDirectiveType(StrCTX& ctx, EDirective& outEDirective, StrSizeT& outDirectiveStartPos);
std::unique_ptr<const Expression> EvaluateStringExpression(std::string strExp);
char ConsumeChar(StrCTX& ctx, uint32_t ignoreField = 0);
char LookChar(StrCTX& ctx, uint32_t ignoreField = 0);
SubStrLoc GetSubLocUntilChar(StrCTX& ctx, char c, bool bKeepDelimiter, uint32_t ignoreField);
SubStrLoc GetSubLocUntilSz(StrCTX& ctx, const char* sz, bool bKeepDelimiter, uint32_t ignoreField);

char ConsumeCharExpected(StrCTX& ctx, char expectedChar, uint32_t ignoreField = 0)
{
	char consumedChar = ConsumeChar(ctx, ignoreField);
	if (consumedChar != expectedChar)
	{
		CTX_ERROR(ctx, "Could not consume expected char '%c', found '%c'", expectedChar, consumedChar);
	}
	return consumedChar;
}

SubStrLoc GetMultiLineCommentSubLoc(StrCTX& ctx, bool bKeepDelimiter)
{
	SubStrLoc mlCommentSubLoc = GetSubLocUntilSz(ctx, "*/", bKeepDelimiter, IGNORE_ML_COMMENT | IGNORE_SL_COMMENT);
	return mlCommentSubLoc;
}

SubStrLoc GetSingleLineCommentSubLoc(StrCTX& ctx, bool bKeepLineReturn)
{
	SubStrLoc slCommentSubLoc = GetSubLocUntilChar(ctx, '\n', bKeepLineReturn, IGNORE_ML_COMMENT | IGNORE_SL_COMMENT);
	return slCommentSubLoc;
}

bool skipEntireLine(StrCTX& ctx)
{
	StrSizeT lastLRLength;
	while (!ctx.IsEnded() && !isLineReturn(ctx.HeadCStr(), &lastLRLength))
	{
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
	DOGE_DEBUG_ASSERT_MESSAGE(c_open != c_close, "Error. Cannot handle multiple scopes when opening and closing sequence are the same.\n");

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

		if (c == c_open) ++pileCount;
		else if (c == c_close) --pileCount;

		if (pileCount == 0) return true;
	}

	return false;
}

SubStrLoc GetMultiScopeCharEnclosedSubLoc(StrCTX& ctx, char c_open, char c_close, bool bKeepDelimiters)
{
	DOGE_DEBUG_ASSERT_MESSAGE(c_open != c_close, "Error. Cannot handle multiple scopes when opening and closing sequence are the same.\n");

	StrSizeT restorePos = ctx.CurrentPos();

	SubStrLoc subLoc;

	if (bKeepDelimiters)
	{
		subLoc.m_start = ctx.CurrentPos();
	}

	int pileCount = 0;

	ConsumeCharExpected(ctx, c_open);

	if (!bKeepDelimiters)
	{
		subLoc.m_start = ctx.CurrentPos();
	}

	++pileCount;

	while (!ctx.IsEnded())
	{
		subLoc.m_end = ctx.CurrentPos();

		char c = ConsumeChar(ctx);
		if (c == '\n')
		{
			CTX_ERROR(ctx, "Unexpected LineReturn interupting '%c''%c' delimited scope", c_open, c_close);
		}

		if (c == c_open) ++pileCount;
		else if (c == c_close) --pileCount;

		if (pileCount == 0) break;
	}

	if (bKeepDelimiters)
	{
		subLoc.m_end = ctx.CurrentPos();
	}

	ctx.SetHead(restorePos);

	return subLoc;
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
			CTX_ERROR(ctx, "Could not retrieve directive type");
		}

		if (isConditionnalDirective(eDirective))
		{
			ctx.SetHead(directiveStartPos);
			ctx.m_bInLineStart = true; // Was valid if retrieveDirectiveType() succeeded, need to restore it since restorinhg manually the head position
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
	SubStrLoc literalSubLoc = GetSubLocUntilChar(ctx, litEndChar, true, IGNORE_ML_COMMENT | IGNORE_SL_COMMENT | INSIDE_LITERAL);

	ctx.SetHead(literalSubLoc.m_end);

	return true;
}

char LookChar(StrCTX& ctx, uint32_t ignoreField)
{
	DOGE_DEBUG_ASSERT_MESSAGE(!ctx.IsEnded(), "Could not consume char, unexpected EOL.\n"); // An error for now, should be checked before consuming

	if (!(ignoreField & IGNORE_ML_COMMENT) && isMultiLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetMultiLineCommentSubLoc(ctx, true);

		if (g_bKeepComment)
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
		SubStrLoc mlCommentSubLoc = GetSingleLineCommentSubLoc(ctx, false);

		if (g_bKeepComment)
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
		if (ctx.HeadCharAtDistance(1) != '\n')
			CTX_ERROR(ctx, "Unsupported single ControlReturn without following LineReturn symbol");

		return '\n';
	}

	return ctx.HeadChar();
}

char ConsumeChar(StrCTX& ctx, uint32_t ignoreField) 
{
	DOGE_DEBUG_ASSERT_MESSAGE(!ctx.IsEnded(), "Could not consume char, unexpected EOL.\n"); // An error for now, should be checked before consuming

	if (!(ignoreField & IGNORE_ML_COMMENT) && isMultiLineCommentStart(ctx.HeadCStr()))
	{
		SubStrLoc mlCommentSubLoc = GetMultiLineCommentSubLoc(ctx, true);

		if (g_bKeepComment)
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
		SubStrLoc mlCommentSubLoc = GetSingleLineCommentSubLoc(ctx, true);

		if (g_bKeepComment)
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
		if (ctx.HeadCharAtDistance(1) != '\n')
			CTX_ERROR(ctx, "Unsupported single ControlReturn without following LineReturn symbol");

		ctx.AdvanceHead();
		ctx.AdvanceHead();

		return '\n';
	}

	char consumedChar = ctx.HeadChar();
	ctx.AdvanceHead();

	return consumedChar;
}

std::vector<SubStrLoc> g_vLinesSubLocs;
int g_iLinesCount = 0;

std::vector<std::string> g_strComments;
std::vector<StrSizeT> g_commentsPositions;

void CleanupAndCheckStrBeforeInclude(StrCTX& localCtx)
{
	StrSizeT lastLRLength;
	SubStrLoc runningLineSubLoc;

	DOGE_DEBUG_ASSERT(localCtx.m_includedScopeInfo.IsEmpty());

	runningLineSubLoc.m_start = 0;

	while (!localCtx.IsEnded())
	{
		//if (runningLineSubLoc.m_start == 2169)
		//{
		//	int breakpoint = -1; 
		//}
#if SUPPORT_ML_COMMENTS
		if (szEqN(localCtx.HeadCStr(), g_mlComment_szOpen, g_mlComment_openLength))
		{
			StrSizeT lineStartBefore = runningLineSubLoc.m_start;
			// Inlining mlComment detection for faster scanning and handling of the line return without polluting other utilities
			SubStrLoc mlCommentSubLoc{ localCtx.CurrentPos(), localCtx.CurrentPos() };
			const char *szBeg = localCtx.HeadCStr(), *sz = szBeg;
			while (*sz && !szEqN(sz, g_mlComment_szClose, g_mlComment_closeLength)) {
				if (*sz == '\r') {
					if (*(sz + 1) != '\n')
						CTX_ERROR(localCtx, "Unsupported single ControlReturn without following LineReturn symbol");
					runningLineSubLoc.m_end = mlCommentSubLoc.m_start + (sz - szBeg);
					localCtx.m_includedScopeInfo.AddLineSubLoc(runningLineSubLoc);
					++localCtx.m_includedScopeInfo.iLinesCount;

					sz += 2;
					runningLineSubLoc.m_start = runningLineSubLoc.m_end + 2;
					continue;
				}
				if (*sz == '\n') {
					runningLineSubLoc.m_end = mlCommentSubLoc.m_start + (sz - szBeg);
					localCtx.m_includedScopeInfo.AddLineSubLoc(runningLineSubLoc);
					++localCtx.m_includedScopeInfo.iLinesCount;

					++sz;
					runningLineSubLoc.m_start = runningLineSubLoc.m_end + 1;
					continue;
				}
				++sz;
			}

			if (!*sz)
			{
				CTX_ERROR(localCtx, "Unexpected End of Line in multi line comment");
			}

			mlCommentSubLoc.m_end += (sz + g_mlComment_closeLength - szBeg);

			if (g_bKeepComment) {
				localCtx.m_includedScopeInfo.strComments.push_back(mlCommentSubLoc.ExtractSubStr(localCtx.m_baseStr));
				localCtx.m_includedScopeInfo.commentsPositions.push_back(mlCommentSubLoc.m_start);
			}

			localCtx.Replace(mlCommentSubLoc, " ", StrCTX::e_insidereplacedzonebehaviour_GOTO_END, true);

			runningLineSubLoc.m_start = lineStartBefore + 1;

			continue;
		}
#endif

#if SUPPORT_SL_COMMENTS
		if (isSingleLineCommentStart(localCtx.HeadCStr()))
		{
			SubStrLoc slCommentSubLoc = GetSingleLineCommentSubLoc(localCtx, false);

			if (g_bKeepComment) {
				localCtx.m_includedScopeInfo.strComments.push_back(slCommentSubLoc.ExtractSubStr(localCtx.m_baseStr));
				localCtx.m_includedScopeInfo.commentsPositions.push_back(slCommentSubLoc.m_start);
			}

			localCtx.Erase(slCommentSubLoc, StrCTX::e_insidereplacedzonebehaviour_GOTO_END, false);

			continue;
		}
#endif

		if (localCtx.HeadChar() == '\\' && isLineReturn(localCtx.HeadCStr() + 1, &lastLRLength)) // Skip line break
		{
			runningLineSubLoc.m_end = localCtx.CurrentPos();
			localCtx.m_includedScopeInfo.AddLineSubLoc(runningLineSubLoc);
			++localCtx.m_includedScopeInfo.iLinesCount;

			localCtx.Erase({ localCtx.CurrentPos(), localCtx.CurrentPos() + 1 + lastLRLength }, StrCTX::e_insidereplacedzonebehaviour_GOTO_START, false);

			runningLineSubLoc.m_start = localCtx.CurrentPos();

			continue;
		}

		if (localCtx.HeadChar() == '\r')
		{
			runningLineSubLoc.m_end = localCtx.CurrentPos();
			localCtx.m_includedScopeInfo.AddLineSubLoc(runningLineSubLoc);
			++localCtx.m_includedScopeInfo.iLinesCount;

			if (localCtx.HeadCharAtDistance(1) != '\n')
				CTX_ERROR(localCtx, "Unsupported single ControlReturn without following LineReturn symbol");

			localCtx.AdvanceHead();
			localCtx.AdvanceHead();

			runningLineSubLoc.m_start = localCtx.CurrentPos();

			continue;
		}

		if (localCtx.HeadChar() == '\n')
		{
			runningLineSubLoc.m_end = localCtx.CurrentPos();
			localCtx.m_includedScopeInfo.AddLineSubLoc(runningLineSubLoc);
			++localCtx.m_includedScopeInfo.iLinesCount;

			localCtx.AdvanceHead();

			runningLineSubLoc.m_start = localCtx.CurrentPos();

			continue;
		}

		localCtx.AdvanceHead();
	}

	localCtx.SetHead(0);
}

bool advanceToLineReturnExpectHSpaceOnly(StrCTX& ctx)
{
	char c;
	while (!ctx.IsEnded() && ((c = LookChar(ctx)) != '\n'))
	{
		if (!isHSpace(c))
		{
			CTX_ERROR(ctx, "Found non space character '%s' while expecting only spaces", c);
			return false; // Encountered non hspace character
		}
		ConsumeChar(ctx);
	}

	if (ctx.IsEnded())
	{
		CTX_ERROR(ctx, "Unexpected End of Line");
		return false; // Unexpected EOL
	}

	return true;
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
		CTX_ERROR(ctx, "Not a valid identifier, wrong first character '%c'", c);
		return false;
	}

	while (!ctx.IsEnded() && LUT_validIdBodyChar[(c = LookChar(ctx))])
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
		ctx.AdvanceHead();
	}

	if (ctx.IsEnded())
	{
		isEOS = true;
		return true; // Not exactly an error, just EOS
	}

	if (LUT_validIdBodyChar[*(ctx.m_head - 1)])
	{
		CTX_ERROR(ctx, "Found identifier with start not correctly separated");
		return false; // Found identifier with start not correctly separated
	}

	StrSizeT idStartPos = ctx.CurrentPos();

	ctx.AdvanceHead();
	while (!ctx.IsEnded() && LUT_validIdBodyChar[ctx.HeadChar()])
	{
		ctx.AdvanceHead();
	}

	outLocation.m_start = idStartPos;
	outLocation.m_end = ctx.CurrentPos();

	return true;
}

bool retrieveDirectiveType(StrCTX& ctx, EDirective& outEDirective, StrSizeT& outDirectiveStartPos)
{
	if (!ctx.m_bInLineStart)
	{
		CTX_ERROR(ctx, "Character # can only happen at start of line content");
	}

	StrSizeT directiveStartPos = ctx.CurrentPos();

	DOGE_DEBUG_ASSERT(ctx.HeadChar() == '#');

	ctx.AdvanceHead();

	std::string strId;
	if (!retrieveIdentifier(ctx, strId))
	{
		CTX_ERROR(ctx, "Could not retrieve identifier");
	}

	EDirective eDirective = szNameToEnum<EDirective>(strId.c_str());
	if (eDirective == e_directive_UNKNOWN)
	{
		CTX_ERROR(ctx, "Unknown directive #%s", strId.c_str());
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
		CTX_ERROR(ctx, "Unexpected End of Line");
		return false;
	}

	ConsumeChar(ctx);

	return true;
}

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

SubStrLoc GetSubLocUntilChar(StrCTX& ctx, char c, bool bKeepDelimiter, uint32_t ignoreField)
{
	SubStrLoc subLocUntil;
	subLocUntil.m_start = ctx.CurrentPos();

	while (LookChar(ctx, ignoreField) != c)
	{
		ConsumeChar(ctx, ignoreField);
	}

	if (bKeepDelimiter)
	{
		ConsumeChar(ctx, ignoreField);
	}

	subLocUntil.m_end = ctx.CurrentPos();

	ctx.SetHead(subLocUntil.m_start);

	return subLocUntil;
}

SubStrLoc GetSubLocUntilSz(StrCTX& ctx, const char* sz, bool bKeepDelimiter, uint32_t ignoreField)
{
	SubStrLoc subLocUntil;
	subLocUntil.m_start = ctx.CurrentPos();

	StrSizeT szLen = strlen(sz);
	while (!szEqN(ctx.HeadCStr(), sz, szLen))
	{
		ConsumeChar(ctx, ignoreField);
	}

	if (bKeepDelimiter)
	{
		ctx.SetHead(ctx.CurrentPos() + szLen);
	}

	subLocUntil.m_end = ctx.CurrentPos();

	ctx.SetHead(subLocUntil.m_start);

	return subLocUntil;
}

//
// This method might seem redundant when pick next char already auto skipping line breaks, but it allows for a faster string building (no more char by char)
bool retrieveMultiLinePPContent(StrCTX& ctx, std::string& strMultiLineContent, bool bKeepEndOfLine = true)
{
	//SubStrLoc mlContentSubLoc = GetSubLocUntilChar(ctx, '\n', bKeepEndOfLine, 0);

	strMultiLineContent.clear();

	while (!ctx.IsEnded() && LookChar(ctx) != '\n')
	{
		strMultiLineContent += ConsumeChar(ctx);
	}

	if (ctx.IsEnded())
	{
		CTX_ERROR(ctx, "Unexpected End of Line");
		return false;
	}

	if (bKeepEndOfLine)
	{
		strMultiLineContent += ConsumeChar(ctx);
	}

	return true;
}

bool handleDirectiveContent_Include(StrCTX& ctx, StrSizeT directiveStartPos)
{
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
		CTX_ERROR(ctx, "#include directive opening delimiter for included filename was not found");
		return false;
	}

	std::string strEnclosed;
	if (!getCharEnclosedString(ctx, enclosing_c_open, enclosing_c_close, strEnclosed))
	{
		CTX_ERROR(ctx, "Could not retrieve the included filename between opening %c and closing %c", enclosing_c_open, enclosing_c_close);
		return false;
	}

	std::string strIncludedFile;
	if (!readFileToString(strEnclosed.c_str(), strIncludedFile))
	{
		CTX_ERROR(ctx, "Could not read included file '%s'", strEnclosed.c_str());
		return false;
	}

	StrCTX includedCtx(std::move(strIncludedFile), strEnclosed);
	CleanupAndCheckStrBeforeInclude(includedCtx);

	preprocess_file_string(includedCtx);

	ctx.AddIncludedContext(includedCtx, directiveStartPos);
	ctx.m_includedScopeInfo.AddToSons(includedCtx.m_includedScopeInfo, directiveStartPos);

	SubStrLoc replacedByIncludeZone{ directiveStartPos, ctx.CurrentPos() };
	ctx.SetHead(directiveStartPos);
	ctx.m_bInLineStart = true; // Start of file means start of line

	ctx.Erase(replacedByIncludeZone, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);

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

bool decomposeStringCleanHSpacesIgnoreSpaceEnclosed(const std::string& strIn, char sep, std::vector<std::string>& vOutStrDecomposed, const char** outSzErrorMsg = NULL)
{
	StrSizeT sepPos;

	SubStrLoc sepSubLoc;
	sepSubLoc.m_start = 0;

	vOutStrDecomposed.clear();

	if (strIn.empty())
	{
		return true; // Empty strings means 0 decomposed str
	}

	const char* sz = strIn.c_str();

	int pileCount = 0;

	while (*sz)
	{
		if (*sz == '(')
		{
			++pileCount;
		}
		else if (*sz == ')')
		{
			--pileCount;
			//DOGE_ASSERT(pileCount >= 0);
			if (pileCount < 0)
			{
				if (outSzErrorMsg)
				{
					sprintf(g_sprintf_buffer, "Incorrect parenthesis enclosing, reached final closing to soon");
					*outSzErrorMsg = g_sprintf_buffer;
				}
				return false;
			}
		}
		else if (*sz == ',' && pileCount == 0)
		{
			sepPos = sz - strIn.c_str();

			sepSubLoc.m_end = sepPos;
			adjustSubStrLoc_CleanExtHSpaces(strIn, sepSubLoc);

			vOutStrDecomposed.push_back(sepSubLoc.ExtractSubStr(strIn));
			sepSubLoc.m_start = sepPos + 1;
		}

		++sz;
	}

	if (pileCount > 0)
	{
		if (outSzErrorMsg)
		{
			sprintf(g_sprintf_buffer, "Incorrect parenthesis enclosing, did not reach final closing");
			*outSzErrorMsg = g_sprintf_buffer;
		}
		return false;
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
		CTX_ERROR(ctx, "Function macro '%s' args declaration needs opening parenthesis", outFnMacro.m_name.c_str());
		return false; // Args declaration needs opening parenthesis
	}

	std::string strMacroArgs;
	if (!getCharEnclosedString(ctx, '(', ')', strMacroArgs))
	{
		CTX_ERROR(ctx, "Function macro '%s' args declaration not correctly terminated", outFnMacro.m_name.c_str());
		return false; // Args declaration not correctly terminated
	}

	std::vector<std::string> vStrArgs;
	decomposeStringCleanHSpaces(strMacroArgs, ',', vStrArgs);

	if (std::any_of(vStrArgs.cbegin(), vStrArgs.cend(), [](const std::string& str) { return str.empty(); }))
	{
		CTX_ERROR(ctx, "Empty parameter found in function macro '%s' declaration", outFnMacro.m_name.c_str());
		return false; // Empty parameter
	}

	for (VEC_SIZE_T(vStrArgs) nArg = 0; nArg < vStrArgs.size(); ++nArg)
	{ 
		const auto& strArg = vStrArgs[nArg];
		decltype(vStrArgs.cbegin()) dupIt;
		if ((dupIt = std::find(vStrArgs.cbegin() + nArg + 1, vStrArgs.cend(), strArg)) != vStrArgs.cend())
		{
			CTX_ERROR(ctx, "Duplicate parameter '%s' in function macro '%s' declaration", dupIt->c_str(), outFnMacro.m_name.c_str());
			return false; // Duplicate parameter
		}
	}

	outFnMacro.m_args.clear();
	outFnMacro.m_args.resize(vStrArgs.size());

	std::string strDefineContent;
	if (!retrieveMultiLinePPContent(ctx, strDefineContent, false))
	{
		return false;
	}

	if (vStrArgs.size() == 0) // No need to build the macro args
	{
		outFnMacro.m_strContent = strDefineContent;
		return true;
	}

	SubStrLoc idLoc;
	StrCTX defineContentCtx{ std::move(std::string(strDefineContent)), "" };
	bool isEOS = false;

	while (!isEOS)
	{
		if (!getNextIdentifierLocation(defineContentCtx, idLoc, isEOS))
		{
			CTX_ERROR(ctx, "Error retrieving next identifier");
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

		gMacroInsert(strDefine, std::move(macro));

		ctx.Erase({ directiveStartPos, ctx.CurrentPos() }, StrCTX::e_insidereplacedzonebehaviour_GOTO_END);

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
			if (!retrieveMultiLinePPContent(ctx, macro.m_strContent, false))
			{
				return false;
			}
		}

		gMacroInsert(strDefine, std::move(macro));

		ctx.Erase({ directiveStartPos, ctx.CurrentPos() }, StrCTX::e_insidereplacedzonebehaviour_GOTO_END);

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

	ctx.Erase({ directiveStartPos, ctx.CurrentPos() }, StrCTX::e_insidereplacedzonebehaviour_GOTO_END);

	return true;
}

bool handleDirectiveContent_Error(StrCTX& ctx, StrSizeT directiveStartPos)
{
	skipHSpaces(ctx);

	std::string strErrorMessage;

	std::string strMultilineContent;
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent, false))
	{
		return false;
	}

	CTX_ERROR(ctx, "Triggered #error directive : %s", strMultilineContent.c_str());

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
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent, false))
	{
		return false;
	}

	ctx.Erase({ directiveStartPos, ctx.CurrentPos() }, StrCTX::e_insidereplacedzonebehaviour_GOTO_END);

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
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent, false))
	{
		return false;
	}

	if (strMultilineContent == "(FXAA_GLSL_120 == 1)")
	{
		int breakpoint = -1;
	}

	preprocess_if_directive_string(strMultilineContent);

	std::unique_ptr<const Expression> expression = EvaluateStringExpression(strMultilineContent);
	bool bYieldedValue = expression->Evaluate() ? true : false;

	std::string strDebugExpression = expression->GetStringExpression();

	bool bMustSkip;
	ctx.m_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

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

	if (!advanceToLineReturnExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after tested #ifdef identifier
	}

	bool bMustSkip;
	ctx.m_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);
	
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

	if (!advanceToLineReturnExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after tested #ifdef identifier
	}

	bool bMustSkip;
	ctx.m_ifCtx.AddIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

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
	if (!retrieveMultiLinePPContent(ctx, strMultilineContent, false))
	{
		return false;
	}

	preprocess_if_directive_string(strMultilineContent);

	std::unique_ptr<const Expression> expression = EvaluateStringExpression(strMultilineContent);
	bool bYieldedValue = expression->Evaluate() ? true : false;

	std::string strDebugExpression = expression->GetStringExpression();

	bool bMustSkip;
	ctx.m_ifCtx.UpdateElseIf(ctx, bYieldedValue, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Else(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!advanceToLineReturnExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after #else
	}

	bool bMustSkip;
	ctx.m_ifCtx.UpdateElse(ctx, directiveStartPos, ctx.CurrentPos(), bMustSkip);

	if (bMustSkip)
	{
		return advanceToNextConditionDirective(ctx);
	}

	return true;
}

bool handleDirectiveContent_Endif(StrCTX& ctx, StrSizeT directiveStartPos)
{
	if (!advanceToLineReturnExpectHSpaceOnly(ctx))
	{
		return false; // Garbage after #else
	}

	bool bMustSkip;
	ctx.m_ifCtx.UpdateEndif(ctx, directiveStartPos, ctx.CurrentPos(), bMustSkip);

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
	case e_directive_error:
		return handleDirectiveContent_Error(ctx, directiveStartPos);
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

void preprocess_file_string(StrCTX& ctx, bool* bPerformedMacroSubstitution)
{
	if (bPerformedMacroSubstitution) *bPerformedMacroSubstitution = false;

	while (!ctx.IsEnded())
	{
		char c = LookChar(ctx);
		if (isHSpace(c))
		{
			ConsumeChar(ctx);
		}
		else if (c == '$')
		{
			LineInfo lineInfo = ctx.m_includedScopeInfo.GetCharPosLineInfo(ctx.CurrentPos());
			printf("$ encountered at line %d char pos %d \n", 1 + lineInfo.line, 1 + lineInfo.charPosInLine);
			ConsumeChar(ctx);
		}
		else if (c == '\n')
		{
			//printf("Line return at line %d.\n", ctx.lineNumber);

			ConsumeChar(ctx);

			++ctx.m_lineNumber;
		}
		else if (c == '#') {
			StrSizeT directiveStartPos;
			EDirective eDirective;
			if (!retrieveDirectiveType(ctx, eDirective, directiveStartPos))
			{
				CTX_ERROR(ctx, "Error retrieving directive type");
			}

			if (!handleDirectiveContent(ctx, eDirective, directiveStartPos))
			{
				CTX_ERROR(ctx, "Invalid directive content");
			}
		}
		else if (isLiteralDelimiterChar(c))
		{
			ConsumeChar(ctx);
			if (!advanceToEndOfLiteral(ctx, c))
			{
				CTX_ERROR(ctx, "Invalid literal termination");
			}
		}
		else if (LUT_validIdFirstChar[c])
		{
			std::string strId;
			SubStrLoc idSubLoc;
			idSubLoc.m_start = ctx.CurrentPos();
			if (!retrieveIdentifier(ctx, strId))
			{
				CTX_ERROR(ctx, "Could not retrieve identifier");
			}
			g_encounteredIdentifiers.push_back(strId);
			idSubLoc.m_end = ctx.CurrentPos();

			if (gMacroIsDefined(strId))
			{
				const Macro& macro = gMacroGet(strId);

				if (bPerformedMacroSubstitution) *bPerformedMacroSubstitution = true;

				if (macro.m_eType == e_macrotype_definition) // Todo : support functions macro
				{
					std::string strMacroContent = macro.m_strContent;

					bool bNeedsOneMorePass;
					StrCTX macroContentCtx(std::move(strMacroContent), "");

					do 
					{
						preprocess_file_string(macroContentCtx, &bNeedsOneMorePass);
						macroContentCtx.ResetHead();
					} while (bNeedsOneMorePass);

					ctx.SetHead(idSubLoc.m_start);
					ctx.Replace(idSubLoc, macroContentCtx.m_baseStr, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
				}
				else
				{
					std::string strMacroArgs;

					skipHSpaces(ctx);

					SubStrLoc macroArgsSubLoc = GetMultiScopeCharEnclosedSubLoc(ctx, '(', ')', false);
					strMacroArgs = macroArgsSubLoc.ExtractSubStr(ctx.m_baseStr);

					std::vector<std::string> vStrArgs;
					const char* szErrorMsg;
					if (!decomposeStringCleanHSpacesIgnoreSpaceEnclosed(strMacroArgs, ',', vStrArgs, &szErrorMsg))
					{
						CTX_ERROR(ctx, "In parsing function macro '%s' call args before expansion : %s", macro.m_name.c_str(), szErrorMsg);
					}

					std::string strFnMacroPatchedContent;
					if (vStrArgs.size() != macro.m_args.size())
					{
						CTX_ERROR(ctx, "Function Macro '%s' expects %d parameters (given %d : '%s')", 
							macro.m_name.c_str(), macro.m_args.size(), vStrArgs.size(), strMacroArgs.c_str());
					}
					macro.PatchString(vStrArgs, strFnMacroPatchedContent);

					ctx.SetHead(macroArgsSubLoc.m_end);
					ConsumeCharExpected(ctx, ')');

					idSubLoc.m_end = ctx.CurrentPos();

					bool bNeedsOneMorePass;
					StrCTX fnMacroContentCtx(std::move(strFnMacroPatchedContent), "");

					do
					{
						preprocess_file_string(fnMacroContentCtx, &bNeedsOneMorePass);
						fnMacroContentCtx.ResetHead();
					} while (bNeedsOneMorePass);

					ctx.SetHead(idSubLoc.m_start);
					ctx.Replace(idSubLoc, fnMacroContentCtx.m_baseStr, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
				}
			}
		}
		else
		{
			ConsumeChar(ctx);
		}
	}
}

void preprocess_if_directive_string(std::string& strBuffer, bool* bPerformedMacroSubstitution)
{
	if (bPerformedMacroSubstitution) *bPerformedMacroSubstitution = false;

	StrCTX ctx{ std::move(strBuffer), "" };

	while (!ctx.IsEnded())
	{
		char c = LookChar(ctx);
		if (isHSpace(c))
		{
			ConsumeChar(ctx);
		}
		else if (c == '\n')
		{
			//printf("Line return at line %d.\n", ctx.lineNumber);

			ConsumeChar(ctx);

			++ctx.m_lineNumber;
		}
		else if (isLiteralDelimiterChar(c))
		{
			ConsumeChar(ctx);
			DOGE_ASSERT_MESSAGE(advanceToEndOfLiteral(ctx, c), "Error at line %d. Invalid literal termination.\n", ctx.m_lineNumber);
		}
		else if (LUT_validIdFirstChar[c])
		{
			std::string strId;
			SubStrLoc idSubLoc;
			idSubLoc.m_start = ctx.CurrentPos();
			DOGE_ASSERT_MESSAGE(retrieveIdentifier(ctx, strId), "Error at line %d. Could not retrieve identifier.\n", ctx.m_lineNumber);
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

					if (bPerformedMacroSubstitution) *bPerformedMacroSubstitution = true;

					if (macro.m_eType == e_macrotype_definition) // Todo : support functions macro
					{
						std::string strMacroContent = macro.m_strContent;

						bool bNeedsOneMorePass;
						do
						{
							preprocess_if_directive_string(strMacroContent, &bNeedsOneMorePass);
						} while (bNeedsOneMorePass);

						ctx.SetHead(idSubLoc.m_start);
						ctx.Replace(idSubLoc, strMacroContent, StrCTX::e_insidereplacedzonebehaviour_GOTO_START);
					}
					else
					{
						std::string strMacroArgs;

						skipHSpaces(ctx);

						SubStrLoc macroArgsSubLoc = GetMultiScopeCharEnclosedSubLoc(ctx, '(', ')', false);
						strMacroArgs = macroArgsSubLoc.ExtractSubStr(ctx.m_baseStr);

						std::vector<std::string> vStrArgs;
						const char* szErrorMsg;
						if (!decomposeStringCleanHSpacesIgnoreSpaceEnclosed(strMacroArgs, ',', vStrArgs, &szErrorMsg))
						{
							CTX_ERROR(ctx, "In parsing function macro '%s' call args before expansion : %s", macro.m_name.c_str(), szErrorMsg);
						}

						std::string strFnMacroPatchedContent;
						macro.PatchString(vStrArgs, strFnMacroPatchedContent);

						ctx.SetHead(macroArgsSubLoc.m_end);
						ConsumeCharExpected(ctx, ')');

						idSubLoc.m_end = ctx.CurrentPos();

						bool bNeedsOneMorePass;
						do
						{
							preprocess_if_directive_string(strFnMacroPatchedContent, &bNeedsOneMorePass);
						} while (bNeedsOneMorePass);

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

	strBuffer = std::move(ctx.m_baseStr);
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
	StrCTX expStrCtx(std::move(strExp), "");

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

			Token numberToken{ ETokenType::NUMBER, fullNumberSubLoc.ExtractSubStr(expStrCtx.m_baseStr) };
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
				Token nameToken{ ETokenType::NAME, idSubLoc.ExtractSubStr(expStrCtx.m_baseStr) };
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
	DOGE_ASSERT(argc == 3);
	DOGE_ASSERT(!szEq(argv[1], argv[2]));

	std::string strFile;
	DOGE_ASSERT(readFileToString(argv[1], strFile));

	StrCTX mainFileStrCtx(std::move(strFile), argv[1]);
	CleanupAndCheckStrBeforeInclude(mainFileStrCtx);
	preprocess_file_string(mainFileStrCtx);

	mainFileStrCtx.PatchWithIncludedContexts();

	//std::cout << "===================== BOF =====================\n";
	//std::cout << strFile;
	//std::cout << "===================== EOF =====================\n\n";

	//std::cout << "Global macros : \n";

	//for (const auto& globalMacro : g_macros)
	//{
	//	std::cout << "\t- " << globalMacro.second.m_name << '\n';
	//}

	//coutlr;

	//for (const auto& globalStrId : g_encounteredIdentifiers)
	//{
	//	//std::cout << "\t~ " << globalStrId << '\n';
	//}

	//coutlr;

	writeStringToFile(argv[2], mainFileStrCtx.m_baseStr);

	return 0;
}

