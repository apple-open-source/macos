/*
 * (c) Copyright 1992, 1993 by Panagiotis Tsirigotis
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */


/*
 * $Id: sioconf.h,v 1.1.1.3 2003/05/22 01:16:35 rbraun Exp $
 */

/*
 * This file has 2 sections:
 *		1. a OS-specific section
 *		2. a CPU/compiler-specific section
 *
 * You can override/redefine any of the constants/macros in this file.
 * by uncommenting the inclusion of customconf.h and placing your own
 * definitions in that file.
 */

/* #include "customconf.h" */



/*
 * OS-specific section.
 *
 * Features here use the flag HAVE_<feature>.
 * List of flags (check the following for macros that can be overridden):
 *
 *			HAVE_MMAP (overridable macros)
 *
 *			HAVE_ATEXIT
 *			HAVE_ONEXIT
 *			HAVE_OTHER_FINALIZER	(must define macros)
 *
 *			HAVE_MEMCPY
 *			HAVE_BCOPY 			(HAVE_MEMCPY will be preferred if both are defined)
 *
 *	At least one of the following flags must be defined. The 2nd and 3rd
 * flags are incompatible.
 *			HAVE_ISATTY
 *			HAVE_SYSVTTY
 *			HAVE_BSDTTY
 */

/*
 * Memory mapping.
 *		The library requires 3 macros: SIO_MMAP, SIO_MUNMAP, SIO_MNEED.
 *		You can selectively override any of them.
 *		Notice that the SIO_MNEED macro is not required. If your system
 *		does not have madvise, you can define the macro as:
 *			#define SIO_MNEED( addr, len )
 */
#ifdef HAVE_MMAP

#if !defined( SIO_MMAP ) || !defined( SIO_MUNMAP ) || !defined( SIO_MNEED )
#include <sys/types.h>
#include <sys/mman.h>
#endif

#ifndef SIO_MMAP
#define SIO_MMAP( addr, len, fd, off )                                  \
         mmap( addr, len, PROT_READ,                                    \
            ( addr == 0 ) ? MAP_PRIVATE : MAP_PRIVATE + MAP_FIXED,      \
               fd, off )
#endif

#ifndef SIO_MUNMAP
#define SIO_MUNMAP( addr, len )  munmap( addr, len )
#endif

#ifndef SIO_MNEED
#if defined(linux)
#define SIO_MNEED( addr, len )
#else
#define SIO_MNEED( addr, len )      (void) madvise( addr, len, MADV_WILLNEED )
#endif
#endif

#endif	/* HAVE_MMAP */

/*
 * N_SIO_DESCRIPTORS is the maximum number of file descriptors
 * supported by the OS
 */
#include <sys/param.h>
#ifdef OPEN_MAX
#define N_SIO_DESCRIPTORS		OPEN_MAX
#else
#define N_SIO_DESCRIPTORS		NOFILE
#endif



/*
 * Finalization function. 
 *
 * The purpose of this function is to do work after your program has
 * called exit(3). In the case of SIO, this means flushing the SIO
 * output buffers.
 *
 * If your system does not support atexit or onexit but has some other
 * way of installing a finalization function, you define the flag 
 * HAVE_FINALIZER. Then you must define the macros 
 *			SIO_FINALIZE and SIO_DEFINE_FIN
 *
 * SIO_FINALIZE attempts to install a finalization function and returns TRUE 
 * if successful, FALSE if unsuccessful.
 * SIO_DEFINE_FIN defines the finalization function (the reason for this macro
 * s that different systems pass different number/type of arguments to the
 * finalization function; the SIO finalization function does not use any
 * arguments).
 */
#if defined(HAVE_ONEXIT) || defined(HAVE_ATEXIT) || defined(HAVE_FINALIZER)

#define HAVE_FINALIZATION_FUNCTION

#if defined( HAVE_ONEXIT ) && defined( HAVE_ATEXIT )
#undef HAVE_ONEXIT
#endif

#ifdef HAVE_ONEXIT
#define SIO_FINALIZE( func )        ( on_exit( func, (caddr_t) 0 ) == 0 )
#define SIO_DEFINE_FIN( func )      static void func ( exit_status, arg )  \
                                          int exit_status ;                \
                                          caddr_t arg ;
#endif	/* HAVE_ONEXIT */

#ifdef HAVE_ATEXIT
#define SIO_FINALIZE( func )        ( atexit( func ) == 0 )
#define SIO_DEFINE_FIN( func )      static void func ()
#endif	/* HAVE_ATEXIT */

#endif	/* HAVE_ONEXIT || HAVE_ATEXIT || HAVE_FINALIZER */


/*
 * HAVE_MEMCPY should be defined if your OS supports the mem* functions
 * (memcpy etc). If not, then you can define HAVE_BCOPY if your OS supports 
 * bcopy.
 */
#if defined( HAVE_MEMCPY ) && defined( HAVE_BCOPY )
#undef HAVE_BCOPY
#endif


/*
 * Support for the isatty(3) function. This function identifies if a 
 * descriptor refers to a terminal.
 *
 * Case 1: isatty(3) is in the C library
 *		--> define HAVE_ISATTY
 *	Case 2: no isatty(3), BSD 4.3 tty handling
 *		--> define HAVE_BSDTTY
 * Case 3: no isatty(3), System V tty handling
 *		--> define HAVE_SYSVTTY
 *
 * The following code checks:
 *		1) that at least one of the flags is defined
 *		2) only one of the BSD, SYS V flags is defined
 */
#if !defined(HAVE_ISATTY) && !defined(HAVE_BSDTTY) && !defined(HAVE_SYSVTTY)
 #error function_isatty_not_available ;
#endif

#ifdef HAVE_ISATTY
#undef HAVE_BSDTTY
#undef HAVE_SYSVTTY
#endif

#if defined(HAVE_BSDTTY) && defined(HAVE_SYSVTTY)
 #error HAVE_BSDTTY_and_HAVE_SYSVTTY_both_defined ;
#endif



/*
 * CPU/compiler-specific section.
 *
 * The following constant affects the behavior of Sprint.
 *
 * Sprint performs integer->string conversions by first converting
 * the integer to the widest int type supported by the CPU/compiler.
 * By default, this is the "long int" type. If your machine has
 * a wider type, you can specify it by defining the WIDE_INT constant.
 * For example:
 *		#define WIDE_INT					long long
 */

