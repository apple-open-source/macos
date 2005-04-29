/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DSLDAPUtils
 */

#include <lber.h>
#include <ldap.h>
#include <sys/time.h>
#include <syslog.h>

#include "PrivateTypes.h"
#include "DSLDAPUtils.h"

void DSSearchCleanUp (	LDAP		   *inHost,
						int				inMsgId )
{
	struct timeval		tv					= { 0, 10 }; //do not anticipate any delay ie. 10 usecs
	LDAPMessage		   *aTestResult			= nil;
	int					aTestLDAPReturnCode = LDAP_RES_SEARCH_ENTRY;

	SetNetworkTimeoutsForHost( inHost, kLDAPDefaultNetworkTimeoutInSeconds );
	//here we attempt to read any remaining results from this LDAP msg chain so that the ld_responses list gets cleaned up in the LDAP FW
	if (inMsgId != 0)
	{
		while( (aTestLDAPReturnCode != 0) && (aTestLDAPReturnCode != -1) && (aTestLDAPReturnCode != LDAP_RES_SEARCH_RESULT) )
		{
			aTestLDAPReturnCode = ldap_result(inHost, inMsgId, LDAP_MSG_ONE, &tv, &aTestResult);
			if (aTestResult != nil)
			{
				ldap_msgfree(aTestResult);
				aTestResult = nil;
			}
			//syslog(LOG_INFO,"DSSearchCleanUp: flush calls to ldap_result for msgID = %d with result = %d", inMsgId, aTestLDAPReturnCode );
		}
	}
} // DSSearchLDAP

void SetNetworkTimeoutsForHost( LDAP* host, int numSeconds )
{
	struct timeval networkTimeout = { numSeconds, 0 };
	ldap_set_option( host, LDAP_OPT_TIMEOUT, &networkTimeout );
	ldap_set_option( host, LDAP_OPT_NETWORK_TIMEOUT, &networkTimeout );
}				
