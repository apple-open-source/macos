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

#include <stdlib.h>
#include <string.h>

#include <NetInfo/dsrecord.h>
#include <NetInfo/dsx500.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/utf-8.h>

/*
 * $OpenLDAP: pkg/ldap/libraries/libldap/getdn.c,v 1.27 2000/06/09 04:45:14
 * mrv Exp $
 */
/*
 * Copyright 1998-2000 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */
/*
 * Portions Copyright (c) 1994 Regents of the University of Michigan. All
 * rights reserved.
 * 
 * getdn.c
 */

static char   **explode_name(char *name, u_int32_t notypes, u_int32_t is_type);

#define INQUOTE		1
#define OUTQUOTE	2

#define NAME_TYPE_X500_RDN	0
#define NAME_TYPE_X500_DN	1
#define NAME_TYPE_NETINFO_PATH	2

char          **
dsx500_explode_dn(char *dn, u_int32_t notypes)
{
	return explode_name(dn, notypes, NAME_TYPE_X500_DN);
}

char          **
dsx500_explode_rdn(char *rdn, u_int32_t notypes)
{
	return explode_name(rdn, notypes, NAME_TYPE_X500_RDN);
}

char           *
dsx500_dn_to_netinfo_string_path (char *dn)
{
	char           *dce, *q, **rdns, **p, *c;
	int             len = 0;

	rdns = explode_name(dn, 0, NAME_TYPE_X500_DN);
	if (rdns == NULL)
	{
		return NULL;
	}

	for (p = rdns; *p != NULL; p++)
	{
		for (c = *p; *c != '\0'; c++)
		{
			if (*c == '/') len++;
			len++;
		}
		len++;
	}

	q = dce = malloc(len + 1);
	if (dce == NULL)
	{
		return NULL;
	}
	p--;			/* get back past NULL */

	for (; p >= rdns; p--)
	{
		*q++ = '/';
		for (c = *p; *c != '\0'; c++)
		{
			if (*c == '/')
				*q++ = '\\';
			*q++ = *c;
		}
	}
	*q = '\0';

	return dce;
}

char           *
dsx500_netinfo_string_path_to_dn (char *netinfo_name)
{
	char           *dn, *q, **rdns, **p, *c;
	int             len = 0;

	if (*netinfo_name == '/')
		netinfo_name++;

	rdns = explode_name(netinfo_name, 0, NAME_TYPE_NETINFO_PATH);
	if (rdns == NULL)
	{
		return NULL;
	}

	for (p = rdns; *p != NULL; p++)
	{
		for (c = *p; *c != '\0'; c++)
		{
			if (NeedEscapeRDN(*c)) len++;
			len++;
		}
		len++;
	}

	q = dn = malloc(len);
	if (dn == NULL)
	{
		return NULL;
	}
	p--;

	for (; p >= rdns; p--)
	{
		for (c = *p; *c != '\0'; c++)
		{
			if (NeedEscapeRDN(*c))
				*q++ = '\\';
			*q++ = *c;
		}
		*q++ = ',';
	}

	*--q = '\0';

	return dn;
}

static char   **
explode_name(char *name, u_int32_t notypes, u_int32_t is_type)
{
	const char     *p, *q, *rdn;
	char          **parts = NULL;
	int             offset, state, have_equals, count = 0, endquote,
	                len;

	/* safe guard */
	if (name == NULL)
		name = "";

	/* skip leading whitespace */
	while (dsutil_utf8_isspace(name))
	{
		DSUTIL_UTF8_INCR(name);
	}

	p = rdn = name;
	offset = 0;
	state = OUTQUOTE;
	have_equals = 0;

	do
	{
		/* step forward */
		p += offset;
		offset = 1;

		switch (*p)
		{
		case '\\':
			if (p[1] != '\0')
			{
				offset = DSUTIL_UTF8_OFFSET(++p);
			}
			break;
		case '"':
			if (state == INQUOTE)
				state = OUTQUOTE;
			else
				state = INQUOTE;
			break;
		case '=':
			if (state == OUTQUOTE)
				have_equals++;
			break;
		case '+':
			if (is_type == NAME_TYPE_X500_RDN)
				goto end_part;
			break;
		case '/':
			if (is_type == NAME_TYPE_NETINFO_PATH)
				goto end_part;
			break;
		case ';':
		case ',':
			if (is_type == NAME_TYPE_X500_DN)
				goto end_part;
			break;
		case '\0':
	end_part:
			if (state == OUTQUOTE)
			{
				int need_ni_name = 0;

				++count;

				if ((is_type == NAME_TYPE_NETINFO_PATH) && (have_equals == 0))
					need_ni_name = 1;

				have_equals = 0;

				if (parts == NULL)
				{
					if ((parts = (char **) malloc(8
						 * sizeof(char *))) == NULL)
						return (NULL);
				}
				else if (count >= 8)
				{
					if ((parts = (char **) realloc(parts,
					      (count + 1) * sizeof(char *)))
					    == NULL)
						return (NULL);
				}
				parts[count] = NULL;
				endquote = 0;

				if (notypes)
				{
					for (q = rdn; q < p && *q != '='; ++q)
					{
						 /* EMPTY */ ;
					}

					if (q < p)
					{
						rdn = ++q;
					}
					if (*rdn == '"')
					{
						++rdn;
					}
					if (p[-1] == '"')
					{
						endquote = 1;
						--p;
					}
				}

				len = p - rdn;
				if (need_ni_name)
					len += sizeof("name=") - 1;

				if ((parts[count - 1] = (char *) calloc(1,
							  len + 1)) != NULL)
				{
					char *r = parts[count - 1];

					if (need_ni_name)
					{
						memmove(r, "name=", len);
						r += sizeof("name=") - 1;
					}

					memmove(r, rdn, len);

					if (!endquote)
					{
						/* skip trailing spaces */
						while (len > 0 && dsutil_utf8_isspace(
						&parts[count - 1][len - 1]))
						{
							--len;
						}
					}
					parts[count - 1][len] = '\0';
				}
				/*
				 *  Don't forget to increment 'p' back to where
				 *  it should be.  If we don't, then we will
				 *  never get past an "end quote."
				 */
				if (endquote == 1)
					p++;

				rdn = *p ? &p[1] : p;
				while (dsutil_utf8_isspace(rdn))
					++rdn;
			} break;
		}
	} while (*p);

	return (parts);
}

