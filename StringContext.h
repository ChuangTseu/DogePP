#pragma once

#include "StringUtils.h"
#include "Match.h"
#include "ErrorLogging.h"

#include <stack>
#include <vector>
#include <algorithm>

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

struct IncludedScopeInfo {
	std::vector<SubStrLoc> vLinesSubLocs;
	std::vector<std::string> strComments;
	std::vector<StrSizeT> commentsPositions;
	int iLinesCount = 0;

	// 
	// Can only merge from moved IncludedScopeInfo for perf reasons
	void MergeFromMovedIncludedScopeInfo(const IncludedScopeInfo& movedScopeInfo, int aboveScopeLineStart) = delete;

	void MergeFromMovedIncludedScopeInfo(IncludedScopeInfo&& movedScopeInfo, int aboveScopeLineStart)
	{
		if (movedScopeInfo.vLinesSubLocs.size())
		{
			for (SubStrLoc& subLoc : movedScopeInfo.vLinesSubLocs) subLoc.Shift(aboveScopeLineStart);
			auto linesSubLocsInsertPointIt = std::lower_bound(vLinesSubLocs.begin(), vLinesSubLocs.end(), movedScopeInfo.vLinesSubLocs.front(),
				[](const SubStrLoc& lhs, const SubStrLoc& rhs){ return lhs.m_start < rhs.m_end;  });
			vLinesSubLocs.insert(linesSubLocsInsertPointIt, movedScopeInfo.vLinesSubLocs.begin(), movedScopeInfo.vLinesSubLocs.end());
		}

		iLinesCount += movedScopeInfo.iLinesCount;

		strComments.insert(strComments.end(),
			std::make_move_iterator(movedScopeInfo.strComments.begin()), std::make_move_iterator(movedScopeInfo.strComments.end())); // Benefit from strings move efficiency

		if (movedScopeInfo.commentsPositions.size())
		{
			for (StrSizeT& subLoc : movedScopeInfo.commentsPositions) subLoc += aboveScopeLineStart;
			auto commentPosisiontsInsertPointIt = std::lower_bound(commentsPositions.begin(), commentsPositions.end(), movedScopeInfo.commentsPositions.front());
			commentsPositions.insert(commentPosisiontsInsertPointIt, movedScopeInfo.commentsPositions.begin(), movedScopeInfo.commentsPositions.end());
		}
	}

	bool IsEmpty() const
	{
		return vLinesSubLocs.empty() && strComments.empty() && commentsPositions.empty() && iLinesCount == 0;
	}
};


struct StrCTX {
	StrCTX(std::string& capturedString) : StrCTX(capturedString, capturedString.cbegin()) {}

	StrCTX(std::string& capturedString, StrCItT customHeadIt) : m_baseStr(capturedString), m_head(customHeadIt) {
		//RegisterIncludedScopeInfo(m_includedScopeInfo);
	}

	std::string& m_baseStr;
	StrCItT m_head;

	bool m_bInLineStart = true;
	int m_lineNumber = 1;

	std::vector<SubStrLoc*> m_vRegisteredSubLocs;
	std::vector<StrSizeT*> m_vRegisteredPositions;

	std::vector<std::vector<SubStrLoc>*> m_vRegisteredSubLocArrays;
	std::vector<std::vector<StrSizeT>*> m_vRegisteredPositionArrays;

	IncludedScopeInfo m_includedScopeInfo;

	template <class DONOTUSETHISFUNCTION>
	void RegisterIncludedScopeInfo(IncludedScopeInfo& includedScopeInfo)
	{
		int willFailIfInstantiated = DONOTUSETHISFUNCTION::PleaseDoNotEncounterAnyMiraculouslyAlreadyDefinedTypeTPublicStaticSymbol;

		RegisterLivePatchableCtxSubLocArray(includedScopeInfo.vLinesSubLocs);
		RegisterLivePatchableCtxPosArray(includedScopeInfo.commentsPositions);
	}

