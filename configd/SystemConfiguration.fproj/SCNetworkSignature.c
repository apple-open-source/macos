/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
 * SCNetworkSignature.c
 * - implementation of SCNetworkSignatureRef API that allows access to
     network identification information
 *
 */
/*
 * Modification History
 *
 * November 6, 2006	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */


#include <netinet/in.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCNetworkSignature.h"
#include "SCNetworkSignaturePrivate.h"

const char * kSCNetworkSignatureActiveChangedNotifyName = NETWORK_ID_KEY ".active";


#pragma mark SCNetworkSignature support routines

static __inline__ SCDynamicStoreRef
store_create(CFAllocatorRef alloc)
{
	return (SCDynamicStoreCreate(alloc, CFSTR("SCNetworkSignature"),
				     NULL, NULL));
}

static CFDictionaryRef
store_copy_id_dict(CFAllocatorRef alloc, SCDynamicStoreRef store)
{
	CFDictionaryRef	id_dict = NULL;
	Boolean		release_store = FALSE;

	if (store == NULL) {
		store = store_create(alloc);
		if (store == NULL) {
			goto done;
		}
		release_store = TRUE;
	}
	id_dict = SCDynamicStoreCopyValue(store,
					  kSCNetworkIdentificationStoreKey);
	if (isA_CFDictionary(id_dict) == NULL) {
		if (id_dict != NULL) {
			CFRelease(id_dict);
			id_dict = NULL;
		}
		goto done;
	}
 done:
	if (release_store) {
		CFRelease(store);
	}
	return (id_dict);
}

#pragma -

#pragma mark SCNetworkSignature APIs

CFStringRef
SCNetworkSignatureCopyActiveIdentifierForAddress(CFAllocatorRef alloc,
						 const struct sockaddr * addr)
{
	CFDictionaryRef		id_dict = NULL;
	CFStringRef		ident = NULL;
	struct sockaddr_in *	sin_p;


	/* only accept 0.0.0.0 (i.e. default) for now */
	sin_p = (struct sockaddr_in *)addr;
	if (addr == NULL
	    || addr->sa_family != AF_INET
	    || addr->sa_len != sizeof(struct sockaddr_in)
	    || sin_p->sin_addr.s_addr != 0) {
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
	id_dict = store_copy_id_dict(alloc, NULL);
	if (id_dict == NULL) {
		_SCErrorSet(kSCStatusFailed);
		goto done;
	}
	ident = CFDictionaryGetValue(id_dict, kStoreKeyPrimaryIPv4Identifier);
	if (isA_CFString(ident) != NULL) {
		CFRetain(ident);
	}
	else {
		_SCErrorSet(kSCStatusFailed);
	}
 done:
	if (id_dict != NULL) {
		CFRelease(id_dict);
	}
	return (ident);
}

CFArrayRef /* of CFStringRef's */
SCNetworkSignatureCopyActiveIdentifiers(CFAllocatorRef alloc)
{
	CFArrayRef		active = NULL;
	int			i;
	int			count = 0;
	CFDictionaryRef		id_dict = NULL;

	id_dict = store_copy_id_dict(alloc, NULL);
	if (id_dict == NULL) {
		goto done;
	}
	active = CFDictionaryGetValue(id_dict, kStoreKeyActiveIdentifiers);
	if (isA_CFArray(active) != NULL) {
		count = CFArrayGetCount(active);
	}
	if (count == 0) {
		active = NULL;
		goto done;
	}
	for (i = 0; i < count; i++) {
		CFStringRef	ident = CFArrayGetValueAtIndex(active, i);

		if (isA_CFString(ident) == NULL) {
			active = NULL;
			goto done;
		}
	}
	CFRetain(active);

 done:
	if (id_dict != NULL) {
		CFRelease(id_dict);
	}
	if (active == NULL) {
		_SCErrorSet(kSCStatusFailed);
	}
	return (active);
}

#pragma mark -
