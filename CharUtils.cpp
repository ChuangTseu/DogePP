#include "CharUtils.h"



char toUpper(char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return c + ('A' - 'a');
	}

	if (c >= 'A' && c <= 'Z')
	{
		return c;
	}

	return '*';
}
