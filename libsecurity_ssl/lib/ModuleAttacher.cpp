/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * ModuleAttacher.cpp
 *
 * Process-wide class which loads and attaches to {CSP, TP, CL} at most
 * once, and detaches and unloads the modules when this code is unloaded.
 */

#include "ModuleAttacher.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <Security/cssmapple.h>
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>

class ModuleAttacher
{
public:
	ModuleAttacher() :
		mCspHand(CSSM_INVALID_HANDLE),
		mClHand(CSSM_INVALID_HANDLE),
		mTpHand(CSSM_INVALID_HANDLE),
		mCssmInitd(false)
			{ }
	~ModuleAttacher();
	CSSM_CSP_HANDLE			getCspHand();
	CSSM_CL_HANDLE			getClHand();
	CSSM_TP_HANDLE			getTpHand();
	CSSM_RETURN				loadAllModules(
								CSSM_CSP_HANDLE &cspHand,
								CSSM_CL_HANDLE	&clHand,
								CSSM_TP_HANDLE	&tpHand);

private:
	/* on all private member functions, mLock held on entry and exit */
	bool					initCssm();
	CSSM_HANDLE				loadModule(
								CSSM_SERVICE_TYPE svcType,	// CSSM_SERVICE_CSP, etc.
								const CSSM_GUID *guid,
								const char *modName);
	void					unloadModule(
								CSSM_HANDLE	hand,
								const CSSM_GUID *guid);

	/* connection to modules, evaluated lazily */
	CSSM_CSP_HANDLE			mCspHand;
	CSSM_TP_HANDLE			mClHand;
	CSSM_TP_HANDLE			mTpHand;
	bool					mCssmInitd;
	Mutex					mLock;
};

/* the single global thing */
static ModuleNexus<ModuleAttacher> moduleAttacher;

static const CSSM_API_MEMORY_FUNCS CA_memFuncs = {
	stAppMalloc,
	stAppFree,
	stAppRealloc,
 	stAppCalloc,
 	NULL
};


/*
 * This only gets called when cspAttacher get deleted, i.e., when this code
 * is actually unloaded from the process's address space.
 */
ModuleAttacher::~ModuleAttacher()
{
	StLock<Mutex> 	_(mLock);

	if(mCspHand != CSSM_INVALID_HANDLE) {
		unloadModule(mCspHand, &gGuidAppleCSP);
	}
	if(mTpHand != CSSM_INVALID_HANDLE) {
		unloadModule(mTpHand, &gGuidAppleX509TP);
	}
	if(mClHand != CSSM_INVALID_HANDLE) {
		unloadModule(mClHand, &gGuidAppleX509CL);
	}
}

static const CSSM_VERSION cssmVers = {2, 0};
static const CSSM_GUID testGuid = { 0xFADE, 0, 0, { 1,2,3,4,5,6,7,0 }};

bool ModuleAttacher::initCssm()
{
	CSSM_RETURN  crtn;
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
	
	if(mCssmInitd) {
		return true;
	}  
	crtn = CSSM_Init (&cssmVers, 
		CSSM_PRIVILEGE_SCOPE_NONE,
		&testGuid,
		CSSM_KEY_HIERARCHY_NONE,
		&pvcPolicy,
		NULL /* reserved */);
	if(crtn != CSSM_OK) {
		#ifndef NDEBUG
		sslErrorLog("CSSM_Init returned %ld", crtn);
		#endif
		return false;
	}
	else {
		mCssmInitd = true;
		return true;
	}
}

CSSM_HANDLE ModuleAttacher::loadModule(
	CSSM_SERVICE_TYPE svcType,	// CSSM_SERVICE_CSP, etc.
	const CSSM_GUID *guid,
	const char *modName)
{
	CSSM_RETURN crtn;
	CSSM_HANDLE hand;
	
	if(!initCssm()) {
		return CSSM_INVALID_HANDLE;
	}
	crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		#ifndef NDEBUG		
		sslErrorLog("ModuleAttacher::loadModule: error (%ld) loading %s\n",
			crtn, modName);
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
		sslErrorLog("ModuleAttacher::loadModule: error (%ld) attaching to %s\n",
			crtn, modName);
		#endif
		return CSSM_INVALID_HANDLE;
	}
	return hand;
}

void ModuleAttacher::unloadModule(
	CSSM_HANDLE	hand,
	const CSSM_GUID *guid)
{
	CSSM_ModuleDetach(hand);
	CSSM_ModuleUnload(guid, NULL, NULL);
}

CSSM_CSP_HANDLE ModuleAttacher::getCspHand()
{
	StLock<Mutex> 	_(mLock);
	
	if(mCspHand != CSSM_INVALID_HANDLE) {
		/* already connected */
		return mCspHand;
	}	
	mCspHand = loadModule(CSSM_SERVICE_CSP, &gGuidAppleCSP, "AppleCSP");
	return mCspHand;
}

CSSM_CL_HANDLE ModuleAttacher::getClHand()
{
	StLock<Mutex> 	_(mLock);
	
	if(mClHand != CSSM_INVALID_HANDLE) {
		/* already connected */
		return mClHand;
	}	
	mClHand = loadModule(CSSM_SERVICE_CL, &gGuidAppleX509CL, "AppleCL");
	return mClHand;
}

CSSM_TP_HANDLE ModuleAttacher::getTpHand()
{
	StLock<Mutex> 	_(mLock);
	
	if(mTpHand != CSSM_INVALID_HANDLE) {
		/* already connected */
		return mTpHand;
	}	
	mTpHand = loadModule(CSSM_SERVICE_TP, &gGuidAppleX509TP, "AppleTP"); 
	return mTpHand;
}

CSSM_RETURN ModuleAttacher::loadAllModules(
	CSSM_CSP_HANDLE &cspHand,
	CSSM_CL_HANDLE	&clHand,
	CSSM_TP_HANDLE	&tpHand)
{
	StLock<Mutex> 	_(mLock);
	
	if(mCspHand == CSSM_INVALID_HANDLE) {
		mCspHand = loadModule(CSSM_SERVICE_CSP, &gGuidAppleCSP, "AppleCSP");
		if(mCspHand == CSSM_INVALID_HANDLE) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
	}
	if(mClHand == CSSM_INVALID_HANDLE) {
		mClHand = loadModule(CSSM_SERVICE_CL, &gGuidAppleX509CL, "AppleCL");
		if(mClHand == CSSM_INVALID_HANDLE) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
	}
	if(mTpHand == CSSM_INVALID_HANDLE) {
		mTpHand = loadModule(CSSM_SERVICE_TP, &gGuidAppleX509TP, "AppleTP");
		if(mTpHand == CSSM_INVALID_HANDLE) {
			return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
		}
	}
	cspHand = mCspHand;
	clHand  = mClHand;
	tpHand  = mTpHand;
	return CSSM_OK;
}

/* public C function to load and attach to all three modules */
CSSM_RETURN attachToModules(
	CSSM_CSP_HANDLE		*cspHand,
	CSSM_CL_HANDLE		*clHand,
	CSSM_TP_HANDLE		*tpHand)
{
	return moduleAttacher().loadAllModules(*cspHand, *clHand, *tpHand);
}

