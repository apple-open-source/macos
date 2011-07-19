/*
 * --------------------------------------------------------------------------
 * tclthread.h --
 *
 * Global header file for the thread extension.
 *
 * Copyright (c) 2002 ActiveState Corporation.
 * Copyright (c) 2002 by Zoran Vasiljevic.
 * 
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: tclThread.h,v 1.23 2010/03/31 08:50:24 vasiljevic Exp $
 * ---------------------------------------------------------------------------
 */

/*
 * Thread extension version numbers are not stored here
 * because this isn't a public export file.
 */

#ifndef _TCL_THREAD_H_
#define _TCL_THREAD_H_

#include <tcl.h>
#include <stdlib.h> /* For strtoul */
#include <string.h> /* For memset and friends */

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

/*
 * For linking against AOLserver require V4 at least
 */

#ifdef NS_AOLSERVER
# include <ns.h>
# if !defined(NS_MAJOR_VERSION) || NS_MAJOR_VERSION < 4
#  error "unsupported AOLserver version"
# endif
#endif

/*
 * Allow for some command names customization.
 * Only thread:: and tpool:: are handled here.
 * Shared variable commands are more complicated.
 * Look into the threadSvCmd.h for more info.
 */

#define THREAD_CMD_PREFIX "thread::"
#define TPOOL_CMD_PREFIX  "tpool::"

/*
 * Exported from threadCmd.c file.
 */

EXTERN int Thread_Init       _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Thread_SafeInit   _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Thread_Unload     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Thread_SafeUnload _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Exported from threadSvCmd.c file.
 */

EXTERN int Sv_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Exported from threadSpCmd.c file.
 */

EXTERN int Sp_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Exported from threadPoolCmd.c file.
 */

EXTERN int Tpool_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Macros for splicing in/out of linked lists
 */

#define SpliceIn(a,b)                          \
    (a)->nextPtr = (b);                        \
    if ((b) != NULL)                           \
        (b)->prevPtr = (a);                    \
    (a)->prevPtr = NULL, (b) = (a)

#define SpliceOut(a,b)                         \
    if ((a)->prevPtr != NULL)                  \
        (a)->prevPtr->nextPtr = (a)->nextPtr;  \
    else                                       \
        (b) = (a)->nextPtr;                    \
    if ((a)->nextPtr != NULL)                  \
        (a)->nextPtr->prevPtr = (a)->prevPtr

/*
 * Utility macros
 */ 

#define TCL_CMD(a,b,c) \
  if (Tcl_CreateObjCommand((a),(b),(c),(ClientData)NULL, NULL) == NULL) \
    return TCL_ERROR

#define OPT_CMP(a,b) \
  ((a) && (b) && (*(a)==*(b)) && (*(a+1)==*(b+1)) && (!strcmp((a),(b))))

#ifndef TCL_TSD_INIT
#define TCL_TSD_INIT(keyPtr) \
  (ThreadSpecificData*)Tcl_GetThreadData((keyPtr),sizeof(ThreadSpecificData))
#endif

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCL_THREAD_H_ */
