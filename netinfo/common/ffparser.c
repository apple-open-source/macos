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
 * ffparser.c
 *
 * Flat File data parser
 * Written by Marc Majka
 */

#include <stdlib.h>
#include <string.h>
#include <NetInfo/dsutil.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void
_remove_key(dsrecord *r, char *k)
{
	dsdata *key;

	if (r == NULL) return;
	if (k == NULL) return;

	key = cstring_to_dsdata(k);
	dsrecord_remove_key(r, key, SELECT_ATTRIBUTE);
	dsdata_release(key);
}

static char *
_value_for_key(dsrecord *r, char *k)
{
	dsattribute *a;
	dsdata *d;
	char *v, *out;

	if (r == NULL) return NULL;
	if (k == NULL) return NULL;

	d = cstring_to_dsdata(k);

	a = dsrecord_attribute(r, d, SELECT_ATTRIBUTE);
	dsdata_release(d);
	if (a == NULL) return NULL;
	if (a->count == 0)
	{
		dsattribute_release(a);
		return NULL;
	}

	v = dsdata_to_utf8string(a->value[0]);
	if (v == NULL)
	{
		dsattribute_release(a);
		return NULL;
	}

	out = copyString(v);
	dsattribute_release(a);
	return out;
}

static void
_set_values_for_key(dsrecord *r, char **v, char *k)
{
	dsattribute *a;
	dsdata *d;
	int i;

	if (r == NULL) return;
	if (v == NULL) return;
	if (k == NULL) return;

	d = cstring_to_dsdata(k);
	dsrecord_remove_key(r, d, SELECT_ATTRIBUTE);
	
	a = dsattribute_new(d);
	dsdata_release(d);

	for (i = 0; v[i] != NULL; i++)
	{
		d = cstring_to_dsdata(v[i]);
		dsattribute_append(a, d);
		dsdata_release(d);
	}

	dsrecord_insert_attribute(r, a, IndexNull, SELECT_ATTRIBUTE);
	dsattribute_release(a);
}

static void
_add_value_for_key(dsrecord *r, char *v, char *k)
{
	dsattribute *a;
	dsdata *d;

	if (r == NULL) return;
	if (v == NULL) return;
	if (k == NULL) return;

	d = cstring_to_dsdata(k);
	a = dsrecord_attribute(r, d, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		a = dsattribute_new(d);
		dsrecord_insert_attribute(r, a, IndexNull, SELECT_ATTRIBUTE);
	}
	
	dsdata_release(d);

	d = cstring_to_dsdata(v);
	dsattribute_append(a, d);
	dsdata_release(d);
	dsattribute_release(a);
}

static void
_add_values_for_key(dsrecord *r, char **v, char *k)
{
	dsattribute *a;
	dsdata *d;
	int i;

	if (r == NULL) return;
	if (v == NULL) return;
	if (k == NULL) return;

	d = cstring_to_dsdata(k);
	a = dsrecord_attribute(r, d, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		a = dsattribute_new(d);
		dsrecord_insert_attribute(r, a, IndexNull, SELECT_ATTRIBUTE);
	}
	
	dsdata_release(d);

	for (i = 0; v[i] != NULL; i++)
	{
		d = cstring_to_dsdata(v[i]);
		dsattribute_append(a, d);
		dsdata_release(d);
	}

	dsattribute_release(a);
}

static void
_set_value_for_key(dsrecord *r, char *v, char *k)
{
	dsattribute *a;
	dsdata *d;

	if (r == NULL) return;
	if (v == NULL) return;
	if (k == NULL) return;

	d = cstring_to_dsdata(k);
	dsrecord_remove_key(r, d, SELECT_ATTRIBUTE);
	
	a = dsattribute_new(d);
	dsdata_release(d);

	d = cstring_to_dsdata(v);
	dsattribute_append(a, d);
	dsdata_release(d);

	dsrecord_insert_attribute(r, a, IndexNull, SELECT_ATTRIBUTE);
	dsattribute_release(a);
}

char **
ff_tokens_from_line(const char *data, const char *sep, int skip_comments)
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096];
	int scanning;

	if (data == NULL) return NULL;
	if (sep == NULL)
	{
		tokens = appendString((char *)data, tokens);
		return tokens;
	}

	len = strlen(sep);

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* stop adding tokens at a # if skip_comments is set */
		if ((skip_comments != 0) && (p[0] == '#')) break;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		while (scanning == 1)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && (scanning == 1); j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
			}
		}
	
		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = 0;
			}
		}

		if ((scanning == 0) && p[0] == '\0')
		{
			/* line ended at a separator - add a null member */
			tokens = appendString("", tokens);
			return tokens;
		}
	}
	return tokens;
}

