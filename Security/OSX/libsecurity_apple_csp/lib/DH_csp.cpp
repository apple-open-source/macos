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
 * DH_csp.cpp - Diffie-Hellman Algorithm factory
 */
 
#include "DH_csp.h"
#include "DH_keys.h"
#include <Security/cssmapple.h>

Allocator *DH_Factory::normAllocator;
Allocator *DH_Factory::privAllocator;

DH_Factory::DH_Factory(Allocator *normAlloc, Allocator *privAlloc)
{
	setNormAllocator(normAlloc);
	setPrivAllocator(privAlloc);
	
	/* NOTE WELL we assume that the RSA_DSA factory has already been instantitated, 
	 * doing the basic init of openssl */
	 
	ERR_load_DH_strings();
}

DH_Factory::~DH_Factory()
{
}

bool DH_Factory::setup(
	AppleCSPSession &session,	
	CSPFullPluginSession::CSPContext * &cspCtx, 
	const Context &context)
{
	switch(context.type()) {
		case CSSM_ALGCLASS_KEYGEN:
			switch(context.algorithm()) {
				case CSSM_ALGID_DH:
					if(cspCtx == NULL) {
						cspCtx = new DHKeyPairGenContext(session, context);
					}
					return true;
				default:
					break;
			}
			break;		

		default:
			break;
	}
	/* not implemented here */
	return false;
}



