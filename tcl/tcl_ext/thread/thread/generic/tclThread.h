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
 * RCS: @(#) $Id: tclThread.h,v 1.16 2004/07/21 20:53:43 vasiljevic Exp $
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

/*
 * Starting from 8.4 core, Tcl API is CONST'ified.
 * Versions < 8 we do not support anyway.
 */

#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION <= 3)
# define CONST84
#endif

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

/*
 * Allow for some command names customization.
 * Only thread:: and tpool:: are handled here.
 * The shared variable is more complicated.
 * Look into the threadSvCmd.h for more info.
 * The reason for this is that eralier versions
 * of AOLserver do not handle namespaced Tcl
 * commands properly.
 */

#ifdef NS_AOLSERVER
# include <ns.h>
# define THNS "thread_"
# define TPNS "tpool_"
#else
# define THNS "thread::"
# define TPNS "tpool::"
#endif

/*
 * Exported from threadCmd.c file.
 */

EXTERN int  Thread_Init     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int  Thread_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

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
    (a)->prevPtr = NULL, (b) = (a);

#define SpliceOut(a,b)                         \
    if ((a)->prevPtr != NULL)                  \
        (a)->prevPtr->nextPtr = (a)->nextPtr;  \
    else                                       \
        (b) = (a)->nextPtr;                    \
    if ((a)->nextPtr != NULL)                  \
        (a)->nextPtr->prevPtr = (a)->prevPtr;

/*
 * Utility macros
 */ 

#define TCL_CMD(a,b,c) \
  if (Tcl_CreateObjCommand((a),(b),(c),(ClientData)NULL, NULL) == NULL) \
    return TCL_ERROR;

#define OPT_CMP(a,b) \
  ((a) && (b) && (*(a)==*(b)) && (*(a+1)==*(b+1)) && (!strcmp((a),(b))))

#ifndef TCL_TSD_INIT
#define TCL_TSD_INIT(keyPtr) \
  (ThreadSpecificData*)Tcl_GetThreadData((keyPtr),sizeof(ThreadSpecificData))
#endif

/*
 * Some functionality within the package depends on the Tcl version.  We
 * require at least 8.3, and more functionality is available for newer
 * versions, so make compatibility defines to compile against 8.3 and work
 * fully in 8.4+.
 *
 * Thanks to Jeff Hobbs for doing this part.
 *
 */
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 3)

/* 412 */
typedef int 
thread_JoinThread           _ANSI_ARGS_((Tcl_ThreadId id, int* result));

/* 413 */
typedef int
thread_IsChannelShared      _ANSI_ARGS_((Tcl_Channel channel));

/* 414 */
typedef int 
thread_IsChannelRegistered  _ANSI_ARGS_((Tcl_Interp* interp,
                                         Tcl_Channel channel));
/* 415 */
typedef void 
thread_CutChannel           _ANSI_ARGS_((Tcl_Channel channel));

/* 416 */
typedef void 
thread_SpliceChannel        _ANSI_ARGS_((Tcl_Channel channel));

/* 417 */
typedef void 
thread_ClearChannelHandlers _ANSI_ARGS_((Tcl_Channel channel));

/* 418 */
typedef int 
thread_IsChannelExisting    _ANSI_ARGS_((CONST char* channelName));

/*
 * Write up some macros hiding some very hackish pointer arithmetics to get
 * at these fields. We assume that pointer to functions are always of the
 * same size.
 */

#define STUB_BASE   ((char*)(&(tclStubsPtr->tcl_CreateThread))) /* field 393 */
#define procPtrSize (sizeof (Tcl_DriverBlockModeProc *))
#define IDX(n)      (((n)-393) * procPtrSize)
#define SLOT(n)     (STUB_BASE + IDX(n))

#define Tcl_JoinThread           (*((thread_JoinThread**)(SLOT(412))))
#define Tcl_IsChannelShared      (*((thread_IsChannelShared**)(SLOT(413))))
#define Tcl_IsChannelRegistered  (*((thread_IsChannelRegistered**)(SLOT(414))))
#define Tcl_CutChannel           (*((thread_CutChannel**)(SLOT(415))))
#define Tcl_SpliceChannel        (*((thread_SpliceChannel**)(SLOT(416))))
#define Tcl_ClearChannelHandlers (*((thread_ClearChannelHandlers**)(SLOT(417))))
#define Tcl_IsChannelExisting    (*((thread_IsChannelExisting**)(SLOT(418))))

#endif /* 8.3 compile compatibility */

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCL_THREAD_H_ */
