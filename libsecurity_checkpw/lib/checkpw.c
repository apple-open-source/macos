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

/*
 *  checkpw.c
 *  utility to authenticate users using crypt with a fallback on Directory Services
 *
 *    Copyright:  (c) 2000 by Apple Computer, Inc., all rights reserved
 *
 */

#include <pwd.h>
#include <stddef.h>		// for offsetof()
#include <stdlib.h> // for malloc()
#include <string.h> // for strcmp()
#include <time.h>
#include <unistd.h> // for usleep(), sleep(), getpid()
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include <DirectoryServiceMIG.h>    // kDSStdMachPortName, etc.
#include "checkpw.h"
#include <syslog.h>

int32_t checkpw_internal_mig( const char* username, const char* password )
{
	int32_t			result			= CHECKPW_FAILURE;
	mach_port_t		dsMachServer	= MACH_PORT_NULL;
	
	if ( bootstrap_look_up(bootstrap_port, kDSStdMachPortName, &dsMachServer) == KERN_SUCCESS )
	{
		if ( dsmig_checkUsernameAndPassword(dsMachServer, (char *)username, (char *)password, &result) != KERN_SUCCESS )
		{
			syslog(LOG_ALERT, "dsmig_checkUsernameAndPassword mach IPC error");
			result = CHECKPW_FAILURE;
		}
	}
	
	return result;
}

int checkpw_internal( const struct passwd* pw, const char* password )
{
	return checkpw(pw->pw_name, password);
}

int checkpw( const char* userName, const char* password )
{
	int				siResult = CHECKPW_FAILURE;
	// workaround for 3965234; I assume the empty string is OK...
	const char	   *thePassword = password ? password : "";

	if (userName == NULL)
		return CHECKPW_UNKNOWNUSER;
	
	siResult = checkpw_internal_mig(userName, thePassword);
	switch (siResult) {
		case CHECKPW_SUCCESS:
		case CHECKPW_UNKNOWNUSER:
		case CHECKPW_BADPASSWORD:
			break;
		default:
			usleep(500000);
			siResult = checkpw_internal_mig(userName, thePassword);
			break;
	}
	
	return siResult;
}

