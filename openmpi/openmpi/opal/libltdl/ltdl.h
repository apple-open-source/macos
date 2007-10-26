/* ltdl.h -- generic dlopen functions
   Copyright (C) 1998-2000, 2004, 2005 Free Software Foundation, Inc.
   Originally by Thomas Tanner <tanner@ffii.org>

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
License along with this library; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301  USA
*/

/* Only include this header file once. */
#if !defined(LTDL_H)
#define LTDL_H 1

#include <libltdl/lt_system.h>
#include <libltdl/lt_error.h>
#include <libltdl/lt_dlloader.h>

LT_BEGIN_C_DECLS


/* LT_STRLEN can be used safely on NULL pointers.  */
#define LT_STRLEN(s)	(((s) && (s)[0]) ? strlen (s) : 0)



/* --- DYNAMIC MODULE LOADING API --- */


typedef	void * lt_dlhandle;	/* A loaded module.  */

/* Initialisation and finalisation functions for libltdl. */
LT_SCOPE int	    lt_dlinit		(void);
LT_SCOPE int	    lt_dlexit		(void);

/* Module search path manipulation.  */
LT_SCOPE int	    lt_dladdsearchdir	 (const char *search_dir);
LT_SCOPE int	    lt_dlinsertsearchdir (const char *before,
						  const char *search_dir);
LT_SCOPE int 	    lt_dlsetsearchpath	 (const char *search_path);
LT_SCOPE const char *lt_dlgetsearchpath	 (void);
LT_SCOPE int	    lt_dlforeachfile	 (
			const char *search_path,
			int (*func) (const char *filename, void *data),
			void *data);

/* Portable libltdl versions of the system dlopen() API. */
LT_SCOPE lt_dlhandle lt_dlopen		(const char *filename);
LT_SCOPE lt_dlhandle lt_dlopenext	(const char *filename);
LT_SCOPE void *	    lt_dlsym		(lt_dlhandle handle, const char *name);
LT_SCOPE const char *lt_dlerror		(void);
LT_SCOPE int	    lt_dlclose		(lt_dlhandle handle);

/* Module residency management. */
LT_SCOPE int	    lt_dlmakeresident	(lt_dlhandle handle);
LT_SCOPE int	    lt_dlisresident	(lt_dlhandle handle);




/* --- PRELOADED MODULE SUPPORT --- */


/* A preopened symbol. Arrays of this type comprise the exported
   symbols for a dlpreopened module. */
typedef struct {
  const char *name;
  void       *address;
} lt_dlsymlist;

typedef int lt_dlpreload_callback_func (lt_dlhandle handle);

LT_SCOPE int	lt_dlpreload	     (const lt_dlsymlist *preloaded);
LT_SCOPE int	lt_dlpreload_default (const lt_dlsymlist *preloaded);
LT_SCOPE int	lt_dlpreload_open    (const char *originator,
				      lt_dlpreload_callback_func *func);

#define lt_preloaded_symbols	lt__PROGRAM__LTX_preloaded_symbols
#define LTDL_SET_PRELOADED_SYMBOLS() 			LT_STMT_START{	\
	extern const lt_dlsymlist lt_preloaded_symbols[];		\
	lt_dlpreload_default(lt_preloaded_symbols);			\
							}LT_STMT_END




/* --- MODULE INFORMATION --- */


/* Associating user data with loaded modules. */
typedef void * lt_dlinterface_id;
typedef int lt_dlhandle_interface (lt_dlhandle handle, const char *id_string);

LT_SCOPE lt_dlinterface_id lt_dlinterface_register (const char *id_string,
					  lt_dlhandle_interface *iface);
LT_SCOPE void	lt_dlinterface_free (lt_dlinterface_id key);
LT_SCOPE void *	lt_dlcaller_set_data  (lt_dlinterface_id key,
					  lt_dlhandle handle, void *data);
LT_SCOPE void *	lt_dlcaller_get_data  (lt_dlinterface_id key,
					  lt_dlhandle handle);


/* Read only information pertaining to a loaded module. */
typedef	struct {
  char *	filename;	/* file name */
  char *	name;		/* module name */
  int		ref_count;	/* number of times lt_dlopened minus
				   number of times lt_dlclosed. */
} lt_dlinfo;

LT_SCOPE const lt_dlinfo *lt_dlgetinfo	    (lt_dlhandle handle);

LT_SCOPE lt_dlhandle	lt_dlhandle_iterate (lt_dlinterface_id iface,
					     lt_dlhandle place);
LT_SCOPE lt_dlhandle	lt_dlhandle_fetch   (lt_dlinterface_id iface,
					     const char *module_name);
LT_SCOPE int		lt_dlhandle_map	    (lt_dlinterface_id iface,
				int (*func) (lt_dlhandle handle, void *data),
				void *data);

#define lt_ptr void *

LT_END_C_DECLS

#endif /*!defined(LTDL_H)*/
