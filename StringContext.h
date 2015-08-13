#pragma once

#include "StringUtils.h"

#include <stack>

#define IT_AS_CSTR(it) (static_cast<const char*>(&(*(it))))

struct RunningIfInfo {
	bool bTrueIsDone = false;
	bool bElseIsDone = false;

	bool bExpectingEndRemainingLineInfo = false;

	int level;

	StrSizeT start_replaced_zone;

	StrSizeT start_remaining_zone;
	StrSizeT end_remaining_zone;

	StrSizeT end_replaced_zone;
};

struct IfCtx {
	std::stack<RunningIfInfo> runningIfStack;
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

	char HeadChar() const {
		return *head;
	}

	const char* HeadCStr() const {
		return IT_AS_CSTR(head);
	}

	void Advance(StrCItT::difference_type amount) {
		std::advance(head, amount);
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



