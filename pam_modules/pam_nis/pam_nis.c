/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 1998-2001 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * Portions Copyright (c) 1998 by Apple Computer, Inc.
 * Portions Copyright (c) 1988 by Sun Microsystems, Inc.
 * Portions Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pwd.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>
#include <netinfo/ni.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#ifdef __NeXT__
#include <mach/port.h>
#include "lookup.h"
#else
#include <mach/mach_port.h>
#include <netinfo/lookup.h>
#endif /* __NeXT__ */

#define PAM_SM_PASSWORD
#include <security/pam_modules.h>
#include <security/_pam_macros.h>
#include <security/pam_mod_misc.h>

#define OLD_PASSWORD_PROMPT "Enter login(NIS) password:"
#define NEW_PASSWORD_PROMPT "New password:"
#define AGAIN_PASSWORD_PROMPT "Retype new password:"

static int      sendConversationMessage(struct pam_conv * aconv, const char *message, int style, struct options *options);
static int      secure_passwords();
static char    *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
extern int	getrpcport(char *, int, int, int);
static struct passwd *ypgetpwnam(char *name, char *domain);

#ifdef __NeXT__
extern port_t _lookupd_port(port_t);
#else
extern mach_port_t _lookupd_port(mach_port_t);
#endif /* __NeXT__ */

/*
 * Change a user's password in NIS/YP.
 */
