/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * dotMacTpAttach.cpp - attach/detach to/from loadable .mac TP
 */
 
#include "dotMacTpAttach.h"
#include <Security/Security.h>

/* SPI; the framework actually contains a static lib we link against */
#include <security_cdsa_utils/cuCdsaUtils.h>

static CSSM_VERSION vers = {2, 0};
static const CSSM_GUID dummyGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

static CSSM_API_MEMORY_FUNCS memFuncs = {
	cuAppMalloc,
	cuAppFree,
	cuAppRealloc,
 	cuAppCalloc,
 	NULL
};

/* load & attach; returns 0 on error */
CSSM_TP_HANDLE dotMacTpAttach()
{
	CSSM_TP_HANDLE tpHand;
	CSSM_RETURN crtn;
	
	if(cuCssmStartup() == CSSM_FALSE) {
		return 0;
	}
	crtn = CSSM_ModuleLoad(&gGuidAppleDotMacTP,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		cuPrintError("CSSM_ModuleLoad(AppleTP)", crtn);
		return 0;
	}
	crtn = CSSM_ModuleAttach (&gGuidAppleDotMacTP,
		&vers,
		&memFuncs,				// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_TP,		// SubserviceFlags
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&tpHand);
	if(crtn) {
		cuPrintError("CSSM_ModuleAttach(AppleTP)", crtn);
		return 0;
	}
	else {
		return tpHand;
	}
}

/* detach & unload */
void dotMacTpDetach(CSSM_TP_HANDLE tpHand)
{
	CSSM_RETURN crtn = CSSM_ModuleDetach(tpHand);
	if(crtn) {
		return;
	}
	CSSM_ModuleUnload(&gGuidAppleDotMacTP, NULL, NULL);
}

