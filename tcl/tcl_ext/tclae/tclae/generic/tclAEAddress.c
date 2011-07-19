/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEAddress.c"
 *                                    created: 8/29/99 {5:02:24 PM} 
 *                                last update: 7/25/10 {10:10:51 PM}
 *  Author: Pete Keleher
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *          POMODORO no seisan
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 1999-2009 Jonathan Guyer
 *               Copyright (c) 1990-1998 Pete Keleher
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
 * 
 * The command/subcommand implementation is from demoCmd.c in
 * _Tcl/Tk for real programmers_
 * Copyright (c) 1997  Clif Flynt. 
 * All rights reserved.
 * 
 * IN NO EVENT SHALL Clif Flynt BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Clif Flynt SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND Clif Flynt HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 * 
 * ========================================================================
 *  See header file for further information
 * ###################################################################
 */
 
#include <string.h>
#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#include <TextUtils.h>
#include <Script.h>
#include <NSLCore.h>
#include <OpenTransportProviders.h>
#include <Gestalt.h>
#include <LaunchServices.h>
#endif

#if TARGET_API_MAC_CARBON
/* Needed for building on Jaguar */
#ifndef typeApplicationBundleID
#define typeApplicationBundleID 'bund'
#endif

#endif

#include "tclAEInt.h"
#include "tclMacOSError.h"

#ifdef MAC_TCL
#include <tclMacInt.h>
#elif TARGET_RT_MAC_MACHO
#include "osxMacTcl.h"
#endif

#ifndef TCLAE_NO_EPPC
typedef struct nameFilter {
    Str32		portName;
    struct nameFilter *	next;
} nameFilter;

typedef struct typeCreatorFilter {
    OSType			portType;
    OSType			portCreator;
    struct typeCreatorFilter *	next;
} typeCreatorFilter;

static typeCreatorFilter *	tclAETypeCreatorFilters;
static nameFilter *		tclAENameFilters;
#endif


static void FreeAEAddressInternalRep(Tcl_Obj * objPtr);
static void DupAEAddressInternalRep(Tcl_Obj * srcPtr, Tcl_Obj * dupPtr);
static void UpdateStringOfAEAddress(Tcl_Obj * objPtr);
static int  SetAEAddressFromAny(Tcl_Interp * interp, Tcl_Obj * objPtr);


/*
 * The structure below defines the Tcl obj AEAddress type.
 */
Tcl_ObjType tclAEAddressType = {
    "AEAddress",		/* name */
    FreeAEAddressInternalRep,	/* freeIntRepProc */
    DupAEAddressInternalRep,	/* dupIntRepProc */
    UpdateStringOfAEAddress,	/* updateStringProc */
    SetAEAddressFromAny		/* setFromAnyProc */
};

/* Local application Regular Expression and indices */

static char * APPL_RE = "^('(....)'|.*)";

/* 
 * <application name>
 * '<4CHR>'
 *
 * 1: application
 * 2: creator code
 */
enum {
   APPL_GeneralRE = 0,
   APPL_ApplicationRE,
   APPL_CreatorRE
};

/* AppleTalk Regular Expression and indices */

static char * AT_RE = "^('(....)'|.*)( on ([^@:]+)(:([^@]+))?(@(.*))?)";

/* 
 * <application name> on <machine>[:type][@zone]
 * '<4CHR>' on <machine>[:type][@zone]
 *
 * 1: application
 * 2: creator code
 * 3: AppleTalk specifier
 * 4: machine name
 * 6: type
 * 8: zone
 */
enum {
   AT_GeneralRE = 0,
   AT_ApplicationRE,
   AT_CreatorRE,
   AT_AddressRE,
   AT_MachineRE,
   AT_TypeDummyRE,
   AT_TypeRE,
   AT_ZoneDummyRE,
   AT_ZoneRE
};
  
/*
* The cmdDefinition structure describes the minimum and maximum number
*  of expected arguments for the subcommand (including cmd and subcommand
*  names), and a usage message to return if the argument
*  count is outside the expected range.
*/

typedef struct cmd_Def {
    char *	usage;
    int		minArgCnt;
    int		maxArgCnt;
} cmdDefinition;

#ifndef TCLAE_NO_EPPC
/* Prototypes for internal routines */
pascal Boolean	Tclae_PortFilter(LocationNameRec *locationName, PortInfoRec *thePortInfo);

static void     deleteFilters();
static int      parseNameFilters(Tcl_Interp *interp, Tcl_Obj *listPtr);
static int	parseTypeCreatorFilters(Tcl_Interp *interp, Tcl_Obj *listPtr);
static int	setTargetLocation(Tcl_Interp *interp, Tcl_Obj *addressObj, LocationNameRec *locationPtr);

Tcl_Obj *	TclaeNewAEAddressObjFromTarget(Tcl_Interp * interp, TargetID * targetPtr);
#endif

Tcl_Obj *	TclaeNewAEAddressObjFromPSN(Tcl_Interp * interp, ProcessSerialNumber thePSN);
#if TARGET_API_MAC_CARBON	    
Tcl_Obj *	TclaeNewAEAddressObjFromCFURL(Tcl_Interp * interp, CFURLRef theURL);
#endif

static int	pStrcmp(ConstStringPtr s1, ConstStringPtr s2);
void 	PStringToUtfAndAppendToObj(Tcl_Obj *objPtr, ConstStringPtr pString);
static Tcl_Obj * PStringToUtfObj(ConstStringPtr pString);
static void	UtfObjToPString(Tcl_Obj *objPtr, StringPtr pString, int len);
static Tcl_Obj * UnsignedLongToTclObj(unsigned int inLong);

static Tcl_Obj *	UtfPathObjFromRef(Tcl_Interp * interp, FSRef *fsrefPtr);
#if !__LP64__
static Tcl_Obj *	UtfPathObjFromSpec(Tcl_Interp * interp, FSSpec *spec);
#endif // !__LP64__
#if !TARGET_API_MAC_CARBON
static int		SpecFromUtfPathObj(Tcl_Interp * interp, Tcl_Obj * pathObj, FSSpec* spec);
#endif 


/* ◊◊◊◊ Public package routines ◊◊◊◊ */

#if TARGET_API_MAC_CARBON

static OSStatus 
AppLaunchNotificationHandler(EventHandlerCallRef	inHandlerCallRef,
			     EventRef			inEvent,
			     void*			inUserData) 
{
    GetEventParameter(inEvent, kEventParamProcessID, 
			 typeProcessSerialNumber, NULL, 
			 sizeof(ProcessSerialNumber), NULL, 
			 inUserData);
			     
     return CallNextEventHandler(inHandlerCallRef, inEvent);
}
 
DEFINE_ONE_SHOT_HANDLER_GETTER( AppLaunchNotificationHandler );

static CFURLRef
TclaeCopyAppURL(Tcl_Interp * interp, Tcl_Obj * appObj)
{
    OSStatus    	err;
    CFURLRef		appURL = NULL;
    OSType      	creator = kLSUnknownCreator;
    CFStringRef		bundleID = NULL;
    CFStringRef		name = NULL;
    AEAddressDesc *	addressDesc;
    
    if (Tclae_GetConstAEDescFromObj(interp, appObj, (const AEDesc **) &addressDesc, true) == TCL_OK) {
	switch (addressDesc->descriptorType) {
	    case typeApplicationURL:
		break;
	    case typeApplicationBundleID: {
		Size	    numChars = AEGetDescDataSize((AEDesc *) addressDesc);
		OSStatus    err;
		Tcl_DString ds;

		Tcl_DStringInit(&ds);
		Tcl_DStringSetLength(&ds, numChars);
		err = AEGetDescData((AEDesc *) addressDesc, Tcl_DStringValue(&ds), numChars);
		bundleID = CFStringCreateWithCString(NULL, Tcl_DStringValue(&ds), kCFStringEncodingUTF8);
		Tcl_DStringFree(&ds);
		}
		break;
	}
    }
    
    if (bundleID == NULL) {
	creator = TclaeGetOSTypeFromObj(appObj);
	if (creator == kLSUnknownCreator) {
	    name = CFStringCreateWithCharacters(NULL, Tcl_GetUnicode(appObj), Tcl_GetCharLength(appObj));		
	}
    }
    
    err = LSFindApplicationForInfo(creator, bundleID, name,
				   NULL, &appURL);
    
    switch (err) {
	case noErr:
	    break;
	case kLSApplicationNotFoundErr:
	    appURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8 *) Tcl_GetString(appObj), Tcl_GetCharLength(appObj), false);
	default:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Unable to launch ",
			     Tcl_GetString(appObj), ": ",
			     Tcl_MacOSError(interp, err),
			     (char *) NULL);
    }
    
    return appURL;
}

