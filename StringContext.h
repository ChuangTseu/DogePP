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

	void UnregisterLivePatchableCtxSubLoc(SubStrLoc& subLoc)
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
};

struct IfCtx {
	std::stack<RunningIfInfo> runningIfStack;

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

