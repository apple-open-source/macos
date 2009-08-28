/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEHandler.c"
 *                                    created: 5/6/2000 {11:34:24 PM} 
 *                                last update: 11/21/06 {11:10:47 PM} 
 *  Author: Tim Endres
 *  Author: Pete Keleher
 *  Author: Jon Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 1992      Tim Endres
 *               Copyright (c) 1990-1998 Pete Keleher
 *               Copyright (c) 1999-2003 Jonathan Guyer
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
 # ========================================================================
 #  Description: 
 # 
 #  History
 # 
 *  modified   by  rev reason
 *  ---------- --- --- -----------
 *  1992?      TE? 1.0 original
 *  2000-05-06 JEG 2.0 using tclAE descriptors
 * ========================================================================
 *  See header file for further information
 * ###################################################################
 */

#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#include <Aliases.h>
#include <AppleEvents.h>
#include <TextUtils.h>
#endif

#include <string.h>

#ifdef MAC_TCL
#include <tclMacInt.h>
#elif TARGET_RT_MAC_MACHO
#include "osxMacTcl.h"
#endif

#include "tclAEInt.h"
#include "tclMacOSError.h"

/* Hash table for storage of AppleEvent handlers */
static Tcl_HashTable   *tclAEEventHandlerHashTable;

/* Hash table for storage of coercion handlers */
static Tcl_HashTable   *tclAECoercionHandlerHashTable;

extern AEReturnID gReturnID;

typedef struct tclAEEventHandler {
	OSType		eventClass;
	OSType		eventID;
	Tcl_Obj		*eventHandlerProc;
	Tcl_Interp	*interp;	
} tclAEEventHandler;

typedef struct tclAECoercionHandler {
	OSType		fromType;
	OSType		toType;
	Tcl_Obj		*coercionHandlerProc;
	Tcl_Interp	*interp;	
} tclAECoercionHandler;

static AEEventHandlerUPP	TclaeEventHandlerUPP;
static AEEventHandlerUPP	TclaeReplyHandlerUPP;

static pascal OSErr TclaeEventHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon);
static pascal OSErr TclaeReplyHandler(const AppleEvent *theAppleEvent, AppleEvent *reply, long handlerRefcon);
static OSErr TclaeDispatchEvent(const AppleEvent *theAppleEvent, AppleEvent *reply, tclAEEventHandler *eventHandlerPtr);

static AECoercePtrUPP		TclaeCoercionHandlerUPP;

static AECoercePtrUPP		Tclaealis2TEXTHandlerUPP;
static AECoercePtrUPP		Tclaefss_2TEXTHandlerUPP;
static AECoercePtrUPP		TclaeTEXT2alisHandlerUPP;
static AECoercePtrUPP		TclaeTEXT2fss_HandlerUPP;
static AECoercePtrUPP		TclaeWILD2TEXTHandlerUPP;

static pascal OSErr	Tclaealis2TEXTHandler(DescType dataType, const void *dataPtr, Size dataSize, DescType toType, long refCon, AEDesc *resultDesc);
static pascal OSErr	Tclaefss_2TEXTHandler(DescType dataType, const void *dataPtr, Size dataSize, DescType toType, long refCon, AEDesc *resultDesc);
static pascal OSErr	TclaeTEXT2alisHandler(DescType dataType, const void *dataPtr, Size dataSize, DescType toType, long refCon, AEDesc *resultDesc);
static pascal OSErr	TclaeTEXT2fss_Handler(DescType dataType, const void *dataPtr, Size dataSize, DescType toType, long refCon, AEDesc *resultDesc);
static pascal OSErr	TclaeWILD2TEXTHandler(DescType dataType, const void *dataPtr, Size dataSize, DescType toType, long refCon, AEDesc *resultDesc);


static pascal OSErr	TclaeCoercionHandler(DescType typeCode, const void *dataPtr, Size dataSize, DescType toType, long handlerRefcon, AEDesc *resultDesc);

static Tcl_HashEntry*	TclaeGetCoercionHandler(Tcl_Interp* interp, OSType fromType, OSType toType, char* handlerProc);
static OSErr TclaeRemoveCoercionHandler(OSType fromType, OSType toType, Tcl_HashEntry* hashEntryPtr);

static Tcl_HashEntry*	TclaeGetEventHandler(Tcl_Interp* interp, OSType eventClass, OSType eventID, char* handlerProc);
static OSErr TclaeRemoveEventHandler(OSType eventClass, OSType eventID, Tcl_HashEntry* hashEntryPtr);

#if TARGET_API_MAC_CARBON
static int TclaeGetPathDString(Tcl_Interp *interp, Tcl_Obj *inPath, 
			       Boolean isDirectory, CFURLPathStyle fromPathStyle, 
			       CFURLPathStyle toPathStyle, Tcl_DString *outDS);
#endif

