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
#else	/* !PPC && !x86 */
#error "FIXME: Unknown machine architecture"
#endif	/* PPC || x86 */
