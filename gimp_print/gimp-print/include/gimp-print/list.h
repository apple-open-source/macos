/*
 * "$Id: list.h,v 1.1.1.1 2004/07/23 06:26:27 jlovell Exp $"
 *
 *   libgimpprint generic list type
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and Michael Natterer (mitch@gimp.org)
 *   Copyright 2002 Roger Leigh (rleigh@debian.org)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_LIST_H
#define GIMP_PRINT_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


struct stp_list_item;
typedef struct stp_list_item stp_list_item_t;
struct stp_list;
typedef struct stp_list stp_list_t;
typedef void (*stp_node_freefunc)(void *);
typedef void *(*stp_node_copyfunc)(const void *);
typedef const char *(*stp_node_namefunc)(const void *);
typedef int (*stp_node_sortfunc)(const void *, const void *);

extern void stp_list_node_free_data(void *item);
extern stp_list_t *stp_list_create(void);
extern stp_list_t *stp_list_copy(const stp_list_t *list);
extern int stp_list_destroy(stp_list_t *list);
extern stp_list_item_t *stp_list_get_start(const stp_list_t *list);
extern stp_list_item_t *stp_list_get_end(const stp_list_t *list);
extern stp_list_item_t *stp_list_get_item_by_index(const stp_list_t *list,
						   int idx);
extern stp_list_item_t *stp_list_get_item_by_name(const stp_list_t *list,
						  const char *name);
extern stp_list_item_t *stp_list_get_item_by_long_name(const stp_list_t *list,
						       const char *long_name);
extern int stp_list_get_length(const stp_list_t *list);

extern void stp_list_set_freefunc(stp_list_t *list, stp_node_freefunc);
extern stp_node_freefunc stp_list_get_freefunc(const stp_list_t *list);

extern void stp_list_set_copyfunc(stp_list_t *list, stp_node_copyfunc);
extern stp_node_copyfunc stp_list_get_copyfunc(const stp_list_t *list);

extern void stp_list_set_namefunc(stp_list_t *list, stp_node_namefunc);
extern stp_node_namefunc stp_list_get_namefunc(const stp_list_t *list);

extern void stp_list_set_long_namefunc(stp_list_t *list, stp_node_namefunc);
extern stp_node_namefunc stp_list_get_long_namefunc(const stp_list_t *list);

extern void stp_list_set_sortfunc(stp_list_t *list, stp_node_sortfunc);
extern stp_node_sortfunc stp_list_get_sortfunc(const stp_list_t *list);

extern int stp_list_item_create(stp_list_t *list,
				stp_list_item_t *next,
				const void *data);
extern int stp_list_item_destroy(stp_list_t *list,
				 stp_list_item_t *item);
extern stp_list_item_t *stp_list_item_prev(const stp_list_item_t *item);
extern stp_list_item_t *stp_list_item_next(const stp_list_item_t *item);
extern void *stp_list_item_get_data(const stp_list_item_t *item);
extern int stp_list_item_set_data(stp_list_item_t *item,
				  void *data);

#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_LIST_H */