/* ◊◊◊◊ Public package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_InstallCoercionHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AEInstallCoercionHandler call. 
 *  This allows Tcl procs to act as coercion handlers.
 *  
 *  tclAE::installCoercionHander <fromType> <toType> <handlerProc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <handlerProc> is registered and added to the coercion handler hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_InstallCoercionHandlerCmd(ClientData clientData, 
                                Tcl_Interp *interp, 
                                int objc, 
                                Tcl_Obj *const objv[])
{
	OSType					fromType;
	OSType					toType;
	tclAECoercionHandler*	coercionHandlerPtr;
	Tcl_HashEntry*			hashEntryPtr;	/* for entry in coercion handler hash table */
    OSErr					err;
    int             		isNew;			/* is hash already used 
											   (shouldn't be!) */
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<fromType> <toType> <coercionHandlerProc>");
		return TCL_ERROR;
	}

    // As far as the AEM is concerned, all registered coercions are handled by 
    // TclaeCoercionHandler()
	if (!TclaeCoercionHandlerUPP) {
		TclaeCoercionHandlerUPP = NewAECoercePtrUPP(TclaeCoercionHandler);
	}
    
	fromType = TclaeGetOSTypeFromObj(objv[1]);
	toType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetCoercionHandler(interp, fromType, toType, NULL);
	
	if (hashEntryPtr == NULL) {
        // Not found. Create a new hash entry for this coercion
        
		coercionHandlerPtr = (tclAECoercionHandler *) ckalloc(sizeof(tclAECoercionHandler));
		coercionHandlerPtr->fromType = fromType;
		coercionHandlerPtr->toType = toType;

		// No need to check isNew because that's the only reason we're here
		hashEntryPtr = Tcl_CreateHashEntry(tclAECoercionHandlerHashTable, 
										   (char *) coercionHandlerPtr, 
										   &isNew);
		if (isNew) {
			// Set hash entry to point at the coercion handler record
			Tcl_SetHashValue(hashEntryPtr, coercionHandlerPtr);
		}
	} else {
        // Found. Get the existing handler from the hash entry.
		coercionHandlerPtr = (tclAECoercionHandler *) Tcl_GetHashValue(hashEntryPtr);
	}

    // Assign the Tcl proc which is to handle this coercion
	coercionHandlerPtr->interp = interp;		 
	coercionHandlerPtr->coercionHandlerProc = objv[3];	
    // Keep proc from being deleted by the interpreter
	Tcl_IncrRefCount(objv[3]);	 
	
    // Register this coercion with the AEM
	err = AEInstallCoercionHandler(fromType, 
								   toType, 
								   (AECoercionHandlerUPP)TclaeCoercionHandlerUPP, 
								   (long) coercionHandlerPtr, 
                                   false, false);
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't install coercion handler: ",
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
 * "Tclae_RemoveCoercionHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AERemoveCoercionHandler call. 
 *  This removes a Tcl proc that has been installed as a coercion handler.
 *  
 *  tclAE::removeCoercionHander <fromType> <toType> <handlerProc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <handlerProc> is deregistered and removed from the coercion handler hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_RemoveCoercionHandlerCmd(ClientData clientData, 
                               Tcl_Interp *interp, 
                               int objc, 
                               Tcl_Obj *const objv[])
{
	OSType					fromType;
	OSType					toType;
	Tcl_HashEntry			*hashEntryPtr;	/* for entry in coercion handler hash table */
    OSErr					err;
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<fromType> <toType> <coercionHandlerProc>");
		return TCL_ERROR;
	}

	fromType = TclaeGetOSTypeFromObj(objv[1]);
	toType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetCoercionHandler(interp, fromType, toType, Tcl_GetString(objv[3]));
		
	if (hashEntryPtr == NULL) {
		err = -1717;	// No coercion handler found
	} else {
		err = TclaeRemoveCoercionHandler(fromType, toType, hashEntryPtr);
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
 * "Tclae_GetCoercionHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetCoercionHandler call. 
 *  This returns the Tcl proc that has been installed as a coercion handler.
 *  
 *  tclAE::getCoercionHander <fromType> <toType>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to registered <handlerProc> 
 *  or OSErr -1717 if none
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetCoercionHandlerCmd(ClientData clientData, 
                            Tcl_Interp *interp, 
                            int objc, 
                            Tcl_Obj *const objv[])
{
	OSType					fromType;
	OSType					toType;
	Tcl_HashEntry*			hashEntryPtr;	/* for entry in coercion handler hash table */
    OSErr					err;

	AECoercionHandlerUPP	handler;
	long 					handlerRefcon;
	Boolean					fromTypeIsDesc;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "<fromType> <toType>");
		return TCL_ERROR;
	}

	fromType = TclaeGetOSTypeFromObj(objv[1]);
	toType = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetCoercionHandler(interp, fromType, toType, NULL);
	
	if (hashEntryPtr == NULL) {
		// Check if there's a non-Tcl coercion handler registered in
		// the application handler table.
		// If there is, return nothing.
		err = AEGetCoercionHandler(fromType, 
								   toType, 
								   &handler,
								   &handlerRefcon, 
	                               &fromTypeIsDesc,
	                               false);
		if (err == errAEHandlerNotFound) {
			// Check if there's a non-Tcl coercion handler registered in
			// the system handler table.
			// If there is, return nothing.
			err = AEGetCoercionHandler(fromType, 
									   toType, 
									   &handler,
									   &handlerRefcon, 
									   &fromTypeIsDesc,
									   true);
		}
	} else {
		tclAECoercionHandler*	coercionHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
		
	    // Ensure this handler is actually registered with the AEM
		err = AEGetCoercionHandler(fromType, 
								   toType, 
								   &handler,
								   &handlerRefcon, 
	                               &fromTypeIsDesc,
	                               false);
	                               
	    if ((err != noErr)
	    ||	(handler != (AECoercionHandlerUPP)TclaeCoercionHandlerUPP)
	    ||	(handlerRefcon != (long) coercionHandlerPtr)
	    ||	fromTypeIsDesc) {
	    	// Something is severely wrong.
	    	// The handler in the coercion hash table is either not
	    	// registered with the AEM at all, or it is inconsistent
	    	// with what the AEM thinks it is.
	    	
	    	// Delete this coercion hash entry.
	    	TclaeRemoveCoercionHandler(fromType, toType, hashEntryPtr);
	    	
	    	if (err == noErr) {
	    		// The AEM didn't report an error, but something was
	    		// wrong anyway. Report handler not found.
		    	err = errAEHandlerNotFound;
		    }
	    } else {
	    	// Return <handlerProc>
	    	Tcl_Obj *handlerProcPtr = coercionHandlerPtr->coercionHandlerProc;
	    	
	    	// Keep interpreter from deleting it
	    	Tcl_IncrRefCount(handlerProcPtr);
	    	
			Tcl_SetObjResult(interp, handlerProcPtr);	    
	    }
	}

	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't find coercion handler: ",
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
 * "Tclae_InstallEventHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AEInstallEventHandler call. 
 *  This allows Tcl procs to act as event handlers.
 *  
 *  tclAE::installEventHander <aeclass> <aeeventID> <eventHandlerProc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <handlerProc> is registered and added to the event handler hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_InstallEventHandlerCmd(ClientData clientData, 
							 Tcl_Interp *interp, 
							 int objc, 
							 Tcl_Obj *const objv[])
{
	OSType				eventClass;
	OSType				eventID;
	tclAEEventHandler*	eventHandlerPtr;
	Tcl_HashEntry*		hashEntryPtr;	/* for entry in event handler hash table */
    OSErr				err;
    int             	isNew;			/* is hash already used 
											   (shouldn't be!) */
	
	if (objc < 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<aeclass> <aeeventID> <eventHandlerProc>");
		return TCL_ERROR;
	}

    // As far as the AEM is concerned, all registered events are handled by 
    // TclaeEventHandler()
	if (!TclaeEventHandlerUPP) {
		TclaeEventHandlerUPP = NewAEEventHandlerUPP(TclaeEventHandler);
	}

	eventClass = TclaeGetOSTypeFromObj(objv[1]);
	eventID = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetEventHandler(interp, eventClass, eventID, NULL);	
	
	if (hashEntryPtr == NULL) {
        // Not found. Create a new hash entry for this event and ID
        
		eventHandlerPtr = (tclAEEventHandler *) ckalloc(sizeof(tclAEEventHandler));
		eventHandlerPtr->eventClass = eventClass;
		eventHandlerPtr->eventID = eventID;

		// No need to check isNew because that's the only reason we're here
		hashEntryPtr = Tcl_CreateHashEntry(tclAEEventHandlerHashTable, 
										   (char *) eventHandlerPtr, 
										   &isNew);
										   
		if (isNew) {
			// Set hash entry to point at the event handler record
			Tcl_SetHashValue(hashEntryPtr, eventHandlerPtr);
		}
	} else {
        // Found. Get the existing handler from the hash entry.
		eventHandlerPtr = (tclAEEventHandler *) Tcl_GetHashValue(hashEntryPtr);
	}

    // Assign the Tcl proc which is to handle this event
	eventHandlerPtr->interp = interp;		 
	eventHandlerPtr->eventHandlerProc = objv[3];	
    // Keep proc from being deleted by the interpreter
	Tcl_IncrRefCount(objv[3]);	 
	
    // Register this event with the AEM
	err = AEInstallEventHandler(eventClass, 
								eventID, 
								TclaeEventHandlerUPP, 
								(long) eventHandlerPtr, 
								false);
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't install event handler: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_RemoveEventHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AERemoveEventHandler call. 
 *  This removes a Tcl proc that has been installed as an event handler.
 *  
 *  tclAE::removeEventHander <AEClass> <AEEventID> <handlerProc>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  <handlerProc> is deregistered and removed from the event handler hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_RemoveEventHandlerCmd(ClientData clientData, 
                            Tcl_Interp *interp, 
                            int objc, 
                            Tcl_Obj *const objv[])
{
	OSType				eventClass;
	OSType				eventID;
	Tcl_HashEntry*		hashEntryPtr;	/* for entry in event handler hash table */
    OSErr				err;
	
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<aeclass> <aeeventID> <eventHandlerProc>");
		return TCL_ERROR;
	}

    // As far as the AEM is concerned, all registered events are handled by 
    // TclaeEventHandler()
	if (!TclaeEventHandlerUPP) {
		TclaeEventHandlerUPP = NewAEEventHandlerUPP(TclaeEventHandler);
	}

	eventClass = TclaeGetOSTypeFromObj(objv[1]);
	eventID = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetEventHandler(interp, eventClass, eventID, Tcl_GetString(objv[3]));	
	
	if (hashEntryPtr == NULL) {
		err = errAEHandlerNotFound;	// No event handler found
	} else {
		err = TclaeRemoveEventHandler(eventClass, eventID, hashEntryPtr);
	}

	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't remove event handler: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetEventHandlerCmd" --
 * 
 *  Tcl wrapper for ToolBox AEGetEventHandler call. 
 *  This returns the Tcl proc that has been installed as an event handler.
 *  
 *  tclAE::getEventHander <aeclass> <aeeventID>
 * 
 * Results:
 *  Tcl result code
 * 
 * Side effects:
 *  result of interp is set to registered <handlerProc> 
 *  or errAEHandlerNotFound if none
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetEventHandlerCmd(ClientData clientData, 
						 Tcl_Interp *interp, 
						 int objc, 
						 Tcl_Obj *const objv[])
{
	OSType				eventClass;
	OSType				eventID;
	Tcl_HashEntry		*hashEntryPtr;	/* for entry in event handler hash table */
    OSErr				err;

	AEEventHandlerUPP	handler;
	long 				handlerRefcon;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "<aeclass> <aeeventID>");
		return TCL_ERROR;
	}

	eventClass = TclaeGetOSTypeFromObj(objv[1]);
	eventID = TclaeGetOSTypeFromObj(objv[2]);
	
	hashEntryPtr = TclaeGetEventHandler(interp, eventClass, eventID, NULL);	
	
	if (hashEntryPtr == NULL) {
		// Check if there's a non-Tcl event handler registered in
		// the application handler table.
		// If there is, return nothing.
		err = AEGetEventHandler(eventClass, 
								eventID, 
								&handler,
								&handlerRefcon, 
								false);
		if (err != errAEHandlerNotFound) {
			// Check if there's a non-Tcl event handler registered in
			// the system handler table.
			// If there is, return nothing.
			err = AEGetEventHandler(eventClass, 
									eventID, 
									&handler,
									&handlerRefcon, 
									true);
		}		
	} else {
		tclAEEventHandler*		eventHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
		
	    // Ensure this handler is actually registered with the AEM
		err = AEGetEventHandler(eventClass, 
								eventID, 
								&handler,
								&handlerRefcon, 
								false);
	                               
	    if ((err != noErr)
	    ||	(handler != TclaeEventHandlerUPP)
	    ||	(handlerRefcon != (long) eventHandlerPtr)) {
	    	// Something is severely wrong.
	    	// The handler in the event handler hash table is either not
	    	// registered with the AEM at all, or it is inconsistent
	    	// with what the AEM thinks it is.
	    	
	    	// Delete this event handler hash entry.
	    	TclaeRemoveEventHandler(eventClass, eventID, hashEntryPtr);
	    	
	    	if (err == noErr) {
	    		// The AEM didn't report an error, but something was
	    		// wrong anyway. Report handler not found
		    	err = errAEHandlerNotFound;
		    }
	    } else {
	    	// Return <handlerProc>
	    	Tcl_Obj *handlerProcPtr = eventHandlerPtr->eventHandlerProc;
	    	
	    	// Keep interpreter from deleting it
	    	Tcl_IncrRefCount(handlerProcPtr);
	    	
			Tcl_SetObjResult(interp, handlerProcPtr);	    
	    }
	}

	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't find event handler: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

