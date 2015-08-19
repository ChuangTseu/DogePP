#pragma once

#include "StringUtils.h"
#include "Match.h"
#include "ErrorLogging.h"
#include "IfContext.h"

#include <vector>
#include <algorithm>

#define IT_AS_CSTR(it) (static_cast<const char*>(&(*(it))))

struct LocationInfos {
	std::string fromFile;
	int iLine;
	int iCharInLine;
};

struct LineInfo {
	StrSizeT line;
	StrSizeT charPosInLine;
};

struct IncludedScopeInfo {
	std::string name;

	std::vector<SubStrLoc> vLinesSubLocs;
	//std::vector<StrSizeT> vLinesIdx;

	std::vector<std::string> strComments;
	std::vector<StrSizeT> commentsPositions;

	int iLinesCount = 0;

	std::vector<IncludedScopeInfo> sons;
	std::vector<SubStrLoc> sonsDelim;

	void AddLineSubLoc(SubStrLoc lineSubLoc)
	{
		if (vLinesSubLocs.size() > 130)
		{
			int breakpoint = -1;
		}
		DOGE_ASSERT(lineSubLoc.m_start <= lineSubLoc.m_end);
		if (vLinesSubLocs.size()) { DOGE_ASSERT(vLinesSubLocs.back().m_end <= lineSubLoc.m_start); }
		vLinesSubLocs.push_back(lineSubLoc);
	}

	void AddToSons(const IncludedScopeInfo& movedScopeInfo, int aboveScopeLineStart)
	{
		for (size_t i = 0; i < sonsDelim.size(); ++i)
		{
			if (sonsDelim[i].IsPosInside(aboveScopeLineStart))
			{
				sons[i].AddToSons(movedScopeInfo, aboveScopeLineStart);
				return;
			}
		}

		sons.push_back(movedScopeInfo);
	}

	LineInfo GetCharPosLineInfo(StrSizeT pos)
	{
		for (size_t i = 0; i < vLinesSubLocs.size(); ++i)
		{
			if (vLinesSubLocs[i].IsPosInside(pos))
			{
				return{ i, pos - vLinesSubLocs[i].m_start };
			}
		}

		return{ npos<StrSizeT>::value, npos<StrSizeT>::value };
	}

#if 0

	// 
	// Can only merge from moved IncludedScopeInfo for perf reasons
	void MergeFromMovedIncludedScopeInfo(const IncludedScopeInfo& movedScopeInfo, int aboveScopeLineStart) = delete;

