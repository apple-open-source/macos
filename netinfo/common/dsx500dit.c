/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (C) 1990 by NeXT, Inc. All rights reserved.
 *
 * Modified July 2000 by Luke Howard <lukeh@darwin.apple.com>
 * to support constructing a fake X.500 DIT from a NetInfo
 * hierarchy. 
 *
 * Modified August 2000 to remove code common to network.c.
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
#include <sys/ioctl.h>

#include <NetInfo/network.h>
#include <NetInfo/nilib2.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsx500.h>
#include <NetInfo/dsx500dit.h>
#include <NetInfo/dsengine.h>

#define ni_name_match_seg(a, b, len) (strncmp(a, b, len) == 0 && (a)[len] == 0)

static const char NAME_NAME[] = "name";
static const char NAME_MASTER[] = "master";
static const char NAME_MACHINES[] = "machines";
static const char NAME_IP_ADDRESS[] = "ip_address";
static const char NAME_SERVES[] = "serves";
static const char NAME_SUFFIX[] = "suffix";
static const char NAME_OU[] = "ou=";
static const char NAME_UNKNOWN[] = "ou=###UNKNOWN###";
static const char NAME_DOTDOT[] = "..";
static const char NAME_DOT[] = ".";

static ni_name
escape_domain(ni_name name)
{
	int extra;
	char *p;
	char *s;
	ni_name newname;

	extra = sizeof(NAME_OU) - 1;
	for (p = name; *p; p++)
	{
		if ((*p == '/') || (*p == '\\')) extra++;
	}
	
	newname = malloc(strlen(name) + extra + 1);
	strcpy(newname, NAME_OU);
	s = newname + sizeof(NAME_OU) - 1;
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
	ni_namelist nl_serves, nl_suffix, *pnl_suffix;
	ni_index i;
	ni_name slash;
	ni_status status;
	char *domain;

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

	status = ni_lookupprop(ni, &nid, NAME_SERVES, &nl_serves);
	if (status != NI_OK) return NULL;

	status = ni_lookupprop(ni, &nid, NAME_SUFFIX, &nl_suffix);
	if (status == NI_OK) pnl_suffix = &nl_suffix;
	else pnl_suffix = NULL;

	for (i = 0; i < nl_serves.ninl_len; i++)
	{
		slash = rindex(nl_serves.ninl_val[i], '/');
		if (slash == NULL) continue;

		if (ni_name_match(slash + 1, tag))
		{
			/* If we've got a value in "suffix" at this index, use instead */
			if (pnl_suffix != NULL &&
				pnl_suffix->ninl_len > i
				&& pnl_suffix->ninl_val[i][0] != '\0')
			{
				domain = copyString(pnl_suffix->ninl_val[i]);
			}
			else
			{
				*slash = '\0';
				domain = escape_domain(nl_serves.ninl_val[i]);
			}
			ni_namelist_free(&nl_serves);
			if (pnl_suffix) ni_namelist_free(pnl_suffix);
			return domain;
		}
	}

	ni_namelist_free(&nl_serves);
	if (pnl_suffix) ni_namelist_free(pnl_suffix);

	return NULL;
}

static char *
dsx500dit_domainof(void *ni, void *parent)
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

	if (sys_is_my_address(&(addr.sin_addr)))
	{
		/* Try all my non-loopback interfaces */
		ilist = sys_interfaces();
		if (ilist == NULL) return ni_name_dup(NAME_UNKNOWN);

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

		sys_interfaces_release(ilist);
	}

	dom = malloc(strlen(tag) + 256);
	sprintf(dom, "%s%s@%s", NAME_OU, tag, inet_ntoa(addr.sin_addr));
	ni_name_free(&tag);
	return dom;
}

