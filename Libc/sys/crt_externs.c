/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
** This file contains interfaces to the symbols defined int the crt modules.
** 3 April 1995
** Matt Watson (mwatson@next.com)
**
*/

#if defined(__DYNAMIC__)
#include "mach-o/dyld.h" /* defines _dyld_lookup_and_bind() */
#define STRINGIFY(a) # a
#define DECLARE_VAR(var, type)					\
        static type * var ## _pointer = 0
#define DECLARE_PROGNAME(var, type)				\
        static type * var ## _pointer = 0;			\
        static type _priv_ ## var = 0
#define SETUP_VAR(var)						\
    if ( var ## _pointer == 0) {				\
        _dyld_lookup_and_bind( STRINGIFY(_ ## var),		\
                (unsigned long *) & var ## _pointer, 0);	\
    }
#define SETUP_PROGNAME(var)					\
    if ( var ## _pointer == 0) {				\
	if(NSIsSymbolNameDefined( STRINGIFY(_ ## var) ))	\
	    _dyld_lookup_and_bind( STRINGIFY(_ ## var),		\
                (unsigned long *) & var ## _pointer, 0);	\
	else {							\
	    char *progname = _dyld_get_image_name(0);		\
	    if(_priv_ ## var = strrchr(progname, '/'))		\
		_priv_ ## var ++;				\
	    else						\
		_priv_ ## var = progname;			\
	    var ## _pointer = & _priv_ ## var;			\
	}							\
    }
#define USE_VAR(var) (var ## _pointer)
#else
#define DECLARE_VAR(var, type) extern type var
#define DECLARE_PROGNAME(var, type) DECLARE_VAR(var, type)
#define SETUP_VAR(var)
#define SETUP_PROGNAME(var) SETUP_VAR(var)
#define USE_VAR(var) (& var)
#endif

char ***_NSGetArgv(void) {
    DECLARE_VAR(NXArgv, char **);
    SETUP_VAR(NXArgv);
    return(USE_VAR(NXArgv));
}

int *_NSGetArgc(void) {
    DECLARE_VAR(NXArgc, int);
    SETUP_VAR(NXArgc);
    return(USE_VAR(NXArgc));
}

char ***_NSGetEnviron(void) {
    DECLARE_VAR(environ, char **);
    SETUP_VAR(environ);
    return(USE_VAR(environ));
}

char **_NSGetProgname(void) {
    DECLARE_PROGNAME(__progname, char *);
    SETUP_PROGNAME(__progname);
    return(USE_VAR(__progname));
}

struct mach_header *_NSGetMachExecuteHeader(void) {
    DECLARE_VAR(_mh_execute_header, struct mach_header);
    SETUP_VAR(_mh_execute_header);
    return(USE_VAR(_mh_execute_header));
}

/*
 * Fix for Radar bug 2200596 --
 * EH symbol definitions for gcc 2.7.2.x implementation of
 * C++ exception handling.  The problem:  the EH implementation
 * has "throw" store stuff into these pointers, and then as the
 * stack is unwound, the code generated into each function (for
 * checking whether this function contains a relevant "catch"
 * clause and for calling destructors for local variables) looks
 * at these globals to find the type and value thrown.
 *
 * The problem was that the compiler generated the symbols as
 * "common" symbols, and common symbols cannot be placed in
 * dynamic shared libraries.  So we must put these guys as
 * "data" symbols into crt0.o or the System Framework (library),
 * and the compiler must generate code that defines the symbols
 * as external references instead of common.
 *
 * I changed the symbol names (added the "_272") to be utterly
 * paranoid about any possible future use of similar names by
 * any future versions of gcc.
 *                      -- D. Landauer, Jan. 1998
 */

void *__eh_pc_gcc_272 = (void *)0;
void *__eh_type_gcc_272 = (void *)0;
void *__eh_value_gcc_272 = (void *)0;

/* This is what egcs uses for its global data pointer */
void *__eh_global_dataptr = (void *)0;
