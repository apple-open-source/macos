// This file implements resolvers to assist dyld in choosing the correct
// implementation of <string.h> routines on ARMv7 processors.  The
// micro-architectural differences between Cortex-A8 and Cortex-A9 are such
// that optimal write loops are quite different on the two processors.
//
// The MakeResolver_a8_a9(name) macro creates a function that dyld calls to
// pick an implementation of the name function.  It does a check to determine
// if it is running on a8 or a9 hardware, and returns a pointer to either
//
//    name$VARIANT$CortexA8
// or
//    name$VARIANT$CortexA9
//
// This resolution only occurs once per process; once a symbol is bound to an
// implementation in dyld, no further calls to the resolver occur.
//
// On unknown implementations of the ARMv7 architecture, the Cortex-A9 variant
// is returned by these resolvers.

#include <arm/arch.h>
#if defined _ARM_ARCH_7 && !defined VARIANT_DYLD

#include <stdlib.h>
#include <machine/cpu_capabilities.h>
#include <mach/machine.h>

#define MakeResolver_a8_a9(name) \
    void * name ## Resolver(void) __asm__("_" #name);\
    void * name ## Resolver(void) {\
        __asm__(".symbol_resolver _" #name);\
        if (*(int *)_COMM_PAGE_CPUFAMILY == CPUFAMILY_ARM_13)\
            return name ## $VARIANT$CortexA8;\
        else\
            return name ## $VARIANT$CortexA9;\
    }

void bcopy$VARIANT$CortexA8(const void *, void *, size_t);
void bcopy$VARIANT$CortexA9(const void *, void *, size_t);
MakeResolver_a8_a9(bcopy)

void *memmove$VARIANT$CortexA8(void *, const void *, size_t);
void *memmove$VARIANT$CortexA9(void *, const void *, size_t);
MakeResolver_a8_a9(memmove)

void *memcpy$VARIANT$CortexA8(void *, const void *, size_t);
void *memcpy$VARIANT$CortexA9(void *, const void *, size_t);
MakeResolver_a8_a9(memcpy)

void bzero$VARIANT$CortexA8(void *, size_t);
void bzero$VARIANT$CortexA9(void *, size_t);
MakeResolver_a8_a9(bzero)

void __bzero$VARIANT$CortexA8(void *, size_t);
void __bzero$VARIANT$CortexA9(void *, size_t);
MakeResolver_a8_a9(__bzero)

void *memset$VARIANT$CortexA8(void *, int, size_t);
void *memset$VARIANT$CortexA9(void *, int, size_t);
MakeResolver_a8_a9(memset)

#else // defined _ARM_ARCH_7 && !defined VARIANT_DYLD

typedef int emptyFilesArentCFiles;

#endif // defined _ARM_ARCH_7 && !defined VARIANT_DYLD
