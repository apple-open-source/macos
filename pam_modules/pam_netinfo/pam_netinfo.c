/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 * Portions Copyright (c) 1998-2001 PADL Software Pty Ltd. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/* pam_netinfo.c created by lukeh on Tue 13-Jun-2000 */

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
#ifdef __NeXT__
#include <mach/port.h>
#include "lookup.h"
#else
#include <mach/mach_port.h>
#include <netinfo/lookup.h>
#endif /* __NeXT__ */

#define PAM_SM_PASSWORD
#include <pam/pam_modules.h>
#include <pam/_pam_macros.h>
#include <pam/pam_mod_misc.h>

#define OLD_PASSWORD_PROMPT "Enter login(NetInfo) password:"
#define NEW_PASSWORD_PROMPT "New password:"
#define AGAIN_PASSWORD_PROMPT "Retype new password:"

static int      sendConversationMessage(struct pam_conv * aconv, const char *message, int style, int *options);
static void    *domain_for_user(char *uname, char *locn, ni_id * dir);
static int      sys_ismyaddress(unsigned long addr);
static int      is_root_on_master(void *d);
static int      secure_passwords();
static int	password_lifetime();
static void     parse_server_tag(char *str, struct sockaddr_in * server, char **t);
static int	netinfo2PamStatus(int nistatus);
static char    *saltchars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

#ifdef __NeXT__
extern port_t _lookupd_port(port_t);
#else
extern mach_port_t _lookupd_port(mach_port_t);
#endif /* __NeXT__ */

int pam_test_option(int *options, int tst, void * foo)
{
	return *options & tst;
}

/*
 * Change a user's password in NetInfo.
 */
