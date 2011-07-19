/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEDesc.c"
 *                                    created: 1/20/2000 {10:47:47 PM} 
 *                                last update: 7/30/10 {11:54:40 PM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 2000-2004 Jonathan Guyer
 *                      All rights reserved
 * ========================================================================
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that the copyright notice and warranty disclaimer appear in
 * supporting documentation.
 * 
 * Jonathan Guyer disclaims all warranties with regard to this software,
 * including all implied warranties of merchantability and fitness.  In
 * no event shall Jonathan Guyer be liable for any special, indirect or
 * consequential damages or any damages whatsoever resulting from loss of
 * use, data or profits, whether in an action of contract, negligence or
 * other tortuous action, arising out of or in connection with the use or
 * performance of this software.
 * ========================================================================
 *  See header file for further information
 * ###################################################################
 */

#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
#include <AEHelpers.h>
#else
#include <AEBuild.h>
#include "AEPrintCarbon.h"
#endif
#endif

#include <string.h>

#ifdef MAC_TCL
#include <tclMacInt.h>
#endif

#include "tclAEInt.h"
#include "tclMacOSError.h"

static CmdReturn *rawFromAEDesc(Tcl_Interp *interp, const AEDesc *theAEDescPtr);
static CmdReturn *dataFromAEDesc(Tcl_Interp *interp, const AEDesc *theAEDescPtr);



/* ◊◊◊◊ Public package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CoerceDataCmd" --
 * 
 *  Tcl wrapper for ToolBox AECoercePtr call. 
 *  It doesn't really mean anything to pass a pointer in Tcl, so pass the
 *  data directly.
 *
 *  tclAE::coerceData <typeCode> <data> <toType>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to new AEDesc object
 * -------------------------------------------------------------------------
 */
int 
Tclae_CoerceDataCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */   
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr       err;				/* result from ToolBox calls */
    void*	dataPtr;			/* pointer to data */
    Size	dataSize;			/* length of data */
    AEDesc*	toAEDescPtr;			/* pointer to coerced AEDesc */ 
    OSType      typeCode;			/* type code of original data */
    
    enum {
	kTypeCode = 1,
	kData,
	kToType
    };
    
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "typeCode data toType");
	return TCL_ERROR;
    }
    
    typeCode = TclaeGetOSTypeFromObj(objv[kTypeCode]);
    
    /* Extract <data> */
    dataPtr = TclaeDataFromObj(interp, typeCode, objv[kData], &dataSize);
    if (dataPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* Allocate the coerced AEDesc */
    toAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
    if (toAEDescPtr == NULL) {
	ckfree(dataPtr);
	return TCL_ERROR;
    }
    
    /* Create an empty AEDesc with of type <typeCode> */		
    err = AECreateDesc(typeNull, NULL, 0L, toAEDescPtr);

    
    /* Coerce data to <toType> and return object for new AEDesc */
    err = AECoercePtr(typeCode, 
		      dataPtr, dataSize,
		      TclaeGetOSTypeFromObj(objv[kToType]), 
		      toAEDescPtr);
    
    ckfree(dataPtr);
    
    if (err != noErr) {
	ckfree((char *)toAEDescPtr);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't coerce |",
			 Tcl_GetString(objv[kData]), "| from '",
			 Tcl_GetString(objv[kTypeCode]), "' to '",
			 Tcl_GetString(objv[kToType]), "': ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;
    } else {
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(toAEDescPtr));
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CoerceDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AECoerceDesc call. 
 *
 *  tclAE::coerceDesc <theAEDesc> <toType>
 *
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for coerced AEDesc
 * -------------------------------------------------------------------------
 */
int 
Tclae_CoerceDescCmd(ClientData clientData,	/* (unused) */  
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr       	err;			/* result from ToolBox calls */
    int			result;			/* result from Tcl calls */
    const AEDesc *	fromAEDescPtr;		/* the original AEDesc */
    AEDesc *		toAEDescPtr = NULL;	/* pointer to coerced AEDesc */
    
    enum {
	kAEDesc = 1,
	kToType
    };
    
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc toType");
	return TCL_ERROR;
    }
    
    /* objv[1] holds original descriptor */
    result = Tclae_GetConstAEDescFromObj(interp, objv[kAEDesc], &fromAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;				
    }
    
    /* Allocate the coerced AEDesc */
    toAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
    if (toAEDescPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* Coerce the AEDesc to the desired type */
    err = AECoerceDesc(fromAEDescPtr, 
		       TclaeGetOSTypeFromObj(objv[kToType]), 
		       toAEDescPtr);	
    
    if (err != noErr) {
	ckfree((char *)toAEDescPtr);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't coerce descriptor to '",
			 Tcl_GetString(objv[kToType]), "': ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;
    } else {	
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(toAEDescPtr));
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CountItemsCmd" --
 * 
 *  Tcl wrapper for ToolBox AECountItems call. 
 *
 *  tclAE::countItems <theAEDescList>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to number of items
 * -------------------------------------------------------------------------
 */
int 
Tclae_CountItemsCmd(ClientData clientData,	/* (unused) */  
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr		err;			/* result from ToolBox calls */		
    int			result;			/* result from Tcl calls */		
    long		count;			/* number of items in AEDescList */		
    const AEDesc *	theAEDescListPtr;	/* pointer to AEDescList */	
    
    
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList");
	return TCL_ERROR;
    }
    
    /* Obtain AEDescList pointer from object */
    result = Tclae_GetConstAEDescFromObj(interp, objv[1], &theAEDescListPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    } 
    
    /* Count items in list (or return error if not a list) */
    err = AECountItems(theAEDescListPtr, &count);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, 
			 "Couldn't count items in \"", 
			 Tcl_GetString(objv[1]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;				
    } 
    
    Tcl_SetObjResult(interp, Tcl_NewIntObj(count));
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CreateDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AECreateDesc call. 
 *
 *  tclAE::createDesc <typeCode> ?data?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for new AEDesc
 * -------------------------------------------------------------------------
 */
int 
Tclae_CreateDescCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr	err;			/* result from ToolBox calls */		
    void*	dataPtr = NULL;		/* pointer to data */
    Size	dataSize = 0;		/* length of data */
    AEDesc*	newAEDescPtr;		/* pointer to new AEDesc */	
    OSType      typeCode;		/* type of AEDesc to create */
    
    enum {
	kTypeCode = 1,
	kData
    };
    
    if ((objc < 2) || (objc > 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "typeCode ?data?");
	return TCL_ERROR;
    }
    
    typeCode = TclaeGetOSTypeFromObj(objv[kTypeCode]);
    
    if (objc == 3) {
	/* Extract <data> */
	dataPtr = TclaeDataFromObj(interp, typeCode, objv[kData], &dataSize);
	
	if (!dataPtr) {
	    return TCL_ERROR;
	}
    }
    
    /* create space for new AEDesc */
    newAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
    if (newAEDescPtr == NULL) {
	ckfree(dataPtr);
	return TCL_ERROR;
    }	
    
    /* Create an empty AEDesc with of type <typeCode> */		
    err = AECreateDesc(typeCode, dataPtr, dataSize, newAEDescPtr);
    
    ckfree(dataPtr);
    
    if (err != noErr) {
	ckfree((char *)newAEDescPtr);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't create descriptor: ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {				
	/* Set interp's result to a object to newAEDescPtr */
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(newAEDescPtr));
	
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CreateListCmd" --
 * 
 *  Tcl wrapper for ToolBox AECreateList call. 
 *
 *  tclAE::createList ?isRecord?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for new AEDescList
 * -------------------------------------------------------------------------
 */
int 
Tclae_CreateListCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr		err;			/* result from ToolBox calls */		
    AEDescList *	newAEDescListPtr;	/* pointer to new AEDescList */		
    int			isRecord = false;    	/* flag for AERecord or AEDescList */
    
    if ((objc < 1) || (objc > 2)) {
	Tcl_WrongNumArgs(interp, 1, objv, "?isRecord?");
	return TCL_ERROR;
    }
    
    if (objc == 2) {
	int		result;
	
	/* Read flag for whether to create AERecord or AEDescList */
	result = Tcl_GetBooleanFromObj(interp, objv[1], &isRecord);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	} 
    }
    
    /* Allocate space for new AEDescList */
    newAEDescListPtr = (AEDescList *) ckalloc(sizeof(AEDescList));
    if (newAEDescListPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* 
     * AECreateDesc() wants a Boolean (unsigned char), but Tcl_GetBooleanFromObj()
     * takes the address of an integer. (sigh)
     */
    err = AECreateList(NULL, 0, (Boolean) isRecord, newAEDescListPtr);		
    if (err != noErr) {
	ckfree((char *)newAEDescListPtr);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't create AEDescList: ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	
	return TCL_ERROR;		
    } else {	
	/* Set interp's result to a object to newAEDescListPtr */
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(newAEDescListPtr));
	
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_DeleteItemCmd" --
 * 
 *  Tcl wrapper for ToolBox AEDeleteItem call 
 *
 *  tclAE::deleteItem <theAEDescList> <item>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Item is deleted from AEDescList
 * -------------------------------------------------------------------------
 */
int 
Tclae_DeleteItemCmd(ClientData clientData,	/* (unused) */  
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr	err;			/* result from ToolBox calls */		
    int		result;			/* result from Tcl calls */				
    AEDesc	*theAEDescListPtr;	/* pointer to AEDescList */	
    int		index;			/* index of item to delete */
    
    
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList item");
	return TCL_ERROR;
    }
    
    /* Obtain AEDescList pointer from object */
    result = Tclae_GetAEDescFromObj(interp, objv[1], &theAEDescListPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;				
    } 
    
    /* Read index of item to delete */
    result = Tcl_GetIntFromObj(interp, objv[2], &index);
    if (result != TCL_OK) {
	return TCL_ERROR;
    } 
    
    /* Delete nth item
     * Tcl is 0-based, but AEDescLists are 1-based.
     */
    err = AEDeleteItem(theAEDescListPtr, index + 1);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, 
			 "Couldn't delete item from \"", 
			 Tcl_GetString(objv[1]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;				
    } else {
	Tcl_InvalidateStringRep(objv[1]);
	Tcl_SetObjResult(interp, objv[1]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_DeleteKeyDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEDeleteKeyDesc call 
 *
 *  tclAE::deleteItem <theAERecord> <theAEKeyword>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Key item is deleted from AERecord
 * -------------------------------------------------------------------------
 */
int 
Tclae_DeleteKeyDescCmd(ClientData clientData,	/* (unused) */ 
		       Tcl_Interp *interp,	/* for results */  
		       int objc,		/* number of arguments */  
		       Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr		err;		/* result from ToolBox calls */		
    int			result;		/* result from Tcl calls */		
    AERecord	*theAERecordPtr;	/* pointer to AERecord */	
    
    
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAERecord theAEKeyword");
	return TCL_ERROR;
    }
    
    /* Obtain AERecord pointer from object */
    result = Tclae_GetAEDescFromObj(interp, objv[1], &theAERecordPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;				
    } 
    
    /* Delete the key item */
    err = AEDeleteKeyDesc(theAERecordPtr, TclaeGetOSTypeFromObj(objv[2]));
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't delete keyword '",
			 Tcl_GetString(objv[2]), "' from \"", 
			 Tcl_GetString(objv[1]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;				
    } 
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_DisposeDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEDisposeDesc call 
 *
 *  tclAE::disposeDesc <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Descriptor is deleted
 * -------------------------------------------------------------------------
 */
int 
Tclae_DisposeDescCmd(ClientData clientData,	/* (unused) */ 
		     Tcl_Interp *interp,	/* for results */  
		     int objc,			/* number of arguments */  
		     Tcl_Obj *const objv[])	/* argument objects */
{
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc");
	return TCL_ERROR;
    }
    
    Tcl_DecrRefCount(objv[1]);
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_DuplicateDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEDuplicateDesc call 
 *
 *  tclAE::duplicateDesc <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for duplicate AEDesc
 * -------------------------------------------------------------------------
 */
int 
Tclae_DuplicateDescCmd(ClientData clientData,	/* (unused) */ 
		       Tcl_Interp *interp,	/* for results */ 
		       int objc,		/* number of arguments */  
		       Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr		err;		/* result from ToolBox calls */
    int			result;		/* result from Tcl calls */
    const AEDesc *	oldAEDescPtr;	/* pointer to old AEDesc */
    AEDesc *		newAEDescPtr;	/* pointer to new AEDesc */
    
    enum {
	kAEDesc = 1
    };
    
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc");
	return TCL_ERROR;
    }
    
    /* Obtain AEDesc from object */
    result = Tclae_GetConstAEDescFromObj(interp, objv[kAEDesc], &oldAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    newAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
    if (newAEDescPtr) {
	err = AEDuplicateDesc(oldAEDescPtr, newAEDescPtr);
	if (err != noErr) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Couldn't duplicate \"",
			     Tcl_GetString(objv[kAEDesc]), "\": ",
			     Tcl_MacOSError(interp, err), 
			     (char *) NULL);
	    ckfree((char *) newAEDescPtr);
	    return TCL_ERROR;				
	} else {
	    /* Set interp's result to object of newAEDescPtr */
	    Tcl_SetObjResult(interp, Tclae_NewAEDescObj(newAEDescPtr));
	    
	    return TCL_OK;
	}
    } else {
	return TCL_ERROR;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetAttributeDataCmd" --
 * 
 *  Tcl emulator for ToolBox AEGetAttributePtr call 
 *
 *  tclAE::getAttributeData <theAppleEvent> <theAEKeyword> ?desiredType? ?typeCodePtr?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to text representation of AppleEvent attribute item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetAttributeDataCmd(ClientData clientData,	/* (unused) */ 
			  Tcl_Interp *interp,		/* for results */ 
			  int objc,			/* number of arguments */ 
			  Tcl_Obj *const objv[])	/* argument objects */
{
    int		result;				/* result from Tcl calls */				
    CmdReturn *	returnStructPtr;		/* result from internal calls */
    AEDesc	tempAEDesc;			/* temporary AEDesc from record */
    Tcl_Obj *	desiredTypePtr = NULL;		/* optional type to cast AEDesc to */
    Tcl_Obj *	typeCodeVarPtr = NULL;		/* optional name of type code variable */
    
    
    if ((objc < 3) || (objc > 5)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAppleEvent theAEKeyword ?desiredType? ?typeCodePtr?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	desiredTypePtr = objv[3];
	if (objc > 4) {
	    /* Optional Tcl variable to store (coerced) AEDesc type */
	    typeCodeVarPtr = objv[4];
	}			
    } 
    
    /* objv[1] holds AppleEvent object */
    /* objv[2] holds attribute keyword */
    result = TclaeGetAttributeDesc(interp, objv[1], objv[2], 
				   NULL, &tempAEDesc);
    if (result != TCL_OK) {
	return TCL_ERROR;
    } 
    
    /* Obtain data from AEDesc */
    returnStructPtr = TclaeDataFromAEDesc(interp, &tempAEDesc, 
					  desiredTypePtr, typeCodeVarPtr);
    AEDisposeDesc(&tempAEDesc);
    
    if (returnStructPtr->object != NULL) {
	Tcl_SetObjResult(interp, returnStructPtr->object);
    }
    result = returnStructPtr->status;
    ckfree((char *)returnStructPtr);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetAttributeDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetAttributeDesc call 
 *
 *  tclAE::getKeyDesc <theAppleEvent> <theAEKeyword> ?desiredType?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for key item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetAttributeDescCmd(ClientData clientData,	/* (unused) */ 
			  Tcl_Interp *interp,		/* for results */ 
			  int objc,			/* number of arguments */ 
			  Tcl_Obj *const objv[])	/* argument objects */
{
    int		result;				/* result from Tcl calls */				
    AEDesc *	theAEDescPtr;			/* pointer to new AEDesc */
    Tcl_Obj *	desiredTypePtr = NULL;		/* optional type to cast AEDesc to */
    
    
    if ((objc < 3) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAppleEvent theAEKeyword ?desiredType?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	desiredTypePtr = objv[3];
    } 
    /* Allocate space for new AEDesc */
    theAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));	
    if (theAEDescPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* objv[1] holds AppleEvent object */
    /* objv[2] holds attribute keyword */
    result = TclaeGetAttributeDesc(interp, objv[1], objv[2], 
				   desiredTypePtr, theAEDescPtr);
    if (result == TCL_OK) {
	/* Set interp's result to a reference to theAEDescPtr */
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(theAEDescPtr));	
	
	return TCL_OK;
    } else {
	ckfree((char *)theAEDescPtr);
	return TCL_ERROR;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetDataCmd" --
 * 
 *  Tcl access for theAEDesc.dataHandle
 *
 *  tclAE::getData <theAEDesc> ?desiredType? ?typeCodePtr?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to text representation of AEDesc
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetDataCmd(ClientData clientData,	/* (unused) */ 
		 Tcl_Interp *interp,	/* for results */ 
		 int objc,		/* number of arguments */ 
		 Tcl_Obj *const objv[])	/* argument objects */
{
    int			result;			/* result from Tcl calls */		
    CmdReturn *		returnStructPtr;	/* result from internal calls */
    const AEDesc *	theAEDescPtr;		/* pointer to new AEDesc */	
    Tcl_Obj *		typeCodeVarPtr = NULL;	/* optional name of type code variable */
    Tcl_Obj *		desiredTypePtr = NULL;	/* optional type to cast AEDesc to */
    
    
    if ((objc < 2) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc ?desiredType? ?typeCodePtr?");
	return TCL_ERROR;
    }
    
    result = Tclae_GetConstAEDescFromObj(interp, objv[1], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    if (objc > 2) {
	/* Optional desired type */
	desiredTypePtr = objv[2];
	if (objc > 3) {
	    /* Optional Tcl variable to store (coerced) AEDesc type */
	    typeCodeVarPtr = objv[3];
	}
    }
    
    /* Obtain (optionally coerced) data from AEDesc */
    returnStructPtr = TclaeDataFromAEDesc(interp, theAEDescPtr, 
					  desiredTypePtr, typeCodeVarPtr);
    
    if (returnStructPtr->object != NULL) {
	Tcl_SetObjResult(interp, returnStructPtr->object);
    }
    result = returnStructPtr->status;	
    ckfree((char *)returnStructPtr);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetDescTypeCmd" --
 * 
 *  Tcl access for theAEDesc.descriptorType
 *
 *  tclAE::getDescType <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to DescType of AEDesc
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetDescTypeCmd(ClientData clientData,	/* (unused) */  
		     Tcl_Interp *interp,	/* for results */
		     int objc,			/* number of arguments */  
		     Tcl_Obj *const objv[])	/* argument objects */
{
    int			result;			/* result from Tcl calls */
    const AEDesc *	theAEDescPtr;		/* pointer to AEDesc */			
    
    
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc");
	return TCL_ERROR;
    }
    
    result = Tclae_GetConstAEDescFromObj(interp,objv[1], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Convert descriptor type to UTF and place in interp result */
    Tcl_SetObjResult(interp, TclaeNewOSTypeObj(theAEDescPtr->descriptorType));
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetKeyDataCmd" --
 * 
 *  Tcl emulator for ToolBox AEGetKeyPtr call 
 *
 *  tclAE::getKeyData <theAERecord> <theAEKeyword> ?desiredType? ?typeCodePtr?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to text representation of AERecord key item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetKeyDataCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */ 
		    int objc,				/* number of arguments */ 
		    Tcl_Obj *const objv[])		/* argument objects */
{
    int			result;						/* result from Tcl calls */				
    CmdReturn	*returnStructPtr;			/* result from internal calls */
    AEDesc      tempAEDesc;					/* temporary AEDesc from record */
    Tcl_Obj		*desiredTypePtr = NULL;		/* optional type to cast AEDesc to */
    Tcl_Obj		*typeCodeVarPtr = NULL;		/* optional name of type code variable */
    
    
    if ((objc < 3) || (objc > 5)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAERecord theAEKeyword ?desiredType? ?typeCodePtr?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	desiredTypePtr = objv[3];
	if (objc > 4) {
	    /* Optional Tcl variable to store (coerced) AEDesc type */
	    typeCodeVarPtr = objv[4];
	}			
    } 
    
    /* objv[1] holds AERecord object */
    /* objv[2] holds AEKeyword */
    result = TclaeGetKeyDesc(interp, objv[1], objv[2], 
			     NULL, &tempAEDesc);
    if (result != TCL_OK) {
	return TCL_ERROR;
    } 
    
    /* Obtain data from AEDesc */
    returnStructPtr = TclaeDataFromAEDesc(interp, &tempAEDesc, 
					  desiredTypePtr, typeCodeVarPtr);
    AEDisposeDesc(&tempAEDesc);
    
    if (returnStructPtr->object != NULL) {
	Tcl_SetObjResult(interp, returnStructPtr->object);
    }
    result = returnStructPtr->status;
    ckfree((char *)returnStructPtr);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetKeyDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetKeyDesc call 
 *
 *  tclAE::getKeyDesc <theAERecord> <theAEKeyword> ?desiredType?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for key item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetKeyDescCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */ 
		    int objc,			/* number of arguments */ 
		    Tcl_Obj *const objv[])	/* argument objects */
{
    int			result;				/* result from Tcl calls */				
    AEDesc		*theAEDescPtr;			/* pointer to new AEDesc */
    Tcl_Obj		*desiredTypePtr = NULL;		/* optional type to cast AEDesc to */
    
    
    if ((objc < 3) || (objc > 4)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAERecord theAEKeyword ?desiredType?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	desiredTypePtr = objv[3];
    } 
    /* Allocate space for new AEDesc */
    theAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));	
    if (theAEDescPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* objv[1] holds AERecord object */
    /* objv[2] holds AEKeyword */
    result = TclaeGetKeyDesc(interp, objv[1], objv[2], 
			     desiredTypePtr, theAEDescPtr);
    if (result == TCL_OK) {
	/* Set interp's result to a reference to theAEDescPtr */
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(theAEDescPtr));	
	
	return TCL_OK;
    } else {
	ckfree((char *)theAEDescPtr);
	return TCL_ERROR;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetNthDataCmd" --
 * 
 *  Tcl emulator for ToolBox AEGetNthPtr call 
 *
 *  tclAE::getNthData <theAEDescList> <index> ?desiredType? ?theAEKeywordPtr? ?typeCodePtr?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to text representation of AEDescList item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetNthDataCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */
		    int objc,			/* number of arguments */ 
		    Tcl_Obj *const objv[])	/* argument objects */
{
    int		result;				/* result from Tcl calls */		
    CmdReturn *	returnStructPtr;		/* result from internal calls */
    AEDesc	tempAEDesc;			/* temporary AEDesc from list */
    Tcl_Obj *	desiredTypePtr = NULL;		/* optional type to cast 
						 * AEDesc to */
    Tcl_Obj *	keywordVarPtr = NULL;		/* optional name of keyword 
						 * variable (if from AERecord) */
    Tcl_Obj *	typeCodeVarPtr = NULL;		/* optional name of type code 
						 * variable */
    
    
    if ((objc < 3) || (objc > 6)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList index ?desiredType? ?theAEKeywordPtr? ?typeCodePtr?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	/* Optional desired type */
	desiredTypePtr = objv[3];
	if (objc > 4) {
	    /* Optional Tcl variable to store keyword if item from AERecord */
	    keywordVarPtr = objv[4];
	    if (objc > 5) {
		/* Optional Tcl variable to store (coerced) AEDesc type */
		typeCodeVarPtr = objv[5];
	    }
	}
    }
    
    /* objv[1] holds AEDescList object */
    /* objv[2] holds index */
    result = TclaeGetNthDesc(interp, objv[1], objv[2], 
			     NULL, keywordVarPtr, &tempAEDesc);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Obtain (optionally coerced) data from AEDesc */
    returnStructPtr = TclaeDataFromAEDesc(interp, &tempAEDesc, 
					  desiredTypePtr, typeCodeVarPtr);
    AEDisposeDesc(&tempAEDesc);
    
    result = returnStructPtr->status;
    if (returnStructPtr->object != NULL) {
	/* Set interp's result to the data */
	Tcl_SetObjResult(interp, returnStructPtr->object);
    }
    
    ckfree((char *)returnStructPtr);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetNthDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetNthDesc call 
 *
 *  tclAE::getNthDesc <theAEDescList> <index> ?desiredType? ?theAEKeywordPtr?
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to object for nth AEDescList item
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetNthDescCmd(ClientData clientData,	/* (unused) */
		    Tcl_Interp *interp,		/* for results */
		    int objc,			/* number of arguments */
		    Tcl_Obj *const objv[])	/* argument objects */
{
    int			result;				/* result from Tcl calls */		
    AEDesc		*theAEDescPtr;			/* pointer to new AEDesc */
    Tcl_Obj		*desiredTypePtr = NULL;		/* optional type to cast 
							 * AEDesc to */
    Tcl_Obj		*keywordVarPtr = NULL;		/* optional name of keyword 
							 * variable (if from AERecord) */
    
    
    if ((objc < 3) || (objc > 5)) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList index ?desiredType? ?theAEKeywordPtr?");
	return TCL_ERROR;
    }
    
    if (objc > 3) {
	/* Optional desired type */
	desiredTypePtr = objv[3];
	if (objc > 4) {
	    /* Optional Tcl variable to store keyword if item from AERecord */
	    keywordVarPtr = objv[4];
	}
    }
    
    /* Allocate space for new AEDesc */
    theAEDescPtr = (AEDesc *) ckalloc(sizeof(AEDesc));		
    if (theAEDescPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* objv[1] holds AEDescList object */
    /* objv[2] holds index */
    result = TclaeGetNthDesc(interp, objv[1], objv[2], 
			     desiredTypePtr, keywordVarPtr, theAEDescPtr);
    if (result == TCL_OK) {
	/* Set interp's result to a reference to theAEDescPtr */
	Tcl_SetObjResult(interp, Tclae_NewAEDescObj(theAEDescPtr));	
	
	return TCL_OK;
    } else {
	ckfree((char *)theAEDescPtr);		
	return TCL_ERROR;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_PutDataCmd" --
 * 
 *  Tcl wrapper for ToolBox AEPutPtr call 
 *
 *  tclAE::putData <theAEDescList> <index> <typeCode> <data>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Item at index is replaced with data
 *  Revised AEDescList is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_PutDataCmd(ClientData clientData,	/* (unused) */  
		 Tcl_Interp *interp,	/* for results */  
		 int objc,		/* number of arguments */  
		 Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr       	err;			/* result from ToolBox calls */
    int			result;			/* result from Tcl calls */				
    void *		dataPtr;		/* pointer to data */
    Size		dataSize;		/* length of data */
    AEDescList *	theAEDescListPtr;	/* pointer to AEDescList */
    int			index;			/* index of item to put */
    OSType		typeCode;		/* type code of data */
    
    enum {
	kAEDescList = 1,
	kIndex,
	kTypeCode,
	kData
    };
    
    if (objc != 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList index typeCode data");
	return TCL_ERROR;
    }
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAEDescList], &theAEDescListPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Read index to obtain AEDesc from */
    result = Tcl_GetIntFromObj(interp, objv[kIndex], &index);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    typeCode = TclaeGetOSTypeFromObj(objv[kTypeCode]);
    
    /* Extract <data> */
    dataPtr = TclaeDataFromObj(interp, typeCode, objv[kData], &dataSize);
    if (dataPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* Put data at index position in AEDescList.
     * Tcl is 0-based, but AEDescLists are 1-based.
     */
    err = AEPutPtr(theAEDescListPtr, index + 1,
		   typeCode,
		   dataPtr, dataSize);
    
    ckfree(dataPtr);
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't put |",
			 Tcl_GetString(objv[kData]), "| as '",
			 Tcl_GetString(objv[kTypeCode]), "' into item #", 
			 Tcl_GetString(objv[kIndex]), " of \"", 
			 Tcl_GetString(objv[kAEDescList]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {
	Tcl_InvalidateStringRep(objv[kAEDescList]);
	Tcl_SetObjResult(interp, objv[kAEDescList]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_PutDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEPutDesc call 
 *
 *  tclAE::putDesc <theAEDescList> <index> <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Item at index is replaced with theAEDesc
 *  Revised AEDescList is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_PutDescCmd(ClientData clientData,	/* (unused) */  
		 Tcl_Interp *interp,	/* for results */
		 int objc,		/* number of arguments */  
		 Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr		err;			/* result from ToolBox calls */
    int			result;			/* result from Tcl calls */		
    const AEDesc *	theAEDescPtr;		/* AEDesc to put in AEDescList */
    AEDescList *	theAEDescListPtr;	/* pointer to AEDescList */
    int			index;			/* index of item to put */
    
    enum {
	kAEDescList = 1,
	kIndex,
	kAEDesc
    };
    
    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDescList index theAEDesc");
	return TCL_ERROR;
    }
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAEDescList], &theAEDescListPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Read index to obtain AEDesc from */
    result = Tcl_GetIntFromObj(interp, objv[kIndex], &index);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* objv[kAEDesc] holds AEDesc object */
    result = Tclae_GetConstAEDescFromObj(interp, objv[kAEDesc], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Put new AEDesc at index position in AEDescList.
     * Tcl is 0-based, but AEDescLists are 1-based.
     */
    err = AEPutDesc(theAEDescListPtr, index + 1, theAEDescPtr);
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't put \"", 
			 Tcl_GetString(objv[kAEDesc]), "\" into item #", 
			 Tcl_GetString(objv[kIndex]), " of \"", 
			 Tcl_GetString(objv[kAEDescList]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {
	Tcl_InvalidateStringRep(objv[kAEDescList]);
	Tcl_SetObjResult(interp, objv[kAEDescList]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_PutKeyDataCmd" --
 * 
 *  Tcl wrapper for ToolBox AEPutKeyPtr call 
 *
 *  tclAE::putKeyData <theAERecord> <theAEKeyword> <typeCode> <data>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Key item is replaced with data
 *  Revised AERecord is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_PutKeyDataCmd(ClientData clientData,	/* (unused) */
		    Tcl_Interp *interp,		/* for results */ 
		    int objc,			/* number of arguments */ 
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr       err;			/* result from ToolBox calls */
    int		result;			/* result from Tcl calls */
    void *	dataPtr;		/* pointer to data */
    Size	dataSize;		/* length of data */
    AERecord *	theAERecordPtr;		/* pointer to AERecord */
    OSType      typeCode;		/* type code of data */
    
    enum {
	kAERecord = 1,
	kAEKeyword,
	kTypeCode,
	kData
    };
    
    if (objc != 5) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAERecord theAEKeyword typeCode data");
	return TCL_ERROR;
    }
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAERecord], &theAERecordPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    typeCode = TclaeGetOSTypeFromObj(objv[kTypeCode]);
    
    /* Extract <data> */
    dataPtr = TclaeDataFromObj(interp, typeCode, objv[kData], &dataSize);
    if (dataPtr == NULL) {
	return TCL_ERROR;
    }
    
    /* Put new AEDesc into key entry of AERecord */
    err = AEPutKeyPtr(theAERecordPtr, 
		      TclaeGetOSTypeFromObj(objv[kAEKeyword]), 
		      typeCode, 
		      dataPtr, dataSize);
    
    ckfree(dataPtr);
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't put |",
			 Tcl_GetString(objv[kData]), "| as '", 
			 Tcl_GetString(objv[kTypeCode]), "' into key '", 
			 Tcl_GetString(objv[kAEKeyword]), "' of \"", 
			 Tcl_GetString(objv[kAERecord]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {
	Tcl_InvalidateStringRep(objv[kAERecord]);
	Tcl_SetObjResult(interp, objv[kAERecord]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_PutKeyDescCmd" --
 * 
 *  Tcl wrapper for ToolBox AEPutKeyDesc call 
 *
 *  tclAE::putKeyDesc <theAERecord> <theAEKeyword> <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Key item is replaced with theAEDesc
 *  Revised AERecord is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_PutKeyDescCmd(ClientData clientData,	/* (unused) */ 
		    Tcl_Interp *interp,		/* for results */  
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr       	err;			/* result from ToolBox calls */
    int			result;			/* result from Tcl calls */		
    const AEDesc *	theAEDescPtr;		/* AEDesc to put in AERecord */
    AERecord *		theAERecordPtr;		/* pointer to AERecord */
    
    enum {
	kAERecord = 1,
	kAEKeyword,
	kAEDesc
    };
    
    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAERecord theAEKeyword theAEDesc");
	return TCL_ERROR;
    }
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAERecord], &theAERecordPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* objv[kAEDesc] holds AEDesc object */
    result = Tclae_GetConstAEDescFromObj(interp, objv[kAEDesc], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Put new AEDesc into key entry of AERecord */
    err = AEPutKeyDesc(theAERecordPtr, 
		       TclaeGetOSTypeFromObj(objv[kAEKeyword]), 
		       theAEDescPtr);	
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't put \"", 
			 Tcl_GetString(objv[kAEDesc]), "\" into key '", 
			 Tcl_GetString(objv[kAEKeyword]), "' of \"", 
			 Tcl_GetString(objv[kAERecord]), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {
	Tcl_InvalidateStringRep(objv[kAERecord]);
	Tcl_SetObjResult(interp, objv[kAERecord]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_ReplaceDescDataCmd" --
 * 
 *  Tcl wrapper for Carbon AEReplaceDescData call 
 *  	and/or 
 *  Tcl access for theAEDesc.type and theAEDesc.dataHandle
 *
 *  tclAE::replaceDescData <theAEDesc> <typeCode> <data>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Revised AEDesc is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_ReplaceDescDataCmd(ClientData clientData,	/* (unused) */ 
			 Tcl_Interp *interp,	/* for results */ 
			 int objc,		/* number of arguments */ 
			 Tcl_Obj *const objv[])	/* argument objects */
{
    OSErr	err;			/* result from ToolBox calls */		
    int		result;			/* result from Tcl calls */		
    AEDesc *	theAEDescPtr;		/* pointer to new AEDesc */	
    OSType	typeCode;		/* type code of data */
    void *	dataPtr;		/* pointer to data */
    Size	dataSize;		/* length of data */
    
    enum {
	kAEDesc = 1,
	kTypeCode,
	kData
    };
    
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc typeCode data");
	return TCL_ERROR;
    }
    
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAEDesc], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    typeCode = TclaeGetOSTypeFromObj(objv[kTypeCode]);
    
    /* Extract <data> */
    dataPtr = TclaeDataFromObj(interp, typeCode, objv[kData], &dataSize);
    if (dataPtr == NULL) {
	return TCL_ERROR;				
    } 
    
#if ACCESSOR_CALLS_ARE_FUNCTIONS // das 25/10/00: Carbonization
    err = AEReplaceDescData(typeCode, dataPtr, dataSize, theAEDescPtr);
#else
    theAEDescPtr->descriptorType = typeCode;
    if (theAEDescPtr->dataHandle) {
	// Get rid of whatever was there before.
	// Can we depend on a non-NULL dataHandle being valid? If not, this is Bad™.
	
	// das - 24 oct 2000
	// well it is Bad™ indeed on the 68k, numerous AEDescs that this
	// routine comes across during a .test don't have a valid handle in
	// dataHandle (either NULL or not a handle at all, the latter is
	// most likely due to some other bug), this might be the same on
	// ppc, but the modern memory manager is probably more robust
	// against DisposeHandle on a invalid handle...  on 68k this
	// crashes hard.
	
	// das - 27 oct 2000
	// I've seen this only on CFM68k, and having looked into it more,
	// only when AEHandlers are involved, something funky must be going
	// on there
	
	DisposeHandle(theAEDescPtr->dataHandle);
    }
    // !!! Can we depend on evaluation of Tcl_GetByteArrayFromObj()
    // before value of dataSize is set?
    err = PtrToHand(dataPtr, &theAEDescPtr->dataHandle, dataSize);
#endif
    
    ckfree(dataPtr);
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't set data of \"", 
			 Tcl_GetString(objv[kAEDesc]), "\" to |",
			 Tcl_GetString(objv[kData]), "|: ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;
    } else {
	Tcl_InvalidateStringRep(objv[kAEDesc]);
	Tcl_SetObjResult(interp, objv[kAEDesc]);
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_SetDescTypeCmd" --
 * 
 *  Tcl access for theAEDesc.descriptorType
 *
 *  tclAE::desc::setDescType <theAEDesc> <toType>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Type of AEDesc is changed to <toType>
 *  Revised AEDesc is placed in interpreter's result
 * -------------------------------------------------------------------------
 */
int 
Tclae_SetDescTypeCmd(ClientData clientData,	/* (unused) */  
		     Tcl_Interp *interp,	/* for results */
		     int objc,			/* number of arguments */  
		     Tcl_Obj *const objv[])	/* argument objects */
{
    int		result;			/* result from Tcl calls */		
    AEDesc *	theAEDescPtr;		/* pointer to AEDesc */			
    
    enum {
	kAEDesc = 1,
	kToType
    };
    
    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc toType");
	return TCL_ERROR;
    }
    
    result = Tclae_GetAEDescFromObj(interp, objv[kAEDesc], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    theAEDescPtr->descriptorType = TclaeGetOSTypeFromObj(objv[kToType]);
    
    Tcl_InvalidateStringRep(objv[kAEDesc]);
    Tcl_SetObjResult(interp, objv[kAEDesc]);
    
    return TCL_OK;   
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae__GetAEDescCmd" --
 * 
 *  Private call to retrieve the AEDesc pointer from the supplied AEDesc reference.
 *  If you call this without my permission, I'll take away your birthday.
 *  
 *  tclAE::_private::_getAEDesc <theAEDesc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Result of interp is set to AEDesc * as ByteArray.
 * -------------------------------------------------------------------------
 */
int
Tclae__GetAEDescCmd(ClientData clientData,	/* (unused) */   
		    Tcl_Interp *interp,		/* for results */
		    int objc,			/* number of arguments */  
		    Tcl_Obj *const objv[])	/* argument objects */
{
    const AEDesc *	theAEDescPtr;		/* pointer to AEDesc */
    int			result;	
    
    enum {
	kAEDesc = 1
    };
    
    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "theAEDesc");
	return TCL_ERROR;
    }
    
    result = Tclae_GetConstAEDescFromObj(interp, objv[kAEDesc], &theAEDescPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    Tcl_SetObjResult(interp, 
		     Tcl_NewByteArrayObj((unsigned char *) &theAEDescPtr, 
					 sizeof(theAEDescPtr)));
    
    return TCL_OK;	
}

/* ◊◊◊◊ Internal package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeDataFromObj" --
 * 
 *  Extract data from supplied object.
 *  If byte array, return it raw, else, perform UtfToExternal conversion
 *  on string before returning it.
 *
 *  !!! Caller is responsible for disposing of data pointer !!!
 * 
 * Results:
 *  pointer to data
 * 
 * Side effects:
 *  Contents of dataSizePtr (if non-NULL) is set to the data length
 * -------------------------------------------------------------------------
 */
void *
TclaeDataFromObj(Tcl_Interp*	interp,		/* for error reporting */ 
		 OSType		typeCode,       /* purported typecode of data */
		 Tcl_Obj*	dataObjPtr,	/* object holding desired data */
		 Size*		dataSizePtr)	/* pointer to hold length of data */
{
    void*		dataPtr;
    void*		tempPtr;
    int			dataSize;
    
    
    if (dataObjPtr->typePtr == Tcl_GetObjType("bytearray")) { // das 25/09/00
	tempPtr = Tcl_GetByteArrayFromObj(dataObjPtr, &dataSize);
	
	dataPtr = ckalloc(dataSize);
	if (dataPtr) {
	    memcpy(dataPtr, tempPtr, dataSize);
	}
    } else {
	switch (typeCode) {
#if TARGET_API_MAC_CARBON	    
	    case typeUnicodeText:
		tempPtr = Tcl_GetUnicodeFromObj(dataObjPtr, &dataSize);
		dataSize *= sizeof(Tcl_UniChar);
		dataPtr = ckalloc(dataSize);
		if (dataPtr) {
		    memcpy(dataPtr, tempPtr, dataSize);
		}		
		break;
	    case typeUTF8Text:
		tempPtr = Tcl_GetStringFromObj(dataObjPtr, &dataSize);
		dataPtr = ckalloc(dataSize);
		if (dataPtr) {
		    memcpy(dataPtr, tempPtr, dataSize);
		}		
		break;
#endif // TARGET_API_MAC_CARBON	    	
	    default: {
		Tcl_DString	dataDS;		/* for conversion from UTF */
		
		/* Convert data from UTF */
		Tcl_UtfToExternalDString(tclAE_macRoman_encoding, Tcl_GetString(dataObjPtr), -1, &dataDS);
		
		dataSize = Tcl_DStringLength(&dataDS);
		dataPtr = ckalloc(dataSize);
		if (dataPtr) {
		    memcpy(dataPtr, Tcl_DStringValue(&dataDS), dataSize);
		}
		
		Tcl_DStringFree(&dataDS);		
	    }
	}
    }
    
    if (dataPtr && dataSizePtr) {
	*dataSizePtr = dataSize;
    }
    
    return dataPtr;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetAttributeDesc" --
 * 
 *  Derive an AE descriptor from the supplied AppleEvent object and
 *  AEKeyword.
 *
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  keyAEDescPtr points to (optionally coerced) AEDesc from AppleEvent key.
 *  keyAEDescPtr must already exist!
 * -------------------------------------------------------------------------
 */
int
TclaeGetAttributeDesc(Tcl_Interp *interp,		/* for results */ 
		      Tcl_Obj *theAppleEventObjPtr,	/* the AppleEvent */
		      Tcl_Obj *theAttributeObjPtr,	/* attribute to retrieve */
		      Tcl_Obj *theDesiredTypeObjPtr,	/* (optional) desired type */
		      AEDesc *keyAEDescPtr)		/* pointer to new AEDesc from key */
{
    OSErr		err;				/* result from ToolBox calls */
    int			result;				/* result from Tcl calls */
    const AppleEvent *	theAppleEventPtr;		/* pointer to AppleEvent */
    DescType		desiredType = typeWildCard;	/* optional type for new AEDesc */
    
    /* Obtain AppleEvent pointer from reference */
    result = Tclae_GetConstAEDescFromObj(interp, theAppleEventObjPtr, &theAppleEventPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    if (theDesiredTypeObjPtr != NULL) {
	/* Optional desired type */
	desiredType = TclaeGetOSTypeFromObj(theDesiredTypeObjPtr);
    }
    
    /* Get key item */
    err = AEGetAttributeDesc(theAppleEventPtr, 
			     TclaeGetOSTypeFromObj(theAttributeObjPtr), 
			     desiredType, 
			     keyAEDescPtr);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't get attribute '",
			 Tcl_GetString(theAttributeObjPtr), "' from \"", 
			 Tcl_GetString(theAppleEventObjPtr), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } 
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetKeyDesc" --
 * 
 *  Derive an AE descriptor from the supplied AERecord object and
 *  AEKeyword.
 *
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  keyAEDescPtr points to (optionally coerced) AEDesc from AERecord key.
 *  keyAEDescPtr must already exist!
 * -------------------------------------------------------------------------
 */
int
TclaeGetKeyDesc(Tcl_Interp *interp,		/* for results */ 
		Tcl_Obj *theAERecordObjPtr,	/* the AERecord */
		Tcl_Obj *theAEKeywordObjPtr,	/* keyword item to retrieve */
		Tcl_Obj *theDesiredTypeObjPtr,	/* (optional) desired type */
		AEDesc *keyAEDescPtr)		/* pointer to new AEDesc from key */
{
    OSErr		err;				/* result from ToolBox calls */
    int			result;				/* result from Tcl calls */
    const AERecord *	theAERecordPtr;			/* pointer to AERecord */
    DescType		desiredType = typeWildCard;	/* optional type for new AEDesc */
    
    /* Obtain AERecord pointer from reference */
    result = Tclae_GetConstAEDescFromObj(interp, theAERecordObjPtr, &theAERecordPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    if (theDesiredTypeObjPtr != NULL) {
	/* Optional desired type */
	desiredType = TclaeGetOSTypeFromObj(theDesiredTypeObjPtr);
    }
    
    /* Get key item */
    err = AEGetKeyDesc(theAERecordPtr, 
		       TclaeGetOSTypeFromObj(theAEKeywordObjPtr), 
		       desiredType, 
		       keyAEDescPtr);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't get keyword '",
			 Tcl_GetString(theAEKeywordObjPtr), "' from \"", 
			 Tcl_GetString(theAERecordObjPtr), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } 
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetNthDesc" --
 * 
 *  Derive an AE descriptor from the supplied AEDescList object and
 *  index.
 *
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  nthAEDescPtr points to (optionally coerced) AEDesc from AEDescList item.
 *  nthAEDescPtr must already exist!
 * -------------------------------------------------------------------------
 */
int
TclaeGetNthDesc(Tcl_Interp *interp,		/* for results */ 
		Tcl_Obj *theAEDescListObjPtr,	/* the AEDescList */
		Tcl_Obj *theIndexObjPtr,	/* nth item to retrieve */
		Tcl_Obj *theDesiredTypeObjPtr,	/* (optional) desired type */
		Tcl_Obj *theKeywordVarObjPtr,	/* to store keyword of item if
						 * from AERecord */
		AEDesc *nthAEDescPtr)		/* pointer to new AEDesc from index */
{
    OSErr		err;				/* result from ToolBox calls */
    int			result;				/* result from Tcl calls */		
    const AEDescList *	theAEDescListPtr;		/* pointer to AEDescList */
    DescType		desiredType = typeWildCard;	/* optional type for new AEDesc */
    AEKeyword		theAEKeyword;			/* Nth keyword, if AERecord */
    int			index;				/* index of item to get */
    
    /* Obtain AEDescList pointer from object */
    result = Tclae_GetConstAEDescFromObj(interp, theAEDescListObjPtr, &theAEDescListPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Read index to obtain AEDesc from */
    result = Tcl_GetIntFromObj(interp, theIndexObjPtr, &index);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    if (theDesiredTypeObjPtr != NULL) {
	/* Optional desired type */
	desiredType = TclaeGetOSTypeFromObj(theDesiredTypeObjPtr);
    }
    
    /* Get nth item. 
     * Tcl is 0-based, but AEDescLists are 1-based.
     */
    err = AEGetNthDesc(theAEDescListPtr, index + 1, desiredType, 
		       &theAEKeyword, nthAEDescPtr);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't get item #",
			 Tcl_GetString(theIndexObjPtr), " from \"", 
			 Tcl_GetString(theAEDescListObjPtr), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } 
    
    if (theKeywordVarObjPtr != NULL) {
	/* Don't set theAEKeyword variable until now in the event that an error
	 * occurs before we're done 
	 */
	Tcl_ObjSetVar2(interp, theKeywordVarObjPtr, NULL, 
		       TclaeNewOSTypeObj(theAEKeyword), 0);		
    }
    
    return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetOSTypeFromObj" --
 * 
 *  Read string value of Tcl_Obj as though it's a FourCharCode
 *  Convert to UTF and return
 *  
 *  We don't use Tcl_GetOSTypeFromObj because we need conversion from UTF
 *  and AEGizmos requires more tolerant padding/truncation to 4 characters
 * 
 * Results:
 *  The extracted OSType
 * 
 * Side effects:
 *  None
 * -------------------------------------------------------------------------
 */
OSType
TclaeGetOSTypeFromObj(Tcl_Obj *objPtr)	/* the input object */
{
    Tcl_DString osTypeDS;		/* for UTF conversion */
    OSType		osType = kLSUnknownCreator; // '    ';
    char		*osTypeStr;
    int			len;
    
    /* Convert object value from UTF */
    osTypeStr = Tcl_UtfToExternalDString(tclAE_macRoman_encoding, Tcl_GetString(objPtr), -1, &osTypeDS);
    len = Tcl_DStringLength(&osTypeDS);
    
    /* Check if OSType was single-quoted by caller */
    if ((osTypeStr[0] == '\'') 
	&& (osTypeStr[len - 1] == '\'')
	&& len == 6) {
	// strip close quote
	osTypeStr[len - 1] = '\0';
	// move past open quote
	osTypeStr += 1;
	len -= 2;
    } else if ((osTypeStr[0] == '‘') 
	       && (osTypeStr[len - 1] == '’')
	       && len == 6) {
	// strip close quote
	osTypeStr[len - 1] = '\0';
	// move past open quote
	osTypeStr += 1;
	len -= 2;
    }
    
    if (len == 4) {
	osType = (OSType) osTypeStr[0] << 24 |
		 (OSType) osTypeStr[1] << 16 |
		 (OSType) osTypeStr[2] <<  8 |
		 (OSType) osTypeStr[3];
    }
    
    Tcl_DStringFree(&osTypeDS);
    
    return osType;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeNewOSTypeObj" --
 * 
 *  Convert a FourCharCode to UTF and place in a new Tcl_Obj
 *  
 *  We don't use Tcl_NewOSTypeObj because we need conversion to UTF
 * 
 * Results:
 *  Pointer to new Tcl_Obj
 * 
 * Side effects:
 *  None
 * -------------------------------------------------------------------------
 */
Tcl_Obj *
TclaeNewOSTypeObj(OSType theOSType)	/* The desired OSType */
{
    char string[5];
    Tcl_Obj *	newOSTypeObj;	/* to hold the result */
    Tcl_DString	theOSTypeDS;	/* for conversion to UTF */
    
    /* Convert OSType to UTF */
    string[0] = (char) (theOSType >> 24);
    string[1] = (char) (theOSType >> 16);
    string[2] = (char) (theOSType >>  8);
    string[3] = (char) (theOSType);
    string[4] = '\0';
    Tcl_DStringInit(&theOSTypeDS);
    Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, string, 
			     -1, &theOSTypeDS);
    /* Create new string object containing OSType */
    newOSTypeObj = Tcl_NewStringObj(Tcl_DStringValue(&theOSTypeDS), -1);
    Tcl_DStringFree(&theOSTypeDS);
    
    return newOSTypeObj;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaePutKeyDesc" --
 * 
 *  Get the AEDescList from the object and put the AEDesc into the
 *  specified index position.
 *
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  keyAEDescPtr is inserted into the AERecord.
 * -------------------------------------------------------------------------
 */
int
TclaePutKeyDesc(Tcl_Interp *interp,		/* for results */ 
		Tcl_Obj *theAERecordObjPtr,	/* the AERecord */
		Tcl_Obj *theAEKeywordObjPtr,	/* keyword item to insert */
		AEDesc *keyAEDescPtr)		/* pointer to AEDesc to place */				
{
    OSErr	err;			/* result from ToolBox calls */
    int		result;			/* result from Tcl calls */
    AERecord *	theAERecordPtr;		/* pointer to AERecord */
    
    
    /* Obtain AERecord pointer from reference */
    result = Tclae_GetAEDescFromObj(interp, theAERecordObjPtr, &theAERecordPtr, true);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    
    /* Put new AEDesc into key entry of AERecord */
    err = AEPutKeyDesc(theAERecordPtr, 
		       TclaeGetOSTypeFromObj(theAEKeywordObjPtr), 
		       keyAEDescPtr);	
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Couldn't put AEDesc into key '", 
			 Tcl_GetString(theAEKeywordObjPtr), "' of \"", 
			 Tcl_GetString(theAERecordObjPtr), "\": ",
			 Tcl_MacOSError(interp, err), 
			 (char *) NULL);
	return TCL_ERROR;		
    } else {
	Tcl_InvalidateStringRep(theAERecordObjPtr);
	Tcl_SetObjResult(interp, theAERecordObjPtr);
	return TCL_OK;
    }
}
	
/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeDataFromAEDesc" --
 * 
 *  Retrieve (possibly coerced) data from AEDesc as Tcl binary data.
 * 
 * Results:
 *  CmdReturn containing Tcl result code and data in Tcl_Obj. 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
CmdReturn *
TclaeDataFromAEDesc(Tcl_Interp *	interp,		/* for error reporting */ 
		    const AEDesc *	theAEDescPtr,	/* pointer to original AEDesc */ 
		    Tcl_Obj *		desiredTypePtr,	/* desired descriptor type 
							    (NULL for no coercion) */  
		    Tcl_Obj *typeCodeVarPtr)		/* name of Tcl variable to 
							   store descriptor type 
							   (NULL for no variable) */ 
{
    CmdReturn *	returnStructPtr;		/* pointer to function result */		
    OSType	typeCode = 0;
    OSType	desiredType = typeWildCard;
    
    if (desiredTypePtr != NULL) {
	desiredType = TclaeGetOSTypeFromObj(desiredTypePtr);
    }
    
    switch (desiredType) {
	case kUnknownType:
	// unknown (but not missing) desiredType means to return
	// descriptor data as raw binary
	returnStructPtr = rawFromAEDesc(interp, theAEDescPtr);		
	typeCode = theAEDescPtr->descriptorType;
	break;
	
	case typeWildCard:
	returnStructPtr = dataFromAEDesc(interp, theAEDescPtr);
	typeCode = theAEDescPtr->descriptorType;
	break;
	
	default: {
	    AEDesc		coercedAEDesc;		/* temporary coerced AEDesc */
	    OSErr       err;				/* result from ToolBox calls */
	    
	    /* Coerce AEDesc to desiredType, if requested */
	    err = AECoerceDesc(theAEDescPtr, desiredType, &coercedAEDesc);
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't coerce descriptor to '", 
				 Tcl_GetString(desiredTypePtr), "': ",
				 Tcl_MacOSError(interp, err), 
				 (char *) NULL);
		returnStructPtr = (CmdReturn *) ckalloc(sizeof(CmdReturn)); // das 25/10/00: Bugfix
		returnStructPtr->object = NULL;
		returnStructPtr->status = TCL_ERROR;
	    } else {
		returnStructPtr = dataFromAEDesc(interp, &coercedAEDesc);
		typeCode = coercedAEDesc.descriptorType;
		AEDisposeDesc(&coercedAEDesc);
	    }
	}
	break;
    }
    
    /* Don't set the typeCode variable until now in the event that an error
     * occurs before we're done 
     */
    if ((typeCodeVarPtr != NULL)
	&&  (returnStructPtr->status == TCL_OK)) {
	Tcl_ObjSetVar2(interp, typeCodeVarPtr, NULL, TclaeNewOSTypeObj(typeCode), 0);
    } 	
    
    return returnStructPtr;
}
	
/* 
 * -------------------------------------------------------------------------
 * 
 * "rawFromAEDesc" --
 * 
 *  Retrieve raw binary data from AEDesc as Tcl ByteArray object.
 * 
 * Results:
 *  CmdReturn containing Tcl result code and data in Tcl_Obj. 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static CmdReturn *
rawFromAEDesc(Tcl_Interp *interp,		/* for error reporting */ 
	      const AEDesc *theAEDescPtr)	/* pointer to original AEDesc */ 
{
    CmdReturn *	returnStructPtr;		/* pointer to function result */		
    Ptr		theData;
    Size	theSize;
    
    /* Initialize the return struct */
    returnStructPtr = (CmdReturn *) ckalloc(sizeof(CmdReturn));
    returnStructPtr->status = TCL_OK;
    returnStructPtr->object = NULL;
    
    theData = TclaeAllocateAndGetDescData(theAEDescPtr, &theSize);
    
    if (theData) {
	returnStructPtr->object = 
	Tcl_NewByteArrayObj((unsigned char *) theData, 
			    theSize);
	
	ckfree(theData);
    } else {
	returnStructPtr->status = TCL_ERROR;
    }
    
    return returnStructPtr;
}
	
/* 
 * -------------------------------------------------------------------------
 * 
 * "dataFromAEDesc" --
 * 
 *  Retrieve data from AEDesc as Tcl object.
 * 
 * Results:
 *  CmdReturn containing Tcl result code and data in Tcl_Obj. 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static CmdReturn *
dataFromAEDesc(Tcl_Interp *interp,		/* for error reporting */ 
	       const AEDesc *theAEDescPtr)	/* pointer to original AEDesc */ 
{
    OSErr       err;			/* result from ToolBox calls */
    CmdReturn *	returnStructPtr;	/* pointer to function result */		
    
    /* Initialize the return struct */
    returnStructPtr = (CmdReturn *) ckalloc(sizeof(CmdReturn));
    returnStructPtr->status = TCL_OK;
    returnStructPtr->object = NULL;
    
    switch (theAEDescPtr->descriptorType) {
	case typeChar: {
	    Tcl_DString	dataDS;		/* for conversion to UTF */
	    char *	theData;
	    Size	theSize;
	    
	    theData = TclaeAllocateAndGetDescData(theAEDescPtr, &theSize);
	    
	    if (theData) {
		/* Convert data to UTF */
		Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, 
					 theData, theSize, &dataDS);
		
		ckfree(theData);
		
		returnStructPtr->object 
		= Tcl_NewStringObj(Tcl_DStringValue(&dataDS), 
				   Tcl_DStringLength(&dataDS));
		
		Tcl_DStringFree(&dataDS);
	    } else {
		returnStructPtr->status = TCL_ERROR;
	    }
	}
	break;
	
#if TARGET_API_MAC_CARBON	    
	case typeUnicodeText: {
	    Tcl_UniChar *	theUnicode;
	    Size		theSize;
	    
	    theUnicode = TclaeAllocateAndGetDescData(theAEDescPtr, &theSize);
	    
	    if (theUnicode) {
		returnStructPtr->object = Tcl_NewUnicodeObj(theUnicode, theSize / sizeof(Tcl_UniChar));
		ckfree((char *) theUnicode);
	    } else {
		returnStructPtr->status = TCL_ERROR;
	    }
	}
	break;
	    
	case typeUTF8Text: {
	    char *	theUTF8;
	    Size	theSize;
	    
	    theUTF8 = TclaeAllocateAndGetDescData(theAEDescPtr, &theSize);
	    
	    if (theUTF8) {
		returnStructPtr->object = Tcl_NewStringObj(theUTF8, theSize);
		ckfree(theUTF8);
	    } else {
		returnStructPtr->status = TCL_ERROR;
	    }
	}
	break;
#endif // TARGET_API_MAC_CARBON	    
	    
	case typeBoolean: {
	    AEDesc		shorAEDesc;		/* for coercion of boolean to integer */
	    short		theData;
	    
	    /* Coerce boolean descriptor to an integer (0 or 1) */
	    err = AECoerceDesc(theAEDescPtr, typeSInt16, &shorAEDesc);
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't coerce descriptor to 'shor': ",
				 Tcl_MacOSError(interp, err), 
				 (char *) NULL);
		returnStructPtr->status = TCL_ERROR;
		return returnStructPtr;				
	    } 
	    /* Create new boolean object from value of AEDesc */
	    TclaeGetDescData(&shorAEDesc, &theData, sizeof(theData));
	    
	    returnStructPtr->object = Tcl_NewBooleanObj(theData);
	    
	    AEDisposeDesc(&shorAEDesc);
	}
	break;
	
	case typeSInt16: {
	    short		theData;
	    
	    TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
	    returnStructPtr->object = Tcl_NewIntObj(theData);
	}
	break;
	
	case typeSInt32: {
#if __LP64__
            short		theData;
            
            TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
            returnStructPtr->object = Tcl_NewIntObj(theData);
#else
	    long		theData;
	    
	    TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
	    returnStructPtr->object = Tcl_NewLongObj(theData);
#endif // __LP64__
	}
	break;
	
        case typeSInt64: {
#if __LP64__
            long		theData;
            
            TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
            returnStructPtr->object = Tcl_NewLongObj(theData);
#else
            long long		theData;
            
            TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
            returnStructPtr->object = Tcl_NewWideIntObj(theData);
#endif // __LP64__
        }
        break;
        
	case typeIEEE32BitFloatingPoint: {
	    double	tempDbl;
	    float	theData;
	    
	    TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
	    tempDbl = theData;
	    returnStructPtr->object = Tcl_NewDoubleObj(tempDbl);
	}
	break;
	
	case typeIEEE64BitFloatingPoint: {
	    double		theData;
	    
	    TclaeGetDescData(theAEDescPtr, &theData, sizeof(theData));
	    returnStructPtr->object = Tcl_NewDoubleObj(theData);
	}
	break;
	
	case typeAEList: {
	    long	theCount, i;	/* total number of items and index in AEDescList */	
	    
	    returnStructPtr->object = Tcl_NewListObj(0, NULL);
	    err = AECountItems((AEDescList *) theAEDescPtr, &theCount);
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't coerce descriptor to 'TEXT': ",
				 Tcl_MacOSError(interp, err), 
				 (char *) NULL);
		returnStructPtr->status = TCL_ERROR;
		return returnStructPtr;				
	    } 
	    
	    /* Tcl is 0-based, but AEDescLists are 1-based. */
	    for (i = 1; i <= theCount; i++) {
		CmdReturn	*elementStructPtr;	/* result from item extraction */
		AEDesc		elementDesc;		/* item AEDesc */
		
		/* Get the ith AEDesc from the AEDescList */
		err = AEGetNthDesc((AEDescList *) theAEDescPtr, i, typeWildCard, 
				   NULL, &elementDesc);
		if (err != noErr) {
		    Tcl_DecrRefCount(returnStructPtr->object);
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, 
				     "Couldn't coerce list item to 'TEXT': ",
				     Tcl_MacOSError(interp, err), 
				     (char *) NULL);
		    returnStructPtr->status = TCL_ERROR;
		    return returnStructPtr;				
		} 
		
		/* Obtain uncoerced data from AEDesc */
		elementStructPtr = TclaeDataFromAEDesc(interp, &elementDesc, 
						       NULL, NULL);
		
		if (elementStructPtr->status != TCL_OK) {
		    ckfree((char *)elementStructPtr);
		    returnStructPtr->status = TCL_ERROR;
		    return returnStructPtr;				
		} 
		
		/* Append item to result list */
		returnStructPtr->status 
		= Tcl_ListObjAppendElement(interp, 
					   returnStructPtr->object, 
					   elementStructPtr->object);
		ckfree((char *)elementStructPtr);
		AEDisposeDesc(&elementDesc);
	    }
	}
	break;
	
	default: {
	    ckfree((char *) returnStructPtr);
	    returnStructPtr = rawFromAEDesc(interp, theAEDescPtr);
	}
	break;
    }
    
    return returnStructPtr;
}
	
#if TARGET_API_MAC_CARBON

Size	TclaeGetDescDataSize(const AEDesc * theAEDesc)
{
    return AEGetDescDataSize(theAEDesc);
}

OSErr TclaeGetDescData(const AEDesc *  theAEDesc,
		       void *          dataPtr,
		       Size            maximumSize)
{
    return AEGetDescData(theAEDesc, dataPtr, maximumSize);
}

#else

Size	TclaeGetDescDataSize(const AEDesc * theAEDesc)
{
    return GetHandleSize(theAEDesc->dataHandle);
}

OSErr TclaeGetDescData(const AEDesc *  theAEDesc,
		       void *          dataPtr,
		       Size            maximumSize)
{
    Size	size = GetHandleSize(theAEDesc->dataHandle);
    
    HLock(theAEDesc->dataHandle);
    if (size > maximumSize) {
	size = maximumSize;
    }
    BlockMoveData(*theAEDesc->dataHandle, dataPtr, size);
    HUnlock(theAEDesc->dataHandle);
    
    return noErr;
}

#endif // TARGET_API_MAC_CARBON
	
void * 
TclaeAllocateAndGetDescData(const AEDesc *	theAEDesc,
			    Size *		sizePtr)
{
    Size	theSize;
    void *	dataPtr;
    
    if (!sizePtr) {
	sizePtr = &theSize;
    }
    
    *sizePtr = TclaeGetDescDataSize(theAEDesc);
    dataPtr = ckalloc(*sizePtr);
    if (TclaeGetDescData(theAEDesc, dataPtr, *sizePtr) != noErr) {
	ckfree(dataPtr);
	return NULL;
    } else {
	return dataPtr;
    }
}
