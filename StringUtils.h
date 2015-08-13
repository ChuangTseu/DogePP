#pragma once

#include <string>

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
};

