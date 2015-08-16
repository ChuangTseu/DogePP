#pragma once

#define LOWEST_PRECEDENCE	(100)
#define HIGHEST_PRECEDENCE	(0)

struct SpecialPrecedence {
	// Value from https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B

	// Ordered in increasing precedence.
	static const int CONDITIONAL = 15;
	static const int CALL = 2;
};


