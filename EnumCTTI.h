#pragma once

template <class ENUMT>
inline const char* EnumToSzName(ENUMT eEnum);

template <class ENUMT>
inline ENUMT szNameToEnum(const char* szName);

#define DECLARE_ENUM_TEMPLATE_CTTI(ENUMT) \
	template <> inline const char* EnumToSzName<ENUMT>(ENUMT eEnum) { return ENUMT##ToSzName(eEnum); } \
	template <> inline ENUMT szNameToEnum<ENUMT>(const char* szName) { return szNameTo##ENUMT(szName); }

