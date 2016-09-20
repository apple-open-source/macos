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
 * AppleX509CLSession.cpp - general CL session support
 */

#include "AppleX509CLSession.h"
#include <security_utilities/debugging.h>

AppleX509CLSession::AppleX509CLSession(
	CSSM_MODULE_HANDLE theHandle,
	CssmPlugin &plug,
	const CSSM_VERSION &version,
	uint32 subserviceId,
	CSSM_SERVICE_TYPE subserviceType,
	CSSM_ATTACH_FLAGS attachFlags,
	const CSSM_UPCALLS &upcalls)
		: CLPluginSession(theHandle, plug, version, subserviceId, 
							subserviceType,attachFlags, upcalls)
{
}

AppleX509CLSession::~AppleX509CLSession()
{
	/* free leftover contents of cache and query maps */
	CLCachedEntry *cachedCert = cacheMap.removeFirstEntry();
	while(cachedCert != NULL) {
		secinfo("clDetach", "CL detach: deleting a cached Cert\n");
		delete cachedCert;
		cachedCert = cacheMap.removeFirstEntry();
	}
	CLQuery *query = queryMap.removeFirstEntry();
	while(query != NULL) {
		secinfo("clDetach", "CL detach: deleting a cached query\n");
		delete query;
		query = queryMap.removeFirstEntry();
	}
}

CLCachedCert *
AppleX509CLSession::lookupCachedCert(CSSM_HANDLE handle)
{
	CLCachedEntry *entry = cacheMap.lookupEntry(handle);
	if(entry != NULL) {
		/* 
		 * we rely on this dynamic cast to detect a bogus lookup 
		 * of a cert via a CRL's handle
		 */
		return dynamic_cast<CLCachedCert *>(entry);
	}
	else {
		return NULL;
	}
}
		
CLCachedCRL	*
AppleX509CLSession::lookupCachedCRL(CSSM_HANDLE handle)
{
	CLCachedEntry *entry = cacheMap.lookupEntry(handle);
	if(entry != NULL) {
		/* 
		 * we rely on this dynamic cast to detect a bogus lookup 
		 * of a CRL via a cert's handle
		 */
		return dynamic_cast<CLCachedCRL *>(entry);
	}
	else {
		return NULL;
	}
}	