/* ◊◊◊◊ Handler callbacks ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeCoercionHandler" --
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
pascal OSErr
TclaeCoercionHandler(DescType		typeCode, 
					 const void	*	dataPtr, 
					 Size			dataSize, 
					 DescType		toType, 
					 long			handlerRefcon, 
					 AEDesc	*		resultDesc)
{
	Tcl_HashEntry *			hashEntryPtr;
	tclAECoercionHandler *	coercionHandlerPtr;
	Tcl_Obj *				objv[5];
	int						result;
	
	enum {
		kHandlerProc = 0,
		kTypeCode,
		kData,
		kToType,
		kResultDesc
	};
    
	// handlerRefcon holds the hash key for this coercion handler
	hashEntryPtr = Tcl_FindHashEntry(tclAECoercionHandlerHashTable, (char *) handlerRefcon);
	
	if (hashEntryPtr == NULL) {
        // This really shouldn't happen
		return errAEHandlerNotFound;
	}
	
	coercionHandlerPtr = (tclAECoercionHandler *) Tcl_GetHashValue(hashEntryPtr);
	
    // Build up Tcl coercion handler command
	objv[kHandlerProc] = coercionHandlerPtr->coercionHandlerProc;
    // Ensure none of the command objects is disposed of by the interpreter
    Tcl_IncrRefCount(objv[kHandlerProc]);
    
    objv[kTypeCode] = TclaeNewOSTypeObj(typeCode);
    Tcl_IncrRefCount(objv[kTypeCode]);

    if (typeCode == typeChar) {
        Tcl_DString	ds;
        
        Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, dataPtr, dataSize, &ds);
        
        objv[kData] = Tcl_NewStringObj(Tcl_DStringValue(&ds), -1); 
        
        Tcl_DStringFree(&ds);
    } else {
        objv[kData] = Tcl_NewByteArrayObj((unsigned char *) dataPtr, dataSize);
    } 
    Tcl_IncrRefCount(objv[kData]);
    
    objv[kToType] = TclaeNewOSTypeObj(toType);
    Tcl_IncrRefCount(objv[kToType]);

    objv[kResultDesc] = Tclae_NewAEDescRefObj(resultDesc);
    Tcl_IncrRefCount(objv[kResultDesc]);
	
    // Execute the coercion handler command
    // [<coercionHandlerProc> <typeCode> <data> <toType> <resultDesc>]
	result = Tcl_EvalObjv(coercionHandlerPtr->interp, 5, objv, TCL_EVAL_GLOBAL);
	
    // Decrement, but don't delete, the handler command 
    Tcl_DecrRefCount(objv[kHandlerProc]);

    Tcl_DecrRefCount(objv[kTypeCode]);
    Tcl_DecrRefCount(objv[kData]);
    Tcl_DecrRefCount(objv[kToType]);

	// Delete the object but not the actual AEDesc (that would be Bad™)
    TclaeDetachAEDescObj(objv[kResultDesc]);

	if (result != TCL_OK) {
        OSErr	err = TclaeErrorCodeFromInterp(coercionHandlerPtr->interp);
        
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
 * "TclaeEventHandler" --
 * 
 *  AEM callback routine for all AppleEvents to be handled by Tcl procs
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  Event is handled. Reply AppleEvent is manipulated.
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeEventHandler(const AppleEvent *theAppleEvent, 
				   AppleEvent *reply, 
				   long handlerRefcon)
{
	Tcl_HashEntry		*hashEntryPtr;
	tclAEEventHandler	*eventHandlerPtr;
    
	// handlerRefcon holds the hash key for this event handler
	hashEntryPtr = Tcl_FindHashEntry(tclAEEventHandlerHashTable, (char *) handlerRefcon);
	
	if (hashEntryPtr == NULL) {
        // This really shouldn't happen
		return errAEHandlerNotFound;
	}
	
	eventHandlerPtr = (tclAEEventHandler *) Tcl_GetHashValue(hashEntryPtr);
	
	return TclaeDispatchEvent(theAppleEvent, reply, eventHandlerPtr);
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeReplyHandler" --
 * 
 *  AEM callback routine for reply events from AppleEvents we sent
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  Event is handled.
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr 
TclaeReplyHandler(const AppleEvent *theAppleEvent, 
				   AppleEvent *reply, 
				   long handlerRefcon)
{
	Tcl_HashEntry		*hashEntryPtr;
	tclAEEventHandler	*eventHandlerPtr;
	
	OSErr				err;
	Boolean				generic = false;
	
	AEReturnID			returnID;
	DescType			typeCode;
	Size				actualSize;
	
    
	// get the returnID attribute of the event and look up the appropriate
	// handler in the hash table
	
	err = AEGetAttributePtr(theAppleEvent, keyReturnIDAttr, typeSInt16, 
							&typeCode, &returnID, sizeof(returnID), &actualSize);
							
	switch (err) {	
		case noErr:	{
				SInt32 hashKey = returnID;
				hashEntryPtr = Tcl_FindHashEntry(tclAEEventHandlerHashTable, (char *) hashKey);
			}
			break;
			
		case errAECoercionFail:
		case errAEDescNotFound:
			// handlerRefcon holds the returnID key for generic replies 
			// (not specified by -Q)
			hashEntryPtr = Tcl_FindHashEntry(tclAEEventHandlerHashTable, (char *) handlerRefcon);

			generic = true;
			break;
			
		default:
			return err;
	}
						
	if (hashEntryPtr == NULL) {
        // This really shouldn't happen
		return errAEHandlerNotFound;
	}
	
	
	eventHandlerPtr = (tclAEEventHandler *) Tcl_GetHashValue(hashEntryPtr);
	
	err = TclaeDispatchEvent(theAppleEvent, reply, eventHandlerPtr);
	
	if (!generic) {
		// -Q handlers are one-time-only
		Tcl_DeleteHashEntry(hashEntryPtr);
		Tcl_DecrRefCount(eventHandlerPtr->eventHandlerProc);
		ckfree((char *) eventHandlerPtr);
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeDispatchEvent" --
 * 
 *  Dispatch theAppleEvent to the appropriate Tcl proc
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  Event is handled.
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static OSErr 
TclaeDispatchEvent(const AppleEvent *theAppleEvent, 
		   AppleEvent *reply, 
		   tclAEEventHandler *eventHandlerPtr)
{
    Tcl_Obj *			objv[3];
    int				result = TCL_OK;
    CONST84_RETURN char *	directResult;
    
    // Build up Tcl event handler command
    objv[0] = eventHandlerPtr->eventHandlerProc;
    
    // Ensure none of the command objects is disposed of by the interpreter
    Tcl_IncrRefCount(objv[0]);
    
    objv[1] = Tclae_NewConstAEDescRefObj(theAppleEvent);
    Tcl_IncrRefCount(objv[1]);
    
    objv[2] = Tclae_NewAEDescRefObj(reply);
    Tcl_IncrRefCount(objv[2]);
    
    // Execute the event handler command
    // [<eventHandlerProc> <theAppleEvent> <reply>]
    result = Tcl_EvalObjv(eventHandlerPtr->interp, 3, objv, TCL_EVAL_GLOBAL);
    
    // Decrement, but don't delete, the handler command 
    Tcl_DecrRefCount(objv[0]);
    
    // Delete the objects and descriptors, 
    // but not the actual AppleEvents (that would be Bad™)
    Tcl_DecrRefCount(objv[1]);
    
    // Can't just decrement, as that will delete the non-const reply
    TclaeDetachAEDescObj(objv[2]);
    Tcl_DecrRefCount(objv[2]);
    
    // See if there was a return value
    directResult = Tcl_GetStringResult(eventHandlerPtr->interp);
    
    if (result == TCL_OK) {
	// If there was a reply from the proc, consider using it as the 
	// direct object of the reply event
	if (strlen(directResult) > 0) {
	    OSErr	err;
	    AEDesc		tempDesc;
	    
	    err = AEGetParamDesc(reply, 
				 keyDirectObject, 
				 typeWildCard, 
				 &tempDesc);
	    
	    AEDisposeDesc(&tempDesc);
	    
	    if (err == errAEDescNotFound) {
		Tcl_DString		resultDS;
		
		Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
					 directResult, -1, 
					 &resultDS);
		
		// There was no user inserted direct object ('----'), so
		// we insert the return value from the Tcl handler.
		err = AEPutParamPtr(reply, 
				    keyDirectObject, 
				    typeChar, 
				    Tcl_DStringValue(&resultDS), 
				    Tcl_DStringLength(&resultDS));
		
		Tcl_DStringFree(&resultDS);
		
		if (err) {
		    Tcl_ResetResult(eventHandlerPtr->interp);
		    Tcl_AppendResult(eventHandlerPtr->interp, "Couldn't put direct object: ",
				     Tcl_MacOSError(eventHandlerPtr->interp, err), 
				     (char *) NULL);
		}
	    }
	}
	return noErr;
    } else {
	OSErr			err;
	Tcl_DString		resultDS;
	
	Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
				 directResult, -1, 
				 &resultDS);
	
	err = AEPutParamPtr(reply, 
			    keyErrorString, 
			    typeChar, 
			    Tcl_DStringValue(&resultDS), 
			    Tcl_DStringLength(&resultDS));
	
	Tcl_DStringFree(&resultDS);
	
	err = TclaeErrorCodeFromInterp(eventHandlerPtr->interp);
	if (err != noErr) {
	    AEPutParamPtr(reply, keyErrorNumber, typeSInt16, &err, sizeof(OSErr));
	} else {
            Tcl_CmdInfo			cmdInfo;
            
            if (!Tcl_GetCommandInfo(eventHandlerPtr->interp, Tcl_GetString(eventHandlerPtr->eventHandlerProc), &cmdInfo)) {
                Tcl_DString		resultDS;
                
                Tcl_DStringInit(&resultDS);
                Tcl_DStringAppend(&resultDS, "invalid command name \"", -1);
                Tcl_DStringAppend(&resultDS, Tcl_GetString(eventHandlerPtr->eventHandlerProc), -1);
                Tcl_DStringAppend(&resultDS, "\"", 1);
                err = AEPutParamPtr(reply, 
                                    keyErrorString, 
                                    typeChar, 
                                    Tcl_DStringValue(&resultDS), 
                                    Tcl_DStringLength(&resultDS));
                                    
                Tcl_DStringFree(&resultDS);
                
                if (err == noErr) {
                    err = errAEHandlerNotFound;
                }
                
                AEPutParamPtr(reply, keyErrorNumber, typeSInt16, &err, sizeof(OSErr));
            } else {
                err = 12345;
	    
                AEPutParamPtr(reply, keyErrorNumber, typeSInt16, &err, sizeof(OSErr));
            }            
	}
	return noErr;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclaealis2TEXTHandler" --
 * 
 *  Translate an AliasRecord to a path
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  result AEDesc is set to a 'TEXT' descriptor holding the path
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr
Tclaealis2TEXTHandler(DescType		typeCode, 
					   const void	*dataPtr, 
					   Size			dataSize, 
					   DescType		toType, 
					   long			handlerRefcon, 
					   AEDesc		*resultDesc)
{
	Boolean		wasChanged;
	FSSpec		fss;
	OSStatus	err;
	AliasHandle aliasH;
		
	PtrToHand(dataPtr, (Handle *) &aliasH, dataSize);
    
    // Identify the target of the alias record
	err = ResolveAlias(NULL, aliasH, &fss, &wasChanged);
	if (err == noErr)  {
		// use Tclaefss_2TEXTHandler to get the paths to the file // das 25/10/00: Carbonization
		err = Tclaefss_2TEXTHandler(typeCode, &fss, sizeof(FSSpec), toType, handlerRefcon, resultDesc);
    } else if (err == fnfErr) {
	// ResolveAlias doesn't work for alias that don't exist yet
	// so we need to be a bit more creative
	
	// None of this will work on Mac OS X
	// For some hammer-headed reason, alias AEDescs /cannot/ refer
	// to a file that doesn't exist yet (tn2022), even though that's a 
	// major reason to have aliases in the first place!!!
    	long	gestalt;
    	
#if TARGET_API_MAC_OSX	
	err = Gestalt(gestaltAliasMgrAttr, &gestalt);
	if (err == noErr 
	&&  (gestalt & (1 << gestaltAliasMgrSupportsFSCalls))) {
	    CFStringRef		pathString;
	    
	    err = FSCopyAliasInfo(aliasH, NULL,NULL, &pathString, NULL, NULL);
	    if (err == noErr) {
		Tcl_DString	ds;
		
		if (CFStringToExternalDString(NULL, pathString, &ds) == TCL_OK) {
		    err = AECreateDesc(typeText, 
		    			Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), 
					resultDesc);
		} else {
		    err = paramErr;
		}
		
		Tcl_DStringFree(&ds);
		CFRelease(pathString);
    	    }	    
	} else 
#endif	
	{
	    AliasInfoType	index;
	    Tcl_DString		ds1, ds2;
	    Tcl_DString *	ds1P = &ds1;
	    Tcl_DString *	ds2P = &ds2;
	    Tcl_DString *	ds3P;
	    
	    Tcl_DStringInit(ds1P);
	    Tcl_DStringInit(ds2P);
	    
	    for (index = 0; ; index++) {
	    	Str63           theString;
		
		err = GetAliasInfo(aliasH, index, theString);
		if (err != noErr || theString[0] == 0) {
		    break;
		}
		
		Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, 
					 (char *) &theString[1], theString[0], ds1P);
					 
					 
		if (Tcl_DStringLength(ds2P) > 0) {
		    Tcl_DStringAppend(ds1P, ":", 1);
		    Tcl_DStringAppend(ds1P, Tcl_DStringValue(ds2P), Tcl_DStringLength(ds2P));
		}
		
		// Swap the pointers
		ds3P = ds2P;
		ds2P = ds1P;
		ds1P = ds3P;
	    }
	    
	    if (err == noErr) {
		Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
					Tcl_DStringValue(ds2P), Tcl_DStringLength(ds2P), ds1P);
		err = AECreateDesc(typeChar, Tcl_DStringValue(ds1P),
					Tcl_DStringLength(ds1P), resultDesc);
	    }
					
	    Tcl_DStringFree(&ds1);
	    Tcl_DStringFree(&ds2);
	}
    }
    DisposeHandle((Handle) aliasH);
    return err;
    
    // tclAE::coerceDesc [tclAE::build::alis "/Volumes/programming/Alpha/Head/Alpha/Alpha Sources/iggy.c"] TEXT
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclaefss_2TEXTHandler" --
 * 
 *  Translate an FSSpec to a path
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  result AEDesc is set to a 'TEXT' descriptor holding the path
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr
Tclaefss_2TEXTHandler(DescType		typeCode, 
		      const void *	dataPtr, 
		      Size		dataSize, 
		      DescType		toType, 
		      long		handlerRefcon, 
		      AEDesc *		resultDesc)
{
    int			len;
    OSErr		err;
    Handle		pathH;
    
    // Obtain the path to the file
    err = FSpPathFromLocation((FSSpec *) dataPtr, &len, &pathH);
    if (err == noErr) {
	HLock(pathH);
	// FSpPathFromLocation() returns a C string, so strip the trailing '\0'
	err = AECreateDesc(toType, *pathH, len, resultDesc);
	DisposeHandle(pathH);
    }
    return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeTEXT2alisHandler" --
 * 
 *  Translate a path to an AliasRecord
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  result AEDesc is set to an 'alis' descriptor for the file
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr
TclaeTEXT2alisHandler(DescType 		dataType,
		      const void *	dataPtr,
		      Size 		dataSize,
		      DescType 		toType,
		      long 		refCon,
		      AEDesc *		resultDesc)
{
#ifdef TCLAE_CARBON_USE_CFURL
    OSErr err = noErr;
    CFURLRef url = NULL;
    CFDataRef dataRef = NULL;

    url=CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, dataPtr, dataSize, TRUE);
    if (url == NULL) return coreFoundationUnknownErr;
    dataRef = CFURLCreateData(kCFAllocatorDefault, url, kCFStringEncodingUTF8, true);
    if (dataRef) {
        CFIndex dataSize = CFDataGetLength(dataRef);
        err = AECreateDesc(typeFileURL, (Ptr)CFDataGetBytePtr(dataRef), dataSize, resultDesc);
        CFRelease(dataRef);
        CFRelease(url);
    } else err = coreFoundationUnknownErr;

    return err;
#else
    FSSpec	fss;
    AliasHandle	alisH;
    OSErr	err;
    
    /* Use this instead of NewAliasMinimalFromFullPath() so that 
     * we can get alii of partial paths, too. Nifty.
     */
    err = FSpLocationFromPath(dataSize, dataPtr, &fss);
    
    switch (err) {
	case noErr: {
	    err = NewAliasMinimal(&fss, &alisH);
	}
	break;
	
#if TARGET_API_MAC_OS8	
	case fnfErr: {
	    err = NewAliasMinimal(&fss, &alisH);
	    /* The file doesn't exist, so FSpLocationFromPath() won't work.
	     * Do the best we can with NewAliasMinimalFromFullPath().
	     * Ultimately, we should implement an alias version of FSpLocationFromPath()
	     */
	    if (err != noErr)
		err = NewAliasMinimalFromFullPath(dataSize, dataPtr, "\p", "\p", &alisH);
#if TARGET_API_MAC_OSX
	    if (err == paramErr) {
		/* possibly we were passed a POSIX path and NewAliasMinimalFromFullPath()
		 * needs an HFS path (although Apple has not deigned to document that 
		 * #*%! fact).
		 */
		
		Tcl_DString	ds;
		int		result;
		
		Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, 
					 dataPtr, dataSize, &ds);
		
		result = TclaeGetPathDString(NULL, 
					     Tcl_NewStringObj(Tcl_DStringValue(&ds), 
							      Tcl_DStringLength(&ds)), 
					     FALSE, 
					     kCFURLPOSIXPathStyle, kCFURLHFSPathStyle, &ds);
		
		Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
					 Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), &ds);
		
		err = NewAliasMinimalFromFullPath(Tcl_DStringLength(&ds), Tcl_DStringValue(&ds), 
						  "", "", &alisH);
	    }
