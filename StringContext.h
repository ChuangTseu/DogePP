#pragma once

#include "StringUtils.h"
#include "Match.h"
#include "ErrorLogging.h"

#include <stack>
#include <vector>

#define IT_AS_CSTR(it) (static_cast<const char*>(&(*(it))))

struct RunningIfInfo {
	bool bTrueIsDone = false;
	bool bElseIsDone = false;

	bool bExpectingEndRemainingLineInfo = false;

	int level;

	StrSizeT start_replaced_zone = npos<StrSizeT>::value;

	StrSizeT start_remaining_zone = npos<StrSizeT>::value;
	StrSizeT end_remaining_zone = npos<StrSizeT>::value;

	StrSizeT end_replaced_zone = npos<StrSizeT>::value;
};

struct StrCTX {
	StrCTX(std::string& capturedString) : baseStr(capturedString), head(baseStr.cbegin()) {}

	StrCTX(std::string& capturedString, StrCItT customHeadIt) : baseStr(capturedString), head(customHeadIt) {}

	std::string& baseStr;
	StrCItT head;

	bool bInLineStart = true;
	int lineNumber = 1;
	int charNumber = 1;
	int colNumber = 1;

	std::vector<SubStrLoc*> vRegisteredSubLocs;
	std::vector<StrSizeT*> vRegisteredPositions;

	void InsertHere(const std::string& str, bool bAdvanceAfterInserted)
	{
		StrSizeT insertPos = CurrentPos();

		baseStr.insert(insertPos, str);

		if (bAdvanceAfterInserted)
		{
			SetHead(insertPos + str.length());
		}
	}

	enum EInsideReplacedZoneBehaviour {
		e_insidereplacedzonebehaviour_GOTO_START, // Place head after the end of the new zone
		e_insidereplacedzonebehaviour_GOTO_END, // Place head to the beginning of the new zone
		e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_STRICT, // Will assert if out of new adapted zone
		e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_CLAMP, // Will behave as GOTO_END if out of new adapted zone
		e_insidereplacedzonebehaviour_ASSERT // Simple assert
	};

	StrSizeT ComputeAdjustedPosAfterReplace(StrSizeT pos, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour)
	{
		if (replacedSubLoc.IsInside(pos))
		{
			switch (eBehaviour)
			{
			case StrCTX::e_insidereplacedzonebehaviour_GOTO_START:
				return(replacedSubLoc.m_start);
				break;
			case StrCTX::e_insidereplacedzonebehaviour_GOTO_END:
				return(replacedSubLoc.m_start + filledLength);
				break;
			case StrCTX::e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_STRICT:
				DOGE_ASSERT(pos - replacedSubLoc.m_start <= filledLength);
				return(pos);
				break;
			case StrCTX::e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_CLAMP:
				if (pos - replacedSubLoc.m_start <= filledLength)
				{
					return(pos);
				}
				else
				{
					return(replacedSubLoc.m_start + filledLength);
				}
				break;
			case StrCTX::e_insidereplacedzonebehaviour_ASSERT:
				DOGE_ASSERT(false); return npos<StrSizeT>::value;
				break;
			default:
				DOGE_ASSERT(false); return npos<StrSizeT>::value; // Unknown behavior enum
				break;
			}
		}
		else if (replacedSubLoc.IsBefore(pos))
		{
			return(pos);
		}
		else // if (replacedSubLoc.IsAfter(headPosBeforeIns))
		{
			return((pos - replacedSubLoc.m_end) + replacedSubLoc.m_start + filledLength);
		}
	}

	SubStrLoc ComputeAdjustedSubStrLocAfterReplace(SubStrLoc subLoc, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour)
	{	
		SubStrLoc adjustedSubLoc;
		adjustedSubLoc.m_start = ComputeAdjustedPosAfterReplace(subLoc.m_start, replacedSubLoc, filledLength, eBehaviour);
		adjustedSubLoc.m_end = ComputeAdjustedPosAfterReplace(subLoc.m_end, replacedSubLoc, filledLength, eBehaviour);

		return adjustedSubLoc;
	}