static char *
mastersuffix(void *ni)
{
	ni_id nid;
	ni_idlist idl;
	ni_namelist nl, nl_suffix;
	ni_index i;
	ni_name slash, machine = NULL, tag = NULL;
	ni_status status;
	char *domain;

	status = ni_root(ni, &nid);
	if (status != NI_OK) return NULL;

	/* read master property */
	status = ni_lookupprop(ni, &nid, NAME_MASTER, &nl);
	if (status != NI_OK || nl.ninl_len == 0) return NULL;

	/* get machine and tag */
	slash = rindex(nl.ninl_val[0], '/');
	if (slash == NULL) return NULL;

	*slash = 0;
	machine = ni_name_dup(nl.ninl_val[0]);
	tag = ni_name_dup(slash + 1);
	ni_namelist_free(&nl);

	status = ni_lookup(ni, &nid, NAME_NAME, NAME_MACHINES, &idl);
	if (status != NI_OK) 
	{
		ni_name_free(&machine);
		ni_name_free(&tag);
		return NULL;
	}

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookup(ni, &nid, NAME_NAME, machine, &idl);
	if (status != NI_OK) 
	{
		ni_name_free(&machine);
		ni_name_free(&tag);
		return NULL;
	}

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookupprop(ni, &nid, NAME_SERVES, &nl);
	if (status != NI_OK) 
	{
		ni_name_free(&machine);
		ni_name_free(&tag);
		return NULL;
	}

	status = ni_lookupprop(ni, &nid, NAME_SUFFIX, &nl_suffix);
	if (status != NI_OK)
	{
		ni_namelist_free(&nl);
		ni_name_free(&machine);
		ni_name_free(&tag);
		return NULL;
	}

	domain = NULL;

	for (i = 0; i < nl.ninl_len; i++)
	{
		slash = rindex(nl.ninl_val[i], '/');
		if (slash == NULL) continue;

		if (ni_name_match(slash + 1, tag))
		{
			if (nl_suffix.ninl_len > i && nl_suffix.ninl_val[i][0] != '\0')
			{
				/* leaks ??? */
				domain = copyString(nl_suffix.ninl_val[i]);
			}
			/* no use reading the serves value because it will be "." */
			break;
		}
	}

	ni_namelist_free(&nl);
	ni_namelist_free(&nl_suffix);

	ni_name_free(&machine);
	ni_name_free(&tag);

	return domain;
}

static ni_status
dsx500dit_pwdomain(void *ni, ni_name *buf)
{
	void *nip;
	ni_status status;
	char *dom, *parent;

	/* Open parent domain */
	nip = ni_new(ni, "..");
	if (nip == NULL)
	{
		/* We're at the root; read suffix from master's host */
		*buf = mastersuffix(ni);
		if (*buf == NULL) *buf = copyString("");
		return NI_OK;
	}

	/* Get parent's name */
	status = dsx500dit_pwdomain(nip, &parent);
	if (status != NI_OK)
	{
		ni_free(nip);
		return status;
	}

	/* Get my name relative to my parent */
	dom = dsx500dit_domainof(ni, nip);

	if (parent[0] == '\0')
	{
		*buf = dom;
	}
	else
	{
		/* Prepend my relative name to my parent's name */
		*buf = dsx500_make_dn(parent, dom);
		ni_name_free(&dom);
	}

	ni_name_free(&parent);
	ni_free(nip);

	return NI_OK;
}

static dsdata *
servesdotdot(void *ni, ni_entry entry)
{
	ni_name name, sep;
	ni_namelist nl, ip;
	ni_index i;
	ni_id id;
	char *ref;
	dsdata *ret;
	int broadcast;

	if (entry.names == NULL) return NULL;

	id.nii_object = entry.id;
	for (i = 0; i < entry.names->ninl_len; i++)
	{
		name = entry.names->ninl_val[i];
		sep = index(name, '/');
		if (sep == NULL) continue;

		if (!ni_name_match_seg(NAME_DOTDOT, name, sep - name)) continue;

		NI_INIT(&nl);
		if (ni_lookupprop(ni, &id, NAME_NAME, &nl) != NI_OK) continue;
		if (nl.ninl_len == 0)
		{
			ni_namelist_free(&nl);
			continue;
		}

		broadcast = 0;

		if (ni_lookupprop(ni, &id, NAME_IP_ADDRESS, &ip) == NI_OK)
		{
			if (ni_namelist_match(ip, "255.255.255.255") != NI_INDEX_NULL)
				broadcast = 1;
			ni_namelist_free(&ip);
		}

		ref = (char *)malloc(broadcast + sizeof("ldap:///") + strlen(nl.ninl_val[0]));
		sprintf(ref, "%sldap://%s/", broadcast ? "c" : "", nl.ninl_val[0]);
		ni_namelist_free(&nl);

		/* referrals are case-insensitive */
		ret = casecstring_to_dsdata(ref);
		free(ref);
		return ret;
	}

	return NULL;
}