char **
ff_netgroup_tokens_from_line(const char *data)
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096], sep[3];
	int scanning, paren;

	if (data == NULL) return NULL;
	strcpy(sep," \t");
	len = 2;

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		paren = 0;
		if (p[0] == '(')
		{
			paren = 1;
			p++;
		}

		while (scanning == 1)
		{
			if (p[0] == '\0') return NULL;
			buf[i++] = p[0];
			p++;
			if (paren == 1)
			{
				if (p[0] == ')') scanning = 0;
			}
			else
			{
				for (j = 0; (j < len) && (scanning == 1); j++)
				{
					if ((p[0] == sep[j]) || (p[0] == '\0')) scanning = 0;
				}					
			}
		}

		if (paren == 1)
		{
			paren = 0;
			if (p[0] == ')') p++;
		}

		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

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

	return tokens;
}

static dsrecord *
ff_parse_magic_cookie(char **tokens)
{
	freeList(tokens);
	tokens = NULL;
	return NULL;
}

dsrecord *
ff_parse_user_A(char *data)
{
	dsrecord *item;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) != 10)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], "passwd");
	_set_value_for_key(item, tokens[2], "uid");
	_set_value_for_key(item, tokens[3], "gid");
	_set_value_for_key(item, tokens[4], "class");
	_set_value_for_key(item, tokens[5], "change");
	_set_value_for_key(item, tokens[6], "expire");
	_set_value_for_key(item, tokens[7], "realname");
	_set_value_for_key(item, tokens[8], "home");
	_set_value_for_key(item, tokens[9], "shell");

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_user(char *data)
{
	/* For compatibility with YP, support 4.3 style passwd files. */
	
	dsrecord *item;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) != 7)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], "passwd");
	_set_value_for_key(item, tokens[2], "uid");
	_set_value_for_key(item, tokens[3], "gid");
	_set_value_for_key(item, tokens[4], "realname");
	_set_value_for_key(item, tokens[5], "home");
	_set_value_for_key(item, tokens[6], "shell");

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_group(char *data)
{
	dsrecord *item;
	char **users;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) == 0)
	{
		freeList(tokens);
		return NULL;
	}

	if (tokens[0][0] == '+')
	{
		return ff_parse_magic_cookie(tokens);
	}

	if (listLength(tokens) < 3)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], "passwd");
	_set_value_for_key(item, tokens[2], "gid");

	if (listLength(tokens) < 4)
	{
		_set_value_for_key(item, "", "users");
	}
	else
	{
		users = ff_tokens_from_line(tokens[3], ",", 0);
		_set_values_for_key(item, users, "users");
		freeList(users);
		users = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_host(char *data)
{
	dsrecord *item;
	char **tokens;
	int len, af;
	struct in_addr a4;
	struct in6_addr a6;
	char paddr[64];
	void *saddr = NULL;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return NULL;
	}

	af = AF_UNSPEC;
	if (inet_aton(tokens[0], &a4) == 1)
	{
		af = AF_INET;
		saddr = &a4;
	}
	else if (inet_pton(AF_INET6, tokens[0], &a6) == 1)
	{
		af = AF_INET6;
		saddr = &a6;
	}

	if (af == AF_UNSPEC)
	{
		freeList(tokens);
		return NULL;
	}

	/* We use inet_pton to convert to a canonical form */
	if (inet_ntop(af, saddr, paddr, 64) == NULL)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	if (af == AF_INET) _set_value_for_key(item, paddr, "ip_address");
	else _set_value_for_key(item, paddr, "ipv6_address");

	_set_values_for_key(item, tokens+1, "name");

	freeList(tokens);
	tokens = NULL;

	return item;
}

static dsrecord *
ff_parse_nna(char *data, char *aKey)
{
	dsrecord *item;
	char **tokens;
	int len;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	len = listLength(tokens);
	if (len < 2)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], aKey);
	_add_values_for_key(item, tokens+2, "name");

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_network(char *data)
{
	return ff_parse_nna(data, "address");
}

dsrecord *
ff_parse_service(char *data)
{
	dsrecord *item;
	char *port;
	char *proto;
	char *pp;

	item = ff_parse_nna(data, "protport");
	if (item == NULL) return NULL;

	pp = _value_for_key(item, "protport");
	if (pp == NULL)
	{
		dsrecord_release(item);
		return NULL;
	}

	port = prefix(pp, '/');
	if (port == NULL)
	{
		free(pp);
		dsrecord_release(item);
		return NULL;
	}

	proto = postfix(pp, '/');
	free(pp);
	if (proto == NULL)
	{
		freeString(port);
		port = NULL;
		dsrecord_release(item);
		return NULL;
	}

	_set_value_for_key(item, port, "port");
	_set_value_for_key(item, proto, "protocol");
	freeString(port);
	port = NULL;
	freeString(proto);
	proto = NULL;

	_remove_key(item, "protport");

	return item;
}