	void Erase(SubStrLoc replacedSubLoc, EInsideReplacedZoneBehaviour eBehaviour)
	{
		StrSizeT headPosBeforeIns = CurrentPos();

		baseStr.erase(replacedSubLoc.m_start, replacedSubLoc.Length());

		SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, 0, eBehaviour));

		for (SubStrLoc* pSubLoc : vRegisteredSubLocs)
		{
			//
			// Note : No fine tuned perRegistered behavior for now, just assert if inside
			*pSubLoc = ComputeAdjustedSubStrLocAfterReplace(*pSubLoc, replacedSubLoc, 0, e_insidereplacedzonebehaviour_ASSERT);
		}
	}

	void Replace(SubStrLoc replacedSubLoc, const std::string& fillingString, EInsideReplacedZoneBehaviour eBehaviour)
	{
		StrSizeT headPosBeforeIns = CurrentPos();
		StrSizeT filledLength = fillingString.length();

		baseStr.replace(replacedSubLoc.m_start, replacedSubLoc.Length(), fillingString);

		SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, filledLength, eBehaviour));

		for (SubStrLoc* pSubLoc : vRegisteredSubLocs)
		{
			//
			// Note : No fine tuned perRegistered behavior for now, just assert if inside
			*pSubLoc = ComputeAdjustedSubStrLocAfterReplace(*pSubLoc, replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
		}
	}

	void Replace(SubStrLoc replacedSubLoc, SubStrLoc fillingSubLoc, EInsideReplacedZoneBehaviour eBehaviour)
	{
		Replace(replacedSubLoc, fillingSubLoc.ExtractSubStr(baseStr), eBehaviour);
	}

	void RegisterLivePatchableCtxSubLoc(SubStrLoc& subLoc)
	{
		vRegisteredSubLocs.push_back(&subLoc);
	}

	void UnregisterLivePatchableCtxSubLoc(const SubStrLoc& subLoc)
	{
		vRegisteredSubLocs.erase(std::find(vRegisteredSubLocs.begin(), vRegisteredSubLocs.end(), &subLoc));
	}


	void RegisterLivePatchableCtxPos(StrSizeT& pos)
	{
		vRegisteredPositions.push_back(&pos);
	}

	void UnregisterLivePatchableCtxPos(StrSizeT& pos)
	{
		vRegisteredPositions.erase(std::find(vRegisteredPositions.begin(), vRegisteredPositions.end(), &pos));
	}

	char HeadChar() const {
		return *head;
	}

	const char* HeadCStr() const {
		return IT_AS_CSTR(head);
	}

	void AdvanceHead()
	{
		DOGE_ASSERT_MESSAGE(!IsEnded(), "Unexpected EOL encountered while advancing head.\n");

		if (HeadChar() == '\n')
		{
			bInLineStart = true;
		}
		else if (bInLineStart && !isHSpace(HeadChar()))
		{
			bInLineStart = false;
		}

		++head;
	}

	void Advance(StrSizeT amount) {
		for (StrSizeT i = 0; i < amount; ++i)
		{
			AdvanceHead();
		}
	}

	void SetHead(StrSizeT pos) {
		head = baseStr.cbegin() + pos;
	}

	StrSizeT CurrentPos() const {
		return std::distance(baseStr.cbegin(), head);
	}

	bool IsEnded() const {
		return head == baseStr.cend();
	}

	StrSizeT ItPos(StrCItT it) const {
		return std::distance(baseStr.cbegin(), it);
	}

	bool ItIsEnd(StrCItT it) const {
		return it == baseStr.cend();
	}

	template <class FN>
	void ExecWithTmpSzSubCStr(SubStrLoc subLoc, const FN& fn) {
		char cRestore = baseStr[subLoc.m_end];
		baseStr[subLoc.m_end] = '\0';

		fn(static_cast<const char*>(&baseStr[subLoc.m_start]));

		baseStr[subLoc.m_end] = cRestore;
	}
};

