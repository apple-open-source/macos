/* pam_rootok module */

/*
 * $Id: pam_rootok.c,v 1.5 2002/03/28 08:43:24 bbraun Exp $
 *
 * Written by Andrew Morgan <morgan@linux.kernel.org> 1996/3/11
 *
 * Portions Copyright (C) 2005-2009 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms of Linux-PAM, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 * 
 * 1. Redistributions of source code must retain any existing copyright
 * notice, and this entire permission notice in its entirety,
 * including the disclaimer of warranties.
 * 
 * 2. Redistributions in binary form must reproduce all prior and current
 * copyright notices, this list of conditions, and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * 3. The name of any author may not be used to endorse or promote
 * products derived from this software without their specific prior
 * written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE. 
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_AUTH

#include <security/pam_modules.h>

/* --- authentication management functions (only) --- */

PAM_EXTERN
int pam_sm_authenticate(pam_handle_t *pamh,int flags,int argc
			,const char **argv)
{
    int retval = PAM_AUTH_ERR;

    if (getuid() == 0)
	retval = PAM_SUCCESS;

	openpam_log(PAM_LOG_DEBUG, "authentication %s", retval==PAM_SUCCESS ? "succeeded":"failed" );

    return retval;
}

PAM_EXTERN
int pam_sm_setcred(pam_handle_t *pamh,int flags,int argc
		   ,const char **argv)
{
    return PAM_SUCCESS;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_rootok_modstruct = {
    "pam_rootok",
    pam_sm_authenticate,
    pam_sm_setcred,
    NULL,
    NULL,
    NULL,
    NULL,
};

#endif

/* end of module definition */
