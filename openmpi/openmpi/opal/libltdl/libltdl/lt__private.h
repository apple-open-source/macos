/* lt__private.h -- internal apis for libltdl
   Copyright (C) 2004, 2005, 2006 Free Software Foundation, Inc.
   Originally by Gary V. Vaughan  <gary@gnu.org>

   NOTE: The canonical source of this file is maintained with the
   GNU Libtool package.  Report bugs to bug-libtool@gnu.org.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

As a special exception to the GNU Lesser General Public License,
if you distribute this file as part of a program or library that
is built using GNU libtool, you may include it under the same
distribution terms that you use for the rest of that program.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301  USA

*/

#if !defined(LT__PRIVATE_H)
#define LT__PRIVATE_H 1

#if defined(LT_CONFIG_H)
#  include LT_CONFIG_H
#else
#  include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined(HAVE_STRING_H)
#  include <string.h>
#else
#  if defined(HAVE_STRINGS_H)
#    include <strings.h>
#  endif
#endif
#if defined(HAVE_MEMORY_H)
#  include <memory.h>
#endif

/* Import internal interfaces...  */
#include "lt__alloc.h"
#include "lt__dirent.h"
#include "lt__strl.h"
#include "lt__glibc.h"

/* ...and all exported interfaces.  */
#include "ltdl.h"

#if defined(WITH_DMALLOC)
#  include <dmalloc.h>
#endif

#if defined(DLL_EXPORT)
#  define LT_GLOBAL_DATA	__declspec(dllexport)
#else
#  define LT_GLOBAL_DATA
#endif

#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8) || __STRICT_ANSI__
#  define __attribute__(x)
# endif
#endif

#ifndef LT__UNUSED
# define LT__UNUSED __attribute__ ((__unused__))
#endif


LT_BEGIN_C_DECLS

#if !defined(errno)
extern int errno;
#endif

LT_SCOPE void	lt__alloc_die_callback (void);


/* For readability:  */
#define strneq(s1, s2)	(strcmp((s1), (s2)) != 0)
#define streq(s1, s2)	(!strcmp((s1), (s2)))



/* --- OPAQUE STRUCTURES DECLARED IN LTDL.H --- */

/* This type is used for the array of interface data sets in each handler. */
typedef struct {
  lt_dlinterface_id	key;
  void *		data;
} lt_interface_data;

typedef struct lt__handle lt__handle;

struct lt__handle {
  lt__handle *		next;
  const lt_dlvtable *	vtable;		/* dlopening interface */
  lt_dlinfo		info;		/* user visible fields */
  int			depcount;	/* number of dependencies */
  lt_dlhandle *		deplibs;	/* dependencies */
  lt_module		module;		/* system module handle */
  void *		system;		/* system specific data */
  lt_interface_data *	interface_data;	/* per caller associated data */
  int			flags;		/* various boolean stats */
};



/* --- ERROR HANDLING --- */

/* Extract the diagnostic strings from the error table macro in the same
   order as the enumerated indices in lt_error.h. */

#define LT__STRERROR(name)	lt__error_string(LT_CONC(LT_ERROR_,name))

#define LT__GETERROR(lvalue)	      (lvalue) = lt__get_last_error()
#define LT__SETERRORSTR(errormsg)     lt__set_last_error(errormsg)
#define LT__SETERROR(errorcode)	      LT__SETERRORSTR(LT__STRERROR(errorcode))

LT_SCOPE const char *lt__error_string	(int errorcode);
LT_SCOPE const char *lt__get_last_error	(void);
LT_SCOPE const char *lt__set_last_error	(const char *errormsg);

LT_END_C_DECLS

#endif /*!defined(LT__PRIVATE_H)*/
