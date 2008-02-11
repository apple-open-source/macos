/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
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

extern void checkpasswd(char *, char *);

#if 0
static int
sys_ismyaddress(unsigned long addr)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[1024]; /* XXX */
	int offset;
	int sock;
	struct sockaddr_in *sin;

	if (addr == htonl(INADDR_LOOPBACK)) return 1;

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock < 0) return 0;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		close(sock);
		return 0;
	}

	offset = 0;

	while (offset <= ifc.ifc_len)
	{
		ifr = (struct ifreq *)(ifc.ifc_buf + offset);
		offset += IFNAMSIZ + ifr->ifr_addr.sa_len;

		if (ifr->ifr_addr.sa_family != AF_INET) continue;
		if (ioctl(sock, SIOCGIFFLAGS, ifr) < 0) continue;

		sin = (struct sockaddr_in *)&ifr->ifr_addr;
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
	int uid;
	char myhostname[MAXHOSTNAMELEN + 1];
	char *p;
	ni_index where;
	ni_proplist	pl;
	int status;
	ni_id dir;
	struct sockaddr_in addr;
	char *tag;

	uid = getuid();
	if (uid != 0) return 0;

	gethostname(myhostname, MAXHOSTNAMELEN);
	p = strchr(myhostname, '.');
	if (p != NULL) *p = '\0';

	status = ni_root(d, &dir);
	if (status != NI_OK) return 0;

	status = ni_read(d, &dir, &pl);
	if (status != NI_OK) return 0;

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
	if (p != NULL) *p = '\0';
	
	p = strchr(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], '.');
	if (p != NULL) *p = '\0';
	
	if (!strcmp(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], myhostname))
	{
		ni_proplist_free(&pl);
		return 1;
	}

	if (!strcmp(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], "localhost"))
	{
		ni_proplist_free(&pl);
		ni_addrtag(d, &addr, &tag);
		if (sys_ismyaddress(addr.sin_addr.s_addr)) return 1;
	}

	ni_proplist_free(&pl);
	return 0;
}

static int
secure_passwords()
{
	void *d, *d1;
	int status;
	ni_index where;
	ni_id dir;
	ni_namelist nl;

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
#endif /* 0 */

static void
parse_server_tag(char *str, struct sockaddr_in *server, char **t)
{
	/* utility to parse a server/tag string */

	int len, i;
	char *host, *tag, *slash;
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
	host = (char *)malloc(i + 1);
	*t = (char *)malloc(len - i);
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
		}
		else
		{
			fprintf(stderr, "Can't find address for %s\n", host);
			free(host);
			free(tag);
			exit(1);
		}
   }

	free(host);
}

static void *
domain_for_user(char *uname, char *locn, ni_id *dir)
{
	char *upath;
	int status;
	void *d, *d1;
	struct sockaddr_in server;
	char *tag;
	int bytag;

	/*
	 * Find the user in NetInfo.
	 */
	upath = malloc(8 + strlen(uname));
	sprintf(upath, "/users/%s", uname);

	if (locn != NULL)
	{
		bytag = 1;

		if (locn[0] == '/') bytag = 0;
		else if (!strncmp(locn, "./", 2)) bytag = 0;
		else if (!strncmp(locn, "../", 3)) bytag = 0;

		if (bytag == 1)
		{
			parse_server_tag(locn, &server, &tag);
			d = ni_connect(&server, tag);
			if (d == (void *)NULL) return (void *)NULL;
		}
		else status = ni_open(NULL, locn, &d);
		status = ni_pathsearch(d, dir, upath);
		free(upath);

		if (status == NI_OK) return d;

		ni_free(d);
		return (void *)NULL;
	}

	status = ni_open(NULL, ".", &d);
	while (status == NI_OK)
	{
		status = ni_pathsearch(d, dir, upath);
		if (status == NI_OK) break;
		d1 = d;
		status = ni_open(d1, "..", &d);
		ni_free(d1);
	}

	free(upath);

	if (status == NI_OK) return d;
	return (void *)NULL;
}

int
netinfo_check_passwd(char *uname, char *locn)
{
	char *oldpw;
	void *d;
	int status;
	ni_id dir;
	ni_namelist	nl;

	d = domain_for_user(uname, locn, &dir);
	if (d == (void *)NULL)
	{
		fprintf(stderr, "user %s not found in NetInfo\n", uname);
		exit(1);
	}

	/*
	 * Read the passwd and uid from NetInfo.
	 */
	status = ni_lookupprop(d, &dir, "passwd", &nl);
	if (status == NI_NOPROP) nl.ni_namelist_len = 0;
	else if (status != NI_OK)
	{
		ni_free(d);
		fprintf(stderr, "NetInfo read failed: %s\n", ni_error(status));
		exit(1);
	}

	oldpw = NULL;
	if (nl.ni_namelist_len > 0) oldpw = nl.ni_namelist_val[0];

	checkpasswd(uname, oldpw);
	ni_free(d);
	return (0);
}
