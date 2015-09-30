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
 * DH_csp.h - Diffie-Hellman Algorithm factory
 */
 
#ifndef	_DH_CSP_H_
#define _DH_CSP_H_

#include <security_cdsa_plugin/CSPsession.h>
#include <AppleCSP.h>

/* Can't include AppleCSPSession.h due to circular dependency */
class AppleCSPSession;

class DH_Factory : public AppleCSPAlgorithmFactory {
public:
    DH_Factory(Allocator *normAlloc = NULL, Allocator *privAlloc = NULL);
	~DH_Factory();
	
    bool setup(
		AppleCSPSession &session,
		CSPFullPluginSession::CSPContext * &cspCtx, 
		const Context &context);

    static void setNormAllocator(Allocator *alloc)
    { assert(!normAllocator); normAllocator = alloc; }
    static void setPrivAllocator(Allocator *alloc)
    { assert(!privAllocator); privAllocator = alloc; }

    // memory allocators
    static Allocator *normAllocator;
    static Allocator *privAllocator;
    
};

#endif	/* _DH_CSP_H_ */
