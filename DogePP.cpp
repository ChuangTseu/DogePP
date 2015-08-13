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
#include <cassert>
#include <memory>
#include <streambuf>
#include <algorithm>

#include "EnumCTTI.h"

#include "PreProcessorUtils.h"
#include "Config.h"
#include "StringUtils.h"
#include "Macro.h"
#include "FileUtils.h"
#include "StringContext.h"
#include "CharUtils.h"
#include "Match.h"

#define VEC_SIZE_T(vec) decltype(vec.size())

#define X_LIST_OF_Directive \
	X(include) \
	X(define) \
	X(pragma) \
	X(ifdef) \
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
	assert(eDirective >= 0 && eDirective < e_directive_COUNT);
	return LUT_EDirective_szName[eDirective];
}

DECLARE_ENUM_TEMPLATE_CTTI(EDirective)

void preprocess_string(std::string& strBuffer);

void insertString(StrCTX& ctx, StrCItT itInsertPoint, const std::string& strInserted)
{
	StrSizeT insertPos = ctx.ItPos(itInsertPoint);
	ctx.baseStr.insert(insertPos, strInserted);

	ctx.SetHead(insertPos + strInserted.length());
}

void replaceWithString(StrCTX& ctx, StrCItT itReplacePoint, size_t replacedLength,
	const std::string& strInserted)
{
	StrSizeT insertPos = ctx.ItPos(itReplacePoint);
	ctx.baseStr.replace(insertPos, replacedLength, strInserted);

	ctx.SetHead(insertPos + strInserted.length());
}

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

