/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  OSF DCE Version 1.0
 */

#ifndef sysdep_incl
#define sysdep_incl

/*
**
**  NAME
**
**      SYSDEP.H
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**      Operating system and compiler dependencies.
**
**  VERSION: DCE 1.0
**
*/

/*
 *  exit status codes
 */
#   define pgm_ok      0
#   define pgm_warning 2
#   define pgm_error   3

/*
** Macro to test a system-specific status code for failure status.
*/
#define ERROR_STATUS(s) ((s) != 0)

/*
** define HASDIRTREE if OS has foo/widget/bar file system.
** if HASDIRTREE, define BRANCHCHAR and BRANCHSTRING appropriately
** define HASPOPEN if system can do popen()
** define HASINODES if system has real inodes returned by stat()
*/

#if defined(__OSF__) || defined(__OSF1__) || defined(__osf__) || defined(BSD) || defined(SYS5) || defined(ultrix) || defined(_AIX) || defined(__ultrix) || defined(_BSD) || defined(__linux__)
#define UNIX
#define HASDIRTREE
#define HASPOPEN
#define HASINODES
#define BRANCHCHAR '/'
#define BRANCHSTRING "/"
#define CD_IDIR "."
#endif

#ifndef CD_IDIR
Porting Message:  You must provide definitions for the symbols
    describing the directory structure available on your platform.
#endif

/*
 * Default DCE include directory
 */
#ifndef DEFAULT_IDIR
# define DEFAULT_IDIR "/usr/include"
#endif
#define DEFAULT_H_IDIR DEFAULT_IDIR
#define INCLUDE_TEMPLATE "#include <dce/%s>\n"
#define USER_INCLUDE_TEMPLATE "#include <%s>\n"
#define USER_INCLUDE_H_TEMPLATE "#include <%s.h>\n"

/*
 * Default DCE auto import path
 */
#ifndef AUTO_IMPORT_FILE
# define AUTO_IMPORT_FILE "dce/nbase.idl"
#endif

/*
** Default filetype names.
*/
#define OBJ_FILETYPE ".o"

/*
** Commands to invoke C-Preprocessor, C-Compiler etc.
*/
#define CPP 			"/usr/bin/xcrun cc -E -x c-header "
#define CC_DEF_CMD	"/usr/bin/xcrun cc -c  -D_SOCKADDR_LEN -D_GNU_SOURCE -D_REENTRANT -D_POSIX_C_SOURCE=3"

/*
** Default suffixes for IDL-generated files.
*/
#ifdef UNIX
# if ENABLE_DCOM
#  define CSTUB_SUFFIX	"_cstub.cxx"
#  define SSTUB_SUFFIX 	"_sstub.cxx"
#  define SAUX_SUFFIX    "_saux.cxx"
#  define CAUX_SUFFIX    "_caux.cxx"
# else
#  define CSTUB_SUFFIX   	"_cstub.c"
#  define SSTUB_SUFFIX   	"_sstub.c"
#  define CAUX_SUFFIX    	"_caux.c"
#  define SAUX_SUFFIX    	"_saux.c"
# endif
# define HEADER_SUFFIX  	".h"
#endif

#ifndef CSTUB_SUFFIX
Porting Message:  You must provide definitions for the files suffixes to
    be used on your platform.
#endif

/*
 * Template for IDL version text emitted as comment into generated files.
 */
#ifndef IDL_VERSION_TEXT
# if ENABLE_DCOM
#  define IDL_VERSION_TEXT "FreeDCE/DCOM " VERSION " with GNU Flex/Bison"
# else
#  define IDL_VERSION_TEXT "FreeDCE " VERSION " with GNU Flex/Bison"
# endif
#endif
#define IDL_VERSION_TEMPLATE "/* Generated by IDL compiler version %s */\n"

/*
** PASS_I_DIRS_TO_CC determines whether the list of import directories, with
** the system IDL directory replaced by the system H directory if present,
** gets passed as command option(s) to the C compiler when compiling stubs.
*/
#ifndef apollo
# define PASS_I_DIRS_TO_CC
#endif

/*
** Environment variables for IDL system include directories
** on supported platforms.
*/
#ifdef DUMPERS
# define NIDL_LIBRARY_EV "NIDL_LIBRARY"
#endif

/*
** Maximum length of IDL identifiers.  Architecturally specified as 31, but
** on platforms whose C (or other) compilers have more stringent lengths,
** this value might have to be less.
*/
#define MAX_ID 128

/*
** Estimation of available stack size in a server stub.  Under DCE threads
** stack overflow by large amounts can result in indeterminant behavior.  If
** the estimated stack requirements for stack surrogates exceeds the value
** below, objects are allocated via malloc instead of on the stack.
*/
#define AUTO_HEAP_STACK_THRESHOLD 7000

/*
** Symbol for 'audible bell' character.  A workaround for the problem that
** some non-stdc compilers incorrectly map '\a' to 'a'.  Might need work
** on a non-stdc EBCDIC platform.
*/
#if defined(__STDC__)
#define AUDIBLE_BELL '\a'
#define AUDIBLE_BELL_CSTR "\\a"
#else
#define AUDIBLE_BELL '\007'
#define AUDIBLE_BELL_CSTR "\\007"
#endif

/*
** Data type of memory returned by malloc.  In ANSI standard compilers, this
** is a void *, but default to char * for others.
*/
#if defined(__STDC__) || defined(vaxc)
#define heap_mem void
#else
#define heap_mem char
#endif

/*
**  Maximum number of characters in a directory path name for a file.  Used
**  to allocate buffer space for manipulating the path name string.
*/
#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

/*
** Define macros for NLS entry points used only in message.c
*/
#if defined(_AIX)
#       define NL_SPRINTF NLsprintf
#       define NL_VFPRINTF NLvfprintf
#else
#       define NL_SPRINTF sprintf
#       define NL_VFPRINTF vfprintf
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#endif /* sysdep_incl */
