/* Apple-specific wrapper to work around differences between 32 and 64 bit opensslconf.h */

#if _WIN64
#include <openssl/opensslconf64.h>
#else
#include <openssl/opensslconf32.h>
#endif