PAM_EXTERN
int 
pam_sm_chauthtok(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	char           *oldHash, *newHash;
	char           *oldPassword = NULL, *newPassword = NULL;
	int             status, tries, maxTries;
	int		amChangingExpiredPassword;
	int             uid, secure, minlen;
	int             ans, ok = -1;
	unsigned int    port;
	struct pam_conv *appconv;
	struct pam_message msg, *pmsg;
	struct pam_response *resp;
	const char     *cmiscptr = NULL;
	char           *uname, *domain, *master;
	char            salt[9];
	CLIENT         *cl;
	static struct yppasswd	yppasswd;
	struct passwd  *pwd;
	struct timeval  tv; 
	struct options	options;

	amChangingExpiredPassword = flags & PAM_CHANGE_EXPIRED_AUTHTOK;

	pam_std_option(&options, NULL, argc, argv);

	status = pam_get_item(pamh, PAM_CONV, (void **) &appconv);
	if (status != PAM_SUCCESS)
		return status;

	status = pam_get_item(pamh, PAM_USER, (void **) &uname);
	if (status != PAM_SUCCESS)
		return status;

	if (uname == NULL)
		return PAM_USER_UNKNOWN;
	
	if (yp_get_default_domain(&domain) != 0)
		return PAM_SERVICE_ERR;

	if (yp_master(domain, "passwd.byname", &master) != 0)
		return PAM_SERVICE_ERR;
	
	port = getrpcport(master, YPPASSWDPROG, YPPASSWDPROC_UPDATE,
		IPPROTO_UDP);

	if (port == 0)
		return PAM_SERVICE_ERR;

	if (port >= IPPORT_RESERVED)
		return PAM_PERM_DENIED;

	pwd = ypgetpwnam(uname, domain);
	if (pwd == NULL)
		return PAM_USER_UNKNOWN;

	oldHash = pwd->pw_passwd;
	
	uid = getuid();
	if (uid != 0 && uid != pwd->pw_uid)
		return PAM_PERM_DENIED;

	status = pam_get_item(pamh, PAM_OLDAUTHTOK, (void **) &oldPassword);
	if (status != PAM_SUCCESS)
		return status;

	if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
	    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL))
	{
		if (pam_get_item(pamh, PAM_AUTHTOK, (void **) &newPassword) != PAM_SUCCESS)
			newPassword = NULL;

		if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) && newPassword == NULL)
			return PAM_AUTHTOK_RECOVER_ERR;
	}

	secure = secure_passwords();
	maxTries = secure ? 3 : 5;
	minlen = secure ? 8 : 5;

	if (flags & PAM_PRELIM_CHECK)
	{
		/*
		 * If we are not root, we should verify the old
		 * password.
		 */
		char           *encrypted;

		if (oldPassword != NULL &&
		   (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
		    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL)))
		{
			encrypted = crypt(oldPassword, oldHash);

			if (oldPassword[0] == '\0' && oldHash != '\0')
				encrypted = ":";
			status = strcmp(encrypted, oldHash) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;
			if (status != PAM_SUCCESS)
			{
				if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL))
					sendConversationMessage(appconv, "NIS password incorrect", PAM_ERROR_MSG, &options);
				else
					sendConversationMessage(appconv, "NIS password incorrect: try again", PAM_ERROR_MSG, &options);
			}
			else
			{
				return PAM_SUCCESS;
			}
		}
		tries = 0;

		while (oldPassword == NULL && tries++ < maxTries)
		{
			pmsg = &msg;
			msg.msg_style = PAM_PROMPT_ECHO_OFF;
			msg.msg = OLD_PASSWORD_PROMPT;
			resp = NULL;

			status = appconv->conv(1, (struct pam_message **) & pmsg, &resp, appconv->appdata_ptr);
			if (status != PAM_SUCCESS)
			{
				return status;
			}
			oldPassword = resp->resp;
			free(resp);

			encrypted = crypt(oldPassword, oldHash);

			if (oldPassword[0] == '\0' && oldHash != '\0')
				encrypted = ":";

			status = strcmp(encrypted, oldHash) == 0 ? PAM_SUCCESS : PAM_AUTH_ERR;

			if (status != PAM_SUCCESS)
			{
				int             abortMe = 0;

				if (oldPassword != NULL && oldPassword[0] == '\0')
					abortMe = 1;

				_pam_overwrite(oldPassword);
				_pam_drop(oldPassword);

				if (!amChangingExpiredPassword & abortMe)
				{
					sendConversationMessage(appconv, "Password change aborted", PAM_ERROR_MSG, &options);
					return PAM_AUTHTOK_RECOVER_ERR;
				}
				else
				{
					sendConversationMessage(appconv, "NIS password incorrect: try again", PAM_ERROR_MSG, &options);
				}
			}
		}

		if (oldPassword == NULL)
		{
			status = PAM_MAXTRIES;
		}
		(void) pam_set_item(pamh, PAM_OLDAUTHTOK, oldPassword);

		return status;
	}			/* PAM_PRELIM_CHECK */

	status = PAM_ABORT;
	tries = 0;

	while (newPassword == NULL && tries++ < maxTries)
	{
		pmsg = &msg;
		msg.msg_style = PAM_PROMPT_ECHO_OFF;
		msg.msg = NEW_PASSWORD_PROMPT;
		resp = NULL;

		status = appconv->conv(1, &pmsg, &resp, appconv->appdata_ptr);
		if (status != PAM_SUCCESS)
		{
			return status;
		}
		newPassword = resp->resp;
		free(resp);

		if (newPassword[0] == '\0')
		{
			free(newPassword);
			newPassword = NULL;
		}

		if (newPassword != NULL)
		{
			if (uid != 0)
			{
				if (oldPassword != NULL && !strcmp(oldPassword, newPassword))
				{
					cmiscptr = "Passwords must differ";
					newPassword = NULL;
				}
				else if (strlen(newPassword) < minlen)
				{
					cmiscptr = "Password too short";
					newPassword = NULL;
				}
			}
		} else
		{
			return PAM_AUTHTOK_RECOVER_ERR;
		}

		if (cmiscptr == NULL)
		{
			/* get password again */
			char           *miscptr;

			pmsg = &msg;
			msg.msg_style = PAM_PROMPT_ECHO_OFF;
			msg.msg = AGAIN_PASSWORD_PROMPT;
			resp = NULL;

			status = appconv->conv(1, &pmsg, &resp, appconv->appdata_ptr);

			if (status != PAM_SUCCESS)
			{
				return status;
			}
			miscptr = resp->resp;
			free(resp);

			if (miscptr[0] == '\0')
			{
				free(miscptr);
				miscptr = NULL;
			}
			if (miscptr == NULL)
			{
				if (!amChangingExpiredPassword)
				{
					sendConversationMessage(appconv, "Password change aborted",
						    PAM_ERROR_MSG, &options);
					return PAM_AUTHTOK_RECOVER_ERR;
				}
			}
			else if (!strcmp(newPassword, miscptr))
			{
				miscptr = NULL;
				break;
			}
			sendConversationMessage(appconv, "You must enter the same password",
						PAM_ERROR_MSG, &options);
			miscptr = NULL;
			newPassword = NULL;
		}
		else
		{
			sendConversationMessage(appconv, cmiscptr, PAM_ERROR_MSG, &options);
			cmiscptr = NULL;
			newPassword = NULL;
		}
	}

	if (cmiscptr != NULL || newPassword == NULL)
	{
		return PAM_MAXTRIES;
	}

	/*
        * Create a random salt
        */
	srandom((int) time((time_t *) NULL));
	salt[0] = saltchars[random() % strlen(saltchars)];
	salt[1] = saltchars[random() % strlen(saltchars)];
	salt[2] = '\0';
	newHash = crypt(newPassword, salt);

	yppasswd.oldpass = oldPassword;
	yppasswd.newpw.pw_name = pwd->pw_name;
	yppasswd.newpw.pw_passwd = newHash;
	yppasswd.newpw.pw_uid = pwd->pw_uid;
	yppasswd.newpw.pw_gid = pwd->pw_gid;
	yppasswd.newpw.pw_gecos = pwd->pw_gecos;
	yppasswd.newpw.pw_dir = pwd->pw_dir;
	yppasswd.newpw.pw_shell = pwd->pw_shell;	

	cl = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (cl == NULL)
		return PAM_SERVICE_ERR;

	cl->cl_auth = authunix_create_default();
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	ans = clnt_call(cl, YPPASSWDPROC_UPDATE,
		xdr_yppasswd, &yppasswd, xdr_int, &ok, tv);

	if (ans != 0)
	{
		sendConversationMessage(appconv, clnt_sperrno(ans), PAM_ERROR_MSG, &options);
		return PAM_PERM_DENIED;
	}

	if (ok != 0)
		return PAM_PERM_DENIED;

	/* tell lookupd to invalidate its cache */
	{
		int i, proc = -1;
		unit lookup_buf[MAX_INLINE_UNITS];
#ifdef __NeXT__
		port_t port;
#else
		mach_port_t port;
#endif

		port = _lookupd_port(0);
		(void) _lookup_link(port, "_invalidatecache", &proc);
		(void) _lookup_one(port, proc, NULL, 0, lookup_buf, &i);
	}

	return PAM_SUCCESS;
}

