/*
 * Copyright (c) 2004-2008, 2011 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * March 9, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <notify.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <servers/bootstrap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <bsm/libbsm.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "dnsinfo_server.h"
#include "dnsinfo_private.h"
#include <network_information.h>

static	CFDataRef	shared_dns_info		= NULL;
static  CFDataRef	shared_nwi_state	= NULL;

__private_extern__
kern_return_t
_shared_dns_infoGet(mach_port_t server, dnsDataOut_t *dataRef, mach_msg_type_number_t *dataLen)
{
	*dataRef = NULL;
	*dataLen = 0;

	if (shared_dns_info != NULL) {
		CFIndex	len;
		Boolean	ok;

		ok = _SCSerializeData(shared_dns_info, (void **)dataRef, &len);
		*dataLen = len;
		if (!ok) {
			return KERN_FAILURE;
		}
	}

	return KERN_SUCCESS;
}


static
kern_return_t
_shared_infoSet_common(mach_port_t		server,
		       dnsData_t		dataRef,
		       mach_msg_type_number_t	dataLen,
		       audit_token_t		audit_token,
		       CFDataRef*		cache,
		       const char*		notify_key)
{
	uid_t		euid		= 0;
	CFDataRef	new_info	= NULL;
	CFDataRef	n_cache		= *cache;

	if ((dataRef != NULL) && (dataLen > 0)) {
		if (!_SCUnserializeData(&new_info, (void *)dataRef, dataLen)) {
			goto error;
		}
	}

	audit_token_to_au32(audit_token,
			    NULL,	// auidp
			    &euid,	// euid
			    NULL,	// egid
			    NULL,	// ruid
			    NULL,	// rgid
			    NULL,	// pid
			    NULL,	// asid
			    NULL);	// tid
	if (euid != 0) {
		goto error;
	}

	if ((n_cache != NULL) &&
	    (new_info != NULL) &&
	    CFEqual(n_cache, new_info)) {
		CFRelease(new_info);
		return KERN_SUCCESS;
	}

	if (n_cache != NULL) CFRelease(n_cache);
	*cache = new_info;

	if (notify_key != NULL) {
		uint32_t	status;

		status = notify_post(notify_key);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("notify_post() failed: %d"), status);
			// notification posting failures are non-fatal
		}
	}

	return KERN_SUCCESS;

    error :

	if (new_info != NULL)    CFRelease(new_info);
	return KERN_FAILURE;
}


__private_extern__
kern_return_t
_shared_dns_infoSet(mach_port_t			server,
		    dnsData_t			dataRef,
		    mach_msg_type_number_t	dataLen,
		    audit_token_t		audit_token)
{
	const char	*notify_key;

	notify_key = _dns_configuration_notify_key();
	return _shared_infoSet_common(server, dataRef, dataLen,
				      audit_token, &shared_dns_info,
				      notify_key);
}


__private_extern__
kern_return_t
_shared_nwi_stateGet(mach_port_t server, dnsDataOut_t *dataRef, mach_msg_type_number_t *dataLen)
{
	*dataRef = NULL;
	*dataLen = 0;

	if (shared_nwi_state != NULL) {
		CFIndex	len;
		Boolean	ok;

		ok = _SCSerializeData(shared_nwi_state, (void **)dataRef, &len);
		*dataLen = len;
		if (!ok) {
			return KERN_FAILURE;
		}
	}

	return KERN_SUCCESS;

}


__private_extern__
kern_return_t
_shared_nwi_stateSet(mach_port_t		server,
		     dnsData_t			dataRef,
		     mach_msg_type_number_t 	dataLen,
		     audit_token_t		audit_token)
{
	const char      *notify_key;

	notify_key = nwi_state_get_notify_key();

	return _shared_infoSet_common(server, dataRef, dataLen,
				      audit_token, &shared_nwi_state,
				      notify_key);
}
