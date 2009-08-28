/* Indirect to real m/ file for universal compilation */
#ifdef __POWERPC__
#include "powermac.h"
#ifndef WORDS_BIG_ENDIAN
#error "Wrong endianess for ppc!"
#endif	/* WORDS_BIG_ENDIAN */
#elif defined(__i386__)
#include "intel386.h"
#define NO_ARG_ARRAY
#ifdef WORDS_BIG_ENDIAN
#error "Wrong endianess for i386!"
#endif	/* WORDS_BIG_ENDIAN */
#elif defined(__x86_64__)
#include "amdx86-64.h"
#undef START_FILES
#undef LIB_STANDARD
#ifdef WORDS_BIG_ENDIAN
#error "Wrong endianess for x86_64!"
#endif	/* WORDS_BIG_ENDIAN */
#else	/* !PPC && !x86 && !x86_64 */
#error "FIXME: Unknown machine architecture"
#endif	/* PPC || x86 || x86_64 */
#undef LD_SWITCH_SYSTEM_TEMACS
#define LD_SWITCH_SYSTEM_TEMACS -Wl,-no_compact_linkedit -nostartfiles -lcrt1.10.5.o
