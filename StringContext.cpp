#include "StringContext.h"


void StrCTX::UnregisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos)
{
	m_vRegisteredPositionArrays.erase(std::find(m_vRegisteredPositionArrays.begin(), m_vRegisteredPositionArrays.end(), &vPos));
}

void StrCTX::RegisterLivePatchableCtxPosArray(std::vector<StrSizeT>& vPos)
{
	m_vRegisteredPositionArrays.push_back(&vPos);
}

void StrCTX::UnregisterLivePatchableCtxPos(StrSizeT& pos)
{
	m_vRegisteredPositions.erase(std::find(m_vRegisteredPositions.begin(), m_vRegisteredPositions.end(), &pos));
}

void StrCTX::RegisterLivePatchableCtxPos(StrSizeT& pos)
{
	m_vRegisteredPositions.push_back(&pos);
}

void StrCTX::UnregisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs)
{
	m_vRegisteredSubLocArrays.erase(std::find(m_vRegisteredSubLocArrays.begin(), m_vRegisteredSubLocArrays.end(), &vSubLocs));
}

void StrCTX::RegisterLivePatchableCtxSubLocArray(std::vector<SubStrLoc>& vSubLocs)
{
	m_vRegisteredSubLocArrays.push_back(&vSubLocs);
}

void StrCTX::UnregisterLivePatchableCtxSubLoc(const SubStrLoc& subLoc)
{
	m_vRegisteredSubLocs.erase(std::find(m_vRegisteredSubLocs.begin(), m_vRegisteredSubLocs.end(), &subLoc));
}

void StrCTX::RegisterLivePatchableCtxSubLoc(SubStrLoc& subLoc)
{
	m_vRegisteredSubLocs.push_back(&subLoc);
}

void StrCTX::Replace(SubStrLoc replacedSubLoc, SubStrLoc fillingSubLoc, EInsideReplacedZoneBehaviour eBehaviour)
{
	Replace(replacedSubLoc, fillingSubLoc.ExtractSubStr(m_baseStr), eBehaviour);
}

void StrCTX::Replace(SubStrLoc replacedSubLoc, const std::string& fillingString, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo /*= true*/)
{
	StrSizeT headPosBeforeIns = CurrentPos();
	StrSizeT filledLength = fillingString.length();

	m_baseStr.replace(replacedSubLoc.m_start, replacedSubLoc.Length(), fillingString);

	SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, filledLength, eBehaviour));

	AdjustAllRegisteredLocations(replacedSubLoc, filledLength);
	if (bPopScopeInfo)
		AdjustScopeInfoLocations(replacedSubLoc, filledLength);
}

void StrCTX::Erase(SubStrLoc replacedSubLoc, EInsideReplacedZoneBehaviour eBehaviour, bool bPopScopeInfo /*= true*/)
{
	StrSizeT headPosBeforeIns = CurrentPos();

	m_baseStr.erase(replacedSubLoc.m_start, replacedSubLoc.Length());

	SetHead(ComputeAdjustedPosAfterReplace(headPosBeforeIns, replacedSubLoc, 0, eBehaviour));

	AdjustAllRegisteredLocations(replacedSubLoc, 0);
	if (bPopScopeInfo)
		AdjustScopeInfoLocations(replacedSubLoc, 0);
}

