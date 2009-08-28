/*
 * Copyright (c) 1999-2008 Apple Computer, Inc. All rights reserved.
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
#include <stdio.h>

#include <security/pam_appl.h>
#include <security/openpam.h>	/* for openpam_ttyconv() */

extern char* progname;
static pam_handle_t *pamh;
static struct pam_conv pamc;

int
pam_passwd(char* uname)
{
	int retval = PAM_SUCCESS;

	/* Initialize PAM. */
	pamc.conv = &openpam_ttyconv;
	pam_start(progname, uname, &pamc, &pamh);

	/* Authenticate. */
	if (PAM_SUCCESS != (retval = pam_authenticate(pamh, 0)))
		goto pamerr;

	/* Authorize. */
	if (PAM_SUCCESS != (retval = pam_acct_mgmt(pamh, 0)) && PAM_NEW_AUTHTOK_REQD != retval)
		goto pamerr;
	
	printf("Changing password for %s.\n", uname);

	/* Change the password. */
	if (PAM_SUCCESS != (retval = pam_chauthtok(pamh, 0)))
		goto pamerr;

	/* Set the credentials. */
	if (PAM_SUCCESS != (retval = pam_setcred(pamh, PAM_ESTABLISH_CRED)))
		goto pamerr;

	/* Open the session. */
	if (PAM_SUCCESS != (retval = pam_open_session(pamh, 0)))
		goto pamerr;	
	
	/* Close the session. */
	if (PAM_SUCCESS != (retval = pam_close_session(pamh, 0)))
		goto pamerr;

pamerr:
	/* Print an error, if needed. */
	if (PAM_SUCCESS != retval)
		fprintf(stderr, "%s: %s\n", progname, pam_strerror(pamh, retval));

	/* Terminate PAM. */
	pam_end(pamh, retval);
	return retval;
}
