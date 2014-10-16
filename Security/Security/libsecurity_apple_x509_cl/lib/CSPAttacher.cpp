/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * CSPAttacher.cpp - process-wide class which loads and attaches to CSP at most
 *				     once, and detaches and unloads the CSP when this code is
 *	 			     unloaded.
 */

#include "CSPAttacher.h"
#include "cldebugging.h"
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_utilities/alloc.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmapple.h>
#include <Security/cssmtype.h>
#include <Security/cssmapi.h>

class CSPAttacher
{
public:
	CSPAttacher() :
		mCspHand(CSSM_INVALID_HANDLE),
		mCspDlHand(CSSM_INVALID_HANDLE)
			{ }
	~CSPAttacher();
	CSSM_CSP_HANDLE			getCspHand(bool bareCsp);
	
private:
	/* connection to CSP and CSPDL, evaluated lazily */
	CSSM_HANDLE				mCspHand;
	CSSM_HANDLE				mCspDlHand;
	Mutex					mLock;
};

/* the single global thing */
static ModuleNexus<CSPAttacher> cspAttacher;

static void *CL_malloc(
	 CSSM_SIZE size,
     void *allocref)
{
	return Allocator::standard().malloc(size);
}

static void CL_free(
     void *memblock,
     void *allocref)
{
	Allocator::standard().free(memblock);
}

static void *CL_realloc(
     void *memblock,
     CSSM_SIZE size,
     void *allocref)
{
	return Allocator::standard().realloc(memblock, size);
}

static void *CL_calloc(
     uint32 num,
     CSSM_SIZE size,
     void *allocref)
{
	return Allocator::standard().calloc(num, size);
}

static const CSSM_API_MEMORY_FUNCS CL_memFuncs = {
	CL_malloc,
	CL_free,
	CL_realloc,
 	CL_calloc,
 	NULL
 };


/*
 * This only gets called when cspAttacher get deleted, i.e., when this code
 * is actually unloaded from the process's address space.
 */
CSPAttacher::~CSPAttacher()
{
	StLock<Mutex> 	_(mLock);

	if(mCspHand != CSSM_INVALID_HANDLE) {
		CSSM_ModuleDetach(mCspHand);
		CSSM_ModuleUnload(&gGuidAppleCSP, NULL, NULL);
	}
	if(mCspDlHand != CSSM_INVALID_HANDLE) {
		CSSM_ModuleDetach(mCspDlHand);
		CSSM_ModuleUnload(&gGuidAppleCSPDL, NULL, NULL);
	}
}

CSSM_CSP_HANDLE CSPAttacher::getCspHand(bool bareCsp)
{
	const char 		*modName;
	CSSM_RETURN		crtn;
	const CSSM_GUID *guid;
	CSSM_VERSION 	vers = {2, 0};
	StLock<Mutex> 	_(mLock);
	CSSM_CSP_HANDLE	cspHand;
	
	if(bareCsp) {
		if(mCspHand != CSSM_INVALID_HANDLE) {
			/* already connected */
			return mCspHand;
		}	
		guid = &gGuidAppleCSP;
		modName = "AppleCSP";
	}
	else {
		if(mCspDlHand != CSSM_INVALID_HANDLE) {
			/* already connected */
			return mCspDlHand;
		}	
		guid = &gGuidAppleCSPDL;
		modName = "AppleCSPDL";
	}
	crtn = CSSM_ModuleLoad(guid,
		CSSM_KEY_HIERARCHY_NONE,
		NULL,			// eventHandler
		NULL);			// AppNotifyCallbackCtx
	if(crtn) {
		clErrorLog("AppleX509CLSession::cspAttach: error (%d) loading %s",
			(int)crtn, modName);
		CssmError::throwMe(crtn);
	}
	crtn = CSSM_ModuleAttach (guid,
		&vers,
		&CL_memFuncs,			// memFuncs
		0,						// SubserviceID
		CSSM_SERVICE_CSP,		// SubserviceFlags 
		0,						// AttachFlags
		CSSM_KEY_HIERARCHY_NONE,
		NULL,					// FunctionTable
		0,						// NumFuncTable
		NULL,					// reserved
		&cspHand);
	if(crtn) {
		clErrorLog("AppleX509CLSession::cspAttach: error (%d) attaching to %s",
			(int)crtn, modName);
		CssmError::throwMe(crtn);
	}
	if(bareCsp) {
		mCspHand = cspHand;
	}
	else {
		mCspDlHand = cspHand;
	}
	return cspHand;
}

/* 
 * Just one public function - "give me a CSP handle".
 *   bareCsp true: AppleCSP
 *   bareCsp false: AppleCSPDL
 */
CSSM_CSP_HANDLE	getGlobalCspHand(bool bareCsp)
{
	return cspAttacher().getCspHand(bareCsp);
}

