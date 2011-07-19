/* pam_nologin module */

/*
 * $Id: pam_nologin.c,v 1.5 2002/03/27 19:20:24 bbraun Exp $
 *
 * Written by Michael K. Johnson <johnsonm@redhat.com> 1996/10/24
 *
 * Portions Copyright (C) 2002-2009 Apple Inc.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <pwd.h>
#include <string.h>

/*
 * here, we make a definition for the externally accessible function
 * in this file (this definition is required for static a module
 * but strongly encouraged generally) it is used to instruct the
 * modules include file to define the function prototypes.
 */

#define PAM_SM_ACCOUNT

#include <security/pam_modules.h>
#include <security/pam_appl.h>


/* --- account management function (only) --- */

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc,
                        const char **argv)
{
     int retval = PAM_SUCCESS;
     int fd;
     const char *username;
     char *mtmp=NULL;
     struct passwd *user_pwd;
     struct passwd pwdbuf;
     char pwbuffer[2 * PATH_MAX];
     struct pam_conv *conversation;
     struct pam_message message;
     struct pam_message *pmessage = &message;
     struct pam_response *resp = NULL;
     struct stat st;

     if ((fd = open("/etc/nologin", O_RDONLY, 0)) >= 0) {
       /* root can still log in; lusers cannot */
       if ((pam_get_user(pamh, &username, NULL) != PAM_SUCCESS)
           || !username) {
		   openpam_log(PAM_LOG_ERROR, "Failed to obtain the username.");
         return PAM_SERVICE_ERR;
       }
       if (getpwnam_r(username, &pwdbuf, pwbuffer, sizeof(pwbuffer), &user_pwd) != 0) {
         openpam_log(PAM_LOG_ERROR, "The getpwnam_r call failed.");
         user_pwd = NULL;
       }
       if (user_pwd && user_pwd->pw_uid == 0) {
         message.msg_style = PAM_TEXT_INFO;
       } else {
	   if (!user_pwd) {
		   openpam_log(PAM_LOG_ERROR, "The user is invalid.");
	       retval = PAM_USER_UNKNOWN;
	   } else {
		   openpam_log(PAM_LOG_ERROR, "Denying non-root login.");
	       retval = PAM_AUTH_ERR;
	   }
	   message.msg_style = PAM_ERROR_MSG;
       }

       /* fill in message buffer with contents of /etc/nologin */
	   if (fstat(fd, &st) < 0) {/* give up trying to display message */
		   openpam_log(PAM_LOG_DEBUG, "Failure displaying the message.");
		   return retval;
	   }
       message.msg = mtmp = malloc(st.st_size+1);
       /* if malloc failed... */
	   if (!message.msg){
		   openpam_log(PAM_LOG_DEBUG, "The message is empty.");
		   return retval;
	   }
       read(fd, mtmp, st.st_size);
       mtmp[st.st_size] = '\000';

       /* Use conversation function to give user contents of /etc/nologin */
       pam_get_item(pamh, PAM_CONV, (const void **)&conversation);
       conversation->conv(1, (const struct pam_message **)&pmessage,
			  &resp, conversation->appdata_ptr);
       free(mtmp);
       if (NULL != resp && NULL != resp->resp) {
         memset(resp->resp, 0, strlen(resp->resp));
       }
     }

     return retval;
}


#ifdef PAM_STATIC

/* static module data */

struct pam_module _pam_nologin_modstruct = {
     "pam_nologin",
     pam_sm_authenticate,
     pam_sm_setcred,
     NULL,
     NULL,
     NULL,
     NULL,
};

#endif

/* end of module definition */
