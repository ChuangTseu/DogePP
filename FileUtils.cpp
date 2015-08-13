#include "FileUtils.h"


bool readFileToString(const char* szFileName, std::string& outStr)
{
	std::ifstream inFile(szFileName, std::ifstream::binary);
	if (!inFile)
	{
		return false;
	}
	outStr.clear();

	inFile.seekg(0, std::ios::end);
	outStr.reserve(static_cast<size_t>(inFile.tellg()));
	inFile.seekg(0, std::ios::beg);

	outStr.assign((std::istreambuf_iterator<char>(inFile)),
		std::istreambuf_iterator<char>());

	return true;
}