PAM_EXTERN
int 
pam_sm_chauthtok(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
	char           *oldHash, *newHash;
	char           *oldPassword = NULL, *newPassword = NULL;
	void           *d;
	int             status, isroot, tries, maxTries;
	int		options = 0;
	int		amChangingExpiredPassword;
	ni_id           dir;
	ni_proplist     pl;
	ni_property     p;
	ni_namelist     nl;
	int             ni_uid, uid, secure, minlen, lifetime;
	ni_index        where;
	struct pam_conv *appconv;
	struct pam_message msg, *pmsg;
	struct pam_response *resp;
	const char     *cmiscptr = NULL;
	char           *uname;
	char            salt[9];
	int i;

	amChangingExpiredPassword = flags & PAM_CHANGE_EXPIRED_AUTHTOK;

	status = pam_get_item(pamh, PAM_CONV, (void **) &appconv);
	if (status != PAM_SUCCESS)
		return status;

	status = pam_get_item(pamh, PAM_USER, (void **) &uname);
	if (status != PAM_SUCCESS)
		return status;

	if (uname == NULL)
		return PAM_USER_UNKNOWN;

	status = pam_get_item(pamh, PAM_OLDAUTHTOK, (void **) &oldPassword);
	if (status != PAM_SUCCESS) {
		return status;
	}

	if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) ||
	    pam_test_option(&options, PAM_OPT_TRY_FIRST_PASS, NULL))
	{
		if (pam_get_item(pamh, PAM_AUTHTOK, (void **) &newPassword) != PAM_SUCCESS)
			newPassword = NULL;

		if (pam_test_option(&options, PAM_OPT_USE_FIRST_PASS, NULL) && newPassword == NULL)
			return PAM_AUTHTOK_RECOVER_ERR;
	}
	d = domain_for_user(uname, NULL, &dir);
	if (d == (void *) NULL)
	{
		syslog(LOG_ERR, "user %s not found in NetInfo", uname);
		return PAM_USER_UNKNOWN;
	}

	/*
	 * These should be configurable in NetInfo.
	 */
	secure = secure_passwords();
	maxTries = secure ? 3 : 5;
	minlen = secure ? 8 : 5;

	/*
         * Read the passwd and uid from NetInfo.
         */
	status = ni_lookupprop(d, &dir, "passwd", &nl);
	if (status == NI_NOPROP)
		nl.ni_namelist_len = 0;
	else if (status != NI_OK)
	{
		ni_free(d);
		syslog(LOG_ERR, "NetInfo read failed: %s", ni_error(status));
		return netinfo2PamStatus(status);
	}
	oldHash = NULL;
	if (nl.ni_namelist_len > 0)
		oldHash = nl.ni_namelist_val[0];

	status = ni_lookupprop(d, &dir, "uid", &nl);
	if (status != NI_OK)
	{
		ni_free(d);
		syslog(LOG_ERR, "NetInfo read failed: %s", ni_error(status));
		return netinfo2PamStatus(status);
	}
	ni_uid = -2;
	if (nl.ni_namelist_len > 0)
		ni_uid = atoi(nl.ni_namelist_val[0]);

	/*
         * See if I'm uid 0 on the master host for the user's NetInfo domain.
         */
	isroot = is_root_on_master(d);
	uid = getuid();
	if (isroot)
	{
		if (flags & PAM_PRELIM_CHECK)
		{
			/* Don't need old password. */
			return PAM_SUCCESS;
		}
	}
	else if (uid != ni_uid)
	{
		ni_free(d);
		return PAM_PERM_DENIED;
	}

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
					sendConversationMessage(appconv, "NetInfo password incorrect", PAM_ERROR_MSG, &options);
				else
					sendConversationMessage(appconv, "NetInfo password incorrect: try again", PAM_ERROR_MSG, &options);
			}
			else
			{
				ni_free(d);
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
				ni_free(d);
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
					ni_free(d);
					return PAM_AUTHTOK_RECOVER_ERR;
				}
				else
				{
					sendConversationMessage(appconv, "NetInfo password incorrect: try again", PAM_ERROR_MSG, &options);
				}
			}
		}

		if (oldPassword == NULL)
		{
			status = PAM_MAXTRIES;
		}
		(void) pam_set_item(pamh, PAM_OLDAUTHTOK, oldPassword);
		ni_free(d);

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
			ni_free(d);
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
			if (isroot == 0)
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
			ni_free(d);
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
				ni_free(d);
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
					ni_free(d);
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
		ni_free(d);
		return PAM_MAXTRIES;
	}
	/*
         * Lock onto the master server.
         */
	ni_needwrite(d, 1);

	/*
         * Authenticate if necessary
         */
	if (isroot == 0)
	{
		ni_setuser(d, uname);
		ni_setpassword(d, oldPassword);
	}
	/*
        * Create a random salt
        */
	srandom((int) time((time_t *) NULL));
	salt[0] = saltchars[random() % strlen(saltchars)];
	salt[1] = saltchars[random() % strlen(saltchars)];
	salt[2] = '\0';
	newHash = crypt(newPassword, salt);

	/*
         * Change the password in NetInfo.
         */
	status = ni_read(d, &dir, &pl);
	if (status != NI_OK)
	{
		ni_free(d);
		syslog(LOG_ERR, "NetInfo read failed: %s", ni_error(status));
		return netinfo2PamStatus(status);
	}
	p.nip_name = "passwd";
	p.nip_val.ni_namelist_len = 1;
	p.nip_val.ni_namelist_val = (ni_name *) malloc(sizeof(ni_name));
	p.nip_val.ni_namelist_val[0] = newHash;

	where = ni_proplist_match(pl, p.nip_name, NULL);
	if (where == NI_INDEX_NULL)
		status = ni_createprop(d, &dir, p, NI_INDEX_NULL);
	else
		status = ni_writeprop(d, &dir, where, p.nip_val);

	if (status != NI_OK)
	{
		ni_free(d);
		syslog(LOG_ERR, "NetInfo write property \"passwd\" failed: %s", ni_error(status));
		return netinfo2PamStatus(status);
	}

	/*
	 * Now, update "change" property. If this fails, we've still
	 * updated the password... perhaps the user should be alerted
	 * of this.
	 */
	lifetime = password_lifetime();
	if (lifetime > 0)
	{
		struct timeval tp;
		char change[64];

		where = ni_proplist_match(pl, "change", NULL);

		gettimeofday(&tp, NULL);
		tp.tv_sec += lifetime;

		snprintf(change, sizeof(change), "%ld", tp.tv_sec);

		p.nip_name = "change";
		p.nip_val.ni_namelist_len = 1;
		p.nip_val.ni_namelist_val[0] = change;

		if (where == NI_INDEX_NULL)
			status = ni_createprop(d, &dir, p, NI_INDEX_NULL);
		else
			status = ni_writeprop(d, &dir, where, p.nip_val);

		if (status != NI_OK)
		{
			ni_free(d);
			syslog(LOG_ERR, "NetInfo write property \"change\" failed: %s", ni_error(status));
			return netinfo2PamStatus(status);
		}
	}

	free(p.nip_val.ni_namelist_val);

	ni_free(d);

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