	void InsertHere(const std::string& str, bool bAdvanceAfterInserted)
	{
		StrSizeT insertPos = CurrentPos();

		m_baseStr.insert(insertPos, str);

		if (bAdvanceAfterInserted)
		{
			SetHead(insertPos + str.length());
		}
		else
		{
			SetHead(insertPos);
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

#define TMP_DEFAULT_REGISTERED_POS_BEHAVIOR e_insidereplacedzonebehaviour_GOTO_START

	void AdjustAllRegisteredLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength)
	{
		//
		// Note : No fine tuned perRegistered behavior for now, just assert if inside
		for (SubStrLoc* pSubLoc : m_vRegisteredSubLocs) 
			*pSubLoc = ComputeAdjustedSubStrLocAfterReplace(*pSubLoc, replacedSubLoc, filledLength, TMP_DEFAULT_REGISTERED_POS_BEHAVIOR);
		for (StrSizeT* pPos : m_vRegisteredPositions) 
			*pPos = ComputeAdjustedPosAfterReplace(*pPos, replacedSubLoc, filledLength, TMP_DEFAULT_REGISTERED_POS_BEHAVIOR);
		for (auto pSubLocArray : m_vRegisteredSubLocArrays)
			for (SubStrLoc& subLoc : *pSubLocArray) 
				subLoc = ComputeAdjustedSubStrLocAfterReplace(subLoc, replacedSubLoc, filledLength, TMP_DEFAULT_REGISTERED_POS_BEHAVIOR);
		for (auto pPositionsArray : m_vRegisteredPositionArrays)
			for (StrSizeT& pos : *pPositionsArray) 
				pos = ComputeAdjustedPosAfterReplace(pos, replacedSubLoc, filledLength, TMP_DEFAULT_REGISTERED_POS_BEHAVIOR);
	}

	void AdjustScopeInfoLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength)
	{
		auto& vComsPos = m_includedScopeInfo.commentsPositions;
		auto& vComsStr = m_includedScopeInfo.strComments;
		auto& vLinesSubLoc = m_includedScopeInfo.vLinesSubLocs;

		{ // Pop out comments (typically from an unselected #if/#else guard zone)
			auto poppedComsBegIt = std::lower_bound(vComsPos.begin(), vComsPos.end(), replacedSubLoc.m_start);

			if (poppedComsBegIt != vComsPos.end()
				&& *poppedComsBegIt < replacedSubLoc.m_end)
			{
				auto poppedComsEndIt = std::lower_bound(poppedComsBegIt, vComsPos.end(), replacedSubLoc.m_end);

				size_t poppedBegPos = std::distance(vComsPos.begin(), poppedComsBegIt);
				size_t poppedEndPos = std::distance(vComsPos.begin(), poppedComsEndIt);

				vComsPos.erase(poppedComsBegIt, poppedComsEndIt);
				for (size_t i = poppedBegPos; i < vComsPos.size(); ++i) 
					vComsPos[i] = ComputeAdjustedPosAfterReplace(vComsPos[i], replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
				vComsStr.erase(vComsStr.begin() + poppedBegPos, vComsStr.begin() + poppedEndPos);
			}
		}

		{ // Pop out lines (typically from an unselected #if/#else guard zone)
			auto poppedLinesBegIt = std::lower_bound(vLinesSubLoc.begin(), vLinesSubLoc.end(), replacedSubLoc,
				[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_start < rhs.m_start; });

			if (poppedLinesBegIt != vLinesSubLoc.end()
				&& poppedLinesBegIt->m_start < replacedSubLoc.m_end)
			{
				auto poppedLinesEndIt = std::lower_bound(poppedLinesBegIt, vLinesSubLoc.end(), replacedSubLoc,
					[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_start < rhs.m_start; });

				size_t poppedBegPos = std::distance(vLinesSubLoc.begin(), poppedLinesBegIt);
				size_t poppedEndPos = std::distance(vLinesSubLoc.begin(), poppedLinesEndIt);

#if POPOUT_LINES
				vLinesSubLoc.erase(poppedLinesBegIt, poppedLinesEndIt);
				for (size_t i = poppedBegPos; i < vLinesSubLoc.size(); ++i)
				{
					if (vLinesSubLoc[i] == replacedSubLoc) // Exact deleted match is just transformed into the 0 sized [begin,begin[ interval
					{
						vLinesSubLoc[i].m_end = vLinesSubLoc[i].m_start;
					}
					else
					{
						vLinesSubLoc[i] = ComputeAdjustedSubStrLocAfterReplace(vLinesSubLoc[i], replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
					}
				}
#else // Just make them zero lines but keep them in the array
				for (size_t i = poppedBegPos; i < vLinesSubLoc.size(); ++i)
				{
					if (vLinesSubLoc[i].m_end <= replacedSubLoc.m_end)
					{
						vLinesSubLoc[i].m_end = vLinesSubLoc[i].m_start;
					}
					else
					{
						vLinesSubLoc[i] = ComputeAdjustedSubStrLocAfterReplace(vLinesSubLoc[i], replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
					}
				}
#endif
			}
		}
		
	}

	void Erase(SubStrLoc replacedSubLoc, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo = true)
	{
		StrSizeT headPosBeforeIns = CurrentPos();

		m_baseStr.erase(replacedSubLoc.m_start, replacedSubLoc.Length());

		SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, 0, eBehaviour));

		AdjustAllRegisteredLocations(replacedSubLoc, 0);
		if (bPopScopeInfo)
			AdjustScopeInfoLocations(replacedSubLoc, 0);
	}

	void Replace(SubStrLoc replacedSubLoc, const std::string& fillingString, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo = true)
	{
		StrSizeT headPosBeforeIns = CurrentPos();
		StrSizeT filledLength = fillingString.length();

		m_baseStr.replace(replacedSubLoc.m_start, replacedSubLoc.Length(), fillingString);

		SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, filledLength, eBehaviour));