static int
secure_passwords()
{
	void           *d, *d1;
	int             status;
	ni_index        where;
	ni_id           dir;
	ni_namelist     nl;

	status = ni_open(NULL, ".", &d);
	while (status == NI_OK)
	{
		dir.nii_object = 0;
		status = ni_lookupprop(d, &dir, "security_options", &nl);
		if (status == NI_OK)
		{
			where = ni_namelist_match(nl, "secure_passwords");
			if (where != NI_INDEX_NULL)
			{
				ni_free(d);
				return 1;
			}
		}
		d1 = d;
		status = ni_open(d1, "..", &d);
		ni_free(d1);
	}

	return 0;
}

static int
sendConversationMessage(struct pam_conv * aconv,
			const char *message, int style, struct options *options)
{
	struct pam_message msg, *pmsg;
	struct pam_response *resp;

	if (pam_test_option(options, PAM_OPT_NO_WARN, NULL))
		return PAM_SUCCESS;

	pmsg = &msg;

	msg.msg_style = style;
	msg.msg = (char *) message;
	resp = NULL;

	return aconv->conv(1, &pmsg, &resp, aconv->appdata_ptr);
}

static char *
pwskip(register char *p)
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

struct passwd *
interpret(struct passwd *pwent, char *line)
{
	register char	*p = line;

	pwent->pw_passwd = "*";
	pwent->pw_uid = 0;
	pwent->pw_gid = 0;
	pwent->pw_gecos = "";
	pwent->pw_dir = "";
	pwent->pw_shell = "";
#ifndef __NeXT__
	pwent->pw_change = 0;
	pwent->pw_expire = 0;
	pwent->pw_class = "";
#endif

	/* line without colon separators is no good, so ignore it */
	if(!strchr(p, ':'))
		return(NULL);

	pwent->pw_name = p;
	p = pwskip(p);
	pwent->pw_passwd = p;
	p = pwskip(p);
	pwent->pw_uid = (uid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gid = (gid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gecos = p;
	p = pwskip(p);
	pwent->pw_dir = p;
	p = pwskip(p);
	pwent->pw_shell = p;
	while (*p && *p != '\n')
		p++;
	*p = '\0';
	return (pwent);
}


static struct passwd *
ypgetpwnam(char *nam, char *domain)
{
	static struct passwd pwent;
	char *val;
	int reason, vallen;
	static char *__yplin = NULL;

	reason = yp_match(domain, "passwd.byname", nam, strlen(nam),
	    &val, &vallen);
	switch(reason) {
	case 0:
		break;
	default:
		return (NULL);
		break;
	}
	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	__yplin = (char *)malloc(vallen + 1);
	strcpy(__yplin, val);
	free(val);

	return(interpret(&pwent, __yplin));
}

#ifdef __NeXT__
struct pam_module _pam_nis_modstruct = {
        "pam_nis",
        PAM_SM_AUTH_ENTRY,
        PAM_SM_SETCRED_ENTRY,
        PAM_SM_ACCOUNT_ENTRY,
        PAM_SM_OPEN_SESSION_ENTRY,
        PAM_SM_CLOSE_SESSION_ENTRY,
        PAM_SM_PASSWORD_ENTRY
};
#else
PAM_MODULE_ENTRY("pam_nis");
#endif /* __NeXT__ */
