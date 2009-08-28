/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEBuild.c"
 *                                    created: 8/16/1999 {1:05:41 AM} 
 *                                last update: 2/4/04 {10:48:01 AM} 
 *  Author: Pete Keleher
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *          POMODORO no seisan
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 1990-2004 Pete Keleher
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
 * ========================================================================
 *  See header file for further information
 * ###################################################################
 */

#ifndef _TCL
#include <tcl.h>
#endif

#include <string.h>

#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#include <AEDataModel.h>
#include <TextUtils.h>
#include <Script.h>
#include <Resources.h>

#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
#include <AEHelpers.h>
#else
#include <AEBuild.h>
#include "AEPrintCarbon.h"
#endif
#endif

#include "tclAEInt.h"
#include "tclMacOSError.h"

/* Definitions */

#ifdef _JEG_DELETED_
extern int outstandingMenuHilite;
#endif

AEReturnID gReturnID = 0;

#define	kModifiersMask		0xFF00 & ~cmdKey	/*	We need all modifiers
													except the command key
													for KeyTrans. */
#define	kOtherCharCodeMask	0x00FF0000			/*	Get the key out of the
													ASCII1 byte. */
static const char	kPeriod	= '.';

//  das 161099
//  CFM68k support for GetEvQHdr from 'tcl8.2.0/mac/tclMacNotify.c'

/* 
 * This is necessary to work around a bug in Apple's Universal header files
 * for the CFM68K libraries.
 */

#ifdef __CFM68K__
#undef GetEventQueue
extern pascal QHdrPtr GetEventQueue(void) THREEWORDINLINE(0x2EBC, 0x0000, 0x014A);
#pragma import list GetEventQueue
#define GetEvQHdr() GetEventQueue()
#endif

//  end CFM68k


/* Prototypes for internal routines */

static pascal Boolean appleEventIdleFunction(EventRecord *, long *, RgnHandle *);
static pascal Boolean appleEventReplyFilter(EventRecord *, long, long, const AEAddressDesc *);

static Boolean AbortInQueue(void);
#if !TARGET_API_MAC_CARBON
static Boolean CommandPeriod(EventRecord *theEvent);
#endif

static int  makeAppleEvent(Tcl_Interp *interp, Tcl_Obj *inAddressNameObjPtr, 
                           Tcl_Obj *inClassObjPtr, Tcl_Obj *inEventIDObjPtr, 
                           AEReturnID inReturnID, AETransactionID inTransactionID,
						   AppleEvent *outEventPtr);