void StrCTX::AdjustScopeInfoLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength)
{
	auto& vComsPos = m_includedScopeInfo.commentsPositions;
	auto& vComsStr = m_includedScopeInfo.strComments;
	auto& vLinesSubLoc = m_includedScopeInfo.vLinesSubLocs;

	{ // Pop out comments (typically from an unselected #if/#else guard zone)
		auto poppedComsBegIt = std::lower_bound(vComsPos.begin(), vComsPos.end(), replacedSubLoc.m_start);
		size_t poppedBegPos = std::distance(vComsPos.begin(), poppedComsBegIt);

		if (poppedComsBegIt != vComsPos.end()
			&& *poppedComsBegIt < replacedSubLoc.m_end)
		{
			auto poppedComsEndIt = std::lower_bound(poppedComsBegIt, vComsPos.end(), replacedSubLoc.m_end);
			size_t poppedEndPos = std::distance(vComsPos.begin(), poppedComsEndIt);

			vComsPos.erase(poppedComsBegIt, poppedComsEndIt);
			vComsStr.erase(vComsStr.begin() + poppedBegPos, vComsStr.begin() + poppedEndPos);
		}

		for (size_t i = poppedBegPos; i < vComsPos.size(); ++i)
			vComsPos[i] = ComputeAdjustedPosAfterReplace(vComsPos[i], replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
	}

		{ // Pop out lines (typically from an unselected #if/#else guard zone)
			auto lbOnEndIt = std::lower_bound(vLinesSubLoc.begin(), vLinesSubLoc.end(), replacedSubLoc,
				[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_end < rhs.m_start; });
			auto lbOnStartIt = std::lower_bound(lbOnEndIt, vLinesSubLoc.end(), replacedSubLoc,
				[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_start < rhs.m_start; });

			auto dist = std::distance(lbOnEndIt, lbOnStartIt);
			DOGE_DEBUG_ASSERT(dist >= 0 && dist < 2);
			if (dist == 1)
			{
				lbOnEndIt->m_end = ComputeAdjustedPosAfterReplace(lbOnEndIt->m_end, replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_GOTO_END);
			}

			/*auto poppedLinesBegIt = std::lower_bound(vLinesSubLoc.begin(), vLinesSubLoc.end(), replacedSubLoc,
				[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_start < rhs.m_start; });*/
			auto poppedLinesBegIt = lbOnStartIt;
			size_t poppedBegPos = std::distance(vLinesSubLoc.begin(), poppedLinesBegIt);

			if (poppedLinesBegIt != vLinesSubLoc.end()
				&& poppedLinesBegIt->m_start < replacedSubLoc.m_end)
			{
				auto poppedLinesEndIt = std::lower_bound(poppedLinesBegIt, vLinesSubLoc.end(), replacedSubLoc,
					[](const SubStrLoc& lhs, const SubStrLoc& rhs) { return lhs.m_start < rhs.m_end; });
				size_t poppedEndPos = std::distance(vLinesSubLoc.begin(), poppedLinesEndIt);
			}

			// Just make them zero lines but keep them in the array
			for (size_t i = poppedBegPos; i < vLinesSubLoc.size(); ++i)
			{
				SubStrLoc& adjSubLoc = vLinesSubLoc[i];

				if (adjSubLoc.m_end <= replacedSubLoc.m_end)
					adjSubLoc.m_start = adjSubLoc.m_end = replacedSubLoc.m_start + filledLength;
				else if (adjSubLoc.IsSubLocInside(replacedSubLoc))
				{
					adjSubLoc.m_end += (-static_cast<int>(replacedSubLoc.Length()) + static_cast<int>(filledLength));
				}
				else
					adjSubLoc = ComputeAdjustedSubStrLocAfterReplace(adjSubLoc, replacedSubLoc, filledLength, e_insidereplacedzonebehaviour_ASSERT);
			}
		}
}

void StrCTX::AdjustAllRegisteredLocations(SubStrLoc replacedSubLoc, StrSizeT filledLength)
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

SubStrLoc StrCTX::ComputeAdjustedSubStrLocAfterReplace(SubStrLoc subLoc, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour)
{
	SubStrLoc adjustedSubLoc;
	adjustedSubLoc.m_start = ComputeAdjustedPosAfterReplace(subLoc.m_start, replacedSubLoc, filledLength, eBehaviour);
	adjustedSubLoc.m_end = ComputeAdjustedPosAfterReplace(subLoc.m_end, replacedSubLoc, filledLength, eBehaviour);

	return adjustedSubLoc;
}

StrSizeT StrCTX::ComputeAdjustedPosAfterReplace(StrSizeT pos, SubStrLoc replacedSubLoc, StrSizeT filledLength, EInsideReplacedZoneBehaviour eBehaviour)
{
	if (replacedSubLoc.IsPosInside(pos))
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
	else if (replacedSubLoc.IsPosBefore(pos))
	{
		return(pos);
	}
	else // if (replacedSubLoc.IsAfter(headPosBeforeIns))
	{
		return((pos - replacedSubLoc.m_end) + replacedSubLoc.m_start + filledLength);
	}
}

void StrCTX::Insert(StrSizeT insertPos, const std::string& str)
{
	m_baseStr.insert(insertPos, str);
}

void StrCTX::InsertHere(const std::string& str, bool bAdvanceAfterInserted)
{
	StrSizeT insertPos = CurrentPos();

	Insert(insertPos, str);

	if (bAdvanceAfterInserted)
	{
		SetHead(insertPos + str.length());
	}
	else
	{
		SetHead(insertPos);
	}
}