dsrecord *
ff_parse_protocol(char *data)
{
	return ff_parse_nna(data, "number");
}

dsrecord *
ff_parse_rpc(char *data)
{
	return ff_parse_nna(data, "number");
}

dsrecord *
ff_parse_mount(char *data)
{
	dsrecord *item;
	char **val;
	char **tokens;
	int len;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 0);
	len = listLength(tokens);
	if (len < 4)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], "dir");
#ifdef _UNIX_BSD_43_
	_set_value_for_key(item, tokens[2], "type");
#else
	_set_value_for_key(item, tokens[2], "vfstype");
#endif

	val = ff_tokens_from_line(tokens[3], ",", 0);
	_set_values_for_key(item, val, "opts");

	freeList(val);
	val = NULL;

	if (len > 4)
		_set_value_for_key(item, tokens[4], "dump_freq");
	else
		_set_value_for_key(item, "0", "dump_freq");

	if (len > 5)
		_set_value_for_key(item, tokens[5], "passno");
	else
		_set_value_for_key(item, "0", "passno");

	freeList(tokens);
	tokens = NULL;

	return item;
}

static dsrecord *
ff_parse_pb(char *data, char c)
{
	char **options;
	char **opt;
	char t[2];
	int i, len;
	dsrecord *item;

	if (data == NULL) return NULL;

	item = dsrecord_new();

	t[0] = c;
	t[1] = '\0';
	options = explode(data, t);

	len = listLength(options);
	if (len < 1)
	{
		freeList(options);
		return NULL;
	}

	_set_value_for_key(item, options[0], "name");

	for (i = 1; i < len; i++)
	{
		opt = explode(options[i], "=");
		if (listLength(opt) == 2) _set_value_for_key(item, opt[1], opt[0]);
		freeList(opt);
		opt = NULL;
	}

	freeList(options);
	options = NULL;

	return item;
}

dsrecord *
ff_parse_printer(char *data)
{
	return ff_parse_pb(data, ':');
}

dsrecord *
ff_parse_bootparam(char *data)
{
	return ff_parse_pb(data, '\t');
}

dsrecord *
ff_parse_bootp(char *data)
{
	dsrecord *item;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 0);
	if (listLength(tokens) < 5)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");
	_set_value_for_key(item, tokens[1], "htype");
	_set_value_for_key(item, tokens[2], "en_address");
	_set_value_for_key(item, tokens[3], "ip_address");
	_set_value_for_key(item, tokens[4], "bootfile");

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_alias(char *data)
{
	dsrecord *item;
	char **members;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, ":", 0);
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");

	members = ff_tokens_from_line(tokens[1], ",", 0);
	_set_values_for_key(item, members, "members");

	freeList(members);
	members = NULL;

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_ethernet(char *data)
{
	dsrecord *item;
	char **tokens;

	if (data == NULL) return NULL;

	tokens = ff_tokens_from_line(data, " \t", 1);
	if (listLength(tokens) < 2)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "en_address");
	_set_value_for_key(item, tokens[1], "name");

	freeList(tokens);
	tokens = NULL;

	return item;
}

dsrecord *
ff_parse_netgroup(char *data)
{
	dsrecord *item;
	char **val;
	char **tokens;
	int i, len;

	if (data == NULL) return NULL;

	tokens = ff_netgroup_tokens_from_line(data);
	if (tokens == NULL) return NULL;

	len = listLength(tokens);
	if (len < 1)
	{
		freeList(tokens);
		return NULL;
	}

	item = dsrecord_new();

	_set_value_for_key(item, tokens[0], "name");

	for (i = 1; i < len; i++)
	{
		val = ff_tokens_from_line(tokens[i], ",", 0);
		if (listLength(val) == 1)
		{
			_add_value_for_key(item, val[0], "netgroups");
			freeList(val);
			val = NULL;
			continue;
		}

		if (listLength(val) != 3)
		{
			dsrecord_release(item);
			freeList(tokens);
			tokens = NULL;
			freeList(val);
			val = NULL;
			return NULL;
		}

		if (val[0][0] != '\0') _add_value_for_key(item, val[0], "hosts");
		if (val[1][0] != '\0') _add_value_for_key(item, val[1], "users");
		if (val[2][0] != '\0') _add_value_for_key(item, val[2], "domains");

		freeList(val);
		val = NULL;
	}

	freeList(tokens);
	tokens = NULL;

	return item;
}