static ni_status
dsx500dit_getparents(void *ni, char *parent_suffix, dsattribute **refs)
{
	ni_id nid;
	ni_idlist idl;
	ni_entrylist entries;
	ni_index i;
	ni_status status;

	*refs = NULL;

	if (parent_suffix == NULL)
	{
		/* We are root. */
		return NI_OK;
	}

	status = ni_root(ni, &nid);
	if (status != NI_OK) return status;

	status = ni_lookup(ni, &nid, NAME_NAME, NAME_MACHINES, &idl);
	if (status != NI_OK) return status;

	/* based on ni_prot_proc.c:hardwired() */
	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_list(ni, &nid, NAME_SERVES, &entries);
	if (status != NI_OK) return status;

	for (i = 0; i < entries.niel_len; i++)
	{
		dsdata *ref;

		ref = servesdotdot(ni, entries.niel_val[i]);
		if (ref != NULL)
		{
			if (*refs == NULL)
			{
				dsdata *key = casecstring_to_dsdata(parent_suffix);

				*refs = dsattribute_new(key);
				dsdata_release(key);
			}

			dsattribute_append(*refs, ref);
			dsdata_release(ref);
		}
	}

	ni_entrylist_free(&entries);

	return NI_OK;
}

static ni_status
serveschild(void *nip, char *base, ni_entry entry, dsattribute ***refs, u_int32_t *count)
{
	ni_name name, sep;
	ni_index i;
	ni_id id;
	ni_status status;
	ni_namelist nl_suffix, nl_name, *pnl_suffix;

	*count = 0;
	*refs = NULL;

	if (entry.names == NULL) return NI_FAILED;

	id.nii_object = entry.id;

	NI_INIT(&nl_name);
	status = ni_lookupprop(nip, &id, NAME_NAME, &nl_name);
	if (status != NI_OK) return status;

	if (nl_name.ninl_len == 0)
	{
		ni_namelist_free(&nl_name);
		return NI_NONAME;
	}

	NI_INIT(&nl_suffix);
	status = ni_lookupprop(nip, &id, NAME_SUFFIX, &nl_suffix);
	if (status == NI_OK) pnl_suffix = &nl_suffix;
	else pnl_suffix = NULL;

	for (i = 0; i < entry.names->ninl_len; i++)
	{
		char *url, *domain, *dn;
		dsdata *_url, *_key;

		name = entry.names->ninl_val[i];
		sep = index(name, '/');
		if (sep == NULL) continue;

		/* skip parent domains */
		if (ni_name_match_seg(NAME_DOTDOT, name, sep - name)) continue;
		/* skip local domain */
		if (ni_name_match_seg(NAME_DOT, name, sep - name)) continue;

		if (pnl_suffix != NULL && pnl_suffix->ninl_len > i && pnl_suffix->ninl_val[i][0] != '\0')
		{
			domain = copyString(pnl_suffix->ninl_val[i]);
		}
		else
		{
			*sep = '\0';
			domain = escape_domain(entry.names->ninl_val[i]);
		}

		dn = dsx500_make_dn(base, domain);
		_key = casecstring_to_dsdata(dn);
		free(domain);
		free(dn);

		url = (char *)malloc(sizeof("ldap:///") + strlen(nl_name.ninl_val[0]));
		sprintf(url, "ldap://%s/", nl_name.ninl_val[0]);
		_url = casecstring_to_dsdata(url);
		free(url);

		if (*count == 0)
			*refs = (dsattribute **)malloc(sizeof(dsattribute *));
		else
			*refs = (dsattribute **)realloc(*refs, (*count + 1) * sizeof(dsattribute *));

		(*refs)[*count] = dsattribute_new(_key);
		dsattribute_append((*refs)[*count], _url);
		dsdata_release(_key);
		dsdata_release(_url);

		(*count)++;
	}


	ni_namelist_free(&nl_name);
	if (pnl_suffix) ni_namelist_free(pnl_suffix);

	return NI_OK;
}

