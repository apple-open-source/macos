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


//
// AppleCSP.h - top-level plugin and session classes
//
#ifndef _APPLE_CSP_H_
#define _APPLE_CSP_H_

#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_plugin/CSPsession.h>

class AppleCSPSession;
class AppleCSPContext; 

/*
 * AppleCSP-specific algorithm factory. 
 */
class AppleCSPAlgorithmFactory {
public:
	AppleCSPAlgorithmFactory() {};
	virtual ~AppleCSPAlgorithmFactory() { };

	// set ctx and return true if you can handle this
	virtual bool setup(
		AppleCSPSession 					&session,
		CSPFullPluginSession::CSPContext 	* &cspCtx, 
		const Context &context) = 0;
		
	/* probably other setup methods, e.g. by CSSM_ALGORITHMS instead of 
	 * context */
};

class AppleCSPPlugin : public CssmPlugin {
    friend class AppleCSPSession;
	friend class AppleCSPContext;
	
public:
    AppleCSPPlugin();
    ~AppleCSPPlugin();

    PluginSession *makeSession(CSSM_MODULE_HANDLE handle,
                               const CSSM_VERSION &version,
                               uint32 subserviceId,
                               CSSM_SERVICE_TYPE subserviceType,
                               CSSM_ATTACH_FLAGS attachFlags,
                               const CSSM_UPCALLS &upcalls);

	Allocator 	&normAlloc()	{return normAllocator; }
    Allocator 	&privAlloc()	{return privAllocator; }

private:
    Allocator 				&normAllocator;	
    Allocator 				&privAllocator;	
	#ifdef	BSAFE_CSP_ENABLE
    AppleCSPAlgorithmFactory	*bSafe4Factory;		// actually subclasses not visible
													// in this header
	#endif
	#ifdef	CRYPTKIT_CSP_ENABLE
	AppleCSPAlgorithmFactory	*cryptKitFactory;		
	#endif
	AppleCSPAlgorithmFactory	*miscAlgFactory;
	#ifdef	ASC_CSP_ENABLE
	AppleCSPAlgorithmFactory	*ascAlgFactory;
	#endif
	AppleCSPAlgorithmFactory	*rsaDsaAlgFactory;
	AppleCSPAlgorithmFactory	*dhAlgFactory;
};


#endif //_APPLE_CSP_H_
