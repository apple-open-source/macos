/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
/*
 * user information (passwd) lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <pwd.h>
#include <netinet/in.h>

#include "_lu_types.h"
#include "lookup.h"
#include "lu_utils.h"
#include "lu_overrides.h"

static lookup_state pw_state = LOOKUP_CACHE;
static struct passwd global_pw;
static int global_free = 1;
static char *pw_data = NULL;
static unsigned pw_datalen;
static int pw_nentries;
static int pw_start = 1;
static XDR pw_xdr;

static void
freeold(void)
{
	if (global_free == 1) return;

	free(global_pw.pw_name);
	free(global_pw.pw_passwd);
	free(global_pw.pw_class);
	free(global_pw.pw_gecos);
	free(global_pw.pw_dir);
	free(global_pw.pw_shell);

	global_free = 1;
}

static void
convert_pw(_lu_passwd *lu_pw)
{
	freeold();

	global_pw.pw_name = strdup(lu_pw->pw_name);
	global_pw.pw_passwd = strdup(lu_pw->pw_passwd);
	global_pw.pw_uid = lu_pw->pw_uid;
	global_pw.pw_gid = lu_pw->pw_gid;
	global_pw.pw_change = lu_pw->pw_change;
	global_pw.pw_class = strdup(lu_pw->pw_class);
	global_pw.pw_gecos = strdup(lu_pw->pw_gecos);
	global_pw.pw_dir = strdup(lu_pw->pw_dir);
	global_pw.pw_shell = strdup(lu_pw->pw_shell);
	global_pw.pw_expire = lu_pw->pw_expire;

	global_free = 0;
}

static struct passwd *
lu_getpwuid(int uid)
{
	unsigned datalen;
	_lu_passwd_ptr lu_pw;
	XDR xdr;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getpwuid_A", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	uid = htonl(uid);
	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)&uid, 1, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		return (NULL);
	}

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	lu_pw = NULL;
	if (!xdr__lu_passwd_ptr(&xdr, &lu_pw) || (lu_pw == NULL))
	{
		xdr_destroy(&xdr);
		return (NULL);
	}

	xdr_destroy(&xdr);

	convert_pw(lu_pw);
	xdr_free(xdr__lu_passwd_ptr, &lu_pw);
	return (&global_pw);
}

static struct passwd *
lu_getpwnam(const char *name)
{
	unsigned datalen;
	char namebuf[_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT];
	XDR outxdr;
	XDR inxdr;
	_lu_passwd_ptr lu_pw;
	static int proc = -1;
	unit lookup_buf[MAX_INLINE_UNITS];

	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "getpwnam_A", &proc) != KERN_SUCCESS)
		{
			return (NULL);
		}
	}

	xdrmem_create(&outxdr, namebuf, sizeof(namebuf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &name))
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}
	
	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, (unit *)namebuf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return (NULL);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&inxdr, lookup_buf, datalen, 
		XDR_DECODE);
	lu_pw = NULL;
	if (!xdr__lu_passwd_ptr(&inxdr, &lu_pw) || (lu_pw == NULL))
	{
		xdr_destroy(&inxdr);
		return (NULL);
	}

	xdr_destroy(&inxdr);

	convert_pw(lu_pw);
	xdr_free(xdr__lu_passwd_ptr, &lu_pw);
	return (&global_pw);
}

#ifdef notdef
static int
lu_putpwpasswd(char *login, char *old_passwd, char *new_passwd)
{
	unsigned datalen;
	int changed;
	XDR xdr;
	static int proc = -1;
	char output_buf[3 * (_LU_MAXLUSTRLEN + BYTES_PER_XDR_UNIT)];
	unit lookup_buf[MAX_INLINE_UNITS];
	XDR outxdr;
	
	if (proc < 0)
	{
		if (_lookup_link(_lu_port, "putpwpasswd", &proc) != KERN_SUCCESS)
		{
			return (0);
		}
	}

	xdrmem_create(&outxdr, output_buf, sizeof(output_buf), XDR_ENCODE);
	if (!xdr__lu_string(&outxdr, &login) ||
	    !xdr__lu_string(&outxdr, &old_passwd) ||
	    !xdr__lu_string(&outxdr, &new_passwd))
	{
		xdr_destroy(&outxdr);
		return (0);
	}

	datalen = MAX_INLINE_UNITS;
	if (_lookup_one(_lu_port, proc, output_buf,
		xdr_getpos(&outxdr) / BYTES_PER_XDR_UNIT, lookup_buf, &datalen)
		!= KERN_SUCCESS)
	{
		xdr_destroy(&outxdr);
		return (0);
	}

	xdr_destroy(&outxdr);

	datalen *= BYTES_PER_XDR_UNIT;
	xdrmem_create(&xdr, lookup_buf, datalen, XDR_DECODE);
	if (!xdr_int(&xdr, &changed))
	{
		xdr_destroy(&xdr);
		return (0);
	}

	xdr_destroy(&xdr);

	return (changed);
}
#endif

static void
lu_endpwent(void)
{
	pw_nentries = 0;
	if (pw_data != NULL)
	{
		freeold();
		vm_deallocate(mach_task_self(), (vm_address_t)pw_data, pw_datalen);
		pw_data = NULL;
	}
}

static int
lu_setpwent(void)
{
	lu_endpwent();
	pw_start = 1;
	return (1);
}

static struct passwd *
lu_getpwent()
{
	static int proc = -1;
	_lu_passwd lu_pw;

	if (pw_start == 1)
	{
		pw_start = 0;

		if (proc < 0)
		{
			if (_lookup_link(_lu_port, "getpwent_A", &proc) != KERN_SUCCESS)
			{
				lu_endpwent();
				return (NULL);
			}
		}

		if (_lookup_all(_lu_port, proc, NULL, 0, &pw_data, &pw_datalen)
			!= KERN_SUCCESS)
		{
			lu_endpwent();
			return (NULL);
		}

#ifdef NOTDEF
/* NOTDEF because OOL buffers are counted in bytes with untyped IPC */
		pw_datalen *= BYTES_PER_XDR_UNIT;
