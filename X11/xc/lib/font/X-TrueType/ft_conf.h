/* ft_conf.h.  Xserver-specific version. */

/* $XFree86: xc/lib/font/X-TrueType/ft_conf.h,v 1.1 2003/11/20 04:03:43 dawes Exp $ */

/* we need the following because there are some typedefs in this file */
#ifndef FT_CONF_H
#define FT_CONF_H

#include <X11/Xmd.h>
#include "servermd.h"
#include "fontmisc.h"
#include <X11/Xfuncproto.h>

#ifndef FONTMODULE

#include <X11/Xos.h>
#include <string.h>
#include <stdio.h>
#ifdef _XOPEN_SOURCE
#include <math.h>
#else
#define _XOPEN_SOURCE   /* to get prototype for hypot on some systems */
#include <math.h>
#undef _XOPEN_SOURCE
#endif
/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

#else

#include "xf86_ansic.h"

#endif /* FONTMODULE */

/* Define to empty if the keyword does not work.  */
#define const _Xconst           /* defined in Xfuncproto.h */

/* Define if you have a working `mmap' system call.  */
/* Defined in Makefile */
/* #undef HAVE_MMAP */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* defined in servermd.h */
#if(IMAGE_BYTE_ORDER==MSBFirst)
#define WORDS_BIGENDIAN 1
#else
#define WORDS_BIGENDIAN 0
#endif


/* Define if the X Window System is missing or not being used.	*/
/* Not relevant. */
/* #undef X_DISPLAY_MISSING */

/* Define if you have the getpagesize function.  */
#ifdef HAVE_MMAP
#define HAVE_GETPAGESIZE 1
#endif

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1           /* provided by Xos.h */

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1          /* provided by Xos.h */

/* Define if you have the <fcntl.h> header file.  */
#undef HAVE_FCNTL_H             /* included by Xos.h if relevant*/

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H            /* included by Xos.h if relevant */

/* Define if you have the <locale.h> header file.  */
/* Not relevant */
/* #undef HAVE_LOCALE_H */

/* Define if you have the <libintl.h> header file.  */
/* Not relevant */
/* #undef HAVE_LIBINTL_H */

/* Define if you have the libintl library.  */
/* Not relevant */
/* #undef HAVE_LIBINTL */

/**********************************************************************/
/*                                                                    */
/*  The following configuration macros can be tweaked manually by     */
/*  a developer to turn on or off certain features or options in the  */
/*  TrueType engine. This may be useful to tune it for specific       */
/*  purposes..                                                        */
/*                                                                    */
/**********************************************************************/

/*************************************************************************/
/* Define this if the underlying operating system uses a different       */
/* character width than 8bit for file names.  You must then also supply  */
/* a typedef declaration for defining 'TT_Text'.  Default is off.        */

/* #undef HAVE_TT_TEXT */


/*************************************************************************/
/* Define this if you want to generate code to support engine extensions */
/* Default is on, but if you're satisfied by the basic services provided */
/* by the engine and need no extensions, undefine this configuration     */
/* macro to save a few more bytes.                                       */

/* #undef  TT_CONFIG_OPTION_EXTEND_ENGINE */


/*************************************************************************/
/* Define this if you want to generate code to support gray-scaling,     */
/* a.k.a. font-smoothing or anti-aliasing. Default is on, but you can    */
/* disable it if you don't need it.                                      */

#undef  TT_CONFIG_OPTION_GRAY_SCALING

/*************************************************************************/
/* Define this if you want to completely disable the use of the bytecode */
/* interpreter.  Doing so will produce a much smaller library, but the   */
/* quality of the rendered glyphs will enormously suffer from this.      */
/*                                                                       */
/* This switch was introduced due to the Apple patents issue which       */
/* emerged recently on the FreeType lists.  We still do not have Apple's */
/* opinion on the subject and will change this as soon as we have.       */

/* #undef   TT_CONFIG_OPTION_NO_INTERPRETER */

#ifndef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#define TT_CONFIG_OPTION_NO_INTERPRETER
#endif

/*************************************************************************/
/* Define this if you want to use a big 'switch' statement within the    */
/* bytecode interpreter. Because some non-optimizing compilers are not   */
/* able to produce jump tables from such statements, undefining this     */
/* configuration macro will generate the appropriate C jump table in     */
/* ttinterp.c. If you use an optimizing compiler, you should leave it    */
/* defined for better performance and code compactness..                 */

#define  TT_CONFIG_OPTION_INTERPRETER_SWITCH

/*************************************************************************/
/* Define this if you want to build a 'static' version of the TrueType   */
/* bytecode interpreter. This will produce much bigger code, which       */
/* _may_ be faster on some architectures..                               */
/*                                                                       */
/* Do NOT DEFINE THIS is you build a thread-safe version of the engine   */
/*                                                                       */
/* #undef TT_CONFIG_OPTION_STATIC_INTERPRETER */

/*************************************************************************/
/* Define this if you want to build a 'static' version of the scan-line  */
/* converter (the component which in charge of converting outlines into  */
/* bitmaps). This will produce a bigger object file for "ttraster.c",    */
/* which _may_ be faster on some architectures..                         */
/*                                                                       */
/* Do NOT DEFINE THIS is you build a thread-safe version of the engine   */
/*                                                                       */
/* #define TT_CONFIG_OPTION_STATIC_RASTER */

/*************************************************************************/
/* Define TT_CONFIG_THREAD_SAFE if you want to build a thread-safe       */
/* version of the library.                                               */

/* #undef  TT_CONFIG_OPTION_THREAD_SAFE */

/**********************************************************************/
/*                                                                    */
/*  The following macros are used to define the debug level, as well  */
/*  as individual tracing levels for each component. There are        */
/*  currently three modes of operation :                              */
/*                                                                    */
/*  - trace mode (define DEBUG_LEVEL_TRACE)                           */
/*                                                                    */
/*      The engine prints all error messages, as well as tracing      */
/*      ones, filtered by each component's level                      */
/*                                                                    */
/*  - debug mode (define DEBUG_LEVEL_ERROR)                           */
/*                                                                    */
/*      Disable tracing, but keeps error output and assertion         */
/*      checks.                                                       */
/*                                                                    */
/*  - release mode (don't define anything)                            */
/*                                                                    */
/*      Don't include error-checking or tracing code in the           */
/*      engine's code. Ideal for releases.                            */
/*                                                                    */
/* NOTE :                                                             */
/*                                                                    */
/*   Each component's tracing level is defined in its own source.     */
/*                                                                    */
/**********************************************************************/

/* Define if you want to use the tracing debug mode */
/* #undef  DEBUG_LEVEL_TRACE */

/* Define if you want to use the error debug mode - ignored if */
/* DEBUG_LEVEL_TRACE is defined                                */
/* #undef  DEBUG_LEVEL_ERROR */

/**************************************************************************/
/* Definition of various integer sizes. These types are used by ttcalc    */
/* and ttinterp (for the 64-bit integers) only..                          */

/* Use X-specific configuration methods */

  typedef INT32      TT_Int32;
  typedef CARD32     TT_Word32;

#if defined(WORD64) || defined(_XSERVER64)
#define LONG64
#define INT64   long
#endif /* WORD64 */

#endif /* FT_CONF_H */
