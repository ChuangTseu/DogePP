//#include "bob.xmlh"
#define BOB_DEF "MICHEL_LE"

#include "alphabet.h"

#define HEY(val /*theValue*/,eEnum /* theEnum   */  ,   /*dummyTesterArg*/ vam /*Reaaly, it's a dummy*/) \
    g_i = hick(val); g_ret = g_eMult * eEnum; while (g_i >= 0)

int main() {
    const char* cb = BOB_DEF;

    #ifdef BOB_DEF
        #define BOB_DEF__ISDEFINED
    #else
            // Popped Comment Plz
        #define BOB_DEF__ISNOTATALLDEFINED
    #endif
}
