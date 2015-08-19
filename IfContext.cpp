#include "IfContext.h"

#include "StringContext.h"

IfCtx g_ifCtx;


void IfCtx::UnregisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx)
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

void IfCtx::RegisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx)
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

void IfCtx::UpdateEndif(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
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

void IfCtx::UpdateElse(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
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

void IfCtx::UpdateElseIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
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

void IfCtx::AddIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip)
{
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

void IfCtx::ClearIgnoreLevel()
{
	bIgnoreUntilLevel = false;
	endIgnoreLevel = -1;
}

void IfCtx::SetIgnoreScopeAboveLevel(int level)
{
	bIgnoreUntilLevel = true;
	endIgnoreLevel = level;
}

bool IfCtx::IsIgnoredScope() const
{
	return IsIgnoreUntilLevel() && (TopLevelCount() > EndIgnoreLevelCount());
}

int IfCtx::EndIgnoreLevelCount() const
{
	return endIgnoreLevel;
}

bool IfCtx::IsIgnoreUntilLevel() const
{
	return bIgnoreUntilLevel;
}

int IfCtx::TopLevelCount() const
{
	return static_cast<int>(runningIfStack.size());
}
