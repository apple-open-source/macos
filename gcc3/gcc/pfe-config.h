/* APPLE LOCAL file PFE */
/* Memory management definitions to simplify building pfe and non-pfe
   enabled compilers.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Originally most of these were defined from the Makefile as -D's.
   They have been moved here to simplify the Makefile generated
   build lines and also to allow other than 1-to-1 correspondence 
   in the actual function definitions (e.g., we have a "kind" of
   allocation argument to the PFE allocators which may or may not
   be used).
   
   The configure.in file (and hence configure) always add pfe-config.h
   to the host_xm_file definition.  This causes pfe-config.h to always
   be included from config.h.  So this file is always required to build
   gcc.  This the reason it is glocated in the gcc directory and not in
   the pfe directory.  */
   
#ifdef PFE

/* A non-zero value of PFE_MALLOC_STATS will enable the tracking of
   mallocs by kind and size.  This is a relatively-expensive
   operation, so it should only be enabled when information on
   malloc patterns is desired.  This should be turned off for a
   production compiler.  */
#define PFE_MALLOC_STATS 1

#define DEF_PFE_ALLOC(sym, str) sym,

/* PFE Allocation kinds passed to the PFE memory allocators.    */
enum pfe_alloc_object_kinds {
#include "pfe-config.def"
  PFE_ALLOC_NBR_OF_KINDS
};

#undef DEF_PFE_ALLOC

#if PFE_MALLOC_STATS
#define PFE_MALLOC(size, kind)		pfe_s_malloc (size, kind)
#define PFE_CALLOC(n, size, kind)	pfe_s_calloc (n, size, kind)
#define PFE_REALLOC(p, size, kind)	pfe_s_realloc (p, size, kind)
#else
#define PFE_MALLOC(size, kind)		pfe_malloc (size)
#define PFE_CALLOC(n, size, kind)	pfe_calloc (n, size)
#define PFE_REALLOC(p, size, kind)	pfe_realloc (p, size)
#endif
#define PFE_FREE			pfe_free
#define PFE_SAVESTRING			pfe_savestring
#define PFE_VARRAY			"(pfe)"

#define GGC_ALLOC(size, kind)		pfe_ggc_alloc (size, kind)
#define GGC_ALLOC_CLEARED(size, kind)	pfe_ggc_alloc_cleared (size, kind)
#define GGC_ALLOC_RTX(nslots)		pfe_ggc_alloc_rtx (nslots)
#define GGC_ALLOC_RTVEC(nelt)		pfe_ggc_alloc_rtvec (nelt)
#define GGC_ALLOC_TREE(length)		pfe_ggc_alloc_tree (length)
#define GGC_ALLOC_STRING(s, len)	pfe_ggc_alloc_string (s, len)
#define GGC_STRDUP(s)			pfe_ggc_strdup (s)

/* Disable the following block of #define's when the pfe_ggc_...
   routines are defined to accept the "kind" argument.  */
#if 1
#define pfe_ggc_alloc(size, kind)	   ggc_alloc (size)
#define pfe_ggc_alloc_cleared(size, kind)  ggc_alloc_cleared (size)
#define pfe_ggc_alloc_rtx(nslots)	   ggc_alloc_rtx (nslots)
#define pfe_ggc_alloc_rtvec(nelt)	   ggc_alloc_rtvec (nelt)
#define pfe_ggc_alloc_tree(length)   	   ggc_alloc_tree (length)
#define pfe_ggc_alloc_string(s, len) 	   ggc_alloc_string (s, len)
#define pfe_ggc_strdup(s) 	   	   ggc_strdup (s)
#else
struct rtx_def;
struct rtvec_def;
union tree_node;
extern void *pfe_ggc_alloc		     PARAMS ((int, enum pfe_alloc_object_kinds));
extern void *pfe_ggc_alloc_cleared	     PARAMS ((int, enum pfe_alloc_object_kinds));
extern struct rtx_def *pfe_ggc_alloc_rtx     PARAMS ((int));
extern struct rtvec_def *pfe_ggc_alloc_rtvec PARAMS ((int));
extern union tree_node *pfe_ggc_alloc_tree   PARAMS ((int));
extern const char *pfe_ggc_alloc_string	     PARAMS ((const char *, int));
extern const char *pfe_ggc_strdup	     PARAMS ((const char *));
#endif

#else

#define PFE_MALLOC(size, kind)		xmalloc (size)
#define PFE_CALLOC(n, size, kind)	xcalloc (n, size)
#define PFE_REALLOC(p, size, kind)	xrealloc (p, size)
#define PFE_FREE			free
#define PFE_SAVESTRING
#define PFE_VARRAY
   
#define GGC_ALLOC(size, kind)		ggc_alloc (size)
#define GGC_ALLOC_CLEARED(size, kind)	ggc_alloc_cleared (size)
#define GGC_ALLOC_RTX(nslots)		ggc_alloc_rtx (nslots)
#define GGC_ALLOC_RTVEC(nelt)		ggc_alloc_rtvec (nelt)
#define GGC_ALLOC_TREE(length)		ggc_alloc_tree (length)
#define GGC_ALLOC_STRING(s, len)	ggc_alloc_string (s, len)
#define GGC_STRDUP(s)			ggc_strdup (s)

#endif
