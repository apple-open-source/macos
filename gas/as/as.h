/* as.h - global header file
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * CAPITALISED names are #defined.
 * "lowercaseT" is a typedef of "lowercase" objects.
 * "lowercaseP" is type "pointer to object of type 'lowercase'".
 * "lowercaseS" is typedef struct ... lowercaseS.
 *
 * #define SUSPECT when debugging.
 * If TEST is #defined, then we are testing a module.
 */

/* These #defines are for parameters of entire assembler. */

/*
 * asserts() from <assert.h> are DISabled when NDEBUG is defined and
 * asserts() from <assert.h> are ENabled  when NDEBUG is undefined.
 * For speed NDEBUG is defined so assert()'s are left out.
#undef NDEBUG
 */
#define NDEBUG

/*
 * For speed SUSPECT is undefined.
#define SUSPECT
 */
#undef SUSPECT

/* These #imports are for type definitions etc. */
#import <stdio.h>
#import <assert.h>
#import <mach/machine.h>

/* These defines are potentially useful */
#undef FALSE
#define FALSE	(0)
#undef TRUE
#define TRUE	(!FALSE)
#define ASSERT	assert

#define BAD_CASE(value)							\
{									\
  as_fatal ("Case value %d unexpected at line %d of file \"%s\"\n",	\
	   value, __LINE__, __FILE__);					\
}

/* These are assembler-wide concepts */
#ifdef SUSPECT
#define register		/* no registers: helps debugging */
#define know(p) ASSERT(p)	/* know() is less ugly than #ifdef SUSPECT/ */
				/* assert()/#endif. */
#else
#define know(p)			/* know() checks are no-op.ed */
#endif


/*
 * main program "as.c" (command arguments etc)
 */

/* ['x'] TRUE if "-x" seen. */
extern char flagseen[128];

/* name of emitted object file, argument to -o if specified */
extern char *out_file_name;

/* TRUE if -force_cpusubtype_ALL is specified */
extern int force_cpusubtype_ALL;

/* set to the corresponding cpusubtype if -arch flag is specified */
extern cpu_subtype_t archflag_cpusubtype;
extern char *specific_archflag;

/* -I path options for .includes */
struct directory_stack {
    struct directory_stack *next;
    char *fname;
};
extern struct directory_stack include_defaults[];
extern struct directory_stack *include;