static int
TclaeLaunch(Tcl_Interp * interp, Tcl_Obj * appObj, Boolean foreGround, Boolean newInstance, ProcessSerialNumber * thePSNp)
{
    LSLaunchURLSpec	lsSpec = {NULL, NULL, NULL, kLSLaunchDefaults, thePSNp};        
    OSStatus		err;
    int			result = TCL_OK;
    const EventTypeSpec eventList[] = {{kEventClassApplication, kEventAppLaunchNotification}};
    EventHandlerRef	handlerRef;
    EventRef		outEvent;
    FSRef		launchLocation;
    CFURLRef		outURL;

    lsSpec.appURL = TclaeCopyAppURL(interp, appObj);
    if (lsSpec.appURL == NULL) {
	// error message already in interpreter
	return TCL_ERROR;
    }
    
    if (!foreGround) {
	lsSpec.launchFlags |= kLSLaunchDontSwitch;
    }
    
    if (newInstance) {
	lsSpec.launchFlags |= kLSLaunchNewInstance;
    } else {
	thePSNp->highLongOfPSN = kNoProcess;
	thePSNp->lowLongOfPSN = kNoProcess;
	
	if (!CFURLGetFSRef(lsSpec.appURL, &launchLocation)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Unable to launch ",
			     Tcl_GetString(appObj), ": ",
			     Tcl_MacOSError(interp, fnfErr),
			     (char *) NULL);
	    return TCL_ERROR;
	}
	
	// We need to be ABSOLUTELY CERTAIN that we don't relaunch the same app, but instead
	// just return its PSN.
	// Relaunching is not itself a problem, but a relaunch will not send kEventAppLaunchNotification
	// so ReceiveNextEvent() below will wait forever 
	while (GetNextProcess(thePSNp) != procNotFound)  {
	    FSRef		processLocation;
	    
	    err = GetProcessBundleLocation(thePSNp, &processLocation);
	    if (err == noErr) {
		// See if the PSNs of this process and the launch request match
		Boolean     running = (FSCompareFSRefs(&processLocation, &launchLocation) == noErr);
		if (!running) {
		    // If they don't match, it's possible that the launch request is for the bundle
		    // executable and not just the bundle 
		    // (/blah/blah/myapp.app/Contents/MacOS/myapp vs. /blah/blah/myapp.app/)
		    //
		    // This seems like an absurd amount of work for this, but nobody on CarbonDev
		    // could offer anything better.
		    CFURLRef    processURL = CFURLCreateFromFSRef(kCFAllocatorDefault, &processLocation);
		    if (processURL) {
			CFBundleRef processBundle = CFBundleCreate(kCFAllocatorDefault, processURL);
			if (processBundle) {
			    CFURLRef    executableURL = CFBundleCopyExecutableURL(processBundle);
			    if (executableURL) {
				FSRef   executableLocation;
				if (CFURLGetFSRef(executableURL, &executableLocation)) {
				    running = (FSCompareFSRefs(&executableLocation, &launchLocation) == noErr);
				}
				CFRelease(executableURL);
			    }
			    CFRelease(processBundle);
			}
		    }
		    CFRelease(processURL);
		}
		if (running) {
		    // Launched app is already running, so return its PSN. If the
		    // -foreground option is specified, bring the process to front
		    // (see Bug 2372 in Alpha-Bugzilla).
			OSErr	theErr = noErr;
			if (foreGround) {
				theErr = SetFrontProcess(thePSNp);
			} 
			if (theErr == noErr) {
				return TCL_OK;
			} else {
				Tcl_ResetResult(interp);
				Tcl_AppendResult(interp, "Unable to foreground ",
                                     Tcl_GetString(appObj), ": ",
                                     Tcl_MacOSError(interp, theErr),
                                     (char *) NULL);
				return TCL_ERROR;
			} 
		}
	    }
	}
    }
    
    err = InstallApplicationEventHandler(GetAppLaunchNotificationHandlerUPP(), 
    					 GetEventTypeCount(eventList), eventList, 
					 thePSNp, &handlerRef); 
    if (err == noErr) {
	err = LSOpenFromURLSpec(&lsSpec, &outURL);
    }
    
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Unable to launch ",
			 Tcl_GetString(appObj), ": ",
			 Tcl_MacOSError(interp, err),
			 (char *) NULL);
	result = TCL_ERROR;
    }
    
    err = ReceiveNextEvent(GetEventTypeCount(eventList), eventList, kEventDurationForever, true, &outEvent);
    err = SendEventToEventTarget(outEvent, GetEventDispatcherTarget());

    RemoveEventHandler(handlerRef);
    CFRelease(lsSpec.appURL);
    
    return result;
}

#else // !TARGET_API_MAC_CARBON

static int
TclaeLaunch(Tcl_Interp * interp, Tcl_Obj * appObj, Boolean foreGround, Boolean newInstance, ProcessSerialNumber * thePSNp)
{
    LaunchParamBlockRec	lRec;
    FSSpec		spec;
    OSStatus		err;

    if (SpecFromUtfPathObj(interp, appObj, &spec) == TCL_ERROR) {
	return TCL_ERROR;
    }

    lRec.launchAppSpec = &spec;
    lRec.launchBlockID = extendedBlock;
    lRec.launchEPBLength = extendedBlockLen;
    lRec.launchControlFlags = launchNoFileFlags | launchContinue;
    if (!foreGround) {
	lRec.launchControlFlags |= launchDontSwitch;
    }
    lRec.launchAppParameters = NULL;
    
    err = LaunchApplication(&lRec);
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Unable to launch ",
			 Tcl_GetString(appObj), ": ",
			 Tcl_MacOSError(interp, err),
			 (char *) NULL);
	return TCL_ERROR;
    }
    
    *thePSNp = lRec.launchProcessSN;
    
    return TCL_OK;
}
#endif // TARGET_API_MAC_CARBON

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_LaunchCmd" --
 * 
 *  Launch the named app into the background.
 *
 *  tclAE::launch [-f] <name>
 * 
 * Results:
 *  ???
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int
Tclae_LaunchCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Boolean			foreGround = false;
    Boolean			newInstance = false;
    ProcessSerialNumber		thePSN;
    int				j;

    /* Definitions for command options */    
    CONST84 char       *options[] = {
	"-foreground", "-newInstance", NULL
    };
    
    enum {
	M_foregroundOption = 0,
	M_newInstanceOption
    };
    
    cmdDefinition optionDefinitions[] = {
	{"-foreground", 2 , 2},
	{"-newInstance", 2 , 2},
    };
    
    for (j = 1; 
	 (j < objc - 1) 
	 && (Tcl_GetString(objv[j])[0] == '-');
	 j++) {
	
	int	cmdnum;
	int	result = Tcl_GetIndexFromObj(interp, objv[j], options, 
					     "option", 0 /* TCL_EXACT */, &cmdnum);
	
	/* 
	 * If the result is not TCL_OK, then the error message is already
	 *    in the Tcl Interpreter, this code can immediately return.
	 */
	
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	
	/*
	 * Check that the argument count matches what's expected for this
	 * Option.
	 */
	
	if (((objc - j + 1) < optionDefinitions[cmdnum].minArgCnt)) {
	    Tcl_WrongNumArgs(interp, 1, objv, optionDefinitions[cmdnum].usage);
	    return TCL_ERROR;
	}
	
	switch (cmdnum) {
	  case M_foregroundOption:
	    foreGround = true;
	    break;
	  case M_newInstanceOption:
	      newInstance = true;
	      break;
	  default:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Bad option: ", Tcl_GetString(objv[j]), 
			     ".  Has no entry in switch.",
			     (char *) NULL);
	    return TCL_ERROR;	    
	}
    }
    
    if (j >= objc) {
	Tcl_WrongNumArgs(interp, 1, objv, "?options? name");
	return TCL_ERROR;
    }

    if (TclaeLaunch(interp, objv[j], foreGround, newInstance, &thePSN) != TCL_OK) {
	return TCL_ERROR;
    } else {
	Tcl_Obj * psnObj = TclaeNewAEAddressObjFromPSN(interp, thePSN);
	if (psnObj != NULL) {
	    Tcl_SetObjResult(interp, psnObj);
	    return TCL_OK;
	} else {
	    return TCL_ERROR;
	}
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_ProcessesCmd" --
 * 
 *  Obtains info on active processes
 *
 *  tclAE::processes
 * 
 * Results:
 *  ???
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int
Tclae_ProcessesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *		processListObj = Tcl_NewObj();
    Tcl_Obj *		processInfoObj = NULL;
    Tcl_Obj *		elementObj = NULL;
    ProcessSerialNumber PSN;
    int			result = TCL_OK;
    
    PSN.highLongOfPSN = 0;
    PSN.lowLongOfPSN = kNoProcess;
    
    while (GetNextProcess(&PSN) != procNotFound)  {
	ProcessInfoRec 	procInfoRec;
	Str255		str;
#if __LP64__
        FSRef		theAppRef;
#else
	FSSpec		theAppSpec;
#endif // __LP64__
	
	procInfoRec.processName = str;
#if __LP64__
        procInfoRec.processAppRef = &theAppRef;
#else
	procInfoRec.processAppSpec = &theAppSpec;
#endif // __LP64__
	procInfoRec.processInfoLength = sizeof(procInfoRec);
	
	if (GetProcessInformation(&PSN, &procInfoRec) == noErr) {
	    processInfoObj = Tcl_NewObj();
	    
	    // Name
	    elementObj = PStringToUtfObj(procInfoRec.processName);
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break;
	    }
	    
	    // Signature
	    elementObj = TclaeNewOSTypeObj(procInfoRec.processSignature);
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break;
	    }
	    
	    // Type
	    elementObj = TclaeNewOSTypeObj(procInfoRec.processType);
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break;
	    }
	    
	    // Launch date
// 	    elementObj = Tcl_NewLongObj(procInfoRec.processLaunchDate);
	    elementObj = UnsignedLongToTclObj(procInfoRec.processLaunchDate);
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break; 
	    }
	    
	    // PSN
	    elementObj = TclaeNewAEAddressObjFromPSN(interp, procInfoRec.processNumber);
	    if (elementObj == NULL) {
		result = TCL_ERROR;
		break;
	    }
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break; 
	    }

	    // Path
#if __LP64__
            elementObj = UtfPathObjFromRef(interp, procInfoRec.processAppRef);
#else
	    elementObj = UtfPathObjFromSpec(interp, procInfoRec.processAppSpec);
#endif // __LP64__
            if (elementObj == NULL) {
                result = TCL_ERROR;
                break;
            }
	    result = Tcl_ListObjAppendElement(interp, processInfoObj, elementObj);
	    if (result != TCL_OK) {
		break; 
	    }
	}
	
	result = Tcl_ListObjAppendElement(interp, processListObj, processInfoObj);
	if (result != TCL_OK) {
	    break; 
	}
    }
    if (result != TCL_OK) {
	Tcl_DecrRefCount(processListObj);
	if (processInfoObj != NULL) {
	    Tcl_DecrRefCount(processInfoObj);
	}
	if (elementObj != NULL) {
	    Tcl_DecrRefCount(elementObj);
	}
	result = TCL_ERROR;
    } else {
	Tcl_SetObjResult(interp, processListObj);
    }
    
    return result;
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_RemoteProcessResolverGetProcessesCmd" --
 * 
 *  Obtains info on active processes
 *
 *  tclAE::remoteProcessResolverGetProcesses
 * 
 * Results:
 *  ???
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int
Tclae_RemoteProcessResolverGetProcessesCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *			processListObj = Tcl_NewObj();
    Tcl_Obj *			processInfoObj = NULL;
    
    CFURLRef 			urlRef;
    AERemoteProcessResolverRef	resolverRef;
    CFStreamError		streamError;
    CFArrayRef			remoteProcessArray;
    CFIndex 			idx, count; 
    int				result = TCL_OK;

    
    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "url");
        return TCL_ERROR;
    }

    urlRef = CFURLCreateWithBytes(kCFAllocatorDefault, 
                                  (UInt8 *) Tcl_GetString(objv[1]), Tcl_GetCharLength(objv[1]), 
                                  kCFStringEncodingUTF8, NULL);
    if (urlRef == NULL) {
        return TCL_ERROR;
    }
    
    resolverRef = AECreateRemoteProcessResolver(kCFAllocatorDefault, urlRef);
    remoteProcessArray = AERemoteProcessResolverGetProcesses(resolverRef, &streamError);
    if (remoteProcessArray == NULL) {
        switch (streamError.domain) {
            case kCFStreamErrorDomainCustom: {
                
            }
            case kCFStreamErrorDomainPOSIX: {
            }
            case kCFStreamErrorDomainMacOSStatus: {
                
            }
/* 
 *             kCFStreamErrorDomainNetDB
 * 
 *             kCFStreamErrorDomainNetServices
 * 
 *             kCFStreamErrorDomainMach
 * 
 *             kCFStreamErrorDomainFTP
 * 
 *             kCFStreamErrorDomainHTTP
 * 
 *             kCFStreamErrorDomainSOCKS
 * 
 *             kCFStreamErrorDomainSystemConfiguration
 * 
 *             kCFStreamErrorDomainSSL
 */
        }
        return TCL_ERROR;
    }
    
    CFRetain(remoteProcessArray);
    AEDisposeRemoteProcessResolver(resolverRef);
    
    count = CFArrayGetCount(remoteProcessArray);
    
    for (idx = 0; idx < count; idx++) {
	CFDictionaryRef theDict = CFArrayGetValueAtIndex(remoteProcessArray, idx);
	CFURLRef	processURL;
	CFURLRef	absoluteProcessURL;
	CFStringRef	name;
	CFNumberRef	number;
	long		value;
	
	
	processInfoObj = Tcl_NewObj();
	
	if (!CFDictionaryGetValueIfPresent(theDict, kAERemoteProcessURLKey, (const void **) &processURL)) {
	    result = TCL_ERROR;
	    break;
	}
	
	absoluteProcessURL = CFURLCopyAbsoluteURL(processURL);
	result = Tcl_ListObjAppendElement(interp, processInfoObj, TclaeNewAEAddressObjFromCFURL(interp, absoluteProcessURL));
	CFRelease(absoluteProcessURL);
	if (result != TCL_OK) {
	    break;
	}
	
	if (!CFDictionaryGetValueIfPresent(theDict, kAERemoteProcessNameKey, (const void **) &name)) {
	    result = TCL_ERROR;
	    break;
	}
	
	result = Tcl_ListObjAppendElement(interp, processInfoObj, CFStringToTclObj(name));
	if (result != TCL_OK) {
	    break;
	}
	
	if (!CFDictionaryGetValueIfPresent(theDict, kAERemoteProcessUserIDKey, (const void **) &number)) {
	    result = TCL_ERROR;
	    break;
	}
	
	CFNumberGetValue(number, kCFNumberLongType, &value);
	
	result = Tcl_ListObjAppendElement(interp, processInfoObj, Tcl_NewLongObj(value));
	if (result != TCL_OK) {
	    break;
	}

	if (!CFDictionaryGetValueIfPresent(theDict, kAERemoteProcessProcessIDKey, (const void **) &number)) {
	    result = TCL_ERROR;
	    break;
	}
	
	CFNumberGetValue(number, kCFNumberLongType, &value);
	
	result = Tcl_ListObjAppendElement(interp, processInfoObj, Tcl_NewLongObj(value));
	if (result != TCL_OK) {
	    break;
	}
	
	result = Tcl_ListObjAppendElement(interp, processListObj, processInfoObj);
	if (result != TCL_OK) {
	    break; 
	}	
    }
    
    CFRelease(remoteProcessArray);
    
    if (result != TCL_OK) {
        Tcl_DecrRefCount(processListObj);
        if (processInfoObj != NULL) {
            Tcl_DecrRefCount(processInfoObj);
        }
        result = TCL_ERROR;
    } else {
        Tcl_SetObjResult(interp, processListObj);
    }
    
    return result;
}

#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC) // das 25/10/00: Carbonization
/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_IPCListPortsCmd" --
 * 
 *  Tcl wrapper for ToolBox IPCListPorts call. 
 *
 *  tclAE::IPCListPorts ppcNoLocation
 *  tclAE::IPCListPorts ppcNBPLocation <objStr> <typeStr> <zoneStr>
 *  tclAE::IPCListPorts ppcXTIAddrLocation <url>
 * 
 * Results:
 *  ???
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_IPCListPortsCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    IPCListPortsPBRec   theIPCListPortsPBRec;
    /* By default, look for any application and any port */
    PPCPortRec          thePPCPortRec = {smRoman, "\p=", ppcByString, "\p="};
    LocationNameRec     theLocationNameRec;
    PortInfoRec	        buffer[256];
    int                 cmdnum;
    int			j;
    
    /* Definitions for primary command variants */
    
    CONST84 char       *keywords[] = {
	"ppcNoLocation", "ppcNBPLocation", "ppcXTIAddrLocation", 
	NULL
    };
    
    enum {
	M_ppcNoLocation = 0,
	M_ppcNBPLocation,
	M_ppcXTIAddrLocation
    };
    
    cmdDefinition definitions[] = {
	{"ppcNoLocation", 2 , 2},
	{"ppcNBPLocation <objStr> <typeStr> <zoneStr>", 5, 5},
	{"ppcXTIAddrLocation <url>", 3, 3},
    };
    
    /* Definitions for command options */
    
    CONST84 char       *options[] = {
	"-n", "-pn", "-pc", NULL
    };
    
    enum {
	M_nameOption = 0,
	M_portNameOption,
	M_portCreatorTypeOption
    };
    
    cmdDefinition optionDefinitions[] = {
	{"-n <name>", 3 , 3},
	{"-pn <portTypeStr>", 3, 3},
	{"-pc <portCreator> <portType>", 4, 4},
    };
    
    for (j = 1; 
	 (j < objc) 
	 && (Tcl_GetString(objv[j])[0] == '-');
	 j++) {
	
	int result = Tcl_GetIndexFromObj(interp, objv[j], options, 
					 "option", TCL_EXACT, &cmdnum);
	
	/* 
	 * If the result is not TCL_OK, then the error message is already
	 *    in the Tcl Interpreter, this code can immediately return.
	 */
	
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	
	/*
	 * Check that the argument count matches what's expected for this
	 * Option.
	 */
	
	if (((objc - j + 1) < optionDefinitions[cmdnum].minArgCnt)) {
	    Tcl_WrongNumArgs(interp, 1, objv, optionDefinitions[cmdnum].usage);
	    return TCL_ERROR;
	}
	
	switch (cmdnum) {
	  case M_nameOption:
	    UtfObjToPString(objv[++j], thePPCPortRec.name, 32);
	    break;
	  case M_portNameOption:
	    thePPCPortRec.portKindSelector = ppcByString;
	    UtfObjToPString(objv[++j], thePPCPortRec.u.portTypeStr, 31);
	    break;
	  case M_portCreatorTypeOption: 
	    thePPCPortRec.portKindSelector = ppcByCreatorAndType;
	    thePPCPortRec.u.port.portCreator = TclaeGetOSTypeFromObj(objv[++j]);
	    thePPCPortRec.u.port.portType = TclaeGetOSTypeFromObj(objv[++j]);
	    break;
	  default:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Bad option: ", Tcl_GetString(objv[j]), 
			     ".  Has no entry in switch.",
			     (char *) NULL);
	    return TCL_ERROR;	    
	}
    }
    
    /*
     * Find this location subcommand in the list of subcommands.  
     * Tcl_GetIndexFromObj returns the offset of the recognized string,
     * which is used to index into the command definitions table.
     */
    
    if (j == objc) {
	/* No location is OK */
	cmdnum = M_ppcNoLocation;
    } else {
	int result = Tcl_GetIndexFromObj(interp, objv[j], keywords, 
					 "location", TCL_EXACT, &cmdnum);
	
	/* 
	 * If the result is not TCL_OK, then the error message is already
	 *    in the Tcl Interpreter, this code can immediately return.
	 */
	
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	
	/*
	 * Check that the argument count matches what's expected for this
	 * Subcommand.
	 */
	
	if (((objc - j + 1) < definitions[cmdnum].minArgCnt) 
	    ||  ((objc - j + 1) > definitions[cmdnum].maxArgCnt) ) {
	    Tcl_WrongNumArgs(interp, 1, objv, definitions[cmdnum].usage);
	    return TCL_ERROR;
	}
    }
    
    /* 
     * The subcommand is recognized, and has a valid number of arguments
     * Process the command.
     */
    
    switch (cmdnum) {
      case M_ppcNoLocation:
	theLocationNameRec.locationKindSelector = ppcNoLocation;
	break;
      case M_ppcNBPLocation:
	theLocationNameRec.locationKindSelector = ppcNBPLocation;
	UtfObjToPString(objv[++j], theLocationNameRec.u.nbpEntity.objStr, 32);
	if (Tcl_GetCharLength(objv[++j]) > 0) {
	    UtfObjToPString(objv[j], theLocationNameRec.u.nbpEntity.typeStr, 32);
	} else {
	    c2pstrcpy(theLocationNameRec.u.nbpEntity.typeStr, "PPCToolBox");
	}
	UtfObjToPString(objv[++j], theLocationNameRec.u.nbpEntity.zoneStr, 32);
	break;
      case M_ppcXTIAddrLocation:
	setTargetLocation(interp, objv[++j], &theLocationNameRec);
	break;
      default:	
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Bad sub-command: ", Tcl_GetString(objv[j]), 
			 ".  Has no entry in switch.",
			 (char *) NULL);
	return TCL_ERROR;	    
    }
    
    theIPCListPortsPBRec.startIndex   = 0;
    theIPCListPortsPBRec.requestCount = 256;
    theIPCListPortsPBRec.portName     = &thePPCPortRec;
    theIPCListPortsPBRec.locationName = &theLocationNameRec;
    theIPCListPortsPBRec.bufferPtr    = buffer;
    
    if (IPCListPorts(&theIPCListPortsPBRec, false) != noErr) {
	Tcl_SetResult(interp, "Error listing ports", TCL_STATIC);
	return TCL_ERROR;
    } else {
	TargetID	target;
	Tcl_Obj *	portList = Tcl_NewObj();
	
	target.location = theLocationNameRec;
	
	for (j = 0; 
	     j < theIPCListPortsPBRec.actualCount 
	     && j <= theIPCListPortsPBRec.requestCount; 
	     j++) {
	    
	    /* What should this be, if anything? */
	    target.sessionID = 0;
	    memcpy(&target.name, 
		   &buffer[j].name, 
		   sizeof (PPCPortRec));
	    /* what about recvrName? */
	    
	    Tcl_ListObjAppendElement(interp, portList,
				     TclaeNewAEAddressObjFromTarget(interp, &target));
	}
	
	Tcl_SetObjResult(interp, portList);
	
	return TCL_OK;
    }
}

/* 
 * -------------------------------------------------------------------------
 * 
 * "Tclae_PPCBrowserCmd" --
 * 
 *  Tcl wrapper for ToolBox PPCBrowser call. Produces a TargetID and returns
 *  a hash key for later access.
 * 
 * Results:
 *  Hash key for the TargetID.
 * 
 * Side effects:
 *  None.
 * -------------------------------------------------------------------------
 */
int 
Tclae_PPCBrowserCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    TargetID		target;
    PortInfoRec		thePortInfo;
    Str255		prompt = "\pChoose a program to link to";
    Str255		label = "\pPrograms";
    char		*arg;
    int			j;
    PPCFilterUPP	theFilterProc = NULL;
    OSStatus		err;
    
    for (j = 1; (j < objc) && ((arg = Tcl_GetString(objv[j]))[0] == '-'); j++) {
	switch (arg[1]) {
	  case 'p':	
	    /* prompt */
	    UtfObjToPString(objv[++j], prompt, 255);
	    break;
	  case 'l':	
	    /* application label */
	    UtfObjToPString(objv[++j], label, 255);
	    break;
	  case 'f':	
	    /* filter */
	    switch (arg[2]) {
	      case 'n':	
		/* names */
		if (parseNameFilters(interp, objv[++j]) != TCL_OK) {
		    return TCL_ERROR;
		}
		theFilterProc = NewPPCFilterUPP(Tclae_PortFilter);
		break;
	      case 'c':	
		/* creator-types */
		if (parseTypeCreatorFilters(interp, objv[++j]) != TCL_OK) {
		    return TCL_ERROR;
		}
		theFilterProc = NewPPCFilterUPP(Tclae_PortFilter);
		break;
	    }
	    break;
	  default:
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "Bad option: ", arg, 
			     ".  Has no entry in switch.",
			     (char *) NULL);
	    return TCL_ERROR;
	}
    }
    
    // !!! Application MUST be in the foreground before this call !!!
    // (although OS 8.6, at least, doesn't seem to mind)
    
    err = PPCBrowser(prompt, label, false, &target.location, &thePortInfo, 
		     theFilterProc, (ConstStr32Param) "");
    if (err != noErr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "PPCBrowser failure: ",
			 Tcl_MacOSError(interp, err),
			 (char *) NULL);
	return TCL_ERROR;
    }	
    
    deleteFilters();
    
    target.name = thePortInfo.name;
    
    Tcl_SetObjResult(interp, TclaeNewAEAddressObjFromTarget(interp, &target));
    
    return TCL_OK;	
}
#endif //!TARGET_API_MAC_CARBON // das 25/10/00: Carbonization

#ifndef TCLAE_NO_EPPC
/* ◊◊◊◊ Quasi-public utilities ◊◊◊◊ */

pascal Boolean Tclae_PortFilter(LocationNameRec *locationName, PortInfoRec *thePortInfo)
{
    int			j, count;
    Boolean		result = false;
    nameFilter *	nextNameFilter;
    typeCreatorFilter *	nextTCFilter;
    
    switch (thePortInfo->name.portKindSelector) {
      case ppcByString: 
	for (nextNameFilter = tclAENameFilters; 
	     nextNameFilter != NULL; 
	     nextNameFilter = nextNameFilter->next) {
	    
	    if (pStrcmp(thePortInfo->name.u.portTypeStr, 
			nextNameFilter->portName) == 0) {
		result = true;
		break;
	    }
	}
	break;
	
      case ppcByCreatorAndType: 
	for (nextTCFilter = tclAETypeCreatorFilters; 
	     nextTCFilter != NULL; 
	     nextTCFilter = nextTCFilter->next) {
	    
	    if ((thePortInfo->name.u.port.portCreator == nextTCFilter->portCreator) 
	    &&	(thePortInfo->name.u.port.portType == nextTCFilter->portType)) {
		result = true;
		break;
	    }
	}
	break;
    }
    
    return result;
}
#endif

/* ◊◊◊◊ Internal package routines ◊◊◊◊ */

/* 
 * -------------------------------------------------------------------------
 * 
 * "TclaeInitAEAddresses" --
 * 
 *  Initialize the AEAddress Tcl object type, allowing Tcl to easily 
 *  reestablish contact with the same process.
 * 
 * Results:
 *  None.
 * 
 * Side effects:
 *  tclAEAddressType is registered.
 * -------------------------------------------------------------------------
 */
void
TclaeInitAEAddresses()
{
    Tcl_RegisterObjType(&tclAEAddressType);
}

/* ◊◊◊◊ Private utilities ◊◊◊◊ */
#ifndef TCLAE_NO_EPPC
static void
deleteFilters()
{
	nameFilter			*nameFilterPtr;
	typeCreatorFilter	*typeCreatorFilterPtr;
    
    while ((nameFilterPtr = tclAENameFilters) != NULL) {
        tclAENameFilters = nameFilterPtr->next;
        ckfree((char *) nameFilterPtr);
	}
	
    while ((typeCreatorFilterPtr = tclAETypeCreatorFilters) != NULL) {
        tclAETypeCreatorFilters = typeCreatorFilterPtr->next;
        ckfree((char *) typeCreatorFilterPtr);
	}
}	

static int   
parseNameFilters(Tcl_Interp *interp, Tcl_Obj *listPtr)
{
    int		res = TCL_OK, count;
    
    if (((res = Tcl_ListObjLength( interp, listPtr, &count )) == TCL_OK)
    &&  (count > 0)) {
	
	int j;
	
	for (j = 0; j < count; j++) {
	    Tcl_Obj *		filterPtr;	
	    nameFilter *	nameFilterPtr = (nameFilter *) ckalloc(sizeof(nameFilter));
	    
	    nameFilterPtr->next = tclAENameFilters;
	    tclAENameFilters = nameFilterPtr;
	    
	    Tcl_ListObjIndex( interp, listPtr, j, &filterPtr );
	    UtfObjToPString(filterPtr, nameFilterPtr->portName, 32);
	}
    }
    
    return res;
}

static int   
parseTypeCreatorFilters(Tcl_Interp *interp, Tcl_Obj *listPtr)
{
    int		result = TCL_OK;
    int		count;
    
    if ((result = Tcl_ListObjLength( interp, listPtr, &count )) == TCL_OK
    &&  count > 0) {
	
	int j;
	
	for (j = 0; j < count; j++) {
	    Tcl_Obj *		filterObj;
	    Tcl_Obj *		codeObj;
	    int			numElements;
	    typeCreatorFilter *	typeCreatorFilterPtr
	    = (typeCreatorFilter *) ckalloc(sizeof(typeCreatorFilter));
	    
	    typeCreatorFilterPtr->next = tclAETypeCreatorFilters;
	    tclAETypeCreatorFilters = typeCreatorFilterPtr;
	    
	    Tcl_ListObjIndex(interp, listPtr, j, &filterObj);
	    if ((result = Tcl_ListObjLength( interp, filterObj, &numElements )) != TCL_OK
	    ||  numElements != 2) {
		result = TCL_ERROR;
		break;
	    }
	    Tcl_ListObjIndex( interp, filterObj, 0, &codeObj );
	    typeCreatorFilterPtr->portType = TclaeGetOSTypeFromObj(codeObj);
	    Tcl_ListObjIndex( interp, filterObj, 1, &codeObj );
	    typeCreatorFilterPtr->portCreator = TclaeGetOSTypeFromObj(codeObj);
	}
    }
    
    return result;
}
#endif

// lifted from oldEndre.c
static Tcl_Obj *
UtfPathObjFromRef(Tcl_Interp * interp, FSRef *fsrefPtr)
{
    Tcl_Obj *	pathObj = NULL;
    OSErr	err;
    Handle	pathString = NULL;
    int		size;
    
    err = FSpPathFromLocation(fsrefPtr, &size, &pathString);
    if (err == noErr) {
	Tcl_DString	ds;
	
	Tcl_DStringInit(&ds);
	HLock(pathString);
	Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, *pathString, size, &ds);
	DisposeHandle(pathString);
	
	pathObj = Tcl_NewStringObj(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
	Tcl_DStringFree(&ds);
    }
    
    return pathObj;
}

#if !__LP64__
static Tcl_Obj *
UtfPathObjFromSpec(Tcl_Interp * interp, FSSpec *spec)
{
    Tcl_Obj *	pathObj = NULL;
    FSRef	fsref;
    OSErr	err;

    err = FSpMakeFSRef(spec, &fsref);
    if (err == noErr) {
        pathObj = UtfPathObjFromRef(interp, &fsref);
    }
    
    return pathObj;
}
#endif // !__LP64__

#if !TARGET_API_MAC_CARBON
// lifted from io.c
static int 
SpecFromUtfPathObj(Tcl_Interp * interp, Tcl_Obj * pathObj, FSSpec* spec) {
    Tcl_DString	ds;
    OSErr	err;
    
    Tcl_UtfToExternalDString(tclAE_macRoman_encoding, Tcl_GetString(pathObj), -1, &ds);
    err = FSpLocationFromPath(Tcl_DStringLength(&ds),Tcl_DStringValue(&ds), spec);
    if (err == noErr) {
	Boolean folder;
	Boolean aliased;
	
	err = ResolveAliasFile(spec, TRUE, &folder, &aliased);
    }
    
    Tcl_DStringFree(&ds);
    
    if (err != noErr) {
	Tcl_AppendResult(interp, "Can't locate '", Tcl_GetString(pathObj), "'", (char *) NULL);
	return TCL_ERROR;
    } else {
	return TCL_OK;
    }
}
#endif // #if !TARGET_API_MAC_CARBON


/*=========================== Pascal Strings ============================*/

static int pStrcmp(ConstStringPtr s1, ConstStringPtr s2)
{
    size_t		len = s1[0];
    size_t		res;
    
    if (s2[0] < len) {
	len = s2[0];
    }
    res = strncmp((const char *) s1+1, (const char *) s2+1, len);
    if (res) {
	return(res);
    }
    return((int)(s1[0] - s2[0]));
}

void 
PStringToUtfAndAppendToObj(Tcl_Obj *objPtr, ConstStringPtr pString)
{
    Tcl_DString		tempDS;
    
    Tcl_DStringInit(&tempDS);
    Tcl_AppendToObj(objPtr, 
		    Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, 
					     (char *) &pString[1], 
					     pString[0], 
					     &tempDS), 
		    Tcl_DStringLength(&tempDS));
    
    Tcl_DStringFree(&tempDS);
}

static Tcl_Obj * 
PStringToUtfObj(ConstStringPtr pString)
{
    Tcl_Obj *		obj = Tcl_NewObj();
	char *			utfStr;
    Tcl_DString		tempDS;
    
    Tcl_DStringInit(&tempDS);
	
	utfStr = Tcl_ExternalToUtfDString(tclAE_macRoman_encoding, 
					     (char *) &pString[1], 
					     pString[0], 
					     &tempDS);
	
    Tcl_AppendToObj(obj, utfStr, Tcl_DStringLength(&tempDS));
    
    Tcl_DStringFree(&tempDS);
    
    return obj;
}

static void UtfObjToPString(Tcl_Obj *objPtr, StringPtr pString, int len)
{
    CFStringRef		theString;
    
    theString = TclObjToCFString(objPtr);
    CFStringGetPascalString(theString, pString, len+1, kCFStringEncodingMacRoman);
}

static Tcl_Obj * 
UnsignedLongToTclObj(unsigned int inLong)
{
    Tcl_Obj *		obj = Tcl_NewObj();
	char			str[64];
	
	sprintf(str, "%u%c", inLong, 0);
	Tcl_AppendToObj(obj, str, strlen(str));

    return obj;
}

/*======================== Tcl AEAddress Object =========================*/

/*
 *----------------------------------------------------------------------
 *
 * FreeAEAddressInternalRep --
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
FreeAEAddressInternalRep(Tcl_Obj *objPtr) /* AEAddress object with internal
					   * representation to free. */
{
    AEAddressDesc *	descPtr = (AEAddressDesc *) objPtr->internalRep.otherValuePtr;
    
    if (descPtr != NULL) {
	AEDisposeDesc(descPtr);
	ckfree((char *) descPtr);
	objPtr->internalRep.otherValuePtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupAEAddressInternalRep --
 *
 *  Initialize the internal representation of an AEAddress Tcl_Obj to a
 *  copy of the internal representation of an existing AEAddress object. 
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Internal rep AEAddressDesc of "srcPtr" is duplicated and stored in
 *  "dupPtr".
 *
 *----------------------------------------------------------------------
 */

static void
DupAEAddressInternalRep(Tcl_Obj *srcPtr, /* Object with internal rep to copy. */
			Tcl_Obj *dupPtr) /* Object with internal rep to set. */
{
    dupPtr->internalRep.otherValuePtr = ckalloc(sizeof(AEAddressDesc));
    
    /* no point in checking the result because we have no way to report it */
    AEDuplicateDesc((AEAddressDesc *) srcPtr->internalRep.otherValuePtr, 
		    (AEAddressDesc *) dupPtr->internalRep.otherValuePtr);
    
    dupPtr->typePtr = &tclAEAddressType;
}

/*------------------- update internal representation --------------------*/

#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
static int
setTargetLocation(Tcl_Interp *interp, Tcl_Obj *addressObj, LocationNameRec *locationPtr)
{
    Tcl_DString		ds;
    
    locationPtr->locationKindSelector = ppcXTIAddrLocation;
    locationPtr->u.xtiType.Reserved[0] = 0;
    locationPtr->u.xtiType.Reserved[1] = 0;
    locationPtr->u.xtiType.Reserved[2] = 0;
    
    Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
			     Tcl_GetString(addressObj), -1, &ds);
    
    /* address is potentially too long (max 96 bytes, see TN1176) */
    /* Apple's solution involves making ugly OpenTransport calls  */
    if (Tcl_DStringLength(&ds) > kMaxPPCXTIAddress) {
	Tcl_DStringFree(&ds);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "TCP/IP address '", 
			 Tcl_GetString(addressObj), "' is too long.",
			 (char *) NULL);
	return TCL_ERROR;
    }					
    
    locationPtr->u.xtiType.xtiAddr.fAddressType = kDNSAddrType;
    BlockMoveData(Tcl_DStringValue(&ds), 
		  locationPtr->u.xtiType.xtiAddr.fAddress, Tcl_DStringLength(&ds));
    
    locationPtr->u.xtiType.xtiAddrLen = Tcl_DStringLength(&ds) + sizeof(UInt16);
    
    
    Tcl_DStringFree(&ds);
    
    return TCL_OK;
}

static int
setTargetApplicationURL(Tcl_Interp * interp, Tcl_Obj *nameObj, TargetID *targetPtr)
{
    Tcl_DString		ds;
    OSErr			err;
    Boolean			textChanged;
    UInt16			len = sizeof(targetPtr->name.name) - 1;
    
#if TARGET_CPU_68K
    UtfObjToPString(nameObj, targetPtr->name.name, len);
#else
    if (NSLLibraryPresent()) {
	Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
				 Tcl_GetString(nameObj), -1, &ds);
	
	err = NSLHexDecodeText(Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), 
			       (char *) targetPtr->name.name, &len, &textChanged);
	
	Tcl_DStringFree(&ds);
	
	if (err != noErr) {
	    return TCL_ERROR;
	}
	
	c2pstr((char *) targetPtr->name.name);
    } else {
	UtfObjToPString(nameObj, targetPtr->name.name, len);
    }    
#endif
    targetPtr->name.nameScript = smRoman;
    
    return TCL_OK;
}

static Tcl_Obj *
decodeApplicationName(Tcl_Obj *nameObj)
{
#if TARGET_CPU_68K
    return nameObj;
#else
    if (NSLLibraryPresent()) {
    Tcl_DString		ds1;
    Tcl_DString		ds2;
    Tcl_Obj *		decodedObj;
    OSErr		err;
    Boolean		textChanged;
    UInt16		len = 255;
    
    Tcl_UtfToExternalDString(tclAE_macRoman_encoding, 
			     Tcl_GetString(nameObj), -1, &ds1);
    
    Tcl_DStringInit(&ds2);
    Tcl_DStringSetLength(&ds2, Tcl_DStringLength(&ds1));
    err = NSLHexDecodeText(Tcl_DStringValue(&ds1), Tcl_DStringLength(&ds1), 
			   Tcl_DStringValue(&ds2), &len, &textChanged);
    
    if (err != noErr) {
	decodedObj = nameObj;
    } else {
	decodedObj = Tcl_NewStringObj(Tcl_DStringValue(&ds2), -1);
    }
    
    Tcl_DStringFree(&ds1);
    Tcl_DStringFree(&ds2);
    
    return decodedObj;
} else {
    return nameObj;
}
#endif
}

static void
setTargetApplicationName(Tcl_Interp * interp, Tcl_Obj *nameObj, TargetID *targetPtr)
{
    targetPtr->location.locationKindSelector = ppcNoLocation;
    
    targetPtr->name.portKindSelector = ppcByString;
    UtfObjToPString(nameObj, targetPtr->name.name, -1);
}

static void
setTargetApplicationCreator(Tcl_Interp * interp, Tcl_Obj *creatorObj, TargetID *targetPtr)
{
    targetPtr->location.locationKindSelector = ppcNoLocation;
    
    targetPtr->name.portKindSelector = ppcByCreatorAndType;
    targetPtr->name.u.port.portCreator = TclaeGetOSTypeFromObj(creatorObj);
    targetPtr->name.u.port.portType = 'ep01';	
}
#endif // TCLAE_NO_EPPC

#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
static void
getApplicationTarget(Tcl_Interp *interp, Tcl_RegExpInfo *reInfo, Tcl_Obj *addressObj, TargetID *targetPtr)
{
    Tcl_Obj *	rangeObj;
    
    targetPtr->location.locationKindSelector = ppcNoLocation;
    
    if (reInfo->matches[AT_CreatorRE].end > reInfo->matches[AT_CreatorRE].start) {
	/* application specified by 'CREA' format */
	rangeObj = Tcl_GetRange(addressObj, reInfo->matches[AT_CreatorRE].start, 
				reInfo->matches[AT_CreatorRE].end-1);
	setTargetApplicationCreator(interp, rangeObj, targetPtr);
    } else {
	/* application specified by name */
	rangeObj = Tcl_GetRange(addressObj, reInfo->matches[AT_ApplicationRE].start, 
				reInfo->matches[AT_ApplicationRE].end-1);
	setTargetApplicationName(interp, rangeObj, targetPtr);
    }
}
#endif //TCLAE_NO_EPPC

/* <application name> on <machine>[:type][@zone] */
/* '4CHR' on <machine>[:type][@zone] */
static int
getAppleTalkAddress(Tcl_Interp *interp, 
		    Tcl_RegExpInfo *reInfo, 
		    Tcl_Obj *addressObj, 
		    AEAddressDesc *addressDesc)
{
    OSStatus	err;
    int			result = TCL_OK;
    SInt32		gestalt;
    
    err = Gestalt(gestaltPPCToolboxAttr, &gestalt);
    if (err == noErr 
    &&  (gestalt & gestaltPPCSupportsOutgoingAppleTalk)) {
#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
	TargetID	target;
	Tcl_Obj *	rangeObj;
	
	getApplicationTarget(interp, reInfo, addressObj, &target);
	
	target.location.locationKindSelector = ppcNBPLocation;
	
	/* machine */
	rangeObj = Tcl_GetRange(addressObj, reInfo->matches[AT_MachineRE].start, 
				reInfo->matches[AT_MachineRE].end-1);
	UtfObjToPString(rangeObj, target.location.u.nbpEntity.objStr, -1);
	
	/* type */
	rangeObj = Tcl_GetRange(addressObj, reInfo->matches[AT_TypeRE].start, 
				reInfo->matches[AT_TypeRE].end-1);
	if (Tcl_GetCharLength(rangeObj) > 0) {
	    UtfObjToPString(rangeObj, target.location.u.nbpEntity.typeStr, -1);
	} else {
	    c2pstrcpy(target.location.u.nbpEntity.typeStr, "PPCToolBox");
	}
	
	/* zone */
	rangeObj = Tcl_GetRange(addressObj, reInfo->matches[AT_ZoneRE].start, 
				reInfo->matches[AT_ZoneRE].end-1);
	if (Tcl_GetCharLength(rangeObj) > 0) {
	    UtfObjToPString(rangeObj, target.location.u.nbpEntity.zoneStr, -1);
	} else {
	    c2pstrcpy(target.location.u.nbpEntity.zoneStr, "*");
	}
	
	// ??? Should we verify the address in any way? What if user wanted 
	// the application with that name, not the (possibly broken) AEAddress?
	err = AECreateDesc(typeTargetID, &target, sizeof(target), addressDesc);
#else
	Tcl_SetResult(interp, 
		      "It is illegal, immoral, and unsanitary to create TargetIDs on this system",
		      TCL_STATIC);
	result = TCL_ERROR;
#endif //TCLAE_NO_EPPC
    } else {
	Tcl_SetResult(interp, 
		      "AppleEvents over AppleTalk are not available", 
		      TCL_STATIC);
	result = TCL_ERROR;
    }
    
    return result;
}

/* look for a local process with this name or creator */
static int
getPSNAddress(Tcl_Interp *interp, Tcl_Obj *addressObj, AEAddressDesc *addressDesc)
{
    ProcessInfoRec	procInfoRec;
    ProcessSerialNumber thePSN;
    Str255		processNameStorage;
    OSStatus		err;
    int			result = TCL_OK;
    static Tcl_Obj *	applStrObj = NULL;
    static Tcl_RegExp 	applRE = NULL;
    
    if (applStrObj == NULL) {
	applStrObj = Tcl_NewStringObj(APPL_RE, -1);
	applRE = Tcl_GetRegExpFromObj(interp, applStrObj, TCL_REG_ADVANCED);
	if (applRE == NULL) {
	    return TCL_ERROR;
	}
    }
    
    thePSN.highLongOfPSN = 0;
    thePSN.lowLongOfPSN = kNoProcess;
    
    procInfoRec.processName = processNameStorage;
#if __LP64__
    procInfoRec.processAppRef = 0L;
#else
    procInfoRec.processAppSpec = 0L;
#endif // __LP64__
    procInfoRec.processInfoLength = sizeof(procInfoRec);
    
    if (Tcl_RegExpExecObj(interp, applRE, addressObj, 0, -1, 0) == 1) {
	Tcl_RegExpInfo		reInfo;
	
	Tcl_RegExpGetInfo(applRE, &reInfo);
	
	if (reInfo.matches[APPL_CreatorRE].end > reInfo.matches[APPL_CreatorRE].start) {
	    OSType	sig = TclaeGetOSTypeFromObj(addressObj);
	    
	    while ((err = GetNextProcess(&thePSN)) != procNotFound) {
		if (GetProcessInformation(&thePSN, &procInfoRec) == noErr) {
		    if (procInfoRec.processSignature == sig) {
			break;
		    }
		}
	    }
	} else {
	    Str255	processName;
	    
	    UtfObjToPString(addressObj, processName, sizeof(processName)-1);
	    
	    while ((err = GetNextProcess(&thePSN)) != procNotFound) {
		if (GetProcessInformation(&thePSN, &procInfoRec) == noErr) {
		    if (pStrcmp((ConstStringPtr) procInfoRec.processName, processName) == 0) {
			break;
		    }
		}
	    }
	}
	
	if (err == noErr) {
	    err = AECreateDesc(typeProcessSerialNumber, &thePSN, sizeof(thePSN), addressDesc);		
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, 
				 "Can't create PSN address from '", 
				 Tcl_GetString(addressObj), "': ",
				 Tcl_MacOSError(interp, err),
				 (char *) NULL);
		result = TCL_ERROR;
	    }
	} else {
#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
	    TargetID	target;
	    
	    getApplicationTarget(interp, &reInfo, addressObj, &target);
	    err = AECreateDesc(typeTargetID, &target, sizeof(target), addressDesc);
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, 
				 "Can't create TargetID address from '", 
				 Tcl_GetString(addressObj), "': ",
				 Tcl_MacOSError(interp, err),
				 (char *) NULL);
		result = TCL_ERROR;
	    }
#else
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, 
			     "Process \"", Tcl_GetString(addressObj), "\" not found",
			     (char *) NULL);
	    result = TCL_CONTINUE;
#endif
	}
    } else {
	result = TCL_ERROR;
    }
    
    return result;
}

static int
getOtherAddress(Tcl_Interp *interp, Tcl_Obj *addressObj, AEAddressDesc *addressDesc)
{	
    static Tcl_Obj *	appleTalkStrObj = NULL;
    static Tcl_RegExp 	appleTalkRE = NULL;
    int			result = TCL_OK;
    
    if (appleTalkStrObj == NULL) {
	appleTalkStrObj = Tcl_NewStringObj(AT_RE, -1);
	appleTalkRE = Tcl_GetRegExpFromObj(interp, appleTalkStrObj, TCL_REG_ADVANCED);
	if (appleTalkRE == NULL) {
	    return TCL_ERROR;
	}
    }
    
    if (Tcl_RegExpExecObj(interp, appleTalkRE, addressObj, 0, -1, 0) == 1) {
	Tcl_RegExpInfo		reInfo;
	
	Tcl_RegExpGetInfo(appleTalkRE, &reInfo);
	
	result = getAppleTalkAddress(interp, &reInfo, addressObj, addressDesc);
    } else {
	result = getPSNAddress(interp, addressObj, addressDesc);
    }
    
    return result;
}

static int
getAEDescAddress(Tcl_Interp *interp, Tcl_Obj *addressObj, AEAddressDesc *addressDesc, int parseGizmo)
{
    int	result = TCL_CONTINUE;
    
    /* if objPtr is already an AEDesc, then see if it's a legitimate
     * AEAddress. If it's not an AEAddress, that's an error.
     * If it's not an AEDesc, continue with other parsers.
     */
    if (Tclae_GetConstAEDescFromObj(interp, addressObj, (const AEDesc **) &addressDesc, parseGizmo) == TCL_OK) {
	switch (addressDesc->descriptorType) {
	    case typeProcessSerialNumber:
	    case typeApplicationURL:
#if TARGET_API_MAC_CARBON	    
	    case typeKernelProcessID:
	    case typeMachPort:
	    case typeApplicationBundleID:
#endif
#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
	    case typeTargetID:
#endif
		result = TCL_OK;
		break;
	    default:
		result = TCL_ERROR;
	}	
    }
    
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SetAEAddressFromAny --
 *
 *  Generate an AEAddress internal form for the Tcl object "objPtr".
 *
 * Results:
 *  The return value is a standard Tcl result. The conversion always
 *  succeeds and TCL_OK is returned.
 *
 * Side effects:
 *  A pointer to an AEAddressDesc built from objPtr's string rep
 *  is stored as objPtr's internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetAEAddressFromAny(Tcl_Interp * interp, /* Used for error reporting if not NULL. */
		    Tcl_Obj *    objPtr) /* The object to convert. */
{
    AEAddressDesc *	addressDesc = NULL;
    char *		name;
    int			result = TCL_OK;
    
    if (getAEDescAddress(interp, objPtr, addressDesc, false) == TCL_CONTINUE) {
	/*
	 * Get "objPtr"s string representation. Make it up-to-date if necessary.
	 */
	
	addressDesc = (AEAddressDesc *) ckalloc(sizeof (AEAddressDesc));
	
	name = objPtr->bytes;
	if (name == NULL) {
	    name = Tcl_GetString(objPtr);
	}
	
	if (Tcl_GetCharLength(objPtr) > 0) {
	    result = getOtherAddress(interp, objPtr, addressDesc);
	} else {
	    /* empty address get's assigned to self */
	    ProcessSerialNumber 	thePSN;
	    OSStatus		err;
	    
	    thePSN.highLongOfPSN = 0L;
	    thePSN.lowLongOfPSN = kCurrentProcess;
	    
	    err = AECreateDesc(typeProcessSerialNumber, &thePSN, sizeof(thePSN), addressDesc);	
	    if (err != noErr) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "Can't create address of self: ", 
				 Tcl_MacOSError(interp, err),
				 (char *) NULL);
		result = TCL_ERROR;
	    }
	}
	
	if (result == TCL_OK) {
	    /*
	     * Free the old internalRep before setting the new one. We do this as
	     * late as possible to allow the conversion code, in particular
	     * GetStringFromObj, to use that old internalRep.
	     */
	    
	    if ((objPtr->typePtr != NULL)
	    &&  (objPtr->typePtr->freeIntRepProc != NULL)) {
		objPtr->typePtr->freeIntRepProc(objPtr);
	    }
		
	    objPtr->internalRep.otherValuePtr = addressDesc;
	} else {
	    ckfree((char *) addressDesc);
	    if (result == TCL_CONTINUE) {
		/* check if it's an AEGizmo */
		result = getAEDescAddress(interp, objPtr, addressDesc, true);
	    }
	}
    }
    
    if (result == TCL_OK) {
	objPtr->typePtr = &tclAEAddressType;
	/* debugging */
/* 	Tcl_InvalidateStringRep(objPtr); */
    } else {
	result = TCL_ERROR;
    }
    
    return result;
}

/*-------------------- update string representation ---------------------*/

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfAEAddress --
 *
 *  Update the string representation for an AEAddressDesc
 *  object.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  The object's string is set to a valid string that results from
 *  the  conversion.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfAEAddress(Tcl_Obj *objPtr) /* AEAddress obj with string rep to update. */
{
    TclaeUpdateStringOfAEDesc(objPtr);
}
static Tcl_Obj *
TclaeNewAEAddressObjFromAEAddressDesc(Tcl_Interp * interp, OSStatus err, AEAddressDesc * addressDesc)
{
    if (err == noErr) {
	Tcl_Obj *	objPtr = Tcl_NewObj();
	
	Tcl_InvalidateStringRep(objPtr);
	objPtr->internalRep.otherValuePtr = addressDesc;
	objPtr->typePtr = &tclAEAddressType;
	
	return objPtr;
    } else {
	ckfree((char *) addressDesc);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Unable to make AEAddress: ",
			 Tcl_MacOSError(interp, err),
			 (char *) NULL);
	return NULL;
    }
}

Tcl_Obj *
TclaeNewAEAddressObjFromPSN(Tcl_Interp * interp, ProcessSerialNumber thePSN)
{
    AEAddressDesc *	addressDesc = (AEAddressDesc *) ckalloc(sizeof(AEAddressDesc));
    OSStatus		err;
    
    err = AECreateDesc(typeProcessSerialNumber, &thePSN, sizeof(thePSN), addressDesc);		
    return TclaeNewAEAddressObjFromAEAddressDesc(interp, err, addressDesc);
}

#if TARGET_API_MAC_CARBON	    
Tcl_Obj *
TclaeNewAEAddressObjFromCFURL(Tcl_Interp * interp, CFURLRef theURL)
{
    AEAddressDesc *	addressDesc = (AEAddressDesc *) ckalloc(sizeof(AEAddressDesc));
    OSStatus		err;
    CFDataRef		dataRef = NULL;

    dataRef = CFURLCreateData(kCFAllocatorDefault, theURL, kCFStringEncodingUTF8, true);
    if (dataRef) {
        CFIndex dataSize = CFDataGetLength(dataRef);
        err = AECreateDesc(typeApplicationURL, (Ptr)CFDataGetBytePtr(dataRef), dataSize, addressDesc);
        CFRelease(dataRef);
    } else {
	err = coreFoundationUnknownErr;
    }

    return TclaeNewAEAddressObjFromAEAddressDesc(interp, err, addressDesc);
}
#endif // TARGET_API_MAC_CARBON	    

#if !TARGET_API_MAC_CARBON && !defined(TCLAE_NO_EPPC)
Tcl_Obj *
TclaeNewAEAddressObjFromTarget(Tcl_Interp * interp, TargetID * targetPtr)
{
    AEAddressDesc *	addressDesc = (AEAddressDesc *) ckalloc(sizeof(AEAddressDesc));
    OSStatus		err;
    
    err = AECreateDesc(typeTargetID, targetPtr, sizeof(TargetID), addressDesc);
    return TclaeNewAEAddressObjFromAEAddressDesc(interp, err, addressDesc);
}
#endif // TCLAE_NO_EPPC

int
Tclae_GetAEAddressDescFromObj(Tcl_Interp *interp, /* Used for error reporting if not NULL. */
			      Tcl_Obj *objPtr,	  /* The object from which to get a int. */
			      AEAddressDesc **addressDescPtr)	/* Place to store resulting AEAddressDesc. */
{
    int	result = TCL_OK;
    
    if (objPtr->typePtr != &tclAEAddressType) {
	result = SetAEAddressFromAny(interp, objPtr);
    }
    
    if (result == TCL_OK) {
	*addressDescPtr = ((AEAddressDesc *) objPtr->internalRep.otherValuePtr);
    }
    
    return result;
    
}
