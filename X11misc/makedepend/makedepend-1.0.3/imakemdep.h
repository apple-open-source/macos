/*

Copyright (c) 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/


/* 
 * This file contains machine-dependent constants for the imake utility.
 * When porting imake, read each of the steps below and add in any necessary
 * definitions.  In general you should *not* edit ccimake.c or imake.c!
 */

#ifdef __UNIXOS2__
#define lstat stat
#endif

#if !defined (CROSSCOMPILE) || defined (CROSSCOMPILE_CPP)
/* predefs:
 *     If your compiler and/or preprocessor define any specific symbols, add
 *     them to the the following table.  The definition of struct symtab is
 *     in util/makedepend/def.h.
 */
#undef DEF_EVALUATE
#undef DEF_STRINGIFY
#define DEF_EVALUATE(__x) #__x
#define DEF_STRINGIFY(_x) DEF_EVALUATE(_x)
static const struct symtab	predefs[] = {
#ifdef apollo
	{"apollo", "1"},
#endif
#if defined(clipper) || defined(__clipper__)
	{"clipper", "1"},
	{"__clipper__", "1"},
	{"clix", "1"},
	{"__clix__", "1"},
#endif
#ifdef ibm032
	{"ibm032", "1"},
#endif
#ifdef ibm
	{"ibm", "1"},
#endif
#ifdef aix
	{"aix", "1"},
#endif
#ifdef sun
	{"sun", "1"},
#endif
#ifdef sun2
	{"sun2", "1"},
#endif
#ifdef sun3
	{"sun3", "1"},
#endif
#ifdef sun4
	{"sun4", "1"},
#endif
#ifdef __sun
	{"__sun", "1"},
#endif
#ifdef __SunOS
	{"__SunOS", "1"},
#endif
#ifdef __SVR4
	{"__SVR4", "1"},
#endif
#ifdef sparc
	{"sparc", "1"},
#endif
#ifdef __sparc
	{"__sparc", "1"},
#endif
#ifdef __sparcv9
	{"__sparcv9", "1"},
#endif
#ifdef __sparc__
	{"__sparc__", "1"},
#endif
#ifdef __sparcv9__
	{"__sparcv9__", "1"},
#endif
#ifdef hpux
	{"hpux", "1"},
#endif
#ifdef __hpux
	{"__hpux", "1"},
#endif
#ifdef __hp9000s800
	{"__hp9000s800", "1"},
#endif
#ifdef __hp9000s700
	{"__hp9000s700", "1"},
#endif
#ifdef vax
	{"vax", "1"},
#endif
#ifdef VMS
	{"VMS", "1"},
#endif
#ifdef cray
	{"cray", "1"},
#endif
#ifdef CRAY
	{"CRAY", "1"},
#endif
#ifdef _CRAY
	{"_CRAY", "1"},
#endif
#ifdef att
	{"att", "1"},
#endif
#ifdef mips
	{"mips", "1"},
#endif
#ifdef __mips__
	{"__mips__", "1"},
#endif
#ifdef ultrix
	{"ultrix", "1"},
#endif
#ifdef stellar
	{"stellar", "1"},
#endif
#ifdef mc68000
	{"mc68000", "1"},
#endif
#ifdef mc68020
	{"mc68020", "1"},
#endif
#ifdef __GNUC__
	{"__GNUC__", DEF_STRINGIFY(__GNUC__)},
#endif
#ifdef __STRICT_ANSI__
	{"__STRICT_ANSI__", "1"},
#endif
#ifdef __STDC__
	{"__STDC__", DEF_STRINGIFY(__STDC__)},
#endif
#ifdef __HIGHC__
	{"__HIGHC__", "1"},
#endif
#ifdef __OPENSERVER__
	{"__OPENSERVER__", DEF_STRINGIFY(__OPENSERVER__)},
#endif
#ifdef _SCO_DS
	{"_SCO_DS", DEF_STRINGIFY(_SCO_DS)},
#endif
#ifdef _SCO_DS_LL
	{"_SCO_DS_LL", DEF_STRINGIFY(_SCO_DS_LL)},
#endif
#ifdef __SCO_VERSION__
	{"__SCO_VERSION__", DEF_STRINGIFY(__SCO_VERSION__)},
#endif
#ifdef __UNIXWARE__
	{"__UNIXWARE__", DEF_STRINGIFY(__UNIXWARE__)},
#endif
#ifdef __USLC__
	{"__USLC__", DEF_STRINGIFY(__USLC__)},
#endif
#ifdef CMU
	{"CMU", "1"},
#endif
#ifdef luna
	{"luna", "1"},
#ifdef luna1
	{"luna1", "1"},
#endif
#ifdef luna2
	{"luna2", "1"},
#endif
#ifdef luna88k
	{"luna88k", "1"},
#endif
#ifdef uniosb
	{"uniosb", "1"},
#endif
#ifdef uniosu
	{"uniosu", "1"},
#endif
#endif
#ifdef ieeep754
	{"ieeep754", "1"},
#endif
#ifdef is68k
	{"is68k", "1"},
#endif
#ifdef m68k
        {"m68k", "1"},
#endif
#ifdef M68k
        {"M68k", "1"},
#endif
#ifdef __m68k__
	{"__m68k__", "1"},
#endif
#ifdef m88k
        {"m88k", "1"},
#endif
#ifdef __m88k__
	{"__m88k__", "1"},
#endif
#ifdef bsd43
	{"bsd43", "1"},
#endif
#ifdef hcx
	{"hcx", "1"},
#endif
#ifdef sony
	{"sony", "1"},
#ifdef SYSTYPE_SYSV
	{"SYSTYPE_SYSV", "1"},
#endif
#ifdef _SYSTYPE_SYSV
	{"_SYSTYPE_SYSV", "1"},
#endif
#endif
#ifdef __OSF__
	{"__OSF__", "1"},
#endif
#ifdef __osf__
	{"__osf__", "1"},
#endif
#ifdef __amiga__
	{"__amiga__", "1"},
#endif
#ifdef __alpha
	{"__alpha", "1"},
#endif
#ifdef __alpha__
	{"__alpha__", "1"},
#endif
#ifdef __DECC
	{"__DECC",  "1"},
#endif
#ifdef __decc
	{"__decc",  "1"},
#endif
#ifdef __unix
	{"__unix", "1"},
#endif
#ifdef __unix__
	{"__unix__", "1"},
#endif
#ifdef __uxp__
	{"__uxp__", "1"},
#endif
#ifdef __sxg__
	{"__sxg__", "1"},
#endif
#ifdef _SEQUENT_
	{"_SEQUENT_", "1"},
	{"__STDC__", "1"},
#endif
#ifdef __bsdi__
	{"__bsdi__", "1"},
#endif
#ifdef nec_ews_svr2
	{"nec_ews_svr2", "1"},
#endif
#ifdef nec_ews_svr4
	{"nec_ews_svr4", "1"},
#endif
#ifdef _nec_ews_svr4
	{"_nec_ews_svr4", "1"},
#endif
#ifdef _nec_up
	{"_nec_up", "1"},
#endif
#ifdef SX
	{"SX", "1"},
#endif
#ifdef nec
	{"nec", "1"},
#endif
#ifdef _nec_ft
	{"_nec_ft", "1"},
#endif
#ifdef PC_UX
	{"PC_UX", "1"},
#endif
#ifdef sgi
	{"sgi", "1"},
#endif
#ifdef __sgi
	{"__sgi", "1"},
#endif
#ifdef _MIPS_FPSET
	{"_MIPS_FPSET", DEF_STRINGIFY(_MIPS_FPSET)},
#endif
#ifdef _MIPS_ISA
	{"_MIPS_ISA", DEF_STRINGIFY(_MIPS_ISA)},
#endif
#ifdef _MIPS_SIM
	{"_MIPS_SIM", DEF_STRINGIFY(_MIPS_SIM)},
#endif
#ifdef _MIPS_SZINT
	{"_MIPS_SZINT", DEF_STRINGIFY(_MIPS_SZINT)},
#endif
#ifdef _MIPS_SZLONG
	{"_MIPS_SZLONG", DEF_STRINGIFY(_MIPS_SZLONG)},
#endif
#ifdef _MIPS_SZPTR
	{"_MIPS_SZPTR", DEF_STRINGIFY(_MIPS_SZPTR)},
#endif
#ifdef __DragonFly__
	{"__DragonFly__", "1"},
#endif
#ifdef __FreeBSD__
	{"__FreeBSD__", "1"},
#endif
#ifdef __OpenBSD__
	{"__OpenBSD__", "1"},
#endif
#ifdef __NetBSD__
	{"__NetBSD__", "1"},
#endif
#ifdef __GNU__
	{"__GNU__", "1"},
#endif
#ifdef __ELF__
	{"__ELF__", "1"},
#endif
#ifdef __UNIXOS2__
	{"__UNIXOS2__", "1"},
#endif
#if defined(__QNX__)
        {"__QNX__", "1"},
#endif
#ifdef __QNXNTO__
        {"__QNXNTO__", "1"},
#endif
# ifdef __powerpc__
	{"__powerpc__", "1"},
# endif
# ifdef __powerpc64__
	{"__powerpc64__", "1"},
# endif
# ifdef PowerMAX_OS
	{"PowerMAX_OS", "1"},
# endif
# ifdef ia64
	{"ia64", "1"},
# endif
# ifdef __ia64__
	{"__ia64__", "1"},
# endif
# if defined (amd64) || defined (x86_64)
	{"amd64", "1"},
	{"x86_64", "1"},
# endif
# if defined (__amd64__) || defined (__x86_64__)
	{"__amd64__", "1"},
	{"__x86_64__", "1"},
# endif
# if defined (__amd64) || defined(__x86_64)
	{"__amd64", "1"},
	{"__x86_64", "1"},
# endif
# ifdef __x86
	{"__x86", "1"},
# endif
# ifdef __i386
	{"__i386", "1"},
# endif
# ifdef __i386__
	{"__i386__", "1"},
# endif
# ifdef __i486__
	{"__i486__", "1"},
# endif
# ifdef __i586__
	{"__i586__", "1"},
# endif
# ifdef __i686__
	{"__i686__", "1"},
# endif
# ifdef __k6__
	{"__k6__", "1"},
# endif
# ifdef i386
	{"i386", "1"},
# endif
# ifdef i486
	{"i486", "1"},
# endif
# ifdef i586
	{"i586", "1"},
# endif
# ifdef i686
	{ "i686", "1"},
# endif
# ifdef k6
	{"k6", "1"},
# endif
# ifdef sparc
	{"sparc", "1"},
# endif
# ifdef __sparc__
	{"__sparc__", "1"},
# endif
# ifdef __s390__
	{"__s390__", "1"},
# endif
# ifdef __hppa__
	{"__hppa__", "1"},
# endif
# ifdef __sh__
	{"__sh__", "1"},
# endif
# ifdef __sh3_
	{"__sh3__", "1"},
# endif
# ifdef __SH3__
	{"__SH3__", "1"},
# endif
# ifdef __SH4__
	{"__SH4__", "1"},
# endif
# ifdef __SH4NOFPU__
	{"__SH4NOFPU__", "1"},
# endif
#if defined(__ppc__)
        {"__ppc__", "1"},
#endif
#if defined(__ppc64__)
        {"__ppc64__", "1"},
#endif
#if defined(__BIG_ENDIAN__)
      {"__BIG_ENDIAN__", "1"},
#endif
#if defined(__LITTLE_ENDIAN__)
      {"__LITTLE_ENDIAN__", "1"},
#endif
#if defined (__CHAR_BIT__)
	{"__CHAR_BIT__", DEF_STRINGIFY(__CHAR_BIT__)},
#endif
#if defined (__BUILTIN_VA_STRUCT)
	{"__BUILTIN_VA_STRUCT", "1"},
#endif
#if defined (__BUILTIN_VA_ARG_INCR)
	{"__BUILTIN_VA_ARG_INCR", "1"},
#endif	
	/* add any additional symbols before this line */
	{NULL, NULL}
};
#undef DEF_EVALUATE
#undef DEF_STRINGIFY
#endif /* CROSSCOMPILE */



