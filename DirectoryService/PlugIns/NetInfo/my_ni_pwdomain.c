/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header my_ni_pwdomain
 * Fast version of ni_pwdomain that does not download all
 * machines records of parent NI domains since it does not
 * use ni_new or ni_open
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
#include <ifaddrs.h>

extern char *inet_ntoa();

static const char NAME_NAME[] = "name";
static const char NAME_MACHINES[] = "machines";
static const char NAME_IP_ADDRESS[] = "ip_address";
static const char NAME_SERVES[] = "serves";
static const char NAME_UNKNOWN[] = "###UNKNOWN###";

#define NI_READ_TIMEOUT 10

/*
 * WARNING!
 * THIS IS A PRIVATE DATA STRUCTURE THAT MAY CHANGE SOME DAY.
 * This is actually part of the NetInfo component but is included here
 * so as not to require a link to NetInfo itself since using ni_rparent.
 * WARNING!
 */
typedef struct ni_private {
	int naddrs;				/* number of addresses */
	struct in_addr *addrs;	/* addresses of servers - network byte order */
	int whichwrite;			/* which one of the above is the master */
	ni_name *tags;			/* tags of servers */
	int pid;				/* pid, to detect forks */
	int tsock;				/* tcp socket */
	int tport;				/* tcp local port name - host byte order */
	CLIENT *tc;				/* tcp client */
	long tv_sec;			/* timeout for this call */
	long rtv_sec;			/* read timeout - 0 if default */
	long wtv_sec;			/* write timeout - 0 if default */
	int abort;				/* abort on timeout? */
	int needwrite;			/* need to lock writes? */
	int uid;				/* user id */
	ni_name passwd;			/* password */
} ni_private;

#define NIP(ni) ((ni_private *)(ni))

static ni_status
my_rparent(void *domain, struct sockaddr_in *addr, char **tag)
{
	CLIENT *c;
	ni_rparent_res *rpres;

	if (domain == NULL) return NI_INVALIDDOMAIN;
	if (addr == NULL) return NI_NONAME;
	if (tag == NULL) return NI_NONAME;

	c = NIP(domain)->tc;
	if (c == NULL) return NI_SYSTEMERR;

	/* send the NI_RPARENT */
	rpres = _ni_rparent_2((void *)0, c);

	if (rpres == NULL) return NI_SYSTEMERR;
	if (rpres->status != NI_OK) return rpres->status;

	/* copy the results into the caller's parameters */
	addr->sin_addr.s_addr = htonl(rpres->ni_rparent_res_u.binding.addr);
	*tag = malloc(strlen(rpres->ni_rparent_res_u.binding.tag) + 1);
	strcpy(*tag, rpres->ni_rparent_res_u.binding.tag);
	/*clean up the tag */
	free(rpres->ni_rparent_res_u.binding.tag);

	return NI_OK;
}

static void *
_ni_connect_parent(void *ni)
{
	struct sockaddr_in addr;
	char *tag;
	ni_status status;
	void *p;
	ni_id nid;

	if (ni == NULL) return NULL;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	tag = NULL;

	status = my_rparent(ni, &addr, &tag);
	if (status != NI_OK) return NULL;

	p = ni_connect(&addr, tag);
	if (tag != NULL) free(tag);

	nid.nii_object = 0;
	nid.nii_instance = 0;

	ni_setreadtimeout(p, NI_READ_TIMEOUT);
	ni_setabort(ni, 1);

	status = ni_self(p, &nid);
	if (status != NI_OK)
	{
		ni_free(p);
		return NULL;
	}

	return p;
}

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

static int
sys_is_my_address(struct in_addr *a)
{
	struct ifaddrs *ifa, *p;

	if (getifaddrs(&ifa) < 0) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		if (a->s_addr == ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr)
		{
			freeifaddrs(ifa);
			return 1;
		}
	}

	freeifaddrs(ifa);
	return 0;
}

static char *
ni_domainof(void *ni, void *parent)
{
	struct sockaddr_in addr;
	ni_name tag;
	ni_name dom;
	ni_status status;
	struct ifaddrs *ifa, *p;

	status = ni_addrtag(ni, &addr, &tag);
	if (status != NI_OK) return ni_name_dup(NAME_UNKNOWN);
	
	dom = finddomain(parent, addr.sin_addr, tag);
	if (dom != NULL)
	{
		ni_name_free(&tag);
		return dom;
	}

	if (getifaddrs(&ifa) < 0) return ni_name_dup(NAME_UNKNOWN);

	if (sys_is_my_address(&(addr.sin_addr)))
	{		
		/* Try all my non-loopback interfaces */
		for (p = ifa; p != NULL; p = p->ifa_next)
		{
			if (p->ifa_addr == NULL) continue;
			if ((p->ifa_flags & IFF_UP) == 0) continue;
			if (p->ifa_addr->sa_family != AF_INET) continue;
			addr.sin_addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;

			if (addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;
	
			dom = finddomain(parent, addr.sin_addr, tag);
			if (dom != NULL)
			{
				ni_name_free(&tag);
				freeifaddrs(ifa);
				return dom;
			}
		}
	}
	freeifaddrs(ifa);

	dom = malloc(strlen(tag) + 256);
	sprintf(dom, "%s@%s", tag, inet_ntoa(addr.sin_addr));
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
	nip = _ni_connect_parent(ni);
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
my_ni_pwdomain(void *ni, ni_name *buf)
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
