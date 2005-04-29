/* 
 * tclXkeylist.h --
 *
 * Extended Tcl keyed list commands and interfaces.
 *-----------------------------------------------------------------------------
 * Copyright 1991-1999 Karl Lehenbauer and Mark Diekhans.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies.  Karl Lehenbauer and
 * Mark Diekhans make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *-----------------------------------------------------------------------------
 *
 * Rcsid: @(#)$Id: tclXkeylist.h,v 1.1 2003/09/03 11:22:24 vasiljevic Exp $
 *-----------------------------------------------------------------------------
 */

#ifndef _KEYLIST_H_
#define _KEYLIST_H_

/* 
 * Keyed list object interface commands
 */

Tcl_Obj* TclX_NewKeyedListObj();

void TclX_KeyedListInit(Tcl_Interp*);
int  TclX_KeyedListGet(Tcl_Interp*, Tcl_Obj*, char*, Tcl_Obj**);
int  TclX_KeyedListSet(Tcl_Interp*, Tcl_Obj*, char*, Tcl_Obj*);
int  TclX_KeyedListDelete(Tcl_Interp*, Tcl_Obj*, char*);
int  TclX_KeyedListGetKeys(Tcl_Interp*, Tcl_Obj*, char*, Tcl_Obj**);

/*
 * Exported for usage in Sv_DuplicateObj. This is slightly
 * modified version of the DupKeyedListInternalRep() function.
 * It does a proper deep-copy of the keyed list object.
 */

void DupKeyedListInternalRepShared(Tcl_Obj*, Tcl_Obj*);

#endif /* _KEYLIST_H_ */

/* EOF $RCSfile: tclXkeylist.h,v $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */

