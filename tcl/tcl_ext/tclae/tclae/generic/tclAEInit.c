/* -*- mode: C; coding: macintosh; -*-
 * ###################################################################
 *  TclAE - AppleEvent extension for Tcl
 * 
 *  FILE: "tclAEInit.c"
 *                                    created: 8/20/99 {9:31:59 AM} 
 *                                last update: 2/6/04 {11:54:18 PM} 
 *  Author: Jonathan Guyer
 *  E-mail: jguyer@his.com
 *    mail: Alpha Cabal
 *          POMODORO no seisan
 *     www: http://www.his.com/jguyer/
 *  
 * ========================================================================
 *               Copyright (c) 1999-2004 Jonathan Guyer
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
#include <Gestalt.h>
#include <AEObjects.h>
#endif

#include "tclAE.h"
#include "tclAEInt.h"

/* global storing the macRoman Tcl_Encoding */
Tcl_Encoding tclAE_macRoman_encoding;

int
Tclae_Init(Tcl_Interp *interp)
{
    OSErr		err;
    long		attr;
    
    //Check for AppleEvents
    err = Gestalt(gestaltAppleEventsAttr, &attr);
    if ((err != noErr)
    ||  !(attr & (1 << gestaltAppleEventsPresent))) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "The AppleEvent Manager is either missing or misbehaving",
			 (char *) NULL);
    }
    err = AEObjectInit();
    
    
    if (Tcl_InitStubs(interp, "8.0", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
	if (TCL_VERSION[0] == '7') {
	    if (Tcl_PkgRequire(interp, "Tcl", "8.0", 0) == NULL) {
		return TCL_ERROR;
	    }
	}
    }
    
    if (Tcl_PkgProvide(interp, TCLAE_NAME, TCLAE_BASIC_VERSION) != TCL_OK) {
		return TCL_ERROR;
    }

    /* Why?!? */
    Tcl_SetVar(interp, "tclAE_version", TCLAE_VERSION, TCL_GLOBAL_ONLY);
    
    tclAE_macRoman_encoding = Tcl_GetEncoding(interp,"macRoman");
    
    TclaeInitAEAddresses();
    TclaeInitAEDescs();
    TclaeInitEventHandlers(interp);
    TclaeInitCoercionHandlers(interp);
    TclaeInitObjectAccessors(interp);
    
    /* Define Tcl commands */
    
    Tcl_CreateObjCommand(interp, "tclAE::build", Tclae_BuildCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::send", Tclae_SendCmd, NULL, 0L);
    
    /* Handler commands */
    
    Tcl_CreateObjCommand(interp, "tclAE::getCoercionHandler", Tclae_GetCoercionHandlerCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getEventHandler", Tclae_GetEventHandlerCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::installCoercionHandler", Tclae_InstallCoercionHandlerCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::installEventHandler", Tclae_InstallEventHandlerCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::removeCoercionHandler", Tclae_RemoveCoercionHandlerCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::removeEventHandler", Tclae_RemoveEventHandlerCmd, NULL, 0L);
    
    /* Target commands */
    
#if !TARGET_API_MAC_CARBON  && !defined(TCLAE_NO_EPPC) // das 25/10/00: Carbonization
    Tcl_CreateObjCommand(interp, "tclAE::IPCListPorts", Tclae_IPCListPortsCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::PPCBrowser", Tclae_PPCBrowserCmd, NULL, 0L);
#endif
    
#if TARGET_API_MAC_CARBON	
    Tcl_CreateObjCommand(interp, "tclAE::getPOSIXPath", Tclae_GetPOSIXPathCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getHFSPath", Tclae_GetHFSPathCmd, NULL, 0L);
#endif

    Tcl_CreateObjCommand(interp, "tclAE::launch", Tclae_LaunchCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::processes", Tclae_ProcessesCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::remoteProcessResolverGetProcesses", Tclae_RemoteProcessResolverGetProcessesCmd, NULL, 0L);
    
    /* AEDesc commands */
    
    Tcl_CreateObjCommand(interp, "tclAE::coerceData", Tclae_CoerceDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::coerceDesc", Tclae_CoerceDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::countItems", Tclae_CountItemsCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::createDesc", Tclae_CreateDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::createList", Tclae_CreateListCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::deleteItem", Tclae_DeleteItemCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::deleteKeyDesc", Tclae_DeleteKeyDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::duplicateDesc", Tclae_DuplicateDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getAttributeData", Tclae_GetAttributeDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getAttributeDesc", Tclae_GetAttributeDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getData", Tclae_GetDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getDescType", Tclae_GetDescTypeCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getKeyData", Tclae_GetKeyDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getKeyDesc", Tclae_GetKeyDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getNthData", Tclae_GetNthDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getNthDesc", Tclae_GetNthDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::putData", Tclae_PutDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::putDesc", Tclae_PutDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::putKeyData", Tclae_PutKeyDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::putKeyDesc", Tclae_PutKeyDescCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::replaceDescData", Tclae_ReplaceDescDataCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::setDescType", Tclae_SetDescTypeCmd, NULL, 0L);
    
    Tcl_CreateObjCommand(interp, "tclAE::_private::_getAEDesc", Tclae__GetAEDescCmd, NULL, 0L);
    
    /* Object commands */
    
    Tcl_CreateObjCommand(interp, "tclAE::setObjectCallbacks", Tclae_SetObjectCallbacksCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::resolve", Tclae_ResolveCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::callObjectAccessor", Tclae_CallObjectAccessorCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::getObjectAccessor", Tclae_GetObjectAccessorCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::installObjectAccessor", Tclae_InstallObjectAccessorCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::removeObjectAccessor", Tclae_RemoveObjectAccessorCmd, NULL, 0L);
    Tcl_CreateObjCommand(interp, "tclAE::disposeToken", Tclae_DisposeTokenCmd, NULL, 0L);
    
    
	return TCL_OK;
}