#endif			
	}
	break;
#endif	
    }
    
    if (err == noErr) {
	HLock((Handle) alisH);
	err = AECreateDesc(toType, *alisH, GetHandleSize((Handle) alisH), resultDesc);
	DisposeHandle((Handle) alisH);
    }
    
    return err;
#endif
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeTEXT2fss_Handler" --
 * 
 *  Translate a path to an FSSpec
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  result AEDesc is set to an 'fss ' descriptor for the file
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr
TclaeTEXT2fss_Handler(DescType 	dataType,
					   const void	*dataPtr,
					   Size 		dataSize,
					   DescType 	toType,
					   long 		refCon,
					   AEDesc 		*resultDesc)
{
	FSSpec		fss;
	OSErr		err;
	
	err = FSpLocationFromPath(dataSize, dataPtr, &fss);
	
	if (err == noErr) {
		err = AECreateDesc(toType, &fss, sizeof(FSSpec), resultDesc);
	}
	
	return err;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeWILD2TEXTHandler" --
 * 
 *  raw data as text
 * 
 * Results:
 *  MacOS error code
 * 
 * Side effects:
 *  result AEDesc is set to a 'TEXT' descriptor holding the data
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
static pascal OSErr
TclaeWILD2TEXTHandler(DescType		typeCode, 
					   const void	*dataPtr, 
					   Size			dataSize, 
					   DescType		toType, 
					   long			handlerRefcon, 
					   AEDesc		*resultDesc)
{
	return AECreateDesc(typeChar, dataPtr, dataSize, resultDesc);
}

/* ◊◊◊◊ Internal package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeErrorCodeFromInterp" --
 * 
 *  Attempt to extract an integer error code from the interpreter result code.
 *  This routine assumes that it's been called in response to a TCL_ERROR.
 *
 *  It expects errorCode to be:
 *		<integer>
 *	or
 *		{<category> <integer> <message>}
 *
 *	or the result to be an integer.
 *
 * 
 * Results:
 *  An OSErr.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
 
OSErr
TclaeErrorCodeFromInterp(Tcl_Interp *interp)
{
    Tcl_Obj *	errorCodePtr;
    int			result;			/* result from Tcl calls */
    int			errorCode = noErr;
    
    // Get the error code
    errorCodePtr = Tcl_ObjGetVar2(interp, 
    							  Tcl_NewStringObj("errorCode", -1), 
    							  NULL, 
    							  TCL_GLOBAL_ONLY);

	// See if errorCode is an integer
	result = Tcl_GetIntFromObj(NULL, errorCodePtr, &errorCode); 

	// If not…
    if (result != TCL_OK) {
    	int		listLength;
    	
    	// See if errorCode is a list
    	result = Tcl_ListObjLength(NULL, errorCodePtr, &listLength);
    	if ((result == TCL_OK)
    	&&	(listLength >= 2)) {
    		Tcl_Obj *	errorSubCodePtr;
    	
    		// See if second item is an integer
			result = Tcl_ListObjIndex(NULL, errorCodePtr, 1, &errorSubCodePtr);
			if (result == TCL_OK) {
        		result = Tcl_GetIntFromObj(NULL, errorSubCodePtr, &errorCode); 
        	}
    	}        
    }
    
    // No error code found, so see if the result was an integer
    if (errorCode == noErr) {
    	errorCodePtr = Tcl_GetObjResult(interp);
		result = Tcl_GetIntFromObj(NULL, errorCodePtr, &errorCode); 
    }
    
    return errorCode;	// coerce integer to 16-bit OSErr
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeInitAEHandlerHashTable" --
 * 
 *  Initialize hash table for AE handlers, allowing AppleEvents to be
 *  handled by Tcl procs.
 * 
 * Results:
 *  None.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int
TclaeInitEventHandlers(Tcl_Interp *interp)
{
	OSErr					err;
	tclAEEventHandler		*eventHandlerPtr = NULL;
	Tcl_HashEntry			*hashEntryPtr;
	int						isNew = 0;

	// Initialize the AE Handler hash table
	tclAEEventHandlerHashTable = (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
	if (tclAEEventHandlerHashTable) {
		Tcl_InitHashTable(tclAEEventHandlerHashTable, TCL_ONE_WORD_KEYS);
	} else {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't initialize AppleEvent handlers", 
						 (char *) NULL);
		return TCL_ERROR;	
	}
	
	// Set up the generic reply handler
	eventHandlerPtr = (tclAEEventHandler *) ckalloc(sizeof(tclAEEventHandler));
	
	eventHandlerPtr->eventClass = kCoreEventClass;
	eventHandlerPtr->eventID = kAEAnswer;

    // Assign the Tcl proc which is to handle this event
	eventHandlerPtr->interp = interp;		 
	eventHandlerPtr->eventHandlerProc = Tcl_NewStringObj("aeom::handleAnswer", -1);
    // Keep proc from being deleted by the interpreter
	Tcl_IncrRefCount(eventHandlerPtr->eventHandlerProc);	 

	do {
		SInt32 hashKey = ++gReturnID;
		hashEntryPtr = Tcl_CreateHashEntry(tclAEEventHandlerHashTable, 
										   (char *) hashKey, &isNew);
	} while (!isNew);
	
										   
	// Set hash entry to point at the event handler record
	Tcl_SetHashValue(hashEntryPtr, eventHandlerPtr);
	
	
    // Register generic reply handler with the AEM
	if (!TclaeReplyHandlerUPP) {
		TclaeReplyHandlerUPP = NewAEEventHandlerUPP(TclaeReplyHandler);
	}
	err = AEInstallEventHandler(kCoreEventClass, kAEAnswer, 
								TclaeReplyHandlerUPP, (UInt32) gReturnID, false);
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't install reply handler: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
    err = AESetInteractionAllowed(kAEInteractWithAll);
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't set interaction to kAEInteractWithAll: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeInitCoercionHandlers" --
 * 
 *  Initialize coercion handlers.
 * 
 * Results:
 *  None.
 * 
 * Side effects:
 *  Coercion handlers activated.
 * -------------------------------------------------------------------------
 */
void
TclaeInitCoercionHandlers(Tcl_Interp *interp)
{
	OSErr	err;
	
	Tclaealis2TEXTHandlerUPP = NewAECoercePtrUPP(Tclaealis2TEXTHandler);
	Tclaefss_2TEXTHandlerUPP = NewAECoercePtrUPP(Tclaefss_2TEXTHandler);
	TclaeTEXT2alisHandlerUPP = NewAECoercePtrUPP(TclaeTEXT2alisHandler);
	TclaeTEXT2fss_HandlerUPP = NewAECoercePtrUPP(TclaeTEXT2fss_Handler);
	TclaeWILD2TEXTHandlerUPP = NewAECoercePtrUPP(TclaeWILD2TEXTHandler);

	err = AEInstallCoercionHandler(typeAlias, typeChar, (AECoercionHandlerUPP)Tclaealis2TEXTHandlerUPP, 0L, false, false);	
	err = AEInstallCoercionHandler(typeFSS, typeChar, (AECoercionHandlerUPP)Tclaefss_2TEXTHandlerUPP, 0L, false, false);	
	err = AEInstallCoercionHandler(typeChar, typeAlias, (AECoercionHandlerUPP)TclaeTEXT2alisHandlerUPP, 0L, false, false);
	err = AEInstallCoercionHandler(typeChar, typeFSS, (AECoercionHandlerUPP)TclaeTEXT2fss_HandlerUPP, 0L, false, false);
//	err = AEInstallCoercionHandler(typeWildCard, typeChar, (AECoercionHandlerUPP)TclaeWILD2TEXTHandlerUPP, 0L, false, false);

	/* Initialize the AE Handler hash table */
	tclAECoercionHandlerHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	if (tclAECoercionHandlerHashTable) {
		Tcl_InitHashTable(tclAECoercionHandlerHashTable, TCL_ONE_WORD_KEYS);
	} else {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Couldn't initialize coercion handlers", 
						 (char *) NULL);
	}
}


/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeRegisterQueueHandler" --
 * 
 *  Tcl wrapper for ToolBox AEInstallEventHandler call. 
 *  This allows Tcl procs to act as event handlers.
 * 
 * Results:
 *  AEReturnID for this call, permitting reply to be assosciated with 
 *  original call
 * 
 * Side effects:
 *  <handlerProc> is registered and added to the event handler hash
 * 
 * --Version--Author------------------Changes-------------------------------
 *    1.0     jguyer@his.com original
 * -------------------------------------------------------------------------
 */
AEReturnID 
TclaeRegisterQueueHandler(
	Tcl_Interp *interp, 
	Tcl_Obj *replyHandlerProc)
{
	Tcl_HashEntry			*hashEntryPtr;
	tclAEEventHandler		*eventHandlerPtr = NULL;
	int						isNew = 0;
	
	
	/* 
	 * This event handler will _not_ be registered with the AEM.
	 * Rather, our generic aevt\ansr handler will look this up
	 * when it receives a reply event.
	 */
	eventHandlerPtr = (tclAEEventHandler *) ckalloc(sizeof(tclAEEventHandler));
	
	/* Would it be better to set these to the class and ID of 
	 * the sent event? 
	 */
	eventHandlerPtr->eventClass = kCoreEventClass;
	eventHandlerPtr->eventID = kAEAnswer;

    // Assign the Tcl proc which is to handle this event
	eventHandlerPtr->interp = interp;		 
	eventHandlerPtr->eventHandlerProc = replyHandlerProc;
    // Keep proc from being deleted by the interpreter
	Tcl_IncrRefCount(replyHandlerProc);	 
						
	do {				
		SInt32 hashKey = ++gReturnID;
		hashEntryPtr = Tcl_CreateHashEntry(tclAEEventHandlerHashTable, 
										   (char *) hashKey, &isNew);
	} while (!isNew);
	
	// Set hash entry to point at the event handler record
	Tcl_SetHashValue(hashEntryPtr, eventHandlerPtr);
	
	return gReturnID;
}


/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetCoercionHandler" --
 * 
 *  Find specified entry in hash table for coercion handlers
 *  If handlerProc is not NULL, it must match 
 * 
 * Results:
 *  Tcl_HashEntry pointer (or NULL) for desired handler 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static Tcl_HashEntry*
TclaeGetCoercionHandler(
	Tcl_Interp*	interp,
	OSType 		fromType, 
	OSType		toType, 
	char*		handlerProc)
{
	Tcl_HashEntry			*hashEntryPtr;		/* for search of AECoercionHandler */
    Tcl_HashSearch			search;				/*    hash list                    */
	tclAECoercionHandler	*coercionHandlerPtr;
	    
    // Search through coercion handler hash table for this type pair
    for (hashEntryPtr = Tcl_FirstHashEntry(tclAECoercionHandlerHashTable, &search);
		 hashEntryPtr != NULL;
		 hashEntryPtr = Tcl_NextHashEntry(&search)) {
		
		coercionHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
		if ((coercionHandlerPtr->fromType == fromType)
		&&  (coercionHandlerPtr->toType == toType)
		&&  (coercionHandlerPtr->interp == interp)) {
			if (handlerProc 
			&&	(strcmp(handlerProc, 
						Tcl_GetString(coercionHandlerPtr->coercionHandlerProc)) != 0)) {
				// handlerProc doesn't match
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
 * "TclaeGetEventHandler" --
 * 
 *  Find specified entry in hash table for event handlers
 *  If handlerProc is not NULL, it must match 
 * 
 * Results:
 *  Tcl_HashEntry pointer (or NULL) for desired handler 
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static Tcl_HashEntry*
TclaeGetEventHandler(
	Tcl_Interp*	interp,
	OSType		eventClass, 
	OSType		eventID,
	char*		handlerProc)
{
	Tcl_HashEntry			*hashEntryPtr;		/* for search of AEEventHandler */
    Tcl_HashSearch			search;				/*    hash list                    */
	tclAEEventHandler		*eventHandlerPtr;
	    
    // Search through event handler hash table for this class and ID
    for (hashEntryPtr = Tcl_FirstHashEntry(tclAEEventHandlerHashTable, &search);
		 hashEntryPtr != NULL;
		 hashEntryPtr = Tcl_NextHashEntry(&search)) {
		
		eventHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
		if ((eventHandlerPtr->eventClass == eventClass)
		&&  (eventHandlerPtr->eventID == eventID)
		&&  (eventHandlerPtr->interp == interp)) {
			if (handlerProc 
			&&	(strcmp(handlerProc, 
						Tcl_GetString(eventHandlerPtr->eventHandlerProc)) != 0)) {
				// handlerProc doesn't match
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
 * "TclaeRemoveCoercionHandler" --
 * 
 *  Remove entry from hash table for coercion handlers, and deregister
 *  coercion with the AEM
 * 
 * Results:
 *  OS Error
 * 
 * Side effects:
 *  Specified coercion is removed
 * -------------------------------------------------------------------------
 */
static OSErr
TclaeRemoveCoercionHandler(
	OSType			fromType, 
	OSType			toType,
	Tcl_HashEntry*	hashEntryPtr)
{
	tclAECoercionHandler*	coercionHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
	
    // Delete the object holding the handler proc
	Tcl_DecrRefCount(coercionHandlerPtr->coercionHandlerProc);	
	// Remove the coercion hash entry 
	Tcl_DeleteHashEntry(hashEntryPtr);
	// Delete the coercion handler structure
	ckfree((char*) coercionHandlerPtr);
	
    // Deregister any coercion for this type-pair with the AEM
	return AERemoveCoercionHandler(fromType, 
								   toType, 
								   (AECoercionHandlerUPP)TclaeCoercionHandlerUPP, 
                            	   false);
}


	
/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeRemoveEventHandler" --
 * 
 *  Remove entry from hash table for event handlers, and deregister
 *  handler with the AEM
 * 
 * Results:
 *  OS Error
 * 
 * Side effects:
 *  Specified handler is removed
 * -------------------------------------------------------------------------
 */
static OSErr
TclaeRemoveEventHandler(
	OSType			eventClass, 
	OSType			eventID,
	Tcl_HashEntry*	hashEntryPtr)
{
	tclAEEventHandler*	eventHandlerPtr = Tcl_GetHashValue(hashEntryPtr);
	
    // Delete the object holding the handler proc
	Tcl_DecrRefCount(eventHandlerPtr->eventHandlerProc);	
	// Remove the coercion hash entry 
	Tcl_DeleteHashEntry(hashEntryPtr);
	// Delete the coercion handler structure
	ckfree((char*) eventHandlerPtr);
	
    // Deregister this handler with the AEM
	return AERemoveEventHandler(eventClass, 
								eventID, 
								TclaeEventHandlerUPP, 
								false);
}


/*==================== POSIX to HFS path conversion =====================*/

#if TARGET_API_MAC_CARBON
/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetPathDString" --
 * 
 *  Places the (HFS or POSIX) path for the supplied (POSIX or HFS) path 
 *  in the supplied DString.
 * 
 * Results:
 *  Tcl status.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static int 
TclaeGetPathDString(Tcl_Interp *interp, Tcl_Obj *inPath, 
		    Boolean isDirectory, CFURLPathStyle fromPathStyle, 
		    CFURLPathStyle toPathStyle, Tcl_DString *outDS)
{
    CFStringRef		strRef;
    CFURLRef		urlRef;
    int			result;
    
    if (UtfToDUtfDString(interp, Tcl_GetString(inPath), -1, outDS) == TCL_ERROR)
	return TCL_ERROR;
    strRef = CFStringCreateWithCStringNoCopy(NULL, Tcl_DStringValue(outDS), 
					     kCFStringEncodingUTF8, kCFAllocatorNull);
    
    if (strRef == NULL) {
	Tcl_SetResult(interp, "Can't allocate CFString", TCL_STATIC);
	return TCL_ERROR;
    } 
    
    urlRef = CFURLCreateWithFileSystemPath(NULL, strRef,
					   fromPathStyle, isDirectory);
    CFRelease(strRef);
    Tcl_DStringFree(outDS);
    if (urlRef == NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Can't get CFURL from '", 
			 Tcl_GetString(inPath), "'",
			 (char *) NULL);
	return TCL_ERROR;
    } 
    
    strRef = CFURLCopyFileSystemPath(urlRef, toPathStyle);
    CFRelease(urlRef);
    if (strRef == NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Can't get path from '", 
			 Tcl_GetString(inPath), "'",
			 (char *) NULL);
	return TCL_ERROR;
    } 
    
    result = CFStringToUtfDString(interp, strRef, outDS);
    CFRelease(strRef);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeGetPath" --
 * 
 *  Return the (HFS or POSIX) path for the supplied (POSIX or HFS) path.
 * 
 * Results:
 *  Translated path.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static int 
TclaeGetPath(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[], 
	     CFURLPathStyle fromPathStyle, CFURLPathStyle toPathStyle)
{
    Boolean		isDirectory = TRUE;
    Tcl_DString		ds;
    int			result;
    
    if ((objc < 2) || (objc > 3)) {
	Tcl_WrongNumArgs(interp, 1, objv, "<path> ?isDirectory?");
	return TCL_ERROR;
    }
    if (objc == 3) {
	int		tmp;
	
	if (Tcl_GetBooleanFromObj(interp, objv[2], &tmp) != TCL_OK) {
	    return TCL_ERROR;
	}
	
	isDirectory = tmp;
    }
    
    result = TclaeGetPathDString(interp, objv[1], isDirectory, fromPathStyle, 
				 toPathStyle, &ds);
    
    if (result == TCL_OK) {
	Tcl_DStringResult(interp, &ds);
    }
    Tcl_DStringFree(&ds);
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetPOSIXPathCmd" --
 * 
 *  Return the POSIX path for the supplied HFS path.
 * 
 * Results:
 *  POSIX path.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetPOSIXPathCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
	return TclaeGetPath(interp, objc, objv, kCFURLHFSPathStyle, kCFURLPOSIXPathStyle);
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_GetHFSPathCmd" --
 * 
 *  Return the HFS path for the supplied POSIX path.
 * 
 * Results:
 *  HFS path.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_GetHFSPathCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
	return TclaeGetPath(interp, objc, objv, kCFURLPOSIXPathStyle, kCFURLHFSPathStyle);
}
#endif	// TARGET_API_MAC_CARBON
