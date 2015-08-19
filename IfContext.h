#pragma once

#include <stack>

#include "StringUtils.h"

struct StrCTX;

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

struct IfCtx {
	std::stack<RunningIfInfo> runningIfStack;
	bool bIgnoreUntilLevel = false;
	int endIgnoreLevel = -1;

	int TopLevelCount() const;

	bool IsIgnoreUntilLevel() const;

	int EndIgnoreLevelCount() const;

	bool IsIgnoredScope() const;

	void SetIgnoreScopeAboveLevel(int level);

	void ClearIgnoreLevel();

	void AddIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip);

	void UpdateElseIf(StrCTX& ctx, bool bEvaluation, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip);

	void UpdateElse(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip);

	void UpdateEndif(StrCTX& ctx, StrSizeT directiveStartPos, StrSizeT directiveEndPos, bool& bOutMustSkip);

	void RegisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx);

	void UnregisterTopRunningInfoSubLocsIfAny(StrCTX& strCtx);
};

//extern IfCtx g_ifCtx;

