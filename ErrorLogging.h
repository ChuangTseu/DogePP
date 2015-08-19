#pragma once

#include <cassert>
#include <cstdio>

#define eprintf(...) fprintf (stderr, __VA_ARGS__)

#define DOGE_PERROR(...) eprintf(__VA_ARGS__); exit(EXIT_FAILURE);

#define DOGE_DEBUG_ASSERT(cond) assert(cond);

#define DOGE_DEBUG_ASSERT_MESSAGE(cond, ...) assert(cond);

#ifdef NDEBUG
#define DOGE_ASSERT(cond) if(!(cond)) { eprintf("Realease Assert triggered over condition : %s. Exit.\n", #cond); }	
#else
#define DOGE_ASSERT(cond) DOGE_DEBUG_ASSERT(cond);
#endif

#define DOGE_ASSERT_ALWAYS() DOGE_ASSERT(false);

#define DOGE_ASSERT_MESSAGE(cond, ...) if (!(cond)) { eprintf("DOGE_ASSERT_MESSAGE : "); eprintf(__VA_ARGS__); DOGE_ASSERT(cond); }
#define DOGE_ASSERT_ALWAYS_MESSAGE(...) eprintf("DOGE_ASSERT_ALWAYS_MESSAGE : "); eprintf(__VA_ARGS__); DOGE_ASSERT_ALWAYS();

