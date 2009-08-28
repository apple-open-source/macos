/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEObjects.c"
 *                                    created: 11/13/00 {10:30:29 PM} 
 *                                last update: 11/6/07 {7:38:13 PM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *          POMODORO no seisan
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright © 2000 Jonathan Guyer
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

#ifndef _TCL
#include <tcl.h>
#endif

#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#include <AEObjects.h>
#endif

#include <string.h>

#include "tclAEInt.h"
#include "tclMacOSError.h"

/* I don't claim that I understand why, by gInterp will retain different
 * values for different applications that invoke TclAE. This is fortunate
 * because the slugs at Apple were too lazy to provide a refcon field
 * in the object callbacks.
 */
 
static Tcl_Interp * gInterp;
static AEDesc		gErrorDesc;


/* Hash table for storage of object accessors */
static Tcl_HashTable *		tclAEObjectAccessorHashTable;

static OSLAccessorUPP 		TclaeObjectAccessorUPP = NULL;
static OSLCompareUPP		TclaeCompareObjectsUPP = NULL;
static OSLCountUPP			TclaeCountObjectsUPP = NULL;
static OSLDisposeTokenUPP	TclaeDisposeTokenUPP = NULL;
static OSLGetMarkTokenUPP	TclaeGetMarkTokenUPP = NULL;
static OSLMarkUPP			TclaeMarkUPP = NULL;
static OSLAdjustMarksUPP	TclaeAdjustMarksUPP = NULL;
static OSLGetErrDescUPP		TclaeGetErrorDescUPP = NULL;

typedef struct tclAEObjectAccessor {
	DescType	desiredClass;
	DescType	containerType;
	Tcl_Obj		*accessorProc;
	Tcl_Interp	*interp;	
} tclAEObjectAccessor;

static Tcl_HashEntry* TclaeGetObjectAccessor(Tcl_Interp* interp, DescType desiredClass, DescType containerType, char* accessorProc);

static pascal OSErr TclaeObjectAccessor(DescType desiredClass, const AEDesc *containerToken, DescType containerClass, DescType keyForm, const AEDesc *keyData, AEDesc *theToken, long theRefcon);
static pascal OSErr TclaeCountObjects(DescType desiredClass, DescType containerClass, const AEDesc *theContainer, long *result);
static pascal OSErr TclaeCompareObjects(DescType comparisonOperator, const AEDesc *theObject, const AEDesc *objectOrDescToCompare, Boolean *result);
static pascal OSErr TclaeDisposeToken(AEDesc *unneededToken);
static pascal OSErr TclaeGetErrorDesc(AEDescPtr *errDescPtr);
static pascal OSErr TclaeGetMarkToken(const AEDesc *containerToken, DescType containerClass, AEDesc *result);
static pascal OSErr TclaeMark(const AEDesc *theToken, const AEDesc *markToken, long markCount);
static pascal OSErr TclaeAdjustMarks(long newStart, long newStop, const AEDesc *markToken);

static OSErr TclaeRemoveObjectAccessor(DescType desiredClass, DescType containerType, Tcl_HashEntry * hashEntryPtr);


/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_SetObjectCallbacksCmd" --
 * 
 *  Tcl wrapper for ToolBox AESetObjectCallbacks call. 
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  •
 * 
 * Side effects:
 *  •
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_SetObjectCallbacksCmd(ClientData clientData,
		                 Tcl_Interp *interp,
		                 int objc,
		                 Tcl_Obj *const objv[])
{
	OSErr		err;					/* result from ToolBox calls */		
//	int			result;					/* result from Tcl calls */	
	Tcl_Obj *	procObj;

	enum {
		kCompareProc = 1,
		kCountProc,
		kDisposeTokenProc,
		kGetMarkTokenProc,
		kMarkProc,
		kAdjustMarksProc,
		kTotalArguments
	};
	
	if (objc != kTotalArguments) {
		Tcl_WrongNumArgs(interp, 1, objv, "<compareProc> <countProc> <disposeTokenProc> <getMarkTokenProc> <markProc> <adjustMarksProc>");
		return TCL_ERROR;
	}
	
	gInterp = interp;
	
    // All compare callbacks are relayed through TclaeCompareObjects()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "compareObjects", objv[kCompareProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeCompareObjectsUPP) {
			TclaeCompareObjectsUPP = NewOSLCompareUPP(TclaeCompareObjects);
		}
	} else {
		DisposeOSLCompareUPP(TclaeCompareObjectsUPP);
	}
	
    // All count callbacks are relayed through TclaeCountObjects()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "countObjects", objv[kCountProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeCountObjectsUPP) {
			TclaeCountObjectsUPP = NewOSLCountUPP(TclaeCountObjects);
		}
	} else {
		DisposeOSLCountUPP(TclaeCountObjectsUPP);
	}
	
    // All dispose token callbacks are relayed through TclaeDisposeToken()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "disposeToken", objv[kDisposeTokenProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeDisposeTokenUPP) {
			TclaeDisposeTokenUPP = NewOSLDisposeTokenUPP(TclaeDisposeToken);
		}
	} else {
		DisposeOSLDisposeTokenUPP(TclaeDisposeTokenUPP);
	}
	
    // All get mark token callbacks are relayed through TclaeGetMarkToken()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "getMarkToken", objv[kGetMarkTokenProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeGetMarkTokenUPP) {
			TclaeGetMarkTokenUPP = NewOSLGetMarkTokenUPP(TclaeGetMarkToken);
		}
	} else {
		DisposeOSLGetMarkTokenUPP(TclaeGetMarkTokenUPP);
	}
	
    // All mark callbacks are relayed through TclaeMark()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "mark", objv[kMarkProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeMarkUPP) {
			TclaeMarkUPP = NewOSLMarkUPP(TclaeMark);
		}
	} else {
		DisposeOSLMarkUPP(TclaeMarkUPP);
	}
	
    // All adjust marks callbacks are relayed through TclaeAdjustMarks()
	procObj = Tcl_SetVar2Ex(interp, "tclAE::_callbacks", "adjustMarks", objv[kAdjustMarksProc], TCL_GLOBAL_ONLY);
    if (Tcl_GetCharLength(procObj) > 0) {
		if (!TclaeAdjustMarksUPP) {
			TclaeAdjustMarksUPP = NewOSLAdjustMarksUPP(TclaeAdjustMarks);
		}
	} else {
		DisposeOSLAdjustMarksUPP(TclaeAdjustMarksUPP);
	}
	
    // All get error desc callbacks are handled by TclaeGetErrorDesc()
	if (!TclaeGetErrorDescUPP) {
		TclaeGetErrorDescUPP = NewOSLGetErrDescUPP(TclaeGetErrorDesc);
	}
	
	err = AESetObjectCallbacks(TclaeCompareObjectsUPP,
								TclaeCountObjectsUPP,
								TclaeDisposeTokenUPP,
								TclaeGetMarkTokenUPP,
								TclaeMarkUPP,
								TclaeAdjustMarksUPP,
								TclaeGetErrorDescUPP);
    
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't set object callbacks: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
        return TCL_ERROR;
	} 
		
	return TCL_OK;		
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_ResolveCmd" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  •
 * 
 * Side effects:
 *  •
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_ResolveCmd(ClientData clientData,
                 Tcl_Interp *interp,
                 int objc,
                 Tcl_Obj *const objv[])
{
	OSErr			err;					/* result from ToolBox calls */		
	int				result;					/* result from Tcl calls */	
    int				j;						/* object variable counter */
    const AEDesc *	objectSpecifier;		/* object specifier record to be resolved */
    AEDesc *		theTokenPtr;      		/* to hold newly created Token */
    short			callbackFlags = kAEIDoMinimum;	
    										/* additional assistance app can provide AEM */
	char *			arg;					/* for option arguments */
	
	/* Scan optional flags */
	for (j = 1; (j < objc) && ((arg = Tcl_GetString(objv[j]))[0] == '-') && (arg[1] != '-'); j++) {
		switch (arg[1]) {
          case 'm':
            callbackFlags |= kAEIDoMarking;
            break;
          case 'w':
            callbackFlags |= kAEIDoWhose;
            break;
		}
	}
    
	if (objc < (j + 1)) {
		Tcl_WrongNumArgs(interp, 1, objv, "?options? <objectSpecifier>");
		return TCL_ERROR;
	}
	
	/* objv[1] holds hash key for original descriptor */
	result = Tclae_GetConstAEDescFromObj(interp, objv[j], &objectSpecifier, true);
	if (result != TCL_OK) {
		return TCL_ERROR;				
	}
    
	/* allocate space for new AEDesc */
    theTokenPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
	
    err = AEResolve(objectSpecifier, callbackFlags, theTokenPtr);
    
	if (err != noErr) {
		ckfree((char *) theTokenPtr);		
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't resolve object \"",
						 Tcl_GetString(objv[1]), "\": ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
        return TCL_ERROR;
	} else {
		Tcl_SetObjResult(interp, Tclae_NewAEDescObj(theTokenPtr));
		return TCL_OK;		
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_CallObjectAccessorCmd" --
 * 
 *  Tcl wrapper for ToolBox AECallObjectAccessor call. 
 *
 *  tclAE::callObjectAccessor <desiredClass> <containerToken> <containerClass> <keyForm> <keyData>
 *
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to hash key for new token
 * -------------------------------------------------------------------------
 */
int 
Tclae_CallObjectAccessorCmd(ClientData clientData,	/* (unused) */  
							Tcl_Interp *interp,		/* for results */  
							int objc,				/* number of arguments */  
							Tcl_Obj *const objv[])	/* argument objects */
{
	OSErr       	err;						/* result from ToolBox calls */
	int				result;						/* result from Tcl calls */
	const AEDesc *	containerToken;				/* the containing AEDesc */
	const AEDesc *	keyData;					/* the AEDesc containing object */
	AEDesc *		tokenPtr = NULL;			/* pointer to new token */
    
	enum {
		kDesiredType = 1,
		kContainerToken,
		kContainerClass,
		kKeyForm,
		kKeyData,
		kTotalArguments
	};
    
	if (objc != kTotalArguments) {
		Tcl_WrongNumArgs(interp, 1, objv, "<desiredClass> <containerToken> <containerClass> <keyForm> <keyData>");
		return TCL_ERROR;
	}
	
	/* objv[kContainerToken] holds reference for the container */
	result = Tclae_GetConstAEDescFromObj(interp, objv[kContainerToken], &containerToken, true);
	if (result != TCL_OK) {
		return TCL_ERROR;				
	}
	/* objv[kKeyData] holds reference for the key data */
	result = Tclae_GetConstAEDescFromObj(interp, objv[kKeyData], &keyData, true);
	if (result != TCL_OK) {
		return TCL_ERROR;				
	}
	
	/* Allocate the coerced AEDesc */
    tokenPtr = (AEDesc *) ckalloc(sizeof(AEDesc));
	if (tokenPtr == NULL) {
		return TCL_ERROR;
	}
		
	err = AECallObjectAccessor(TclaeGetOSTypeFromObj(objv[kDesiredType]),
							   containerToken,
							   TclaeGetOSTypeFromObj(objv[kContainerClass]),
							   TclaeGetOSTypeFromObj(objv[kKeyForm]),
							   keyData,
							   tokenPtr);
		
	if (err != noErr) {
		ckfree((char *)tokenPtr);
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't call object accessor: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
        return TCL_ERROR;
	} else {	
        Tcl_SetObjResult(interp, Tclae_NewAEDescObj(tokenPtr));
        return TCL_OK;
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetObjectAccessorCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetObjectAccessorr call. 
 *  This returns the Tcl proc that has been installed as an object accessor.
 *  
 *  tclAE::getObjectAccessor <desiredClass> <containerType>
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to registered <handlerProc> 
 *  or errAEAccessorNotFound if none
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetObjectAccessorCmd(ClientData clientData, 
                           Tcl_Interp *interp, 
                           int objc, 
                           Tcl_Obj *const objv[])
{
	DescType				desiredClass;
	DescType				containerType;
	Tcl_HashEntry *			hashEntryPtr;	/* for entry in coercion handler hash table */
    OSErr					err;

	OSLAccessorUPP			accessor;
	long 					accessorRefcon;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "<desiredClass> <containerType>");
		return TCL_ERROR;
	}

	desiredClass = TclaeGetOSTypeFromObj(objv[1]);
	containerType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetObjectAccessor(interp, desiredClass, containerType, NULL);
	
	if (hashEntryPtr == NULL) {
		// Check if there's a non-Tcl coercion handler registered in
		// the application handler table.
		// If there is, return nothing.
		err = AEGetObjectAccessor(desiredClass, 
                                  containerType, 
                                  &accessor,
                                  &accessorRefcon, 
                                  false);
		if (err == errAEAccessorNotFound) {
			// Check if there's a non-Tcl coercion handler registered in
			// the system handler table.
			// If there is, return nothing.
			err = AEGetObjectAccessor(desiredClass, 
                                      containerType, 
                                      &accessor,
                                      &accessorRefcon, 
                                      true);
		}
	} else {
		tclAEObjectAccessor*	accessorPtr = Tcl_GetHashValue(hashEntryPtr);
		
	    // Ensure this accessor is actually registered with the AEM
		err = AEGetObjectAccessor(desiredClass, 
                                  containerType, 
                                  &accessor,
                                  &accessorRefcon, 
                                  false);
	                               
	    if ((err != noErr)
	    ||	(accessor != (OSLAccessorUPP)TclaeObjectAccessorUPP)
	    ||	(accessorRefcon != (long) accessorPtr)) {
	    	// Something is severely wrong.
	    	// The accessor in the accessor hash table is either not
	    	// registered with the AEM at all, or it is inconsistent
	    	// with what the AEM thinks it is.
	    	
	    	// Delete this coercion hash entry.
	    	TclaeRemoveObjectAccessor(desiredClass, containerType, hashEntryPtr);
	    	
	    	if (err == noErr) {
	    		// The AEM didn't report an error, but something was
	    		// wrong anyway. Report handler not found.
		    	err = errAEAccessorNotFound;
		    }
	    } else {
	    	// Return <handlerProc>
	    	Tcl_Obj *accessorProcPtr = accessorPtr->accessorProc;
	    	
	    	// Keep interpreter from deleting it
	    	Tcl_IncrRefCount(accessorProcPtr);
	    	
			Tcl_SetObjResult(interp, accessorProcPtr);	    
	    }
	}

	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't find object accessor: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	} else {
		return TCL_OK;
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_InstallObjectAccessorCmd" --
 * 
 *  Tcl wrapper for ToolBox AEInstallObjectAccessor call. 
 *  This allows Tcl procs to act as object accessors.
 *  
 *  tclAE::installObjectAccessor <desiredClass> <containerType> <theAccessor>
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <theAccessor> is registered and added to the object accessor hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_InstallObjectAccessorCmd(ClientData clientData, 
                               Tcl_Interp *interp, 
                               int objc, 
                               Tcl_Obj *const objv[])
{
	DescType				desiredClass;
	DescType				containerType;
	tclAEObjectAccessor *	objectAccessorPtr;
	Tcl_HashEntry *			hashEntryPtr;	/* for entry in object accessor hash table */
    OSErr					err;
    int             		isNew;			/* is hash already used 
											   (shouldn't be!) */
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<desiredClass> <containerType> <theAccessor>");
		return TCL_ERROR;
	}

    // As far as the AEM is concerned, all registered accessors are handled by 
    // TclaeObjectAccessor()
	if (!TclaeObjectAccessorUPP) {
		TclaeObjectAccessorUPP = NewOSLAccessorUPP(TclaeObjectAccessor);
	}
    
	desiredClass = TclaeGetOSTypeFromObj(objv[1]);
	containerType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetObjectAccessor(interp, desiredClass, containerType, NULL);
	
	if (hashEntryPtr == NULL) {
        // Not found. Create a new hash entry for this accessor
        
		objectAccessorPtr = (tclAEObjectAccessor *) ckalloc(sizeof(tclAEObjectAccessor));
		objectAccessorPtr->desiredClass = desiredClass;
		objectAccessorPtr->containerType = containerType;

		// No need to check isNew because that's the only reason we're here
		hashEntryPtr = Tcl_CreateHashEntry(tclAEObjectAccessorHashTable, 
										   (char *) objectAccessorPtr, 
										   &isNew);
		if (isNew) {
			// Set hash entry to point at the accessor record
			Tcl_SetHashValue(hashEntryPtr, objectAccessorPtr);
		}
	} else {
        // Found. Get the existing handler from the hash entry.
		objectAccessorPtr = (tclAEObjectAccessor *) Tcl_GetHashValue(hashEntryPtr);
	}

    // Assign the Tcl proc which is to handle this accessor
	objectAccessorPtr->interp = interp;		 
	objectAccessorPtr->accessorProc = objv[3];	
    // Keep proc from being deleted by the interpreter
	Tcl_IncrRefCount(objv[3]);	 
	
    // Register this accessor with the AEM
	err = AEInstallObjectAccessor(desiredClass,
								  containerType,
                                  TclaeObjectAccessorUPP,
                                  (long) objectAccessorPtr,
                                  false);
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't install object accessor: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	} else {
		return TCL_OK;
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_RemoveObjectAccessorCmd" --
 * 
 *  Tcl wrapper for ToolBox AERemoveObjectAccessor call. 
 *  This removes a Tcl proc that has been installed as an object accessor.
 *  
 *  tclAE::removeObjectAccessor <desiredClass> <containerType> <theAccessor>
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <theAccessor> is deregistered and removed from the object accessor hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_RemoveObjectAccessorCmd(ClientData clientData, 
                              Tcl_Interp *interp, 
                              int objc, 
                              Tcl_Obj *const objv[])
{
	DescType				desiredClass;
	DescType				containerType;
	Tcl_HashEntry			*hashEntryPtr;	/* for entry in coercion handler hash table */
    OSErr					err;
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<desiredClass> <containerType> <theAccessor>");
		return TCL_ERROR;
	}

	desiredClass = TclaeGetOSTypeFromObj(objv[1]);
	containerType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetObjectAccessor(interp, 
                                          desiredClass, 
                                          containerType, 
                                          Tcl_GetString(objv[3]));
		
	if (hashEntryPtr == NULL) {
		err = errAEAccessorNotFound;
	} else {
		err = TclaeRemoveObjectAccessor(desiredClass, 
                                        containerType, 
                                        hashEntryPtr);
	}

	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't remove coercion handler: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	} else {
		return TCL_OK;
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_DisposeTokenCmd" --
 * 
 *  Tcl wrapper for ToolBox AEDisposeToken call 
 *
 *  tclAE::disposeToken <theToken>
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  clientData					(unused)
 *  interp				In		for results
 *  objc				In		number of arguments
 *  objv				In		argument objects
 *  
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  Token is deleted
 * -------------------------------------------------------------------------
 */
int 
Tclae_DisposeTokenCmd(ClientData clientData,
                      Tcl_Interp *interp,
                      int objc,
                      Tcl_Obj *const objv[])
{
	AEDesc *	tokenPtr;    
	int			result;
    
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "<theToken>");
		return TCL_ERROR;
	}
	
	/* Obtain AEDesc pointer from reference and dispose of it */
	result = Tclae_GetAEDescFromObj(interp, objv[1], &tokenPtr, true);
	if (result != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (tokenPtr) {
		OSErr			err;	/* result from ToolBox calls */		
		
		err = AEDisposeToken(tokenPtr);
		/* !!! what if this wasn't ckalloc'ed? 
		 * shouldn't ever happen
		 */
		ckfree((char *)tokenPtr);
		if (err != noErr) {
			Tcl_ResetResult(interp);
			Tcl_AppendResult(interp, "Couldn't dispose of \"",
							 Tcl_GetString(objv[1]), "\": ",
							 Tcl_MacOSError(interp, err), 
							 (char *) NULL);
			return TCL_ERROR;				
		} 
	} else {
		/* 
		 * No such hash entry. 
		 * Throw a slightly bogus "descriptor not found" error  
		 */
		
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't dispose of \"",
						 Tcl_GetString(objv[1]), "\": ",
						 Tcl_MacOSError(interp, errAEDescNotFound), 
						 (char *) NULL);
		return TCL_ERROR;				
    }
	
    return TCL_OK;
}

/* ◊◊◊◊ Object callbacks ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeObjectAccessor" --
 * 
 *  AEM callback routine for all coercions to be handled by Tcl procs
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  ???
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeObjectAccessor(DescType		desiredClass,
					const AEDesc *	containerToken,
					DescType		containerClass,
					DescType		keyForm, 
					const AEDesc *	keyData,
					AEDesc *		theToken, 
					long			theRefcon)
{
	enum {
		kAccessorProc = 0,
		kDesiredClass,
		kContainerToken,
		kContainerClass,
		kKeyForm,
		kKeyData,
		kTheToken,
		kTotalArguments
	};
	Tcl_Obj *				objv[kTotalArguments];
	
	Tcl_HashEntry *			hashEntryPtr;
	tclAEObjectAccessor *	accessorPtr;
	int						result;
	Tcl_CmdInfo				cmdInfo;
	
	// theRefcon holds the hash key for this object accessor
	hashEntryPtr = Tcl_FindHashEntry(tclAEObjectAccessorHashTable, (char *) theRefcon);
	
	if (hashEntryPtr == NULL) {
        // This really shouldn't happen
		return errAEAccessorNotFound;
	}
	
	accessorPtr = (tclAEObjectAccessor *) Tcl_GetHashValue(hashEntryPtr);
	
	// Apparent bug in Tcl_EvalObjv. 
	// If <accessorProc> is undefined in interp, we crash with 
	// an unmapped memory exception, instead of getting an interpreter error
	//    invalid command name "<accessorProc>"
	result = Tcl_GetCommandInfo(accessorPtr->interp, 
								Tcl_GetString(accessorPtr->accessorProc), 
								&cmdInfo);
	if (!result) {
		Tcl_ResetResult(accessorPtr->interp);
		Tcl_AppendResult(accessorPtr->interp, 
						 "Couldn't find object accessor \"",
						 Tcl_GetString(accessorPtr->accessorProc), "\": ",
						 Tcl_MacOSError(accessorPtr->interp, errAEAccessorNotFound), 
						 (char *) NULL);
		return errAEAccessorNotFound;
	}

    // Build up Tcl object accessor command
	objv[kAccessorProc] = accessorPtr->accessorProc;
    // Ensure none of the command objects is disposed of by the interpreter
    Tcl_IncrRefCount(objv[kAccessorProc]);
    
    objv[kDesiredClass] = TclaeNewOSTypeObj(desiredClass);
    objv[kContainerToken] = Tclae_NewConstAEDescRefObj(containerToken);
    objv[kContainerClass] = TclaeNewOSTypeObj(containerClass);
    objv[kKeyForm] = TclaeNewOSTypeObj(keyForm);
    objv[kKeyData] = Tclae_NewConstAEDescRefObj(keyData);
	
    objv[kTheToken] = Tclae_NewAEDescObj(theToken);
    Tcl_IncrRefCount(objv[kTheToken]);
	
    // Execute the coercion handler command
    // [<accessorProc> <desiredClass> <containerToken> <containerClass> <keyForm> <keyData> <theToken>]
	result = Tcl_EvalObjv(accessorPtr->interp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
	
    // Decrement, but don't delete, the handler command 
    Tcl_DecrRefCount(objv[kAccessorProc]);

	// Delete the reference but not the actual AEDesc (that would be Bad™)
	// Can't just decrement, as that will delete the non-const token
	TclaeDetachAEDescObj(objv[kTheToken]);
    Tcl_DecrRefCount(objv[kTheToken]);

	if (result != TCL_OK) {
        OSErr	err = TclaeErrorCodeFromInterp(accessorPtr->interp);
        
        if (err != noErr) {
			return err;
	    } else {
			return errAECoercionFail;
		}
	} else {
		return noErr;	
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeCountObjects" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 *  desiredClass		In		
 *  containerClass		In		
 *  theContainer		In		
 *  countPtr			Out		
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeCountObjects(DescType desiredClass, 
                  DescType containerClass, 
                  const AEDesc *theContainer, 
                  long *countPtr)
{
	enum {
		kCountProc = 0,
		kDesiredClass,
		kContainerClass,
		kContainer,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kCountProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "countObjects", TCL_GLOBAL_ONLY);
	if (!objv[kCountProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kCountProc]);
	
	objv[kDesiredClass] = TclaeNewOSTypeObj(desiredClass);
	objv[kContainerClass] = TclaeNewOSTypeObj(containerClass);
    objv[kContainer] = Tclae_NewConstAEDescRefObj(theContainer);
    
    // Execute the object count command
    // set count [<countProc> <desiredClass> <containerClass> <container>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	} else {
		result = Tcl_GetLongFromObj(gInterp, Tcl_GetObjResult(gInterp), countPtr);
		
		if (result != TCL_OK) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeCompareObjects" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeCompareObjects(DescType comparisonOperator, 
                    const AEDesc *theObject, 
                    const AEDesc *objectOrDescToCompare, 
                    Boolean *comparisonPtr)
{
	enum {
		kCompareProc = 0,
		kComparisonOperator,
		kObject,
		kObjectOrDescToCompare,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kCompareProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "compareObjects", TCL_GLOBAL_ONLY);
	if (!objv[kCompareProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kCompareProc]);
	
	objv[kComparisonOperator] = TclaeNewOSTypeObj(comparisonOperator);
    objv[kObject] = Tclae_NewConstAEDescRefObj(theObject);
    objv[kObjectOrDescToCompare] = Tclae_NewConstAEDescRefObj(objectOrDescToCompare);
    
    // Execute the object comparison command
    // set comparison [<compareProc> <comparisonOperator> <theObject> <objectOrDescToCompare>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	} else {
		int	temp;
		
		result = Tcl_GetBooleanFromObj(gInterp, Tcl_GetObjResult(gInterp), &temp);
		*comparisonPtr = temp;
		
		if (result != TCL_OK) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeDisposeToken" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeDisposeToken(AEDesc *unneededToken)
{
	enum {
		kDisposeProc = 0,
		kUnneededToken,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kDisposeProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "disposeToken", TCL_GLOBAL_ONLY);
	if (!objv[kDisposeProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kDisposeProc]);
	
    objv[kUnneededToken] = Tclae_NewAEDescRefObj(unneededToken);
    Tcl_IncrRefCount(objv[kUnneededToken]);
    
    // Execute the token disposal command
    // [<disposeTokenPro> <unneededToken>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	TclaeDetachAEDescObj(objv[kUnneededToken]);
    Tcl_DecrRefCount(objv[kUnneededToken]);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetErrorDesc" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeGetErrorDesc(AEDescPtr *errDescPtr)
{
	*errDescPtr = &gErrorDesc;
	
	return noErr;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetMarkToken" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeGetMarkToken(const AEDesc *containerToken, 
                  DescType containerClass, 
                  AEDesc *resultDesc)
{
	enum {
		kGetMarkTokenProc = 0,
		kContainerToken,
		kContainerClass,
		kResultDesc,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kGetMarkTokenProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "getMarkToken", TCL_GLOBAL_ONLY);
	if (!objv[kGetMarkTokenProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kGetMarkTokenProc]);
	
    objv[kContainerToken] = Tclae_NewConstAEDescRefObj(containerToken);
	objv[kContainerClass] = TclaeNewOSTypeObj(containerClass);
	
    objv[kResultDesc] = Tclae_NewAEDescRefObj(resultDesc);
    Tcl_IncrRefCount(objv[kResultDesc]);
    
    // Execute the get mark token command
    // [<getMarkTokenProc> <containerToken> <containerClass> <resultDesc>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	TclaeDetachAEDescObj(objv[kResultDesc]);
    Tcl_DecrRefCount(objv[kResultDesc]);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeMark" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  
 * 
 * Side effects:
 *  
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeMark(const AEDesc *theToken, 
          const AEDesc *markToken, 
          long markCount)
{
	enum {
		kMarkProc = 0,
		kTheToken,
		kMarkToken,
		kMarkCount,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kMarkProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "mark", TCL_GLOBAL_ONLY);
	if (!objv[kMarkProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kMarkProc]);
	
    objv[kTheToken] = Tclae_NewConstAEDescRefObj(theToken);
    objv[kMarkToken] = Tclae_NewConstAEDescRefObj(markToken);
    objv[kMarkCount] = Tcl_NewLongObj(markCount);
    
    // Execute the mark command
    // [<markProc> <theToken> <markToken> <markCount>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeAdjustMarks" --
 * 
 *  
 * 
 * Argument     Default In/Out Description
 * ------------ ------- ------ ---------------------------------------------
 * 
 * Results:
 *  •
 * 
 * Side effects:
 *  •
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeAdjustMarks(long newStart, 
                 long newStop, 
                 const AEDesc *markToken)
{
	enum {
		kAdjustMarksProc = 0,
		kNewStart,
		kNewStop,
		kMarkToken,
		kTotalArguments
	};
	Tcl_Obj *	objv[kTotalArguments];
	
	OSErr	err = noErr;
	int		result;
	
	
	objv[kAdjustMarksProc] = Tcl_GetVar2Ex(gInterp, "tclAE::_callbacks", "adjustMarks", TCL_GLOBAL_ONLY);
	if (!objv[kAdjustMarksProc]) {
		return errAEEventNotHandled;
	}
    Tcl_IncrRefCount(objv[kAdjustMarksProc]);
	
    objv[kNewStart] = Tcl_NewLongObj(newStart);
    objv[kNewStop] = Tcl_NewLongObj(newStop);
    objv[kMarkToken] = Tclae_NewConstAEDescRefObj(markToken);
    
    // Execute the adjust marks command
    // [<adjustMarksProc> <newStart> <newStop> <markToken>]
	result = Tcl_EvalObjv(gInterp, kTotalArguments, objv, TCL_EVAL_GLOBAL);
    
	if (result != TCL_OK) {
        err = TclaeErrorCodeFromInterp(gInterp);
        
        if (err == noErr) {
			err = errAEEventNotHandled;
		}
	}
	
	return err;
}

/* ◊◊◊◊ Internal routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetObjectAccessor" --
 * 
 *  Find specified entry in hash table for object accessors
 *  If accessorProc is not NULL, it must match 
 * 
 * Results:
 *  Tcl_HashEntry pointer (or NULL) for desired accessor 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static Tcl_HashEntry*
TclaeGetObjectAccessor(Tcl_Interp*	interp,
                       DescType		desiredClass, 
                       DescType		containerType, 
                       char*		accessorProc)
{
	Tcl_HashEntry			*hashEntryPtr;		/* for search of AEObjectAccessor */
    Tcl_HashSearch			search;				/*    hash list                    */
	tclAEObjectAccessor		*objectAccessorPtr;
	    
    // Search through coercion handler hash table for this type pair
    for (hashEntryPtr = Tcl_FirstHashEntry(tclAEObjectAccessorHashTable, &search);
		 hashEntryPtr != NULL;
		 hashEntryPtr = Tcl_NextHashEntry(&search)) {
		
		objectAccessorPtr = Tcl_GetHashValue(hashEntryPtr);
		if ((objectAccessorPtr->desiredClass == desiredClass)
		&&  (objectAccessorPtr->containerType == containerType)
		&&  (objectAccessorPtr->interp == interp)) {
			if (accessorProc 
			&&	(strcmp(accessorProc, 
						Tcl_GetString(objectAccessorPtr->accessorProc)) != 0)) {
				// accessorProc doesn't match
				continue;
			} else {
	        	// found
				break;
			}
		}
    }
    
    return hashEntryPtr;
}	

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeInitObjectAccessors" --
 * 
 *  Initialize object accessors.
 * 
 * Results:
 *  None.
 * 
 * Side effects:
 *  Object accessors activated.
 * -------------------------------------------------------------------------
 */
