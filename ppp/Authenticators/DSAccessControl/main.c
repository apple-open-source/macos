/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add support for authentication through Directory Services.
 *
----------------------------------------------------------------------------- */


#include <stdio.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <membership.h>
#include <membershipPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../../Helpers/pppd/pppd.h"
#include "../../Helpers/pppd/pppd.h"
#include "../DSAuthentication/DSUser.h"

#define VPN_SERVICE_NAME	"vpn"

static CFBundleRef 	bundle = 0;

static int dsaccess_authorize_user(char* user_name, int len);

/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */

int start(CFBundleRef ref)
{

    bundle = ref;

    CFRetain(bundle);

    // setup hooks
    acl_hook = dsaccess_authorize_user;
            
    info("Directory Services Authorization plugin initialized\n");
        
    return 0;
}


//----------------------------------------------------------------------
//	dsaccess_authorize_user
//----------------------------------------------------------------------
static int dsaccess_authorize_user(char* name, int len)
{

    tDirReference			dirRef;
    tDirStatus				dsResult = eDSNoErr;
    int						authorized = 0;
    tDirNodeReference 		searchNodeRef;
    tAttributeValueEntryPtr	gUID;
    UInt32					searchNodeCount;
    char*					user_name;
	uuid_t					userid;
	int						result;
	int						ismember;
    
    if (len < 1) {
        error("DSAccessControl plugin: invalid user name has zero length\n");
        return 0;
    }
    if ((user_name = (char*)malloc(len + 1)) == 0) {
        error("DSAccessControl plugin: unable to allocate memory for user name\n");
        return 0;
    }
    bcopy(name, user_name, len);
    *(user_name + len) = 0;

    if ((dsResult = dsOpenDirService(&dirRef)) == eDSNoErr) {  
        // get the search node ref
        if ((dsResult = dsauth_get_search_node_ref(dirRef, 1, &searchNodeRef, &searchNodeCount)) == eDSNoErr) {
            // get the user's generated user Id
            if ((dsResult = dsauth_get_user_attr(dirRef, searchNodeRef, user_name, kDS1AttrGeneratedUID, &gUID)) == eDSNoErr) {
                if (gUID != 0) {
					if (!mbr_string_to_uuid(gUID->fAttributeValueData.fBufferData, userid)) {
						// check if user is member authorized
						result = mbr_check_service_membership(userid, VPN_SERVICE_NAME, &ismember);
						if (result == ENOENT || (result == 0 && ismember != 0))
							authorized = 1;
					}
					dsDeallocAttributeValueEntry(dirRef, gUID);
                }
            }
            dsCloseDirNode(searchNodeRef);		// close the search node
        }
        dsCloseDirService(dirRef);
    }

    if (authorized)
        notice("DSAccessControl plugin: User '%s' authorized for access\n", user_name);
    else
        notice("DSAccessControl plugin: User '%s' not authorized for access\n", user_name);
    free(user_name);
    return authorized;
}

