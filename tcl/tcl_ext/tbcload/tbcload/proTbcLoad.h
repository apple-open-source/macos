/*
 * proTbcLoad.h --
 *
 *  Declarations of the interfaces exported by the tbcload package.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: proTbcLoad.h,v 1.4 2000/10/31 23:30:51 welch Exp $
 */

#ifndef _PROTBCLOAD_H
# define _PROTBCLOAD_H

# include "tcl.h"

# ifdef BUILD_tbcload
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
# endif

/*
 *----------------------------------------------------------------
 * Procedures exported by cmpRead.c and cmpRPkg.c
 *----------------------------------------------------------------
 */

EXTERN int	Tbcload_EvalObjCmd _ANSI_ARGS_((ClientData dummy,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN int	Tbcload_ProcObjCmd _ANSI_ARGS_((ClientData dummy,
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
EXTERN CONST char *
		TbcloadGetPackageName _ANSI_ARGS_((void));

EXTERN int	Tbcload_Init _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int	Tbcload_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _PROTBCLOAD_H */