#endif

		xdrmem_create(&pw_xdr, pw_data, pw_datalen,
			XDR_DECODE);
		if (!xdr_int(&pw_xdr, &pw_nentries))
		{
			xdr_destroy(&pw_xdr);
			lu_endpwent();
			return (NULL);
		}
	}

	if (pw_nentries == 0)
	{
		xdr_destroy(&pw_xdr);
		lu_endpwent();
		return (NULL);
	}

	bzero(&lu_pw, sizeof(lu_pw));
	if (!xdr__lu_passwd(&pw_xdr, &lu_pw))
	{
		xdr_destroy(&pw_xdr);
		lu_endpwent();
		return (NULL);
	}

	pw_nentries--;
	convert_pw(&lu_pw);
	xdr_free(xdr__lu_passwd, &lu_pw);
	return (&global_pw);
}

static char *loginName = NULL;
static uid_t loginUid = -1;

extern char *getlogin(void);

struct passwd *
getpwuid(uid_t uid)
{
    if (uid != 0) {
        if (loginName == NULL) {
            char *l = getlogin();
            if (l != NULL) {
                struct passwd *p = getpwnam(l);
                if (p != NULL) {
                    loginUid = p->pw_uid;
                    loginName = l;
                }
            }
        }
        if (uid == loginUid) {
            LOOKUP1(lu_getpwnam, _old_getpwnam,  loginName, struct passwd);
        }
    }
    LOOKUP1(lu_getpwuid, _old_getpwuid,  uid, struct passwd);
}

struct passwd *
getpwnam(const char *name)
{
	LOOKUP1(lu_getpwnam, _old_getpwnam,  name, struct passwd);
}

#ifdef notdef
/*
 * putpwpasswd() is not supported with anything other than netinfo
 * right now.
 * old_passwd is clear text.
 * new_passwd is encrypted.
 */
#define _old_passwd(name, oldpass, newpass) 0
int
putpwpasswd(char *login, char *old_passwd, char *new_passwd)
{
	if (_lu_running()) return (lu_putpwpasswd(login, old_passwd, new_passwd));
	return (old_passwd(login, old_passwd, new_passwd));
}
#endif

struct passwd *
getpwent(void)
{
	GETENT(lu_getpwent, _old_getpwent, &pw_state, struct passwd);
}

int
setpwent(void)
{
	INTSETSTATEVOID(lu_setpwent, _old_setpwent, &pw_state);
}

void
endpwent(void)
{
	UNSETSTATE(lu_endpwent, _old_endpwent, &pw_state);
}