		AdjustAllRegisteredLocations(replacedSubLoc, filledLength);
		if (bPopScopeInfo)
			AdjustScopeInfoLocations(replacedSubLoc, filledLength);
	}

	void Replace(SubStrLoc replacedSubLoc, SubStrLoc fillingSubLoc, EInsideReplacedZoneBehaviour eBehaviour)
	{
		Replace(replacedSubLoc, fillingSubLoc.ExtractSubStr(m_baseStr), eBehaviour);
	}

	void RegisterLivePatchableCtxSubLoc(SubStrLoc& subLoc)
	{
		m_vRegisteredSubLocs.push_back(&subLoc);
	}

	void UnregisterLivePatchableCtxSubLoc(const SubStrLoc& subLoc)
	{
		m_vRegisteredSubLocs.erase(std::find(m_vRegisteredSubLocs.begin(), m_vRegisteredSubLocs.end(), &subLoc));
	}


	void RegisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs)
	{
		m_vRegisteredSubLocArrays.push_back(&vSubLocs);
	}

	void UnregisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs)
	{
		m_vRegisteredSubLocArrays.erase(std::find(m_vRegisteredSubLocArrays.begin(), m_vRegisteredSubLocArrays.end(), &vSubLocs));
	}


	void RegisterLivePatchableCtxPos(StrSizeT& pos)
	{
		m_vRegisteredPositions.push_back(&pos);
	}

	void UnregisterLivePatchableCtxPos(StrSizeT& pos)
	{
		m_vRegisteredPositions.erase(std::find(m_vRegisteredPositions.begin(), m_vRegisteredPositions.end(), &pos));
	}


	void RegisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos)
	{
		m_vRegisteredPositionArrays.push_back(&vPos);
	}

	void UnregisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos)
	{
		m_vRegisteredPositionArrays.erase(std::find(m_vRegisteredPositionArrays.begin(), m_vRegisteredPositionArrays.end(), &vPos));
	}


	char HeadChar() const {
		return *m_head;
	}

	char HeadCharAtDistance(int distance) const {
		return *(m_head + distance);
	}

	const char* HeadCStr() const {
		return IT_AS_CSTR(m_head);
	}

	void AdvanceHead()
	{
		DOGE_ASSERT_MESSAGE(!IsEnded(), "Unexpected EOL encountered while advancing head.\n");

		if (HeadChar() == '\n')
		{
			m_bInLineStart = true;
		}
		else if (m_bInLineStart && !isHSpace(HeadChar()))
		{
			m_bInLineStart = false;
		}

		++m_head;
	}

	void Advance(StrSizeT amount) {
		for (StrSizeT i = 0; i < amount; ++i)
		{
			AdvanceHead();
		}
	}

	void SetHead(StrSizeT pos) {
		m_head = m_baseStr.cbegin() + pos;
	}

	StrSizeT CurrentPos() const {
		return std::distance(m_baseStr.cbegin(), m_head);
	}

	bool IsEnded() const {
		return m_head == m_baseStr.cend();
	}

	StrSizeT ItPos(StrCItT it) const {
		return std::distance(m_baseStr.cbegin(), it);
	}

	bool ItIsEnd(StrCItT it) const {
		return it == m_baseStr.cend();
	}

	template <class FN>
	void ExecWithTmpSzSubCStr(SubStrLoc subLoc, const FN& fn) {
		char cRestore = m_baseStr[subLoc.m_end];
		m_baseStr[subLoc.m_end] = '\0';

		fn(static_cast<const char*>(&m_baseStr[subLoc.m_start]));

		m_baseStr[subLoc.m_end] = cRestore;
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
		DOGE_ASSERT_MESSAGE(TopLevelCount() > 0, "Unexpected #else : no opening clause before\n");

		RunningIfInfo& runningIfInfo = runningIfStack.top();

		DOGE_ASSERT_MESSAGE(!runningIfInfo.bElseIsDone, "Unexpected #elif : #else for this clause has already been processed\n");

		bool bTrueWasDoneBefore = runningIfInfo.bTrueIsDone;

		if (bTrueWasDoneBefore)
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
			if (bEvaluation)
			{
				runningIfInfo.bTrueIsDone = true; // Mandatory if else is reached while not true_is_set

				runningIfInfo.start_remaining_zone = directiveEndPos;
				runningIfInfo.bExpectingEndRemainingLineInfo = true;
			}
			else
			{
				// Nothing
			}			
		}

		// Update skip/ignore mechanism

		if (IsIgnoredScope()) // Already in ignored {scope}
		{
			bOutMustSkip = true;
		}
		else
		{
			if (bTrueWasDoneBefore || !bEvaluation) // Is false else {scope}
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
				//printf("%s---> Replacing zone [%d, %d[ with zone [%d, %d[\n", tabIndentStr(runningIfInfo.level),
				//	runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone,
				//	runningIfInfo.start_remaining_zone, runningIfInfo.end_remaining_zone);

				SubStrLoc replacedSubLoc{ runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone };
				SubStrLoc fillingdSubLoc{ runningIfInfo.start_remaining_zone, runningIfInfo.end_remaining_zone };
				ctx.Replace(replacedSubLoc, fillingdSubLoc, StrCTX::e_insidereplacedzonebehaviour_ASSERT);
			}
			else
			{
				//printf("%s---> No selected zone.\n", tabIndentStr(runningIfInfo.level));

				SubStrLoc erasedSubLoc{ runningIfInfo.start_replaced_zone, runningIfInfo.end_replaced_zone };
				ctx.Erase(erasedSubLoc, StrCTX::e_insidereplacedzonebehaviour_ASSERT);
			}

			//printf("\n");

			bOutMustSkip = false;
		}
		else // Already in ignored {scope}
		{
			if (TopLevelCount() == EndIgnoreLevelCount()) // Leaving the ignored scope level, clear ignore mechanism
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

