#pragma once

template <class ENUMT>
const char* EnumToSzName(ENUMT eEnum);

template <class ENUMT>
ENUMT szNameToEnum(const char* szName);

#define DECLARE_ENUM_TEMPLATE_CTTI(ENUMT) \
	template <> const char* EnumToSzName<ENUMT>(ENUMT eEnum) { return ENUMT##ToSzName(eEnum); } \
	template <> ENUMT szNameToEnum<ENUMT>(const char* szName) { return szNameTo##ENUMT(szName); }

