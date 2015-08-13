#pragma once

#define LR_TYPE_LF 0
#define LR_TYPE_CRLF 1

#if LR_TYPE_LF
#define LR_LENGTH 1
#elif LR_TYPE_CRLF
#define LR_LENGTH 2
#endif

