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

/*
 * Copyright (c) 1997, Apple Computer Inc.
 * All rights reserved.
 *
 * Attempt to locate an LDAP server.
 *
 * 0. Try the environment.
 * 1. Try /locations/lookupd/agents/LDAPAgent in NetInfo.
 * 2. Try /etc/lookupd/agents/LDAPAgent in the filesystem.
 * 3. XXX use DNS SRV records.
 *
 * Luke Howard, December 1997.
 */


#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <netinfo/ni.h>
#include <NetInfo/config.h>
#ifdef _UNIX_BSD_43_
extern char *strdup(char *);
#endif

#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

#define GETCONF_SUCCESS			0
#define GETCONF_FAILED			(-1)

static int env_getconf(char **ldaphost, char **basedn, int *port);

/* NetInfo */
static int ni_getconf(char **ldaphost, char **basedn, int *port);
static ni_status ni_getConfiguration(void *ni, char **ldaphost, char **basedn, int *port);
#define NI_GETNAME_FIRST_VALUE		0
#define NI_GETNAME_ALL_VALUES		1
static int ni_getName(ni_namelist *nl, char **val, int getall);

#define MAX_NI_LEVELS 			256
#define NAME_NI_LDAP_DIRECTORY	"/locations/lookupd/agents/LDAPAgent"
#define NAME_HOST	"host"
#define NAME_SUFFIX	"suffix"
#define NAME_PORT	"port"

/* flat files */
static char *ff_getLineFromFile(FILE *fp);
static void ff_truncateString(char *string);
static int ff_parseLine(const char *data, char **key, char **val);
static int ff_getconf(char **ldaphost, char **basedn, int *port); 

#define NAME_FF_LDAP_DIRECTORY	"/etc/lookupd/agents/LDAPAgent/global"

/* Cover */
typedef int (*locator_t)(char **, char **, int *);

static locator_t locators[] = { env_getconf, ni_getconf, ff_getconf, NULL };
#ifdef notdef
static locator_t locators[] = { ff_getconf, ni_getconf, NULL };
#endif

/*
 * Open a new LDAP connection to the nearest LDAP server.
 * At the moment this looks in /locations/lookupd/agents/LDAPAgent,
 * for the properties host, suffix, and port, then the flat file.
 * Perhaps we should also try DNS.
 */

LDAP *ldap_new( void )
{
	char *host = NULL;
	char *base = NULL;
	int port = LDAP_PORT;
	LDAP *res;
	locator_t *plocator;

	for (plocator = locators; *plocator != NULL; plocator++)
	{
		if (((*plocator)(&host, &base, &port)) == GETCONF_SUCCESS)
		{

#ifdef notdef
			printf("ldap_new(): host %s, base %s, port %d\n",
				host == NULL ? "(null)" : host,
				base == NULL ? "(null)" : base,
				port );
#endif
	
			res = ldap_open(host, port);
			
			if (res != NULL)
				res->ld_defbase = base;
			else
				free(base);

			free(host);
	    
			return res;
		}
	}

	return NULL;
}

/* try the environment */
static int env_getconf(char **ldaphost, char **basedn, int *port)
{
	char *sport;
	char *host, *base;

	host = getenv("LDAPHOST");
	if (host == NULL)
		return GETCONF_FAILED;

	*ldaphost = strdup(host);

	base = getenv("LDAP_BINDDN");
	*basedn = (base == NULL) ? NULL : strdup(base);

	sport = getenv("LDAPPORT");
	*port = (sport == NULL) ? LDAP_PORT : atoi(sport); 

	return GETCONF_SUCCESS;
}

/* this code is based on the NetInfo lookup routine in sendmail */
static int ni_getconf(char **ldaphost, char **basedn, int *port)
{
	void *ni = NULL;
	void *lastni = NULL;
	int i;
	ni_status nis;

	*ldaphost = NULL;
	*basedn = NULL;
	*port = LDAP_PORT;

	nis = ni_open(NULL, ".", &ni);
	
	if (nis != NI_OK)
		return GETCONF_FAILED;
	
	for (i = 0; i < MAX_NI_LEVELS - 1; ++i)
	{
		if (nis != NI_OK || ni_getConfiguration(ni, ldaphost, basedn, port) == NI_OK)
		{
			break;
		}

		if (lastni != NULL)
		{
			ni_free(lastni);
		}
		lastni = ni;
		nis = ni_open(lastni, "..", &ni);
	}
		
	if (lastni != NULL && ni != lastni)
	{
		ni_free(lastni);
	}

	if (ni != NULL)
	{
		ni_free(ni);
	}

	if (*ldaphost == NULL)
	{
		ldap_memfree(*basedn);
		return GETCONF_FAILED;
	}

	return GETCONF_SUCCESS;
}


static int ni_getName(ni_namelist *nl, char **val, int get_all)
{
	if (nl->ninl_len > 0)
	{
		if (get_all == NI_GETNAME_FIRST_VALUE)
		{
			*val = ni_name_dup(nl->ninl_val[0]);
		}
		else
		{
			int i;
			int len = 0;

			for (i = 0; i < nl->ninl_len; i++)
			{
				len += strlen(nl->ninl_val[i]) + 1;
			}

			*val = (char *)malloc(len);
			if (*val == NULL) return GETCONF_FAILED;
			strcpy(*val, "");

			for (i = 0; i < nl->ninl_len; i++)
			{
				if (i > 0)
					strcat(*val, " ");
				strcat(*val, nl->ninl_val[i]);
			}
		}
		return GETCONF_SUCCESS;
	}
	return GETCONF_FAILED;
}