/*
 * get_next_substring(), rdn_attr_type(), rdn_attr_value(), and
 * build_new_dn().
 *
 * Copyright 1999, Juan C. Gomez, All rights reserved.
 * This software is not subject to any license of Silicon Graphics
 * Inc. or Purdue University.
 *
 * Redistribution and use in source and binary forms are permitted
 * without restriction or fee of any kind as long as this notice
 * is preserved.
 *
 */

/*
 * get_next_substring:
 * 
 * Gets next substring in s, using d (or the end of the string '\0') as a string
 * delimiter, and places it in a duplicated memory space. Leading spaces are
 * ignored. String s **must** be null-terminated.
 */

static char    *
get_next_substring(const char *s, char d)
{

	char           *str, *r;

	r = str = malloc(strlen(s) + 1);
	if (r == NULL)
	{
		return NULL;
	}

	/* Skip leading spaces */

	while (*s && dsutil_utf8_isspace(s))
	{
		s++;
	}

	/* Copy word */

	while (*s && (*s != d))
	{

		/*
		 * Don't stop when you see trailing spaces may be a
		 * multi-word string, i.e. name=John Doe!
		 */

		*str++ = *s++;

	}

	*str = '\0';

	return r;

}

/* 
 * These functions are not cognizant of escaped equal
 * signs in RDNs. They need to be fixed, preferably using
 * the latest OpenLDAP DN parsing code.
 */

/*
 * rdn_attr_type:
 * 
 * Given a string (i.e. an rdn) of the form: "attribute_type = attribute_value"
 * this function returns the type of an attribute, that is the string
 * "attribute_type" which is placed in newly allocated memory. The returned
 * string will be null-terminated.
 */

char           *
dsx500_rdn_attr_type(char *s)
{
	return get_next_substring(s, '=');
}


/*
 * rdn_attr_value:
 * 
 * Given a string (i.e. an rdn) of the form: "attribute_type = attribute_value"
 * this function returns "attribute_value" which is placed in newly allocated
 * memory. The returned string will be null-terminated and may contain spaces
 * (i.e. "John Doe\0").
 */

char           *
dsx500_rdn_attr_value(char *rdn)
{
	const char     *str;

	if ((str = strchr(rdn, '=')) != NULL)
	{
		return get_next_substring(++str, '\0');
	}
	return NULL;
}


u_int32_t
dsx500_validate_rdn(char *rdn)
{
	/* just a simple check for now */
	return strchr(rdn, '=') != NULL;
}


/*
 * build_new_dn:
 * 
 * Used by ldbm/bdb2_back_modrdn to create the new dn of entries being renamed.
 * 
 * new_dn = parent (p_dn)  + separator(s) + rdn (newrdn) + null.
 */

char           *
dsx500_make_dn(char *p_dn, char *newrdn)
{
	char           *new_dn;

	if (p_dn == NULL || p_dn[0] == '\0')
	{
		return copyString(newrdn);
	}

	new_dn = (char *) malloc(strlen(p_dn) + strlen(newrdn) + 3);
	if (new_dn == NULL)
	{
		return NULL;
	}

	strcpy(new_dn, newrdn);
	strcat(new_dn, ",");
	strcat(new_dn, p_dn);

	return new_dn;
}