	void MergeFromMovedIncludedScopeInfo(IncludedScopeInfo&& movedScopeInfo, int aboveScopeLineStart)
	{
		AddToSons(movedScopeInfo, aboveScopeLineStart);

		DOGE_ASSERT_MESSAGE(movedScopeInfo.vLinesSubLocs.size() > 0, "Error at include directive in file %s ppchar %d. Included file %s needs at least one line (including its line return).\n", name.c_str(), aboveScopeLineStart, movedScopeInfo.name.c_str());
		sonsDelim.push_back({ movedScopeInfo.vLinesSubLocs.front().m_start + aboveScopeLineStart, 
			movedScopeInfo.vLinesSubLocs.back().m_end + aboveScopeLineStart });

		if (movedScopeInfo.vLinesSubLocs.size())
		{
			for (SubStrLoc& subLoc : movedScopeInfo.vLinesSubLocs) subLoc.Shift(aboveScopeLineStart);
			auto linesSubLocsInsertPointIt = std::lower_bound(vLinesSubLocs.begin(), vLinesSubLocs.end(), movedScopeInfo.vLinesSubLocs.front(),
				[](const SubStrLoc& lhs, const SubStrLoc& rhs){ return lhs.m_start < rhs.m_end;  });
			vLinesSubLocs.insert(linesSubLocsInsertPointIt, movedScopeInfo.vLinesSubLocs.begin(), movedScopeInfo.vLinesSubLocs.end());

			for (size_t i = 0; i < movedScopeInfo.vLinesSubLocs.size() - 1; ++i)
			{
				DOGE_ASSERT(movedScopeInfo.vLinesSubLocs[i].m_end <= movedScopeInfo.vLinesSubLocs[i + 1].m_start);
			}
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

#endif

	bool IsEmpty() const
	{
		return vLinesSubLocs.empty() && strComments.empty() && commentsPositions.empty() && iLinesCount == 0;
	}
};

#define CTX_ERROR(ctx, fmt, ...) { sprintf(g_sprintf_buffer, fmt, __VA_ARGS__); ctx.ErrorFromCurrentPos(g_sprintf_buffer); }


struct StrCTX {
	StrCTX(std::string&& movedString, std::string strName) :
		m_baseStr(std::move(movedString)), m_head(m_baseStr.cbegin()), m_name(strName)
	{
		m_includedScopeInfo.name = m_name;

		RegisterLivePatchableCtxPosArray(m_includedContextsPositions);
	}

	std::string m_name;

	std::string m_baseStr;
	StrCItT m_head;

	bool m_bInLineStart = true;
	int m_lineNumber = 1;

	std::vector<SubStrLoc*> m_vRegisteredSubLocs;
	std::vector<StrSizeT*> m_vRegisteredPositions;

	std::vector<std::vector<SubStrLoc>*> m_vRegisteredSubLocArrays;
	std::vector<std::vector<StrSizeT>*> m_vRegisteredPositionArrays;

	IncludedScopeInfo m_includedScopeInfo;

	IfCtx m_ifCtx;

	std::vector<StrCTX> m_includedContexts;
	std::vector<StrSizeT> m_includedContextsPositions;

	void ErrorFromCurrentPos(const char* szErrorMessage)
	{
		LineInfo lineInfo = m_includedScopeInfo.GetCharPosLineInfo(CurrentPos());

		eprintf("\
In file '%s' at line %d (char pos %d) :\n\
	ERROR : %s\n\
", m_name.c_str(), lineInfo.line + 1, lineInfo.charPosInLine + 1, szErrorMessage);


		exit(1);
	}

	void WarningFromCurrentPos(const char* szErrorMessage)
	{
		LineInfo lineInfo = m_includedScopeInfo.GetCharPosLineInfo(CurrentPos());

		eprintf("\
In file '%s' at line %d (char pos %d) :\n\
	Warning : %s\n\
", m_name, lineInfo.line + 1, lineInfo.charPosInLine + 1, szErrorMessage);
	}

	void AddIncludedContext(const StrCTX& includedCtx, StrSizeT includePosition)
	{
		m_includedContexts.push_back(includedCtx);
		m_includedContextsPositions.push_back(includePosition);
	}

	void PatchWithIncludedContexts()
	{
		StrSizeT adjustedOffset = 0;

		for (size_t nIncludedContext = 0; nIncludedContext < m_includedContexts.size(); ++nIncludedContext)
		{
			m_includedContexts[nIncludedContext].PatchWithIncludedContexts();
			Insert(m_includedContextsPositions[nIncludedContext] + adjustedOffset, m_includedContexts[nIncludedContext].m_baseStr);
			adjustedOffset += m_includedContexts[nIncludedContext].m_baseStr.length();
		}
	}

	enum EInsideReplacedZoneBehaviour {
		e_insidereplacedzonebehaviour_GOTO_START, // Place head after the end of the new zone
		e_insidereplacedzonebehaviour_GOTO_END, // Place head to the beginning of the new zone
		e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_STRICT, // Will assert if out of new adapted zone
		e_insidereplacedzonebehaviour_ADAPT_TO_NEW_ZONE_CLAMP, // Will behave as GOTO_END if out of new adapted zone
		e_insidereplacedzonebehaviour_ASSERT // Simple assert
	};

	void Insert(StrSizeT insertPos, const std::string& str);
	void InsertHere(const std::string& str, bool bAdvanceAfterInserted);
	void Erase(SubStrLoc replacedSubLoc, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo = true);
	void Replace(SubStrLoc replacedSubLoc, const std::string& fillingString, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo = true);
	void Replace(SubStrLoc replacedSubLoc, SubStrLoc fillingSubLoc, EInsideReplacedZoneBehaviour eBehaviour);

	void RegisterLivePatchableCtxSubLoc(SubStrLoc& subLoc);
	void UnregisterLivePatchableCtxSubLoc(const SubStrLoc& subLoc);

	void RegisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs);
	void UnregisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs);

	void RegisterLivePatchableCtxPos(StrSizeT& pos);
	void UnregisterLivePatchableCtxPos(StrSizeT& pos);

	void RegisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos);
	void UnregisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos);

	StrSizeT ComputeAdjustedPosAfterReplace(StrSizeT pos, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour);
	SubStrLoc ComputeAdjustedSubStrLocAfterReplace(SubStrLoc subLoc, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour);

#define TMP_DEFAULT_REGISTERED_POS_BEHAVIOR e_insidereplacedzonebehaviour_GOTO_START

	void AdjustAllRegisteredLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength);
	void AdjustScopeInfoLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength);


	bool IsEnded() const { return m_head == m_baseStr.cend(); }
	StrSizeT CurrentPos() const { return std::distance(m_baseStr.cbegin(), m_head); }
	void SetHead(StrSizeT pos) { m_head = m_baseStr.cbegin() + pos; }
	void ResetHead() { SetHead(0); }
	const char* HeadCStr() const { return IT_AS_CSTR(m_head); }
	char HeadChar() const { return *m_head; }
	char HeadCharAtDistance(int distance) const { return *(m_head + distance); }

	void AdvanceHead() {
		DOGE_ASSERT_MESSAGE(!IsEnded(), "Unexpected EOL encountered while advancing head.\n");
		if (HeadChar() == '\n') {
			m_bInLineStart = true;
		}
		else if (m_bInLineStart && !isHSpace(HeadChar())) {
			m_bInLineStart = false;
		}
		++m_head;
	}

	void Advance(StrSizeT amount) { for (StrSizeT i = 0; i < amount; ++i) AdvanceHead(); }

	template <class FN>
	void ExecWithTmpSzSubCStr(SubStrLoc subLoc, const FN& fn)
	{
		char cRestore = m_baseStr[subLoc.m_end];
		m_baseStr[subLoc.m_end] = '\0';

		fn(static_cast<const char*>(&m_baseStr[subLoc.m_start]));

		m_baseStr[subLoc.m_end] = cRestore;
	}
};