static ni_status ni_getConfiguration(void *ni, char **ldaphost, char **basedn, int *port)
{
	ni_id nid;
	ni_proplist pl;
	ni_status nis;
	int i;
	char *sPort = NULL;

	NI_INIT(&pl);
	
	if ((nis = ni_pathsearch(ni, &nid, NAME_NI_LDAP_DIRECTORY)) != NI_OK)
	{
		return nis;
	}
	
	if ((nis = ni_read(ni, &nid, &pl)) != NI_OK)
	{
		return nis;
	}
		
	for (i = 0; i < pl.ni_proplist_len; i++)
	{
		if (ni_name_match(pl.nipl_val[i].nip_name, NAME_HOST))
		{
			if (ni_getName(&pl.nipl_val[i].nip_val, ldaphost, NI_GETNAME_ALL_VALUES) == GETCONF_FAILED)
			{
				return NI_FAILED;
			}
		}
		else if (ni_name_match(pl.nipl_val[i].nip_name, NAME_SUFFIX))
		{
			(void)ni_getName(&pl.nipl_val[i].nip_val, basedn, NI_GETNAME_FIRST_VALUE);
		}
		else if (ni_name_match(pl.nipl_val[i].nip_name, NAME_PORT))
		{
			if (ni_getName(&pl.nipl_val[i].nip_val, &sPort, NI_GETNAME_FIRST_VALUE) == GETCONF_SUCCESS)
			{
				*port = atoi(sPort);
			}
		}
	}
	
	ni_proplist_free(&pl);

	if (sPort != NULL)
		free(sPort);

	return NI_OK;
}


/* Most of this code is pinched from lookupd. */

static char *ff_getLineFromFile(FILE *fp)
{
	char s[1024];
	char *out;
	int len;

	s[0] = '\0';

	fgets(s, sizeof(s), fp);
	if (s == NULL || s[0] == '\0')
		return NULL;

	if (s[0] == '#')
		return strdup("#");

	len = strlen(s) - 1;
	s[len] = '\0';

	out = strdup(s);

	return out;
}

static void ff_truncateString(char *string)
{
	char *sepLoc;

	sepLoc = strchr(string, ' ');
	if (sepLoc != NULL)
		*sepLoc = '\0';

	return;
}

static int ff_parseLine(const char *data, char **key, char **val)
{
	int i, j, len;
	char buf[4096];
	int scanning;

	char sep[] = " \t";
	const char *p;

	len = strlen(sep);

	p = data;

	*key = NULL;
	*val = NULL;

	while (p[0] != '\0')
	{
		/* skip leading whitespace */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		while (scanning)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && scanning; j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
			}
		}

		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';

		if (*key == NULL)
		{
			*key = strdup(buf);
			if (*key == NULL) return GETCONF_FAILED;
		}
		else
		{
			if (*val == NULL)
			{
				/* First value? Get some memory. We may overestimate
				 * this depending on the separators, but that's
				 * acceptable.
				 */
				*val = (char *)malloc(strlen(data) - strlen(*key));
				if (*val == NULL) return GETCONF_FAILED;
				strcpy(*val, "");
			}
			else
			{
				strcat(*val, " ");
			}
			strcat(*val, buf);
		}

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = 1;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = 0;
			}
		}
	}

	return GETCONF_SUCCESS;
}

static int ff_getconf(char **ldaphost, char **basedn, int *port)
{
	/* Lookupd now supports /etc/lookupd as a source of
	 * for configuration information. Thanks, Marc.
	 * This code is based on lookupd.
	 */

	FILE *fp;
	char *line;

	fp = fopen(NAME_FF_LDAP_DIRECTORY, "r");
	if (fp == NULL)
		return GETCONF_FAILED;

	while (NULL != (line = ff_getLineFromFile(fp)))
	{
		char *key, *val;

		if (line[0] == '#')
		{
			ldap_memfree(line);
			line = NULL;
			continue;
		}

		if (ff_parseLine(line, &key, &val) == GETCONF_FAILED)
			return GETCONF_FAILED;

		if (key == NULL)
			continue;

		if (!strcmp(key, NAME_HOST))
		{
			if (val != NULL)
			{
				if (*ldaphost != NULL)
					free(*ldaphost);
				*ldaphost = val;
			}
		}
		else if (!strcmp(key, NAME_SUFFIX))
		{
			if (val != NULL)
			{
				ff_truncateString(val);
				if (*basedn != NULL)
					free(*basedn);
				*basedn = val;
			}
		}
		else if (!strcmp(key, NAME_PORT))
		{
			if (val != NULL)
			{
				ff_truncateString(val);
				*port = atoi(val);
				free(val);
			}
		}
	}

	/* only the host matters */
	if (*ldaphost == NULL)
	{
		free(*basedn);
		*basedn = NULL;
		return GETCONF_FAILED;
	}

	return GETCONF_SUCCESS;
}

