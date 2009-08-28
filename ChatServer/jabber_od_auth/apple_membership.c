/*
 *
 * Copyright (c) 2005, Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 * 
*/

#include "apple_authenticate.h"

#include <sys/types.h>
#include <pwd.h>
#include <Security/checkpw.h>

#include <syslog.h>
#include <CoreServices/CoreServices.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFPropertyList.h>
#include <OpenDirectory/OpenDirectory.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesConstPriv.h>

#include <membership.h>
#include <membershipPriv.h>

/* -----------------------------------------------------------------
    int od_auth_check_service_membership()
    
	ARGS:
		username (IN) user name (UTF8 string)
		service  (IN) service name (UTF8 string)

	RETURNS: int
		0 = Failed: user is not a service member OR unable to access service ACL
		1 = OK: user is a member OR no restrictions for the selected service

   ----------------------------------------------------------------- */
int od_auth_check_service_membership(const char* userName, const char* service)
{
	syslog(LOG_USER | LOG_NOTICE, "%s: checking user \"%s\" access for service \"%s\"", 
	       __PRETTY_FUNCTION__, userName, service);

	// get the uuid for the user
	int mbrErr = 0;
	uuid_t user_uuid;
	if (mbrErr = mbr_user_name_to_uuid(userName, user_uuid)){
		syslog(LOG_ERR, "%s: mbr_user_name_to_uuid returns %s", __PRETTY_FUNCTION__, strerror(mbrErr));
		return 0;
	}	
	
	// First check whether there is a access list defined for the service. If 
	// none exists, then all users are permitted to access the service.
	int isMember = 0;
	mbrErr = mbr_check_service_membership(user_uuid, service, &isMember);
	syslog(LOG_USER | LOG_NOTICE, "%s: mbr_check_service_membership returned %d", __PRETTY_FUNCTION__, mbrErr);
	if (0 != mbrErr) {
		if (mbrErr == ENOENT)	// no ACL exists
			syslog(LOG_USER | LOG_NOTICE, "%s: no access restrictions found", __PRETTY_FUNCTION__);
		else
			syslog(LOG_ERR, "%s: mbr_check_service_membership returns %s", __PRETTY_FUNCTION__, strerror(mbrErr));
		return (mbrErr == ENOENT) ? 1 : 0;
	}

	// Now check whether the requesting user is a memeber of the service access list
	syslog(LOG_ERR, "%s: user \"%s\" %s authorized to access service \"%s\"", 
		   __PRETTY_FUNCTION__, userName, (1 == isMember ? "is" : "is not"), service);

	return (1 == isMember) ? 1 : 0;
}