/* Marc's NetInfo stuff begins. */
static int
sys_ismyaddress(unsigned long addr)
{
	struct ifconf   ifc;
	struct ifreq   *ifr;
	char            buf[1024];	/* XXX */
	int             offset;
	int             sock;
	struct sockaddr_in *sin;

	if (addr == htonl(INADDR_LOOPBACK))
		return 1;

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0)
		return 0;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *) &ifc) < 0)
	{
		close(sock);
		return 0;
	}
	offset = 0;

	while (offset <= ifc.ifc_len)
	{
		ifr = (struct ifreq *) (ifc.ifc_buf + offset);
#ifdef __NeXT__
		offset += IFNAMSIZ;
#else
		offset += IFNAMSIZ + ifr->ifr_addr.sa_len;
#endif /* __NeXT__ */

		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		if (ioctl(sock, SIOCGIFFLAGS, ifr) < 0)
			continue;

		sin = (struct sockaddr_in *) & ifr->ifr_addr;
		if ((ifr->ifr_flags & IFF_UP) &&
		    (!(ifr->ifr_flags & IFF_LOOPBACK)) &&
		    (sin->sin_addr.s_addr == addr))
		{
			close(sock);
			return 1;
		}
	}

	close(sock);
	return 0;
}

static int
is_root_on_master(void *d)
{
	int             uid;
	char            myhostname[MAXHOSTNAMELEN + 1];
	char           *p;
	ni_index        where;
	ni_proplist     pl;
	int             status;
	ni_id           dir;
	struct sockaddr_in addr;
	char           *tag;

	uid = getuid();
	if (uid != 0)
		return 0;

	gethostname(myhostname, MAXHOSTNAMELEN);
	p = strchr(myhostname, '.');
	if (p != NULL)
		*p = '\0';

	status = ni_root(d, &dir);
	if (status != NI_OK)
		return 0;

	status = ni_read(d, &dir, &pl);
	if (status != NI_OK)
		return 0;

	where = ni_proplist_match(pl, "master", NULL);
	if (where == NI_INDEX_NULL)
	{
		ni_proplist_free(&pl);
		return 0;
	}
	if (pl.ni_proplist_val[where].nip_val.ni_namelist_len == 0)
	{
		ni_proplist_free(&pl);
		fprintf(stderr, "No value for NetInfo master property\n");
		return 0;
	}
	p = strchr(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], '/');
	if (p != NULL)
		*p = '\0';

	p = strchr(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], '.');
	if (p != NULL)
		*p = '\0';

	if (!strcmp(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], myhostname))
	{
		ni_proplist_free(&pl);
		return 1;
	}
	if (!strcmp(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], "localhost"))
	{
		ni_proplist_free(&pl);
		ni_addrtag(d, &addr, &tag);
		if (sys_ismyaddress(addr.sin_addr.s_addr))
			return 1;
	}
	ni_proplist_free(&pl);
	return 0;
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
password_lifetime()
{
	void           *d, *d1;
	int             status;
	ni_id           dir;
	ni_namelist     nl;

	status = ni_open(NULL, ".", &d);
	while (status == NI_OK)
	{
		status = ni_pathsearch(d, &dir, "/users");
		if (status == NI_OK)
		{
			status = ni_lookupprop(d, &dir, "password_lifetime", &nl);
			if (status == NI_OK)
			{
				if (nl.ni_namelist_val[0] != NULL)
				{
					ni_free(d);
					/* free namelist? */
					return atoi(nl.ni_namelist_val[0]);
				}
			}
		}
		d1 = d;
		status = ni_open(d1, "..", &d);
		ni_free(d1);
	}

	return -1;
}

