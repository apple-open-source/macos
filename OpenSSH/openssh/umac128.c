//
//  umac128.c
//  Portable OpenSSH uses a makefile and -D options from Makefile.in:
//	    -DUMAC_OUTPUT_LEN=16 -Dumac_new=umac128_new \
//      -Dumac_update=umac128_update -Dumac_final=umac128_final \
//      -Dumac_delete=umac128_delete
//
//  to create umac128.o from umac.c.
//
//  For our Xcode-based project, we do this:
//

#define UMAC_OUTPUT_LEN (16)

#define umac_new umac128_new
#define umac_update umac128_update
#define umac_final umac128_final
#define umac_delete umac128_delete


#include "umac.c"