bool skipEntireLine(StrCTX& ctx)
{
	while (!ctx.IsEnded() && !isLineReturn(ctx.HeadCStr()))
	{
		*(*(std::string::iterator*)&ctx.head) = '-';
		ctx.Advance(1);
	}

	if (ctx.IsEnded())
	{
		return false; // No EOL found
	}

	ctx.Advance(LR_LENGTH);

	return true;
}

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
			fprintf(stderr, "Error at line %d char %d col %d. Could not correctly skip commented section (multiline comment).\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber);
			exit(EXIT_FAILURE);
		}

		return skipCtxHeadToNextValid(ctx);
	}

	if (isSingleLineCommentStart(ctx.HeadCStr()))
	{
		ctx.Advance(COMMENT_SL_LENGTH);
		if (!skipEntireLine(ctx))
		{
			fprintf(stderr, "Error at line %d char %d col %d. Could not correctly skip commented line (singleline comment).\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber);
			exit(EXIT_FAILURE);
		}

		return skipCtxHeadToNextValid(ctx);
	}

	if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1)) // Skip line break
	{
		ctx.Advance(1 + LR_LENGTH);

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

char pickNextChar(StrCTX& ctx)
{
	char c;
	assert(skipCtxHeadToNextValid(ctx));
	return ctx.HeadChar();
	return c;
}

void skipHSpaces(StrCTX& ctx)
{
	while (isHSpace(pickNextChar(ctx)))
	{
		ctx.Advance(1);
	}
}

bool retrieveIdentifier(StrCTX& ctx, std::string& outStrId)
{
	outStrId.clear();

	char c = pickNextChar(ctx);
	if (LUT_validIdFirstChar[c])
	{
		outStrId += c;
		ctx.Advance(1);
	}
	else
	{
		return false;
	}

	while (LUT_validIdBodyChar[(c = pickNextChar(ctx))])
	{
		outStrId += c;
		ctx.Advance(1);
	}

	return true;
}

bool getNextIdentifierLocation(StrCTX& ctx, SubStrLoc& outLocation, bool& isEOS)
{
	isEOS = false;

	while (!ctx.ItIsEnd(ctx.head) && !LUT_validIdFirstChar[*ctx.head])
	{
		++ctx.head;
	}

	if (ctx.ItIsEnd(ctx.head))
	{
		isEOS = true;
		return true; // Not exactly an error, just EOS
	}

	if (LUT_validIdBodyChar[*(ctx.head - 1)])
	{
		return false; // Found identifier with start not correctly separated
	}

	StrCItT itStart = ctx.head;
	StrCItT itEnd;

	++ctx.head;
	while (LUT_validIdBodyChar[*ctx.head])
	{
		++ctx.head;
	}

	itEnd = ctx.head;

	outLocation.m_start = ctx.ItPos(itStart);
	outLocation.m_end = ctx.ItPos(itEnd);

	return true;
}

bool getCharEnclosedString(StrCTX& ctx, char c_open, char c_close,
	std::string& strOutEnclosed)
{
	if (pickNextChar(ctx) != c_open)
	{
		return false;
	}

	ctx.Advance(1);

	strOutEnclosed.clear();

	char c;
	while (!ctx.IsEnded() && ((c = pickNextChar(ctx)) != c_close))
	{
		strOutEnclosed += c;
		ctx.Advance(1);
	}

	if (ctx.IsEnded())
	{
		return false;
	}

	ctx.Advance(1);

	return true;
}

//
// This method might seem redundant when pick next char already auto skip line breaks, but it allows for a faster string building (no more char by char)
bool retrieveMultiLinePPContent(StrCTX& ctx, std::string& strMultiLineContent)
{
	StrCItT itMultiLineContentPartStart = ctx.head;
	StrCItT itMultiLineContentPartEnd;

	bool bLineBreakFound = false;

	strMultiLineContent.clear();

	while (!ctx.IsEnded() && !isLineReturn(ctx.HeadCStr()))
	{
		if (ctx.HeadChar() == '\\' && isLineReturn(ctx.HeadCStr() + 1)) // Skip line break
		{
			itMultiLineContentPartEnd = ctx.head - 1;
			strMultiLineContent.append(itMultiLineContentPartStart, itMultiLineContentPartEnd);

			ctx.Advance(1 + LR_LENGTH);
			itMultiLineContentPartStart = ctx.head;
		}
		else
		{
			assert(skipCtxHeadToNextValid(ctx));
			ctx.Advance(1);
		}
	}

	itMultiLineContentPartEnd = ctx.head;
	strMultiLineContent.append(itMultiLineContentPartStart, itMultiLineContentPartEnd);

	return true;
}

bool handleDirectiveContent_Include(StrCTX& ctx)
{
	StrCItT itIncludeInsertPoint = ctx.head - strlen("#include");

	if (!isHSpace(pickNextChar(ctx)))
	{
		return false;
	}

	skipHSpaces(ctx);

	char enclosing_c_open;
	char enclosing_c_close;

	bool bIsSytemInclude;

	if (pickNextChar(ctx) == '"')
	{
		enclosing_c_open = '"';
		enclosing_c_close = '"';
		bIsSytemInclude = false;
	}
	else if (pickNextChar(ctx) == '<')
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

	preprocess_string(strIncludedFile);

	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), strIncludedFile);

	return true;
}

