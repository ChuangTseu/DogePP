#pragma once

#include <fstream>
#include <string>

bool readFileToString(const char* szFileName, std::string& outStr);
bool writeStringToFile(const char* szFileName, const std::string& inStr);
