/* -*- Mode: c++ -*-
 *
 *  $Id: xotcl.h,v 1.13 2007/09/18 19:27:33 neumann Exp $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 *  This software is based upon MIT Object Tcl by David Wetherall and
 *  Christopher J. Lindblad, that contains the following copyright
 *  message:
 * 
 *  Copyright 1993 Massachusetts Institute of Technology
 * 
 *  Permission to use, copy, modify, distribute, and sell this
 *  software and its documentation for any purpose is hereby granted
 *  without fee, provided that the above copyright notice appear in
 *  all copies and that both that copyright notice and this permission
 *  notice appear in supporting documentation, and that the name of
 *  M.I.T. not be used in advertising or publicity pertaining to
 *  distribution of the software without specific, written prior
 *  permission.  M.I.T. makes no representations about the suitability
 *  of this software for any purpose.  It is provided "as is" without
 *  express or implied warranty.
 * */

#ifndef _xotcl_h_
#define _xotcl_h_

#include "tcl.h"

#undef TCL_STORAGE_CLASS
#ifdef BUILD_xotcl
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_XOTCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

/* use documented interface to link XOTcl state to an interpreter */
#define USE_ASSOC_DATA 1

/* new namespace support (post xotcl 1.2.0) */
#define NAMESPACEINSTPROCS 1

/* activate bytecode support 
#define XOTCL_BYTECODE
*/

#define KEEP_TCL_CMD_TYPE 1

/* activate/deacticate assert 
#define NDEBUG 1
*/
#define NDEBUG 1

/* activate/deacticate memory tracing 
#define XOTCL_MEM_TRACE 1
#define XOTCL_MEM_COUNT 1
*/
/*#define REFCOUNTED 1*/

/*
#define XOTCLOBJ_TRACE 1
#define REFCOUNT_TRACE 1
#define DISPATCH_TRACE 1
*/

/* activate/deacticate profiling information at the end
   of running the program
#define PROFILE
*/

/* make self, proc and class in instproc and procs
#define AUTOVARS
*/

/* turn  tracing output on/off
#define CALLSTACK_TRACE 1
#define DISPATCH_TRACE 1
#define NAMESPACE_TRACE 1
#define OBJDELETION_TRACE 1
#define STACK_TRACE 1
*/

#ifdef XOTCL_MEM_COUNT
# define DO_FULL_CLEANUP 1
#endif

#ifdef AOL_SERVER
# ifndef TCL_THREADS
#  define TCL_THREADS
# endif
#endif

#ifdef TCL_THREADS
# define DO_CLEANUP
#endif

#ifdef DO_FULL_CLEANUP
# define DO_CLEANUP
#endif

/*
 * prevent old TCL-versions
 */

#if TCL_MAJOR_VERSION < 8
# error Tcl distribution is TOO OLD, we require at least tcl8.0
#endif

#if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<1
# define PRE81
#else
# if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION==1
#  define V81
# endif
#endif
#if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<2
# define PRE82
#endif
#if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<3
# define PRE83
#endif
#if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<4
# define PRE84
#endif
#if TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION<5
# define PRE85
#endif

#if !defined(FORWARD_COMPATIBLE)
# if defined(PRE85)
#  define FORWARD_COMPATIBLE 1
# else
#  define FORWARD_COMPATIBLE 0
# endif
#endif

#define XOTCL_NONLEAF_METHOD (ClientData)0x01

/* 
 * A special definition used to allow this header file to be included 
 * in resource files so that they can get obtain version information from
 * this file.  Resource compilers don't like all the C stuff, like typedefs
 * and procedure declarations, that occur below.
 */

#ifndef RC_INVOKED

/*
#ifdef __cplusplus
extern "C" {
#endif
*/


/*
 * The structures XOTcl_Object and XOTcl_Class define mostly opaque 
 * data structures for the internal use strucures XOTclObject and 
 * XOTclClass (both defined in XOTclInt.h). Modification of elements 
 * visible elements must be mirrored in both incarnations.
 */

typedef struct XOTcl_Object {
  Tcl_Obj *cmdName;
} XOTcl_Object;

typedef struct XOTcl_Class {
  struct XOTcl_Object object;
} XOTcl_Class;


/*
 * Include the public function declarations that are accessible via
 * the stubs table.
 */
#include "xotclDecls.h"

/*
 * Xotcl_InitStubs is used by extensions  that can be linked
 * against the xotcl stubs library.  If we are not using stubs
 * then this reduces to package require.
 */

#ifdef USE_XOTCL_STUBS

# ifdef __cplusplus
extern "C"
# endif
CONST char *
Xotcl_InitStubs _ANSI_ARGS_((Tcl_Interp *interp, CONST char *version, int exact));
#else
# define Xotcl_InitStubs(interp, version, exact) \
      Tcl_PkgRequire(interp, "XOTcl", version, exact)
#endif

#endif /* RC_INVOKED */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _xotcl_h_ */