static ni_status
dsx500dit_getchildren(void *ni, char *suffix, dsattribute ***refs, u_int32_t *count)
{
	ni_id nid;
	ni_idlist idl;
	ni_entrylist entries;
	ni_index i;
	ni_status status;
	dsrecord *r;

	*refs = NULL;
	*count = 0;

	status = ni_root(ni, &nid);
	if (status != NI_OK) return status;

	status = ni_lookup(ni, &nid, NAME_NAME, NAME_MACHINES, &idl);
	if (status != NI_OK) return status;

	/* based on ni_prot_proc.c:hardwired() */
	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);
	status = ni_list(ni, &nid, NAME_SERVES, &entries);
	if (status != NI_OK) return status;

	r = dsrecord_new();
	if (r == NULL) return NI_FAILED;

	for (i = 0; i < entries.niel_len; i++)
	{
		dsattribute **_refs;
		u_int32_t _count, j;

		/* return a set of { namingcontext, referral } attributes */
		if (serveschild(ni, suffix, entries.niel_val[i], &_refs, &_count) == NI_OK)
		{
			for (j = 0; j < _count; j++)
			{
				dsrecord_merge_attribute(r, _refs[j], SELECT_ATTRIBUTE);
				dsattribute_release(_refs[j]);
			}
			free(_refs);
		}
	}

	ni_entrylist_free(&entries);

	/* ownership switcheroo */

	*refs = r->attribute;
	r->attribute = NULL;

	*count = r->count;
	r->count = 0;

	dsrecord_release(r);

	return *count ? NI_OK : NI_FAILED;
}

dsx500dit *dsx500dit_retain(dsx500dit *i)
{
	if (i == NULL) return i;
	i->retain++;
	return i;
}

void dsx500dit_release(dsx500dit *info)
{
	u_int32_t i;

	if (info == NULL) return;

	info->retain--;
	if (info->retain > 0) return;

	dsdata_release(info->local_suffix);

	if (info->parent_referrals != NULL)
	{
		dsattribute_release(info->parent_referrals);
	}

	if (info->child_referrals != NULL)
	{
		for (i = 0; i < info->child_count; i++)
		{
			dsattribute_release(info->child_referrals[i]);
		}
		free(info->child_referrals);
	}

	free(info);
}

dsx500dit *dsx500dit_new(dsengine *s)
{
	void *ni, *nip;
	int freeit = 0, status;
	char *local_suffix, *parent_suffix;
	dsx500dit *info;

	if (s == NULL) return NULL;

	info = (dsx500dit *)malloc(sizeof(dsx500dit));
	if (info == NULL) return NULL;

	info->local_suffix = NULL;
	info->parent_referrals = NULL;
	info->child_count = 0;
	info->child_referrals = NULL;
	info->retain = 1;

	if (s->store->flags & DSSTORE_FLAGS_REMOTE_NETINFO)
	{
		ni = s->store->index[0];
	}
	else
	{
		dsdata *k, *v;
		dsattribute *a;
		dsrecord *r;
		char *tag, *p;
		struct sockaddr_in server;

		status = dsengine_fetch(s, 0, &r);
		if (status != DSStatusOK)
		{
			dsx500dit_release(info);
			return NULL;
		}

		k = cstring_to_dsdata((char *)NAME_MASTER);
		a = dsrecord_attribute(r, k, SELECT_ATTRIBUTE);
		dsdata_release(k);
		v = dsattribute_value(a, 0);
		dsattribute_release(a);
		p = dsdata_to_cstring(v);

		ni = NULL;

		if (p)
		{
			if (ni_parse_server_tag(p, &server, &tag) == NI_PARSE_OK)
			{
				ni = ni_connect(&server, tag);
				free(tag);
			}
		}
		
		dsdata_release(v);
		dsrecord_release(r);

		/* Couldn't parse master attribute. */
		if (ni == NULL)
		{
			info->local_suffix = casecstring_to_dsdata("");
			return info;
		}

		freeit = 1;
	}

	/* Open parent domain */
	nip = ni_new(ni, "..");
	if (nip == NULL)
	{
		/* We are root. */
		local_suffix = mastersuffix(ni);
		if (local_suffix == NULL) local_suffix = copyString("");
		parent_suffix = NULL;
	}
	else
	{
		char *dom;

		/* Get parent's name */
		if (dsx500dit_pwdomain(nip, &parent_suffix) != NI_OK)
		{
			dsx500dit_release(info);
			return NULL;
		}

		dom = dsx500dit_domainof(ni, nip);
		if (parent_suffix[0] == '\0')
		{
			local_suffix = dom;
		}
		else
		{
			local_suffix = dsx500_make_dn(parent_suffix, dom);
			ni_name_free(&dom);
		}

		ni_free(nip);
	}

	info->local_suffix = casecstring_to_dsdata(local_suffix);

	(void) dsx500dit_getparents(ni, parent_suffix, &info->parent_referrals);
	(void) dsx500dit_getchildren(ni, local_suffix, &info->child_referrals, &info->child_count);

	ni_name_free(&local_suffix);
	ni_name_free(&parent_suffix);

	if (freeit) 
	{
		ni_free(ni);
	}

	return info;
}