/* ◊◊◊◊ Public package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_BuildCmd" --
 * 
 *  Build an AEDesc, based on Tcl arguments. 
 *  Arguments are in AEGizmos form:
 *  
 *  tclAE::build theAEGizmo
 *                            
 * Results:
 *  Tcl result code.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_BuildCmd(ClientData clientData,	/* (unused) */  
			   Tcl_Interp *interp,		/* for results */  
			   int objc,				/* number of arguments */   
			   Tcl_Obj *const objv[])	/* argument objects */
{
	int		result = TCL_OK;		/* result from Tcl calls */	
	Tcl_Obj *	aeDescObj;	
	
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "theAEGizmo");
		return TCL_ERROR;
	}
	
	/* objv[1] holds AEGizmo string */
	aeDescObj = Tcl_DuplicateObj(objv[1]);
	
	result = Tclae_ConvertToAEDesc(interp, aeDescObj);
	if (result == TCL_OK) {
	    Tcl_InvalidateStringRep(aeDescObj);
	    Tcl_SetObjResult(interp, aeDescObj);
	} 
		
	return result;		
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_SendCmd" --
 * 
 *  Build and send AppleEvent, based on Tcl arguments. 
 *  Arguments are in AEGizmos form:
 *  
 *  tclAE::send [<flags>] <app (address key|name|'creator')> \
 *                            <aeclass> <aeeventID> [<event parameters>]*
 *                            
 *  Flags are:
 *    -dr: don't record (when used with -s flag)
 *    -dx: don't execute (when used with -s flag)
 *    -p: print reply with AEPrint before returning it (if absent, return parsed AEDesc identifier).
 *    -q: queued reply requested (register handler with currentReplyHandler)
 *    -Q <proc>: queued reply requested (handler proc specified directly)
 *    -r: direct reply requested
 *    -s: send event to "self" (kCurrentProcess). Omit application address if '-s' is used
 *    -t <timeout>: specifies event timeout in ticks
 *    -z <transactionID>: perform event with given transaction ID (obtained from a misc/begi event).
 *    --: Don't process further flags
 * 
 * Results:
 *  Tcl result code.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_SendCmd(
    ClientData clientData, 
    Tcl_Interp *interp, 
    int objc, 
    Tcl_Obj *const objv[])
{
    OSErr           		err;
	int                     result = TCL_OK;
	
	AppleEvent				theEvent, *theReplyPtr;
	AEIdleUPP				appleEventIdleFunctionUPP = NULL;
	AEFilterUPP				appleEventReplyFilterUPP = NULL;
	AEReturnID				returnID = kAutoGenerateReturnID;
	AETransactionID			transactionID = kAnyTransactionID;
	Tcl_Obj					*callbackObj;
	Tcl_Obj					*addressNameObj;
	Boolean					isLocalIdleCallback;
	Boolean					isLocalReplyCallback;
	
	//int						isNew = 0;
	
	int						timeout = kAEDefaultTimeout;
	int						j;
	int						requiredArgs = 3;
	int						replyFlag = kAENoReply;
	Boolean				    printResult = false;
	Boolean					sendToSelf = false;
	
	char                    *arg;
		
	Tcl_Obj *				classObj;
	Tcl_Obj *				eventIDObj;
		
	
	/* What to do with this?!? */
#ifdef _JEG_DELETED_
    	if (outstandingMenuHilite) {
    		HiliteMenu(0);
    		outstandingMenuHilite = false;
    	}
#endif _JEG_DELETED_
	
	/* Scan optional flags */
	for (j = 1; (j < objc) && ((arg = Tcl_GetString(objv[j]))[0] == '-') && (arg[1] != '-'); j++) {
		switch (arg[1]) {
			case 'd':
				switch (arg[2]) {
					case 'r':
						replyFlag |= kAEDontRecord;
						break;
						
					case 'x':
						replyFlag |= kAEDontExecute;
						break;
				}
				break;
			case 'r':
				replyFlag |= kAEWaitReply | kAECanInteract;
				break;
				
			case 'Q':
				returnID = TclaeRegisterQueueHandler(interp, objv[++j]);
				replyFlag |= kAEQueueReply | kAECanInteract;
				replyFlag &= ~kAENoReply;
				break;  /* drop-through cases are the Devil's work */
				
			case 'q':
				returnID = TclaeRegisterQueueHandler(interp, Tcl_NewStringObj("::tclAE::legacyQueueHandler", -1));
				replyFlag |= kAEQueueReply | kAECanInteract;
				replyFlag &= ~kAENoReply;
				break;
				
			case 't':
			    result = Tcl_GetIntFromObj(interp, objv[++j], &timeout);
			    if (result != TCL_OK) {
			        return TCL_ERROR;
			    }
				break;
				
			case 'p':
				/* this is here for legacy */
				/* all Tcl_Obj * form of AEDescs are always printed */
				printResult = true;
				break;
				
			case 's':
				sendToSelf = true;
				requiredArgs--;
				break;
			
			case 'z':
				result = Tcl_GetLongFromObj(interp, objv[++j], &transactionID);
				if (result != TCL_OK) {
					return TCL_ERROR;
				}
				break;
		}
	}
	if (objc < (j + requiredArgs)) {
		Tcl_WrongNumArgs(interp, 1, objv, "?options? <application> <AEClass> <AEEventID> [<event parameters>]*");
		return TCL_ERROR;
	}
	
	if (sendToSelf) {
		addressNameObj = Tcl_NewObj();
	} else {
		addressNameObj = objv[j++];
	}
	
	classObj = objv[j++];
	eventIDObj = objv[j++];
	/* Create an empty AppleEvent of specified address, class, and eventID */
	result = makeAppleEvent(interp, addressNameObj, classObj, eventIDObj, returnID, transactionID, &theEvent);    
	if ( result != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* Read args in keyword-value pairs */
	while (j < (objc - 1)) {
	    AEKeyword       keyword;
	    Tcl_Obj *       theDescriptorObjPtr;
	    const AEDesc *	theAEDescPtr;
	    
		/* Read the keyword */
	    keyword = TclaeGetOSTypeFromObj(objv[j++]);		
		theDescriptorObjPtr = objv[j++];

    	result = Tclae_GetConstAEDescFromObj(interp, theDescriptorObjPtr, &theAEDescPtr, true);
		if (result != TCL_OK) {
			return TCL_ERROR;
		}
	
		err = AEPutParamDesc(&theEvent, keyword, theAEDescPtr);
		if (err != noErr) {
    		AEDisposeDesc(&theEvent);
    		
    		Tcl_ResetResult(interp);
    		Tcl_AppendResult(interp, "Couldn't put parameter: ",
    						 Tcl_MacOSError(interp, err), 
    						 (char *) NULL);
    		return TCL_ERROR;		
		}
	}
	// Flush any intermediate errors.
	Tcl_ResetResult(interp);
	
	callbackObj = Tcl_GetVar2Ex(interp, "tclAE::_callbacks", "idle", TCL_GLOBAL_ONLY);
	if (callbackObj) {
		memcpy(&appleEventIdleFunctionUPP, 
	           Tcl_GetByteArrayFromObj(callbackObj, NULL), 
	           sizeof(AEIdleUPP));
	    isLocalIdleCallback = false;
	} else {
		appleEventIdleFunctionUPP = NewAEIdleUPP(appleEventIdleFunction);
	    isLocalIdleCallback = true;
	}

	callbackObj = Tcl_GetVar2Ex(interp, "tclAE::_callbacks", "reply", TCL_GLOBAL_ONLY);
	if (callbackObj) {
		memcpy(&appleEventReplyFilterUPP, 
	           Tcl_GetByteArrayFromObj(callbackObj, NULL), 
	           sizeof(AEFilterUPP));
	    isLocalReplyCallback = false;
	} else {
		appleEventReplyFilterUPP = NewAEFilterUPP(appleEventReplyFilter);
	    isLocalReplyCallback = true;
	}
	
	theReplyPtr = (AppleEvent *) ckalloc(sizeof(AppleEvent));
	
	while (result != TCL_ERROR) {
		/* Send the newly created event */
		err = AESend(&theEvent, theReplyPtr, replyFlag, 
					 kAENormalPriority, timeout,
					 appleEventIdleFunctionUPP, appleEventReplyFilterUPP);

		switch (err) {
			case noErr:
				if ((replyFlag & kAEWaitReply) == kAEWaitReply) {
					/* Return a reference the reply descriptor */
					Tcl_SetObjResult(interp, Tclae_NewAEDescRefObj(theReplyPtr));
				}
				result = TCL_OK;
				break;
			case procNotFound:
				if (result == TCL_CONTINUE) {
					result = TCL_ERROR;
				} else {
					AEAddressDesc 			* theAddressPtr;
					
					/* force existence of string form (but don't overwrite any already there) */
					Tcl_GetString(addressNameObj);
					if ((addressNameObj->typePtr != NULL)
					&&  (addressNameObj->typePtr->freeIntRepProc != NULL)) {
						addressNameObj->typePtr->freeIntRepProc(addressNameObj);
					}
					addressNameObj->typePtr = NULL; /* &tclStringType; */
					result = Tclae_GetAEAddressDescFromObj(interp, addressNameObj,	&theAddressPtr);
					if (result != TCL_OK) {
						result = TCL_ERROR;
					} else {
						err = AEPutAttributeDesc(&theEvent, keyAddressAttr, theAddressPtr);
						if (err != noErr) {
							Tcl_ResetResult(interp);
							Tcl_AppendResult(interp, "Unable to set event address: ",
											 Tcl_MacOSError(interp, err), 
											 (char *) NULL);
							result = TCL_ERROR;				
						} else {
							result = TCL_CONTINUE;
						}
					}
				}
				break;
			default:
				ckfree((char *)theReplyPtr);
				Tcl_ResetResult(interp);
				Tcl_AppendResult(interp, "AESend failed: ",
								 Tcl_MacOSError(interp, err), 
								 (char *) NULL);
				result = TCL_ERROR;				
		}
		
		if (result == TCL_OK) {
			break;
		}
	}
	
	
	if (isLocalIdleCallback) {
		DisposeAEIdleUPP(appleEventIdleFunctionUPP);
	}
	if (isLocalReplyCallback) {
		DisposeAEFilterUPP(appleEventReplyFilterUPP);
	}

	AEDisposeDesc(&theEvent);
		
	return result;
}

/* ◊◊◊◊ Quasi-public callbacks ◊◊◊◊ */


static pascal Boolean 
appleEventIdleFunction(EventRecord *event, long *sleepTime, RgnHandle *mouseRegion)
{
	if (AbortInQueue()) {
		return true;
	} else {
	    Tcl_DoOneEvent(TCL_DONT_WAIT);
	    return false;
	}
}

static pascal Boolean 
appleEventReplyFilter (EventRecord *event, long returnID, long transID, const AEAddressDesc *sender)
{
	return true;
}

/* ◊◊◊◊ Private utilities ◊◊◊◊ */

static Boolean 
AbortInQueue(void)
{
	EvQElPtr	queueEntryPtr;
	Boolean		result;
	Boolean		foreground;
	OSErr		err;
	
	ProcessSerialNumber frontPSN;
	ProcessSerialNumber myPSN = {0, kCurrentProcess};
	result = false;
	
	err = GetFrontProcess(&frontPSN);
	err = SameProcess(&frontPSN, &myPSN, &foreground);

	if (foreground) {
#if TARGET_API_MAC_CARBON // das 25/10/00: Carbonization
		result=CheckEventQueueForUserCancel();
#else
		// Running under Mac OS, so walk the queue. The head is in
		// the low-memory location returned by GetEvQHdr, and
		// follows the normal OS queue conventions. For each entry
		// in the queue, call “CommandPeriod” to test it.

		result = false;
		queueEntryPtr = (EvQElPtr) GetEvQHdr()->qHead;
		while (!result && (queueEntryPtr != nil)) {
			result = CommandPeriod((EventRecord *) &queueEntryPtr->evtQWhat);
			if (!result) {
				queueEntryPtr = (EvQElPtr) queueEntryPtr->qLink;
			}
		}	/* Scanning queue */
#endif
	}	/* If front process */
	return result;
}

/*******************************************************************************

	CommandPeriod

	Radical cool way to see if the event record represents a Command-.
	keepers. Normally, you might wonder: “What’s the problem? All you have to
	do is check the modifiers field to see if the command-key is down, and
	check the message field to see what key was pressed.” Well, the problem is
	that under some systems, holding down the Command key negates any effect
	the Shift key has. This means that on systems where the period is a
	shifted character, when you hold down the Command key, you won’t be able
	to press period.

	The way to fix this is to rerun the sequence of events involved in mapping
	a key code into an ASCII value, except that this time we don’t factor the
	Command key into the equation.

	The event record has everything we need. It has the modifier keys that
	were pressed at the time of the event, and it has the key code. What we do
	is take the modifiers, clear the bit that says the Command key was
	pressed, and pass the modified modifiers and the key code to KeyTrans.
	After that, we’ll be able to examine the resulting ASCII value on its own
	merits.

	From Harvey’s Technote #263: International Canceling

*******************************************************************************/
#if !TARGET_API_MAC_CARBON
static Boolean
CommandPeriod(EventRecord *theEvent)
{
	Boolean  result;
	short    keyCode;
	long     virtualKey, keyInfo, lowChar, highChar, state, keyCID;
	Handle   hKCHR;

	result = false;

	if ((theEvent->what == keyDown) || (theEvent->what == autoKey)) {

		// See if the command key is down.  If it is, find out the ASCII
		// equivalent for the accompanying key.

		if ((theEvent->modifiers & cmdKey) != 0 ) {

			virtualKey = (theEvent->message & keyCodeMask) >> 8;

			// Mask out the command key and merge in the virtualKey
			keyCode	= (theEvent->modifiers & kModifiersMask) | virtualKey;
			state	= 0;

			keyCID	= GetScriptVariable(GetScriptManagerVariable(smKeyScript), smScriptKeys);
			hKCHR	= GetResource('KCHR', keyCID);

			if (hKCHR != nil) {
				keyInfo = KeyTranslate(*hKCHR, keyCode, (unsigned long*)&state);
				ReleaseResource( hKCHR );
			} else {
				keyInfo = theEvent->message;
			}

			lowChar =  keyInfo & charCodeMask;
			highChar = (keyInfo & kOtherCharCodeMask) >> 16;
			if ((lowChar == kPeriod) || (highChar == kPeriod))
				result = true;

		}  // end the command key is down
	}  // end key down event

	return result;
}
#endif

/* 
 * -------------------------------------------------------------------------
 * 
 * "makeAppleEvent" --
 * 
 *  Create an AppleEvent for the given event class and ID.
 * 
 * Results:
 *  AE address is returned in outEventPtr.
 *  Returns TCL_OK on success.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
static int
makeAppleEvent(Tcl_Interp   *interp, 
               Tcl_Obj      *inAddressNameObjPtr, 
               Tcl_Obj      *inClassObjPtr, 
               Tcl_Obj      *inEventIDObjPtr, 
               AEReturnID	inReturnID,
			   AETransactionID	inTransactionID,
               AppleEvent   *outEventPtr)
{
	AEAddressDesc *			theAddressPtr;
	OSErr					err;
	int						result;
	
	/* Decipher the address object */
	result = Tclae_GetAEAddressDescFromObj(interp, inAddressNameObjPtr,	&theAddressPtr);
	if (result != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* Make an empty AppleEvent of appropriate class and ID */
	err = AECreateAppleEvent(TclaeGetOSTypeFromObj(inClassObjPtr), 
	                         TclaeGetOSTypeFromObj(inEventIDObjPtr), 
	                         theAddressPtr, 
	                         inReturnID, 
							 inTransactionID, 
	                         outEventPtr);
	                         
	if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Make AppleEvent failed: ",
						 Tcl_MacOSError(interp, err), 
						 (char *) NULL);
		return TCL_ERROR;
	}
	
	return TCL_OK;
}