struct IfCtx {
	std::stack<RunningIfInfo> runningIfStack;
	bool bIgnoreUntilLevel;
	int endIgnoreLevel;

	int TopLevelCount() const {
		return static_cast<int>(runningIfStack.size());
	}

	bool IsIgnoreUntilLevel() const {
		return bIgnoreUntilLevel;
	}

	int EndIgnoreLevelCount() const {
		return endIgnoreLevel;
	}

	bool IsIgnoredScope() const {
		return IsIgnoreUntilLevel() && (TopLevelCount() > EndIgnoreLevelCount());
	}

	void SetIgnoreScopeAboveLevel(int level) {
		bIgnoreUntilLevel = true;
		endIgnoreLevel = level;
	}

	void ClearIgnoreLevel() {
		bIgnoreUntilLevel = false;
		endIgnoreLevel = -1;
	}

	void AddIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip) {
		RunningIfInfo runningIfInfo;
		runningIfInfo.bElseIsDone = false;
		runningIfInfo.bTrueIsDone = bEvaluation;
		runningIfInfo.start_replaced_zone = directiveStartPos;

		runningIfInfo.level = TopLevelCount();

		if (bEvaluation)
		{
			runningIfInfo.start_remaining_zone = directiveEndPos;
			runningIfInfo.bExpectingEndRemainingLineInfo = true;
		}

		RegisterTopRunningInfoSubLocsIfAny(ctx);
		runningIfStack.push(runningIfInfo);

		// Update skip/ignore mechanism

		if (IsIgnoredScope()) // Already in ignored {scope}
		{
			bOutMustSkip = true;
		}
		else
		{
			if (!bEvaluation) // Is false if {scope}
			{
				bOutMustSkip = true;
				SetIgnoreScopeAboveLevel(TopLevelCount()); // Ignore this if {scope}
			}
			else
			{
				bOutMustSkip = false;
				ClearIgnoreLevel();
			}
		}
	}

	void UpdateElseIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
	{

	}

	void UpdateElse(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
	{
		DOGE_ASSERT_MESSAGE(TopLevelCount() > 0, "Unexpected #else : no opening clause before\n");

		RunningIfInfo& runningIfInfo = runningIfStack.top();

		DOGE_ASSERT_MESSAGE(!runningIfInfo.bElseIsDone, "Unexpected #else : #else for this clause has already been processed\n");

		bool bTrueWasDoneBefore = runningIfInfo.bTrueIsDone;

		runningIfInfo.bElseIsDone = true;

		if (runningIfInfo.bTrueIsDone)
		{
			if (runningIfInfo.bExpectingEndRemainingLineInfo)
			{
				runningIfInfo.end_remaining_zone = directiveStartPos;
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

			runningIfInfo.start_remaining_zone = directiveEndPos;
			runningIfInfo.bExpectingEndRemainingLineInfo = true;
		}

		// Update skip/ignore mechanism

		bOutMustSkip = IsIgnoredScope() || bTrueWasDoneBefore;

		// Update skip/ignore mechanism

		if (IsIgnoredScope()) // Already in ignored {scope}
		{
			bOutMustSkip = true;
		}
		else
		{
			if (bTrueWasDoneBefore) // Is false else {scope}
			{
				bOutMustSkip = true;
				SetIgnoreScopeAboveLevel(TopLevelCount()); // Ignore this else {scope}
			}
			else
			{
				bOutMustSkip = false;
				ClearIgnoreLevel();
			}
		}
	}

	void UpdateEndif(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
	{
		DOGE_ASSERT_MESSAGE(TopLevelCount() > 0, "Unexpected #endif : no opening clause before\n");

		if (!IsIgnoredScope()) // If live scope, perform ctx updating by replacing the whole scope with the selected one if any
		{
			RunningIfInfo& runningIfInfo = runningIfStack.top();

			if (runningIfInfo.bTrueIsDone)
			{
				if (runningIfInfo.bExpectingEndRemainingLineInfo)
				{
					runningIfInfo.end_remaining_zone = directiveStartPos;
					runningIfInfo.bExpectingEndRemainingLineInfo = false;
				}
				else
				{
					// Nothing
				}
			}

			runningIfInfo.end_replaced_zone = directiveEndPos;

			if (runningIfInfo.bTrueIsDone)
			{
				printf("%s---> Replacing zone [%d, %d[ with zone [%d, %d[\n", tabIndentStr(runningIfInfo.level),
					runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone,
					runningIfInfo.start_remaining_zone, runningIfInfo.end_remaining_zone);

				SubStrLoc replacedSubLoc{ runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone };
				SubStrLoc fillingdSubLoc{ runningIfInfo.start_remaining_zone, runningIfInfo.end_remaining_zone };
				ctx.Replace(replacedSubLoc, fillingdSubLoc, StrCTX::e_insidereplacedzonebehaviour_ASSERT);
			}
			else
			{
				printf("%s---> No selected zone.\n", tabIndentStr(runningIfInfo.level));

				SubStrLoc erasedSubLoc{ runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone };
				ctx.Erase(erasedSubLoc, StrCTX::e_insidereplacedzonebehaviour_ASSERT);
			}

			printf("\n");

			bOutMustSkip = false;
		}
		else // Already in ignored {scope}
		{
			if (TopLevelCount() - 1 == EndIgnoreLevelCount()) // Leaving the ignored scope level, clear ignore mechanism
			{
				bOutMustSkip = false;
				ClearIgnoreLevel();
			}
			else
			{
				bOutMustSkip = true;
			}
		}

		// Pop scope
		runningIfStack.pop();
		UnregisterTopRunningInfoSubLocsIfAny(ctx);
	}

	void RegisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx)
	{
		if (!runningIfStack.empty())
		{
			RunningIfInfo& topRunningInfo = runningIfStack.top();

			DOGE_ASSERT(topRunningInfo.start_replaced_zone != npos<StrSizeT>::value);
			strCtx.RegisterLivePatchableCtxPos(topRunningInfo.start_replaced_zone);

			DOGE_ASSERT(topRunningInfo.end_replaced_zone == npos<StrSizeT>::value);

			if (topRunningInfo.start_remaining_zone != npos<StrSizeT>::value)
			{
				strCtx.RegisterLivePatchableCtxPos(topRunningInfo.start_remaining_zone);
			}

			if (topRunningInfo.end_remaining_zone != npos<StrSizeT>::value)
			{
				strCtx.RegisterLivePatchableCtxPos(topRunningInfo.end_remaining_zone);
			}
		}
	}

	void UnregisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx)
	{
		if (!runningIfStack.empty())
		{
			RunningIfInfo& topRunningInfo = runningIfStack.top();

			DOGE_ASSERT(topRunningInfo.start_replaced_zone != npos<StrSizeT>::value);
			strCtx.UnregisterLivePatchableCtxPos(topRunningInfo.start_replaced_zone);

			DOGE_ASSERT(topRunningInfo.end_replaced_zone == npos<StrSizeT>::value);

			if (topRunningInfo.start_remaining_zone != npos<StrSizeT>::value)
			{
				strCtx.UnregisterLivePatchableCtxPos(topRunningInfo.start_remaining_zone);

				if (topRunningInfo.end_remaining_zone != npos<StrSizeT>::value)
				{
					strCtx.UnregisterLivePatchableCtxPos(topRunningInfo.end_remaining_zone);
				}
			}
			else
			{
				DOGE_ASSERT(topRunningInfo.end_remaining_zone == npos<StrSizeT>::value);
			}			
		}
	}
};

extern IfCtx g_ifCtx;