void
TclaeInitObjectAccessors(Tcl_Interp *interp)
{
	/* Store identifier for the global error descriptor */
        Tcl_Obj *	newObj = Tclae_NewAEDescRefObj(&gErrorDesc);
        Tcl_IncrRefCount(newObj);
        Tcl_SetVar2Ex(interp, "tclAE::errorDesc", NULL, newObj, TCL_GLOBAL_ONLY);
    
	/* Initialize the AE Handler hash table */
	tclAEObjectAccessorHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	if (tclAEObjectAccessorHashTable) {
		Tcl_InitHashTable(tclAEObjectAccessorHashTable, TCL_ONE_WORD_KEYS);
	} else {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't initialize object accessors", 
						 (char *) NULL);
	}
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeRemoveObjectAccessor" --
 * 
 *  Remove entry from hash table for object accessors, and deregister
 *  accessor with the AEM
 * 
 * Results:
 *  OS Error
 * 
 * Side effects:
 *  Specified accessor is removed
 * -------------------------------------------------------------------------
 */
static OSErr
TclaeRemoveObjectAccessor(
	DescType				desiredClass,
	DescType				containerType,
	Tcl_HashEntry *			hashEntryPtr)
{
	tclAEObjectAccessor*	accessorPtr = Tcl_GetHashValue(hashEntryPtr);
	
    // Delete the object holding the accessor proc
	Tcl_DecrRefCount(accessorPtr->accessorProc);	
	// Remove the coercion hash entry 
	Tcl_DeleteHashEntry(hashEntryPtr);
	// Delete the coercion handler structure
	ckfree((char*) accessorPtr);
	
    // Deregister any accessor for this type-pair with the AEM
	return AERemoveObjectAccessor(desiredClass, 
								  containerType, 
								  TclaeObjectAccessorUPP, 
                            	  false);
}
