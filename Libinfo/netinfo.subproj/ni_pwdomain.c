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
 * Copyright (C) 1990 by NeXT, Inc. All rights reserved.
 */

/*
 * ni_pwdomain function: present working domain for a netinfo handle
 *
 * usage:
 * 	ni_status ni_pwdomain(void *ni, ni_name *buf)
 *
 * pwd is returned in buf, which can be freed with ni_name_free
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <netinfo/ni.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "sys_interfaces.h"

extern char *inet_ntoa();

static const char NAME_NAME[] = "name";
static const char NAME_MACHINES[] = "machines";
static const char NAME_IP_ADDRESS[] = "ip_address";
static const char NAME_SERVES[] = "serves";
static const char NAME_UNKNOWN[] = "###UNKNOWN###";

static ni_name
escape_domain(ni_name name)
{
	int extra;
	char *p;
	char *s;
	ni_name newname;

	extra = 0;
	for (p = name; *p; p++)
	{
		if ((*p == '/') || (*p == '\\')) extra++;
	}
	
	newname = malloc(strlen(name) + extra + 1);
	s = newname;
	for (p = name; *p; p++)
	{
		if ((*p == '/') || (*p == '\\')) *s++ = '\\';
		*s++ = *p;
	}

	*s = 0;
	return newname;
	
}

static char *
finddomain(void *ni, struct in_addr addr, ni_name tag)
{
	ni_id nid;
	ni_idlist idl;
	ni_namelist nl;
	ni_index i;
	ni_name slash;
	ni_name domain;
	ni_status status;

	status = ni_root(ni, &nid);
	if (status != NI_OK) return NULL;

	status = ni_lookup(ni, &nid, NAME_NAME, NAME_MACHINES, &idl);
	if (status != NI_OK) return NULL;

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookup(ni, &nid, NAME_IP_ADDRESS, inet_ntoa(addr), &idl);
	if (status != NI_OK) return NULL;

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookupprop(ni, &nid, NAME_SERVES, &nl);
	if (status != NI_OK) return NULL;

	for (i = 0; i < nl.ninl_len; i++)
	{
		slash = rindex(nl.ninl_val[i], '/');
		if (slash == NULL) continue;

		if (ni_name_match(slash + 1, tag))
		{
			*slash = 0;
			domain = escape_domain(nl.ninl_val[i]);
			ni_namelist_free(&nl);
			return domain;
		}
	}

	ni_namelist_free(&nl);

	return NULL;
}

static char *
ni_domainof(void *ni, void *parent)
{
	struct sockaddr_in addr;
	ni_name tag;
	ni_name dom;
	ni_status status;
	interface_list_t *ilist;
	int i;

	status = ni_addrtag(ni, &addr, &tag);
	if (status != NI_OK) return ni_name_dup(NAME_UNKNOWN);
	
	dom = finddomain(parent, addr.sin_addr, tag);
	if (dom != NULL)
	{
		ni_name_free(&tag);
		return dom;
	}

	ilist = sys_interfaces();
	if (ilist == NULL) return ni_name_dup(NAME_UNKNOWN);
	if (sys_is_my_address(ilist, &(addr.sin_addr)))
	{
		/* Try all my non-loopback interfaces */
		for (i = 0; i < ilist->count; i++)
		{
			if (ilist->interface[i].addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
	
			addr.sin_addr.s_addr = ilist->interface[i].addr.s_addr;
			dom = finddomain(parent, addr.sin_addr, tag);
			if (dom != NULL)
			{
				ni_name_free(&tag);
				sys_interfaces_release(ilist);
				return dom;
			}
		}
	}
	sys_interfaces_release(ilist);

	dom = malloc(strlen(tag) + 256);
	sprintf(dom, "%s@%s", tag, inet_ntoa(addr.sin_addr.s_addr));
	ni_name_free(&tag);
	return dom;
}

static ni_status
_ni_pwdomain(void *ni, ni_name *buf)
{
	void *nip;
	ni_status status;
	int len;
	char *dom;

	/* Open domain name */
	nip = ni_new(ni, "..");
	if (nip == NULL)
	{
		(*buf) = malloc(2);
		(*buf)[0] = 0;
		return NI_OK;
	}

	/* Get parent's name */
	status = _ni_pwdomain(nip, buf);
	if (status != NI_OK) return status;

	/* Get my name relative to my parent */
	dom = ni_domainof(ni, nip);

	/* Append my relative name to my parent's name */
	len = strlen(*buf);
	*buf = realloc(*buf, len + 1 + strlen(dom) + 1);
	(*buf)[len] = '/';
	strcpy(&(*buf)[len + 1], dom);
	ni_name_free(&dom);
	ni_free(nip);

	return NI_OK;
}

/*
 * Just call the recursive ni_pwdomain above, and then fix for case of root
 * domain or error
 */
ni_status
ni_pwdomain(void *ni, ni_name *buf)
{
	ni_status status;

	*buf = NULL;
	status = _ni_pwdomain(ni, buf);
	if (status != NI_OK)
	{
		if (*buf != NULL) ni_name_free(buf);
		return status;
	}

	if ((*buf)[0] == 0)
	{
		(*buf)[0] = '/';
		(*buf)[1] = 0;
	}

	return NI_OK;
}