bool decomposeString(const std::string& strIn, char sep, std::vector<std::string>& vOutStrDecomposed)
{
	StrSizeT oldAfterSepPos = 0;
	StrSizeT sepPos;
	
	vOutStrDecomposed.clear();

	while ((sepPos = strIn.find(sep, oldAfterSepPos)) != std::string::npos)
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

	while ((sepPos = strIn.find(sep, sepSubLoc.m_start)) != std::string::npos)
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
	if (!(pickNextChar(ctx) == '('))
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

bool handleDirectiveContent_Define(StrCTX& ctx)
{
	StrCItT itIncludeInsertPoint = ctx.head - strlen("#define");

	if (!isHSpace(pickNextChar(ctx)))
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

	char c = pickNextChar(ctx);
	if (c == '(')
	{
		macro.m_eType = e_macrotype_function;

		if (!parseFunctionMacroDefinition(ctx, macro))
		{
			return false;
		}

		sprintf(g_sprintf_buffer, "<<< #define fn : '%s', args number : '%d', content : '%s' >>>", strDefine.c_str(), macro.m_args.size(), macro.m_strContent.c_str());
		replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

		gMacroInsert(strDefine, std::move(macro));

		return true;
	}
	else if (isHSpace(c) || c == '\n')
	{
		macro.m_eType = e_macrotype_definition;

		skipHSpaces(ctx);

		if (pickNextChar(ctx) == '\n')
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

		sprintf(g_sprintf_buffer, "<<< #define : '%s', content : '%s' >>>", strDefine.c_str(), macro.m_strContent.c_str());
		replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

		gMacroInsert(strDefine, std::move(macro));

		return true;
	}
	else
	{
		return false; // Unauthorized character after defined identifier
	}
}

bool handleDirectiveContent_Pragma(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent_If(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent_Elif(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent_Ifdef(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent_Else(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent_Endif(StrCTX& ctx)
{
	printf("handleDirectiveContent_Pragma()\n");

	StrCItT itIncludeInsertPoint = ctx.head - strlen("#pragma");

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

	sprintf(g_sprintf_buffer, "<<< #pragma : '%s', content : '%s' >>>", strPragmaId.c_str(), strMultilineContent.c_str());
	replaceWithString(ctx, itIncludeInsertPoint, std::distance(itIncludeInsertPoint, ctx.head), std::string(g_sprintf_buffer));

	return true;
}

bool handleDirectiveContent(StrCTX& ctx, EDirective eDirective)
{
	switch (eDirective)
	{
	case e_directive_include:
		return handleDirectiveContent_Include(ctx);
	case e_directive_define:
		return handleDirectiveContent_Define(ctx);
	case e_directive_pragma:
		return handleDirectiveContent_Pragma(ctx);
	case e_directive_if:
		return handleDirectiveContent_If(ctx);
	case e_directive_ifdef:
		return handleDirectiveContent_Ifdef(ctx);
	case e_directive_elif:
		return handleDirectiveContent_Elif(ctx);
	case e_directive_else:
		return handleDirectiveContent_Else(ctx);
	case e_directive_endif:
		return handleDirectiveContent_Endif(ctx);
	default:
		return false;
	}

	return true;
}

void preprocess_string(std::string& strBuffer)
{
	StrCTX ctx{ strBuffer, strBuffer.cbegin() };

	while (!ctx.IsEnded())
	{
		assert(skipCtxHeadToNextValid(ctx));
		if (pickNextChar(ctx) == ' ')
		{
			ctx.Advance(1);

			++ctx.charNumber;
			++ctx.colNumber;
		}
		else if (pickNextChar(ctx) == '\t')
		{
			ctx.Advance(1);

			++ctx.charNumber;
			ctx.colNumber += 4;
		}
		else if (pickNextChar(ctx) == '\n')
		{
			printf("Line return at line %d.\n", ctx.lineNumber);

			ctx.Advance(1);

			++ctx.lineNumber;
			ctx.bInLineStart = true;

			ctx.charNumber = 1;
			ctx.colNumber = 1;
		}
		else if (pickNextChar(ctx) == '#') {
			if (!ctx.bInLineStart)
			{
				fprintf(stderr, "Error at line %d char %d col %d. Character # can only happen at start of line content.\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber);
				exit(EXIT_FAILURE);
			}

			ctx.Advance(1);

			ctx.bInLineStart = false;

			++ctx.charNumber;
			++ctx.colNumber;

			std::string strId;
			if (!retrieveIdentifier(ctx, strId))
			{
				fprintf(stderr, "Error at line %d char %d col %d. Invalid identifier.\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber);
				exit(EXIT_FAILURE);
			}

			ctx.charNumber += strId.length();
			ctx.colNumber += strId.length();

			printf("Found # identifier #%s.\n", strId.c_str());

			EDirective eDirective = szNameToEnum<EDirective>(strId.c_str());
			if (eDirective == e_directive_UNKNOWN)
			{
				fprintf(stderr, "Error at line %d char %d col %d. Unknown directive #%s.\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber, strId.c_str());
				exit(EXIT_FAILURE);
			}

			if (!handleDirectiveContent(ctx, eDirective))
			{
				fprintf(stderr, "Error at line %d char %d col %d. Invalid directive content.\n", ctx.lineNumber, ctx.charNumber, ctx.colNumber);
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			ctx.Advance(1);

			ctx.bInLineStart = false;

			++ctx.charNumber;
			++ctx.colNumber;
		}
	}
}

struct CondiTestInfo {
	EDirective eCondDir;
	StrSizeT linePos;
	bool bYields;
};

/*
#if BIGGER_PICTURE
	#if N == 1
	#elif N == 2
		#ifdef GO_DEEP
		#else
		#endif
	#elif N == 3
	#else
	#endif
#endif
*/

std::vector<CondiTestInfo> condiTestData {
	{ e_directive_if, 12, true },
	{ e_directive_if, 14, false },
	{ e_directive_elif, 17, true },
	{ e_directive_ifdef, 19, false },
	{ e_directive_else, 21, {} },
	{ e_directive_endif, 23, {} },
	{ e_directive_elif, 25, false },
	{ e_directive_else, 28, {} },
	{ e_directive_endif, 31, {} },
	{ e_directive_endif, 33, {} },
};

const char* tabIndentStr(int level)
{
	switch (level)
	{
	case 0: return "";
	case 1: return "\t";
	case 2: return "\t\t";
	case 3: return "\t\t\t";
	case 4: return "\t\t\t\t";
	case 5: return "\t\t\t\t\t";
	case 6: return "\t\t\t\t\t\t";
	case 7: return "\t\t\t\t\t\t\t";
	case 8: return "\t\t\t\t\t\t\t\t";

	default: assert(false); return "";
	}
}

int main(int argc, char* argv[])
{
	//std::string strFile;
	//assert(readFileToString("testfile.txt", strFile));
	//preprocess_string(strFile);

	//std::cout << "===================== BOF =====================\n";
	//std::cout << strFile;
	//std::cout << "===================== EOF =====================\n\n";

	//std::cout << "Global macros : \n";

	//for (const auto& globalMacro : g_macros)
	//{
	//	std::cout << "\t-" << globalMacro.second.m_name << '\n';
	//}

	std::string strExclDirTest;

	for (int i = 0; i < 40; ++i)
	{
		strExclDirTest += std::to_string(i);
		strExclDirTest += '\n';
	}

	IfCtx ifCtx;

	for (size_t nCond = 0; nCond < condiTestData.size(); ++nCond)
	{
		const CondiTestInfo& currentCond = condiTestData[nCond];

		switch (currentCond.eCondDir)
		{

		case e_directive_if:
		{
			RunningIfInfo runningIfInfo;
			runningIfInfo.bElseIsDone = false;
			runningIfInfo.bTrueIsDone = currentCond.bYields;
			runningIfInfo.start_replaced_zone = currentCond.linePos;

			runningIfInfo.level = ifCtx.runningIfStack.size();

			printf("%s#if at line %d yields %s\n", tabIndentStr(runningIfInfo.level), 
				currentCond.linePos, currentCond.bYields ? "'true'" : "'false'");

			if (currentCond.bYields)
			{
				runningIfInfo.start_remaining_zone = currentCond.linePos + 1;
				runningIfInfo.bExpectingEndRemainingLineInfo = true;
			}

			ifCtx.runningIfStack.push(runningIfInfo);
		}

			break;

		case e_directive_ifdef:
		{
			RunningIfInfo runningIfInfo;
			runningIfInfo.bElseIsDone = false;
			runningIfInfo.bTrueIsDone = currentCond.bYields;
			runningIfInfo.start_replaced_zone = currentCond.linePos;

			runningIfInfo.level = ifCtx.runningIfStack.size();

			printf("%s#ifdef at line %d yields %s\n", tabIndentStr(runningIfInfo.level), 
				currentCond.linePos, currentCond.bYields ? "'true'" : "'false'");

			if (currentCond.bYields)
			{
				runningIfInfo.start_remaining_zone = currentCond.linePos + 1;
				runningIfInfo.bExpectingEndRemainingLineInfo = true;
			}

			ifCtx.runningIfStack.push(runningIfInfo);
		}

			break;

		case e_directive_elif:
		{
			assert(ifCtx.runningIfStack.size() > 0);

			RunningIfInfo& runningIfInfo = ifCtx.runningIfStack.top();

			assert(!runningIfInfo.bElseIsDone);

			if (runningIfInfo.bTrueIsDone)
			{
				printf("%s#elif at line %d yields %s\n", tabIndentStr(runningIfInfo.level), currentCond.linePos, "'whatever'");

				if (runningIfInfo.bExpectingEndRemainingLineInfo)
				{
					runningIfInfo.end_remaining_zone = currentCond.linePos;
					runningIfInfo.bExpectingEndRemainingLineInfo = false;
				}
				else
				{
					// Nothing
				}
			}
			else
			{
				runningIfInfo.bTrueIsDone = currentCond.bYields;

				printf("%s#elif at line %d yields %s\n", tabIndentStr(runningIfInfo.level), 
					currentCond.linePos, currentCond.bYields ? "'true'" : "'false'");
				
				if (currentCond.bYields)
				{
					runningIfInfo.start_remaining_zone = currentCond.linePos + 1;
					runningIfInfo.bExpectingEndRemainingLineInfo = true;
				}
			}
		}

			break;

		case e_directive_else:
		{
			assert(ifCtx.runningIfStack.size() > 0);

			RunningIfInfo& runningIfInfo = ifCtx.runningIfStack.top();

			assert(!runningIfInfo.bElseIsDone);

			runningIfInfo.bElseIsDone = true;

			if (runningIfInfo.bTrueIsDone)
			{
				printf("%s#else at line %d yields %s\n", tabIndentStr(runningIfInfo.level), currentCond.linePos, "'whatever'");

				if (runningIfInfo.bExpectingEndRemainingLineInfo)
				{
					runningIfInfo.end_remaining_zone = currentCond.linePos;
					runningIfInfo.bExpectingEndRemainingLineInfo = false;
				}
				else
				{
					// Nothing
				}
			}
			else
			{
				runningIfInfo.bTrueIsDone = true; // Mandatory if else is reached while not true_is_set

				printf("%s#else at line %d triggers mandatory %s\n", tabIndentStr(runningIfInfo.level), currentCond.linePos, "'true'");

				runningIfInfo.start_remaining_zone = currentCond.linePos + 1;
				runningIfInfo.bExpectingEndRemainingLineInfo = true;
			}
		}

			break;

		case e_directive_endif:
		{
			assert(ifCtx.runningIfStack.size() > 0);

			RunningIfInfo& runningIfInfo = ifCtx.runningIfStack.top();

			if (runningIfInfo.bTrueIsDone)
			{
				if (runningIfInfo.bExpectingEndRemainingLineInfo)
				{
					runningIfInfo.end_remaining_zone = currentCond.linePos;
					runningIfInfo.bExpectingEndRemainingLineInfo = false;
				}
				else
				{
					// Nothing
				}
			}

			runningIfInfo.end_replaced_zone = currentCond.linePos + 1;

			printf("%s#endif at line %d\n", tabIndentStr(runningIfInfo.level), currentCond.linePos);

			if (runningIfInfo.bTrueIsDone)
			{
				printf("%s---> Replacing zone [%d, %d[ with zone [%d, %d[\n", tabIndentStr(runningIfInfo.level),
					runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone,
					runningIfInfo.start_remaining_zone, runningIfInfo.end_remaining_zone);

				strExclDirTest.replace(
					runningIfInfo.start_replaced_zone * 2, runningIfInfo.end_replaced_zone * 2 - runningIfInfo.start_replaced_zone * 2,
					strExclDirTest.substr(runningIfInfo.start_remaining_zone * 2, runningIfInfo.end_remaining_zone * 2 - runningIfInfo.start_remaining_zone * 2));

				StrSizeT replacedLineCount = runningIfInfo.end_replaced_zone - runningIfInfo.start_replaced_zone;
				
				for (size_t nRemainingCond = nCond + 1; nRemainingCond < condiTestData.size(); ++nRemainingCond)
				{
					assert(condiTestData[nRemainingCond].linePos > runningIfInfo.start_replaced_zone);
					condiTestData[nRemainingCond].linePos -= replacedLineCount;
				}
			}
			else
			{
				printf("%s---> No selected zone.\n", tabIndentStr(runningIfInfo.level));
			}

			printf("\n");

			ifCtx.runningIfStack.pop();
		}

			break;

		default:
			break;
		}
	}

	std::cout << "====================================\n";
	std::cout << strExclDirTest;
	std::cout << "====================================\n";

	return 0;
}

