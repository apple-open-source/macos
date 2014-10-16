/*
 * Copyright (c) 2000-2012 Apple Inc. All Rights Reserved.
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

#include <security/pam_appl.h>
#include <security/openpam.h>

#include "checkpw.h"
#include <syslog.h>
#include <unistd.h>

#define PAM_STACK_NAME "checkpw"

static
int checkpw_internal_pam( const char* uname, const char* password )
{
	int checkpwret = CHECKPW_FAILURE;

	int pamret = PAM_SUCCESS;
	pam_handle_t *pamh;
	struct pam_conv pamc;
	pamc.conv = &openpam_nullconv;

	pamret = pam_start(PAM_STACK_NAME, uname, &pamc, &pamh);
	if (PAM_SUCCESS != pamret)
	{
		syslog(LOG_WARNING,"PAM: Unable to start pam.");
		goto pamerr_no_end;
	}

	pamret = pam_set_item(pamh, PAM_AUTHTOK, password);
	if (PAM_SUCCESS != pamret)
	{
		syslog(LOG_WARNING,"PAM: Unable to set password.");
		goto pamerr;
	}

	pamret = pam_authenticate(pamh, 0);
	if (PAM_SUCCESS != pamret)
	{
		syslog(LOG_WARNING,"PAM: Unable to authenticate.");
		checkpwret = CHECKPW_BADPASSWORD;
		goto pamerr;
	}

	pamret = pam_acct_mgmt(pamh, 0);
	if (PAM_SUCCESS != pamret)
	{
		if (PAM_NEW_AUTHTOK_REQD == pamret)
		{
			syslog(LOG_WARNING,"PAM: Unable to authorize, password needs to be changed.");
		} else {
			syslog(LOG_WARNING,"PAM: Unable to authorize.");
		}

		goto pamerr;
	}

	checkpwret = CHECKPW_SUCCESS;

pamerr:
	pam_end(pamh, pamret);
pamerr_no_end:
	return checkpwret;

}

#warning TODO: this should be declared in some header.
int checkpw_internal( const struct passwd* pw, const char* password );
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
	
	siResult = checkpw_internal_pam(userName, thePassword);
	switch (siResult) {
		case CHECKPW_SUCCESS:
		case CHECKPW_UNKNOWNUSER:
		case CHECKPW_BADPASSWORD:
			break;
		default:
			usleep(500000);
			siResult = checkpw_internal_pam(userName, thePassword);
			break;
	}
	
	return siResult;
}

