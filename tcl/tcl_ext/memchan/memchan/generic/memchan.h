/* memchan.h - Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * Public API to Memchan
 *
 * $Id: memchan.h,v 1.1 2004/11/09 23:11:00 patthoyts Exp $
 *
 */

#ifndef _MEMCHAN_H_INCLUDE 
#define _MEMCHAN_H_INCLUDE

#include <tcl.h>

/*
 * Windows needs to know which symbols to export.  Unix does not.
 * BUILD_Memchan should be undefined for Unix.
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_Memchan
#define TCL_STORAGE_CLASS DLLEXPORT
#else
#ifdef USE_MEMCHAN_STUBS
#define TCL_STORAGE_CLASS
#else
#define TCL_STORAGE_CLASS DLLIMPORT
#endif /* USE_MEMCHAN_STUBS */
#endif /* BUILD_Memchan */


#ifdef __cplusplus
extern "C" {
#endif

#include "memchanDecls.h"

#ifdef USE_MEMCHAN_STUBS
EXTERN CONST char * 
Memchan_InitStubs(Tcl_Interp *interp, CONST char *version, int exact);
#endif

#ifdef __cplusplus
}
#endif /* C++ */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _MEMCHAN_H_INCLUDE */
