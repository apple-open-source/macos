/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
 *
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * ModuleAttacher.c
 *
 * Process-wide class which loads and attaches to {CSP, TP, CL} at most
 * once, and detaches and unloads the modules when this code is unloaded.
 */

#include "ssl.h"
#if USE_CDSA_CRYPTO

#include "ModuleAttacher.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include <Security/cssmapple.h>
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>

#include <pthread.h>

static pthread_mutex_t gAttachLock = PTHREAD_MUTEX_INITIALIZER;
static CSSM_CSP_HANDLE gCSPHandle = CSSM_INVALID_HANDLE;
static CSSM_CL_HANDLE gCLHandle = CSSM_INVALID_HANDLE;
static CSSM_TP_HANDLE gTPHandle = CSSM_INVALID_HANDLE;

static const CSSM_API_MEMORY_FUNCS CA_memFuncs = {
	stAppMalloc,
	stAppFree,
	stAppRealloc,
 	stAppCalloc,
 	NULL
};
static const CSSM_VERSION cssmVers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

static CSSM_HANDLE loadModule(
	CSSM_SERVICE_TYPE svcType,	// CSSM_SERVICE_CSP, etc.
	const CSSM_GUID *guid,
	const char *modName)
{
	CSSM_RETURN crtn;
	CSSM_HANDLE hand;

	crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		#ifndef NDEBUG
		sslErrorLog("loadModule: error (%lu) loading %s\n",
			(unsigned long)crtn, modName);
		#endif
		return CSSM_INVALID_HANDLE;
	}
	crtn = CSSM_ModuleAttach (guid,
		&cssmVers,
		&CA_memFuncs,			// memFuncs
		0,						// SubserviceID
		svcType,				// SubserviceFlags
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&hand);
	if(crtn) {
		#ifndef NDEBUG
		sslErrorLog("loadModule: error (%lu) attaching to %s\n",
			(unsigned long)crtn, modName);
		#endif
		return CSSM_INVALID_HANDLE;
	}
	return hand;
}


static CSSM_RETURN doAttachToModules(void)
{
	CSSM_RETURN  crtn;
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
	CSSM_HANDLE cspHandle, clHandle, tpHandle;

	/* Check if we got the lock after some other thread did the
	   initialization. */
	if (gCSPHandle)
		return CSSM_OK;

	crtn = CSSM_Init (&cssmVers,
		CSSM_PRIVILEGE_SCOPE_NONE,
		&testGuid,
		CSSM_KEY_HIERARCHY_NONE,
		&pvcPolicy,
		NULL /* reserved */);
	if(crtn != CSSM_OK) {
		#ifndef NDEBUG
		sslErrorLog("CSSM_Init returned %lu", (unsigned long)crtn);
		#endif
		return crtn;
	}

	cspHandle = loadModule(CSSM_SERVICE_CSP, &gGuidAppleCSP, "AppleCSP");
	if (cspHandle == CSSM_INVALID_HANDLE)
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	clHandle = loadModule(CSSM_SERVICE_CL, &gGuidAppleX509CL, "AppleCL");
	if (clHandle == CSSM_INVALID_HANDLE)
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	tpHandle = loadModule(CSSM_SERVICE_TP, &gGuidAppleX509TP, "AppleTP");
	if (tpHandle == CSSM_INVALID_HANDLE)
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;

	gCSPHandle = cspHandle;
	gCLHandle = clHandle;
	gTPHandle = tpHandle;

	return CSSM_OK;
}

/* Public C function to load and attach to all three modules. */
CSSM_RETURN attachToModules(
	CSSM_CSP_HANDLE		*cspHand,
	CSSM_CL_HANDLE		*clHand,
	CSSM_TP_HANDLE		*tpHand)
{
	CSSM_RETURN result;
	if (gCSPHandle && gCLHandle && gTPHandle)
		result = CSSM_OK;
	else
	{
		pthread_mutex_lock(&gAttachLock);
		result = doAttachToModules();
		pthread_mutex_unlock(&gAttachLock);
	}

	*cspHand = gCSPHandle;
	*clHand = gCLHandle;
	*tpHand = gTPHandle;

	return result;
}

#endif	/* USE_CDSA_CRYPTO */

