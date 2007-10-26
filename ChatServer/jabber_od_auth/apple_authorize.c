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

#include "apple_authorize.h"

#include <membershipPriv.h>
#include <membership.h>
#include <errno.h>

/* -----------------------------------------------------------------
	od_auth_check_sacl(const char *inUser) - Check kChatService ACL
		inUser - username in utf-8
		inService - name of the service in utf-8 
		
		NOTE: the service name is not the group name, the transformation currently goes like
			this: "service" -> "com.apple.access_service"
			
	RETURNS:
		kAuthorized if the user is authorized (or no ACL exists)
		kNotAuthorized if the user is not authorized or does not exist

   ----------------------------------------------------------------- */
int	od_auth_check_sacl(const char *inUser)
{
	uuid_t	user_uuid;
	
	int	isMember = 0;
	int	mbrErr = ENOENT;
	
	if( (mbrErr = mbr_user_name_to_uuid(inUser, user_uuid)) != 0)
	{
        	return kNotAuthorized;
	}	
	
	if((mbrErr = mbr_check_service_membership(user_uuid, kChatService, &isMember)) != 0)
	{
		if(mbrErr == ENOENT){	// no ACL exists
			return kAuthorized;	
		} else {
			return kNotAuthorized;
		}
	}

	if(isMember == kAuthorized)
	{
		return kAuthorized;
	} else {
		return kNotAuthorized;
	}
} /* od_auth_check_sacl */