static void
parse_server_tag(char *str, struct sockaddr_in * server, char **t)
{
	/* utility to parse a server/tag string */

	int             len, i;
	char           *host, *tag, *slash;
	struct hostent *hent;

	len = strlen(str);

	/* find the "/" character */
	slash = index(str, '/');

	/* check to see if the "/" is missing */
	if (slash == NULL)
	{
		fprintf(stderr, "incorrect format \"%s\" for domain name\n", str);
		exit(1);
	}
	/* find the location of the '/' */
	i = slash - str;

	/* check if host string is empty */
	if (i == 0)
	{
		fprintf(stderr, "incorrect format \"%s\" for domain name\n", str);
		fprintf(stderr, "no server name specified\n");
		exit(1);
	}
	/* check if tag string is empty */
	if (i == (len - 1))
	{
		fprintf(stderr, "incorrect format \"%s\" for domain name\n", str);
		fprintf(stderr, "no tag specified\n");
		exit(1);
	}
	/* allocate some space for the host and tag */
	host = (char *) malloc(i + 1);
	*t = (char *) malloc(len - i);
	tag = *t;

	/* copy out the host */
	strncpy(host, str, i);
	host[i] = '\0';

	/* copy out the tag */
	strcpy(tag, slash + 1);

	/* try interpreting the host portion as an address */
	server->sin_addr.s_addr = inet_addr(host);

	if (server->sin_addr.s_addr == -1)
	{
		/* This isn't a valid address.  Is it a known hostname? */
		hent = gethostbyname(host);
		if (hent != NULL)
		{
			/* found a host with that name */
			bcopy(hent->h_addr, &server->sin_addr, hent->h_length);
		} else
		{
			fprintf(stderr, "Can't find address for %s\n", host);
			free(host);
			free(tag);
			exit(1);
		}
	}
	free(host);
}

static void    *
domain_for_user(char *uname, char *locn, ni_id * dir)
{
	char           *upath;
	int             status;
	void           *d, *d1;
	struct sockaddr_in server;
	char           *tag;
	int             bytag;

	if (uname == NULL)
		return NULL;

	/*
         * Find the user in NetInfo.
         */
	upath = malloc(8 + strlen(uname));
	sprintf(upath, "/users/%s", uname);

	if (locn != NULL)
	{
		bytag = 1;

		if (locn[0] == '/')
			bytag = 0;
		else if (!strncmp(locn, "./", 2))
			bytag = 0;
		else if (!strncmp(locn, "../", 3))
			bytag = 0;

		if (bytag == 1)
		{
			parse_server_tag(locn, &server, &tag);
			d = ni_connect(&server, tag);
			if (d == (void *) NULL)
				return (void *) NULL;
		} else
			status = ni_open(NULL, locn, &d);
		status = ni_pathsearch(d, dir, upath);
		free(upath);

		if (status == NI_OK)
			return d;

		ni_free(d);
		return (void *) NULL;
	}
	status = ni_open(NULL, ".", &d);
	while (status == NI_OK)
	{
		status = ni_pathsearch(d, dir, upath);
		if (status == NI_OK)
			break;
		d1 = d;
		status = ni_open(d1, "..", &d);
		ni_free(d1);
	}

	free(upath);

	if (status == NI_OK)
		return d;
	return (void *) NULL;
}

/* PAM stuff begins */
static int
sendConversationMessage(struct pam_conv * aconv,
			const char *message, int style, int *options)
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

static int
netinfo2PamStatus(int nistatus)
{
	switch (nistatus) {
		case NI_OK:
			return PAM_SUCCESS;
		case NI_NOUSER:
			return PAM_USER_UNKNOWN;
		case NI_AUTHERROR:
			return PAM_AUTH_ERR;
		case NI_PERM:
			return PAM_PERM_DENIED;
		default:
			return PAM_SERVICE_ERR;
	}

	/* NOT REACHED */
	return PAM_SERVICE_ERR;
}

#ifdef PAM_STATIC
#ifdef __NeXT__
struct pam_module _pam_netinfo_modstruct = {
        "pam_netinfo",
        PAM_SM_AUTH_ENTRY,
        PAM_SM_SETCRED_ENTRY,
        PAM_SM_ACCOUNT_ENTRY,
        PAM_SM_OPEN_SESSION_ENTRY,
        PAM_SM_CLOSE_SESSION_ENTRY,
        PAM_SM_PASSWORD_ENTRY
};
#else
PAM_MODULE_ENTRY("pam_netinfo");
#endif /* __NeXT__ */
#endif

