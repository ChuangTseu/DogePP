#pragma once

#include <string>

#include "ErrorLogging.h"

template <class T>
struct npos {
	static const T value = static_cast<T>(-1);
};

#define szEq(lsz, rsz) (!strcmp((lsz), (rsz)))
#define szEqN(lsz, rsz, n) (!strncmp((lsz), (rsz), (n)))

typedef std::string::const_iterator StrCItT;
typedef std::string::size_type StrSizeT;

extern char g_sprintf_buffer[4096];

struct SubStrLoc {
	StrSizeT m_start; // Inclusive
	StrSizeT m_end; // Exclusive

	StrSizeT Length() const {
		return m_end - m_start;
	}

	std::string ExtractSubStr(const std::string& strFrom) {
		return strFrom.substr(m_start, Length());
	}

	void Shift(int distance)
	{
		m_start += distance;
		m_end += distance;
	}

	bool IsPosInside(StrSizeT pos)
	{
		return pos >= m_start && pos < m_end;
	}

	bool IsSubLocInside(SubStrLoc subLoc)
	{
		return subLoc.m_start >= m_start && subLoc.m_end < m_end;
	}

	bool IsPosBefore(StrSizeT pos)
	{
		return pos < m_start;
	}

	bool IsPosAfter(StrSizeT pos)
	{
		return pos >= m_end;
	}
};

inline bool operator==(const SubStrLoc& lhs, const SubStrLoc& rhs)
{
	return lhs.m_start == rhs.m_start && lhs.m_end == rhs.m_end;
}

static const char* LUT_TabIndentLevelStr[] = {
	"",
	"\t",
	"\t\t",
	"\t\t\t",
	"\t\t\t\t",
	"\t\t\t\t\t",
	"\t\t\t\t\t\t",
	"\t\t\t\t\t\t\t",
	"\t\t\t\t\t\t\t\t",
	"\t\t\t\t\t\t\t\t\t",
	"\t\t\t\t\t\t\t\t\t\t"
};

inline const char* tabIndentStr(int level)
{
	static char szManyTabs[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tt\t\t\t\t\t\t\t\t\t\tt\t\t\t\t\t\t\t";
	static const int len = static_cast<int>(strlen(szManyTabs));

	static int last_level = -1;

	DOGE_ASSERT(level >= 0 && level < len);

	if (last_level > 0)
	{
		szManyTabs[last_level] = '\t';
	}

	szManyTabs[level] = '\0';
	last_level = level;

	return szManyTabs;
}

