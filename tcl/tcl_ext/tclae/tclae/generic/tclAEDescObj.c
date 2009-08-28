/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEDescObj.c"
 *                                    created: 10/29/02 {5:58:36 PM} 
 *                                last update: 2/3/04 {6:06:14 PM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *          POMODORO no seisan
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 2002-2004 Jonathan Guyer
 *                      All rights reserved
 * ========================================================================
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that the copyright notice and warranty disclaimer appear in
 * supporting documentation.
 * 
 * The Authors disclaim all warranties with regard to this software,
 * including all implied warranties of merchantability and fitness.  In
 * no event shall the Authors be liable for any special, indirect or
 * consequential damages or any damages whatsoever resulting from loss of
 * use, data or profits, whether in an action of contract, negligence or
 * other tortuous action, arising out of or in connection with the use or
 * performance of this software.
 * 
 * ========================================================================
 *  See header file for further information
 * ###################################################################
 */

#include "tclAEInt.h"
#include "tclMacOSError.h"

#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
#  define TCLAE_CARBON_USE_PRIVATE_AEPRINT 1

#  ifdef TCLAE_CARBON_USE_PRIVATE_AEPRINT
#    include "AEPrintCarbon.h"
#  else
#    include <AEHelpers.h>
#  endif
#else
#  include <string.h>
#  include <AEBuild.h>
#  include "AEPrintCarbon.h"
#endif


/*=========================== AEDesc Tcl_Obj ============================*/

static void	FreeAEDescInternalRep(Tcl_Obj *objPtr);
static void	DupAEDescInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
static int	SetAEDescFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

static Tcl_ObjType tclAEDescType = {
    "AEDesc",			/* name */
    FreeAEDescInternalRep,	/* freeIntRepProc */
    DupAEDescInternalRep,	/* dupIntRepProc */
    TclaeUpdateStringOfAEDesc,	/* updateStringProc */
    SetAEDescFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * FreeAEDescInternalRep --
 *
 *  Frees the resources associated with a AEAddress object's internal
 *  representation.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeAEDescInternalRep(Tcl_Obj *objPtr) /* AEDesc object with internal
					   * representation to free. */
{
    AEDesc *	descPtr = (AEDesc *) objPtr->internalRep.otherValuePtr;
    
    if (descPtr != NULL) {
	AEDisposeDesc(descPtr);
	ckfree((char *) descPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupAEDescInternalRep --
 *
 *  Initialize the internal representation of an AEDesc Tcl_Obj to a
 *  copy of the internal representation of an existing AEDesc object. 
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Internal rep AEDesc of "srcPtr" is duplicated and stored in
 *  "dupPtr".
 *
 *----------------------------------------------------------------------
 */

static void
DupAEDescInternalRep(Tcl_Obj *srcPtr, /* Object with internal rep to copy. */
		     Tcl_Obj *dupPtr) /* Object with internal rep to set. */
{
    dupPtr->internalRep.otherValuePtr = ckalloc(sizeof(AEDesc));
    
    /* no point in checking the result because we have no way to report it */
    AEDuplicateDesc((const AEDesc *) srcPtr->internalRep.otherValuePtr, 
		    (AEDesc *) dupPtr->internalRep.otherValuePtr);
    
    dupPtr->typePtr = &tclAEDescType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetAEDescFromAny --
 *
 *  Generate an AEDesc internal form for the Tcl object "objPtr".
 *
 * Results:
 *  The return value is a standard Tcl result. The conversion always
 *  succeeds and TCL_OK is returned.
 *
 * Side effects:
 *  A pointer to an AEDesc built from objPtr's string rep
 *  is stored as objPtr's internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetAEDescFromAny(Tcl_Interp * interp, /* Used for error reporting if not NULL. */
		 Tcl_Obj *    objPtr) /* The object to convert. */
{
    OSStatus		err;
    AEDesc *		newAEDescPtr = (AEDesc *) ckalloc(sizeof (AEDesc));
    Tcl_DString		gizmoDS;	/* for conversion of AEGizmo from UTF 
               		        	 * to external */
    
    /* 
     * objPtr is an AEGizmos string.
     * Convert string from UTF and build it.
     */
    Tcl_UtfToExternalDString(tclAE_macRoman_encoding, Tcl_GetString(objPtr), -1, &gizmoDS);
#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
    err = AEBuildDesc(newAEDescPtr, NULL, Tcl_DStringValue(&gizmoDS));
#else
    err = AEBuild(newAEDescPtr, Tcl_DStringValue(&gizmoDS));
#endif
    Tcl_DStringFree(&gizmoDS);
    
    if (err != noErr) {
	ckfree((char *) newAEDescPtr);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't build descriptor: ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;				
    } else {
	/*
	 * Free the old internalRep before setting the new one. We do this as
	 * late as possible to allow the conversion code, in particular
	 * GetStringFromObj, to use that old internalRep.
	 */
	
	if ((objPtr->typePtr != NULL)
	&&  (objPtr->typePtr->freeIntRepProc != NULL)) {
	    objPtr->typePtr->freeIntRepProc(objPtr);
	}
	
	objPtr->internalRep.otherValuePtr = (VOID *) newAEDescPtr;
	objPtr->typePtr = &tclAEDescType;
	
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclaeUpdateStringOfAEDesc --
 *
 *  Update the string representation for an AEDesc object.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  The object's string is set to a valid string that results from
 *  the conversion.
 *
 *----------------------------------------------------------------------
 */

void
TclaeUpdateStringOfAEDesc(Tcl_Obj *objPtr) /* AEDesc obj with string rep to update. */
{
    const AEDesc *	descPtr = objPtr->internalRep.otherValuePtr;
    OSErr		err;		/* result from ToolBox calls */
    long		gizmoSize;	/* size of AEGizmo string */
    Tcl_DString 	gizmoDS;	/* for UTF conversion of AEGizmo */
    Handle		textH = NULL;
    
    /* Determine size of AEGizmo representation of theAEDescPtr (length+1) */
    err = AEPrintSize(descPtr, &gizmoSize);
    if (err == noErr) {
#if !TARGET_API_MAC_CARBON || defined(TCLAE_CARBON_USE_PRIVATE_AEPRINT) 
	textH = NewHandle(gizmoSize);
	err = AEPrint(descPtr, *textH, gizmoSize);
#else
	err = AEPrintDescToHandle(descPtr, &textH);
#endif
    }
	
    if (err == noErr) {
	/* Set DString to UTF of AEGizmo string */
	HLock(textH);
	Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, *textH, gizmoSize-1, &gizmoDS);
    } else {
	Tcl_DStringInit(&gizmoDS);
	Tcl_DStringAppend(&gizmoDS, "Couldn't print AEDesc: ", -1);
	Tcl_DStringAppend(&gizmoDS, Tcl_MacOSError(NULL, err), -1);
    }
    
    if (textH) {
	DisposeHandle(textH);
    }
    
    objPtr->length = Tcl_DStringLength(&gizmoDS);
    objPtr->bytes = (char *) ckalloc(objPtr->length + 1);
    memcpy(objPtr->bytes, Tcl_DStringValue(&gizmoDS), objPtr->length + 1);
    
    Tcl_DStringFree(&gizmoDS);
    
}

/*======================== const AEDesc Tcl_Obj =========================*/

static int	SetConstAEDescFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

static Tcl_ObjType tclConstAEDescType = {
    "const AEDesc",		/* name */
    NULL,			/* freeIntRepProc */
    DupAEDescInternalRep,	/* dupIntRepProc */
    TclaeUpdateStringOfAEDesc,	/* updateStringProc */
    SetConstAEDescFromAny	/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * SetConstAEDescFromAny --
 *
 *  Throw an error on any attempt to set a const AEDesc
 *
 * Results:
 *  TCL_ERROR
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
SetConstAEDescFromAny(Tcl_Interp * interp, /* Used for error reporting if not NULL. */
		      Tcl_Obj *    objPtr) /* The object to convert. */
{
    if (interp) {
	Tcl_AppendResult(interp, "cannot (re)build object of type \"",
			 tclConstAEDescType.name, "\"", NULL);
    }
    return TCL_ERROR;
}

/*====================== AEDesc reference Tcl_Obj =======================*/

static void	FreeAEDescRefInternalRep(Tcl_Obj *objPtr);
static void	DupAEDescRefInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *dupPtr);
static int	SetAEDescRefFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void	UpdateStringOfAEDescRef(Tcl_Obj *objPtr);

static Tcl_ObjType tclAEDescRefType = {
    "AEDesc reference",		/* name */
    FreeAEDescRefInternalRep,	/* freeIntRepProc */
    DupAEDescRefInternalRep,	/* dupIntRepProc */
    UpdateStringOfAEDescRef,	/* updateStringProc */
    SetAEDescRefFromAny		/* setFromAnyProc */
};

/*
 *----------------------------------------------------------------------
 *
 * FreeAEDescRefInternalRep --
 *
 *  Frees the resources associated with a AEAddress object's internal
 *  representation.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeAEDescRefInternalRep(Tcl_Obj *objPtr) /* AEDesc reference object with internal
					   * representation to free. */
{
    Tcl_DecrRefCount((Tcl_Obj *) objPtr->internalRep.otherValuePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DupAEDescRefInternalRep --
 *
 *  Initialize the internal representation of an AEDesc reference Tcl_Obj
 *  to a copy of the internal representation of an existing AEDesc
 *  reference object.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Internal rep AEDesc reference of "srcPtr" is duplicated and stored in
 *  "dupPtr".
 *
 *----------------------------------------------------------------------
 */

static void
DupAEDescRefInternalRep(Tcl_Obj *srcPtr, /* Object with internal rep to copy. */
			Tcl_Obj *dupPtr) /* Object with internal rep to set. */
{
    Tcl_Obj *	contents = srcPtr->internalRep.otherValuePtr;
    
    dupPtr->internalRep.otherValuePtr = contents;
    Tcl_IncrRefCount(contents);
    dupPtr->typePtr = &tclAEDescRefType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetAEDescRefFromAny --
 *
 *  Generate an AEDesc internal form for the Tcl object "objPtr".
 *
 * Results:
 *  The return value is a standard Tcl result. The conversion always
 *  succeeds and TCL_OK is returned.
 *
 * Side effects:
 *  A pointer to an AEDesc built from objPtr's string rep
 *  is stored as objPtr's internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetAEDescRefFromAny(Tcl_Interp * interp, /* Used for error reporting if not NULL. */
		 Tcl_Obj *    objPtr) /* The object to convert. */
{
    int			result = TCL_OK;
    Tcl_Obj *		newAEDescObj;
    
    newAEDescObj = Tcl_NewStringObj(Tcl_GetString(objPtr), -1);
    
    result = Tcl_ConvertToType(interp, newAEDescObj, &tclAEDescType);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(newAEDescObj);
	return TCL_ERROR;				
    } else {
	/*
	 * Free the old internalRep before setting the new one. We do this as
	 * late as possible to allow the conversion code, in particular
	 * GetStringFromObj, to use that old internalRep.
	 */
	
	if ((objPtr->typePtr != NULL)
	&&  (objPtr->typePtr->freeIntRepProc != NULL)) {
	    objPtr->typePtr->freeIntRepProc(objPtr);
	}
	
	Tcl_IncrRefCount(newAEDescObj);
	objPtr->internalRep.otherValuePtr = newAEDescObj;
	objPtr->typePtr = &tclAEDescRefType;
	
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfAEDescRef --
 *
 *  Update the string representation for an AEDesc object.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  The object's string is set to a valid string that results from
 *  the conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfAEDescRef(Tcl_Obj *objPtr) /* AEDesc obj with string rep to update. */
{
    Tcl_Obj *	contents = objPtr->internalRep.otherValuePtr;
    char *	bytes;
    
    Tcl_InvalidateStringRep(contents);
    bytes = Tcl_GetStringFromObj(contents, &objPtr->length);
    objPtr->bytes = (char *) ckalloc(objPtr->length+1);
    memcpy(objPtr->bytes, bytes, objPtr->length + 1);
}

/*========================== external routines ==========================*/

Tcl_Obj *
Tclae_NewAEDescObj(AEDesc *descPtr)
{
    Tcl_Obj *	newObj = Tcl_NewObj();
    
    if (newObj) {
	Tcl_InvalidateStringRep(newObj);
	newObj->internalRep.otherValuePtr = descPtr;
	newObj->typePtr = &tclAEDescType;
    }
    
    return newObj;
}

Tcl_Obj *
Tclae_NewAEDescRefObj(AEDesc *descPtr)
{
    Tcl_Obj *	newObj = Tcl_NewObj();
    
    if (newObj) {
	Tcl_InvalidateStringRep(newObj);
	newObj->internalRep.otherValuePtr = Tclae_NewAEDescObj(descPtr);
	newObj->typePtr = &tclAEDescRefType;
    }
    
    return newObj;
}

Tcl_Obj *
Tclae_NewConstAEDescObj(const AEDesc *descPtr)
{
    Tcl_Obj *	newObj = Tcl_NewObj();
    
    if (newObj) {
	Tcl_InvalidateStringRep(newObj);
	newObj->internalRep.otherValuePtr = (VOID *) descPtr;
	newObj->typePtr = &tclConstAEDescType;
    }
    
    return newObj;
}

Tcl_Obj *
Tclae_NewConstAEDescRefObj(const AEDesc *descPtr)
{
    Tcl_Obj *	newObj = Tcl_NewObj();
    
    if (newObj) {
	Tcl_InvalidateStringRep(newObj);
	newObj->internalRep.otherValuePtr = Tclae_NewConstAEDescObj(descPtr);
	newObj->typePtr = &tclAEDescRefType;
    }
    
    return newObj;
}

void
TclaeDetachAEDescObj(Tcl_Obj *	obj)
{
    if (obj->typePtr == &tclAEDescRefType) {
	TclaeDetachAEDescObj((Tcl_Obj *) obj->internalRep.otherValuePtr);
    } else if (obj->typePtr == &tclAEDescType
	||	obj->typePtr == &tclConstAEDescType) {
	obj->internalRep.otherValuePtr = NULL;
    }
}

int
Tclae_GetAEDescObjFromObj(Tcl_Interp *interp,	/* Used for error reporting if not NULL. */
			  Tcl_Obj *objPtr,		/* The object from which to get a int. */
			  Tcl_Obj **descObjHdl,	/* Place to store resulting AEDesc. */
			  int parseGizmo)	/* If not already an AEDesc, parse string form as an AEGizmo? */
{
    int		result = TCL_OK;
    
    *descObjHdl = objPtr;
    
    if (objPtr->typePtr == &tclAEDescRefType) {
	*descObjHdl = (Tcl_Obj *) objPtr->internalRep.otherValuePtr;
    } else if (objPtr->typePtr != &tclAEDescType
	&&	objPtr->typePtr != &tclConstAEDescType) {
		if (parseGizmo) {
			result = SetAEDescFromAny(interp, *descObjHdl);
		} else {
			result = TCL_ERROR;
		}
    }
    
    return result;
}

int
Tclae_GetAEDescFromObj(Tcl_Interp *interp,	/* Used for error reporting if not NULL. */
		       Tcl_Obj *objPtr,		/* The object from which to get a int. */
		       AEDesc **descPtr,	/* Place to store resulting AEDesc. */
		       int parseGizmo)		/* If not already an AEDesc, parse string form as an AEGizmo? */
{
    int		result = TCL_OK;
    Tcl_Obj *	descObj;
    
    *descPtr = NULL;
    result = Tclae_GetAEDescObjFromObj(interp, objPtr, &descObj, parseGizmo);
    if (result == TCL_OK) {
	if (descObj->typePtr == &tclConstAEDescType) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Constant AEDesc \"",
			     Tcl_GetString(objPtr), 
			     "\" cannot be modified ",
			     (char *) NULL);
	    result = TCL_ERROR;
	} else {
	    *descPtr = (AEDesc *) descObj->internalRep.otherValuePtr;
	}
    }
    
    return result;
}
int
Tclae_GetConstAEDescFromObj(Tcl_Interp *interp,		/* Used for error reporting if not NULL. */
			    Tcl_Obj *objPtr,		/* The object from which to get a int. */
			    const AEDesc **descPtr,	/* Place to store resulting AEDesc. */
			    int parseGizmo)		/* If not already an AEDesc, parse string form as an AEGizmo? */
{
    int		result = TCL_OK;
    Tcl_Obj *	descObj;
    
    *descPtr = NULL;
    result = Tclae_GetAEDescObjFromObj(interp, objPtr, &descObj, parseGizmo);
    if (result == TCL_OK) {
	*descPtr = (const AEDesc *) descObj->internalRep.otherValuePtr;
    }
    
    return result;
}
int
Tclae_ConvertToAEDesc(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    return Tcl_ConvertToType(interp, objPtr, &tclAEDescType);
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeInitAEDescs" --
 * 
 *  Initialize the AEDesc Tcl object types, allowing Tcl to 
 *  communicate with the AppleEvent manager.
 * 
 * Results:
 *  None.
 * 
 * Side effects:
 *  tclAEDescType, tclConstAEDescType, & tclAEDescRefType are registered.
 * -------------------------------------------------------------------------
 */
void
TclaeInitAEDescs()
{
    Tcl_RegisterObjType(&tclAEDescType);
    Tcl_RegisterObjType(&tclConstAEDescType);
    Tcl_RegisterObjType(&tclAEDescRefType);
}


