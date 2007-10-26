#if __LP64__
#   define mig_external __private_extern__
#   include "iokitmig64.c"
#else
#   include "iokitmig32.c"
#endif
