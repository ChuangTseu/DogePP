#pragma once

#define S4(i)    S1((i)),   S1((i)+1),     S1((i)+2),     S1((i)+3)
#define S16(i)   S4((i)),   S4((i)+4),     S4((i)+8),     S4((i)+12)
#define S64(i)   S16((i)),  S16((i)+16),   S16((i)+32),   S16((i)+48)
#define S256(i)  S64((i)),  S64((i)+64),   S64((i)+128),  S64((i)+192)
#define S1024(i) S256((i)), S256((i)+256), S256((i)+512), S256((i)+768)

#define coutlr std::cout << '\n';

#define DOGE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define DOGE_ARRAY_BEGIN(arr) (&arr[0])
#define DOGE_ARRAY_END(arr) (DOGE_ARRAY_BEGIN(arr) + DOGE_ARRAY_SIZE(arr))

#define EASINT(eEnum) (static_cast<int>(eEnum))

