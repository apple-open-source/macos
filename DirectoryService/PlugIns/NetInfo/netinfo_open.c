/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * @header netinfo_open
 * packaged management of opening netinfo hierarchy nodes with timeout values
 * and determination of domain name with caching of the value
 * main routines for use outside of this package are:
 * ni_status netinfo_open(void *ni, char *name, void **newni, int timeout)
 * ni_status netinfo_connect(struct in_addr *addr, const char *tag, void **ni, int timeout)
 * ni_status netinfo_local(void **domain, int timeout)
 * char* netinfo_domainname(void *ni)
 * void netinfo_clear(void)
 */

#include <netinfo/ni.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/clnt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <notify.h>
#include "netinfo_open.h"

#define LOCAL_PORT 1033
#define NAME_UNKNOWN "###UNKNOWN###"
#define NUM_RETRY 4
#define SHARED_FLAGS_ISROOT   0x00000001
#define SHARED_ISROOT_TIMEOUT 300

/* Private API from Libinfo */
typedef struct ni_private
{
	int naddrs;
	struct in_addr *addrs;
	int whichwrite;
	ni_name *tags;
	int pid;
	int tsock;
	int tport;
	CLIENT *tc;
	long tv_sec;
	long rtv_sec;
	long wtv_sec;
	int abort;
	int needwrite;
	int uid;
	ni_name passwd;
} ni_private;

typedef struct shared_ref_s
{
	void *domain;
	char *relname;
	uint32_t flags;
	time_t timeout;
	uint32_t isroot_time;
	struct shared_ref_s *parent;
} shared_ref_t;

static int shared_ref_count = 0;
static shared_ref_t *shared_ref_local = NULL;
static shared_ref_t **shared_ref = NULL;
static int shared_ref_local_token = -1;

static shared_ref_t *
_shared_ni_new_ref(void *domain, int timeout)
{
	shared_ref_t *a;

	if (domain == NULL) return NULL;

	if (shared_ref_count == 0) shared_ref = (shared_ref_t **)calloc(1, sizeof(shared_ref_t *));
	else shared_ref = (shared_ref_t **)realloc(shared_ref, (shared_ref_count + 1) * sizeof(shared_ref_t *));

	if (shared_ref == NULL) return NULL;

	a = (shared_ref_t *)calloc(1, sizeof(shared_ref_t));
	if (a == NULL) return NULL;

	a->domain = domain;
	a->timeout = timeout;

	shared_ref[shared_ref_count] = a;
	shared_ref_count++;

	return a;
}

static ni_status
_shared_ni_ping(unsigned short port, struct in_addr addr, int t)
{
	struct sockaddr_in sin;
	int sock;
	CLIENT *cl;
	struct timeval timeout, retry;
	enum clnt_stat stat;

	memset(&sin, 0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_port = port;
	sin.sin_addr = addr;

	timeout.tv_sec = t;
	timeout.tv_usec = 0;

	retry.tv_sec = t / NUM_RETRY;
	retry.tv_usec = (t - (NUM_RETRY * retry.tv_sec)) * (1000000 / NUM_RETRY);

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) return NI_FAILED;

	cl = clntudp_create(&sin, NI_PROG, NI_VERS, timeout, &sock);
	if (cl == NULL)
	{
		close(sock);
		return NI_FAILED;
	}

	clnt_control(cl, CLSET_RETRY_TIMEOUT, (char *)&retry);

	stat = clnt_call(cl, _NI_PING, (xdrproc_t)xdr_void, (char *)NULL, (xdrproc_t)xdr_void, (char *)NULL, timeout);
	clnt_destroy(cl);
	close(sock);

	if (stat != RPC_SUCCESS) return NI_FAILED;

	return NI_OK;
}

static int
_shared_ni_match(shared_ref_t *s, struct in_addr *a, const char *t)
{
	ni_private *ni;
	unsigned long i;

	if (s == NULL) return 0;
	if (s->domain == NULL) return 0;
	if (a == NULL) return 0;
	if (t == NULL) return 0;

	ni = (ni_private *)s->domain;
	if (ni == NULL) return 0;

	for (i = 0; i < ni->naddrs; i++)
	{
		if ((ni->addrs[i].s_addr == a->s_addr) && (strcmp(ni->tags[i], t) ==  0)) return 1;
	}

	return 0;
}

static shared_ref_t *
_shared_ni_find(struct in_addr *addr, const char *tag)
{
	int i;

	for (i = 0; i < shared_ref_count; i++)
	{
		if (_shared_ni_match(shared_ref[i], addr, tag)) return shared_ref[i];
	}

	return NULL;
}

static shared_ref_t *
_shared_ni_find_ni(void *ni)
{
	int i;

	if (ni == NULL) return NULL;

	for (i = 0; i < shared_ref_count; i++)
	{
		if (shared_ref[i]->domain == ni) return shared_ref[i];
	}

	return NULL;
}

static void
_shared_ni_add_addr_tag(ni_private *ni, ni_name addrtag)
{
	struct in_addr addr;
	ni_name tag;
	char *slash;

	slash = strchr(addrtag, '/');
	if (slash == NULL) return;

	tag = slash + 1;
	if (tag[0] == '\0') return;

	*slash = '\0';

	if (inet_aton(addrtag, &addr) == 0) return;

	if (ni->naddrs == 0)
	{
		ni->addrs = (struct in_addr *)calloc(1, sizeof(struct in_addr));
		if (ni->addrs == NULL) return;

		ni->tags = (ni_name *)calloc(1, sizeof(ni_name));
		if (ni->tags == NULL) return;
	}
	else
	{
		ni->addrs = (struct in_addr *)realloc(ni->addrs, ((ni->naddrs + 1) * sizeof(struct in_addr)));
		if (ni->addrs == NULL) return;

		ni->tags = (ni_name *)realloc(ni->tags, ((ni->naddrs + 1) * sizeof(ni_name)));
		if (ni->tags == NULL) return;
	}

	ni->addrs[ni->naddrs] = addr;
	ni->tags[ni->naddrs] = ni_name_dup(tag);
	ni->naddrs++;
}

static int
_shared_ni_add_addr(void *ni, ni_index ido, ni_name tag, ni_private *target_ni)
{
	ni_id id;
	ni_namelist nl;
	struct in_addr addr;
	int i;
	ni_status status;

	if (ni == NULL) return 0;
	if (tag == NULL) return 0;
	if (target_ni == NULL) return 0;

	id.nii_object = ido;
	NI_INIT(&nl);

	status = ni_lookupprop(ni, &id, "ip_address", &nl);
	if (status != NI_OK) return 0;

	if (nl.ni_namelist_len == 0) return 0;

	if (target_ni->naddrs == 0) 
	{
		target_ni->addrs = (struct in_addr *)malloc(nl.ni_namelist_len * sizeof(struct in_addr));
		target_ni->tags = (ni_name *)malloc(nl.ni_namelist_len * sizeof(ni_name));
	}
	else
	{
		target_ni->addrs = (struct in_addr *)realloc(target_ni->addrs, ((target_ni->naddrs + nl.ni_namelist_len) * sizeof(struct in_addr)));
		target_ni->tags = (ni_name *)realloc(target_ni->tags, ((target_ni->naddrs + nl.ni_namelist_len) * sizeof(ni_name)));
	}

	for (i = 0; i < nl.ni_namelist_len; i++)
	{
		addr.s_addr = inet_addr(nl.ni_namelist_val[i]);
		target_ni->addrs[target_ni->naddrs] = addr;
		target_ni->tags[target_ni->naddrs] = ni_name_dup(tag);
		target_ni->naddrs++;
	}

	ni_namelist_free(&nl);
	return 1;
}

static ni_private *
_shared_ni_alloc(int timeout)
{
	ni_private *ni;

	ni = (ni_private *)calloc(1, sizeof(*ni));
	ni->whichwrite = -1;
	ni->pid = getpid();
	ni->tsock = -1;
	ni->tport = -1;
	ni->rtv_sec = timeout;
	ni->wtv_sec = timeout;
	ni->abort = 1;
	ni->uid = getuid();
	return ni;
}

static int
_shared_ni_serves_match(const char *domain, ni_name domtag, ni_name *tag)
{
	int len = strlen(domain);
	ni_name sep;

	sep = index(domtag, '/');
	if (sep == NULL) return 0;

	if ((strncmp(domain, domtag, len) == 0) && (domtag[len] == '/'))
	{
		*tag = ni_name_dup(sep + 1);
		return 1;
	}

	return 0;
}

static ni_private *
_shared_ni_csopen(void *ni, const char *name, int timeout)
{
	ni_id nid;
	ni_idlist ids;
	ni_entrylist entries;
	ni_proplist pl;
	ni_index i;
	ni_index j;
	ni_name tag;
	ni_private *nip;
	ni_status status;

	if (ni == NULL) return NULL;
	if (name == NULL) return NULL;
	if (!strcmp(name, "..")) return NULL;

	nip = _shared_ni_alloc(timeout);

	ni_setreadtimeout(ni, timeout);
	ni_setwritetimeout(ni, timeout);
	ni_setabort(ni, 1);

	if (!strcmp(name, "."))
	{
		/* check for server list */
		NI_INIT(&pl);
		if (ni_statistics(ni, &pl) == NI_OK)
		{
			i = ni_proplist_match(pl, "domain_servers", NULL);
			if (i != NI_INDEX_NULL)
			{
				if (pl.ni_proplist_val[i].nip_val.ni_namelist_len > 0)
				{
					for (j = 0; j < pl.ni_proplist_val[i].nip_val.ni_namelist_len; j++)
					{
						_shared_ni_add_addr_tag(nip, pl.ni_proplist_val[i].nip_val.ni_namelist_val[j]);
					}

					ni_proplist_free(&pl);
					return nip;
				}
			}

			ni_proplist_free(&pl);
		}
	}

	if (ni_root(ni, &nid) != NI_OK)
	{
		free(nip);
		return NULL;
	}

	NI_INIT(&ids);
	if (ni_lookup(ni, &nid, "name", "machines", &ids) != NI_OK)
	{
		free(nip);
		return NULL;
	}

	nid.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);

	NI_INIT(&entries);
	if (ni_list(ni, &nid, "serves", &entries) != NI_OK)
	{
		free(nip);
		return NULL;
	}

	for (i = 0; i < entries.niel_len; i++)
	{
		if (entries.niel_val[i].names == NULL) continue;

		for (j = 0; j < entries.niel_val[i].names->ni_namelist_len; j++)
		{
			if (_shared_ni_serves_match(name, entries.niel_val[i].names->ni_namelist_val[j], &tag))
			{
				_shared_ni_add_addr(ni, entries.niel_val[i].id, tag, nip);
				ni_name_free(&tag);
			}
		}
	}

	ni_entrylist_free(&entries);

	if (nip->naddrs == 0)
	{
		free(nip);
		return NULL;
	}

	nid.nii_object = 0;
	nid.nii_instance = 0;

	status = ni_self(nip, &nid);

	if (status != NI_OK) 
	{
		ni_free(nip);
		return NULL;
	}

	return nip;
}

static shared_ref_t *
_shared_ni_connection(struct in_addr *addr, const char *tag, int timeout)
{
	struct sockaddr_in sa;
	void *d, *tmp;
	ni_status status;
	ni_id root;
	shared_ref_t *s;

	if (addr == NULL) return NULL;
	if (tag == NULL) return NULL;

	s = _shared_ni_find(addr, tag);
	if (s != NULL) return s;

	memset(&sa, 0, sizeof(struct in_addr));
	sa.sin_family = AF_INET;
	sa.sin_addr = *addr;

	d = ni_connect(&sa, tag);
	if (d == NULL) return NULL;

	ni_setreadtimeout(d, timeout);
	ni_setwritetimeout(d, timeout);
	ni_setabort(d, 1);

	if (strcmp(tag, "local") != 0)
	{
		tmp = _shared_ni_csopen(d, ".", timeout);
		ni_free(d);
		d = tmp;

		if (d == NULL) return NULL;

		ni_setreadtimeout(d, timeout);
		ni_setwritetimeout(d, timeout);
		ni_setabort(d, 1);
	}

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(d, &root);

	if (status != NI_OK) 
	{
		ni_free(d);
		return NULL;
	}

	s = _shared_ni_new_ref(d, timeout);
	return s;
}

static shared_ref_t *
_shared_ni_local(int timeout)
{
	struct sockaddr_in sin;
	int sock, status;
	void *d;
	ni_id root;

	if (shared_ref_local != NULL) return shared_ref_local;

	memset(&sin, 0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(LOCAL_PORT);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	status = connect(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	close(sock);
	if (status < 0) return NULL;

	status = _shared_ni_ping(sin.sin_port, sin.sin_addr, timeout);
	if (status != NI_OK) return NULL;

	d = ni_connect(&sin, "local");
	ni_setreadtimeout(d, timeout);
	ni_setwritetimeout(d, timeout);
	ni_setabort(d, 1);

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(d, &root);

	if (status != NI_OK)
	{
		ni_free(d);
		return NULL;
	}

	shared_ref_local = _shared_ni_new_ref(d, timeout);

	status = notify_register_plain(NETINFO_BINDING_KEY, &shared_ref_local_token);
	if (status != NOTIFY_STATUS_OK) shared_ref_local_token = -1;

	return shared_ref_local;
}

static shared_ref_t *
_shared_ni_parent(shared_ref_t *s, int timeout)
{
	ni_rparent_res rpres;
	ni_private *ni;
	struct in_addr addr;
	shared_ref_t *p;
	struct timeval tnew, tcurr;
	time_t now;
	enum clnt_stat rpc_status;
	ni_status status;
	int nstatus, binding;

	if (s == NULL) return NULL;
	if (s->parent != NULL) return s->parent;

	now = 0;

	if ((s == shared_ref_local) && (s->flags & SHARED_FLAGS_ISROOT))
	{
		if (shared_ref_local_token == -1)
		{
			nstatus = notify_register_plain(NETINFO_BINDING_KEY, &shared_ref_local_token);
			if (nstatus != NOTIFY_STATUS_OK) shared_ref_local_token = -1;
		}

		if (shared_ref_local_token != -1)
		{
			nstatus = notify_get_state(shared_ref_local_token, &binding);
			if ((nstatus == NOTIFY_STATUS_OK) && (binding != BINDING_STATE_NETROOT))
			{
				s->isroot_time = 0;
				s->flags &= ~SHARED_FLAGS_ISROOT;
			}
		}
	}

	if (s->flags & SHARED_FLAGS_ISROOT)
	{
		now = time(NULL);
		if (now <= (s->isroot_time + SHARED_ISROOT_TIMEOUT)) return NULL;
		s->flags &= ~SHARED_FLAGS_ISROOT;
		s->isroot_time = 0;
	}

	ni = (ni_private *)s->domain;

	status = NI_FAILED;

	if (ni->tc != NULL)
	{
		memset(&rpres, 0, sizeof(ni_rparent_res));

		tnew.tv_sec = timeout;
		tnew.tv_usec = 0;

		tcurr.tv_sec = 0;
		tcurr.tv_usec = 0;

		clnt_control(ni->tc, CLGET_TIMEOUT, (void *)&tcurr);
		clnt_control(ni->tc, CLSET_TIMEOUT, (void *)&tnew);

		rpc_status = clnt_call(ni->tc, _NI_RPARENT, (void *)xdr_void, NULL, (void *)xdr_ni_rparent_res, &rpres, tnew);

		clnt_control(ni->tc, CLSET_TIMEOUT, (void *)&tcurr);

		if (rpc_status == RPC_SUCCESS) status = rpres.status;
	}

	if (status == NI_NETROOT)
	{
		if (now == 0) now = time(NULL);
		s->isroot_time = now;
		s->flags |= SHARED_FLAGS_ISROOT;
		return NULL;
	}

	if (status != NI_OK) return NULL;

	addr.s_addr = htonl(rpres.ni_rparent_res_u.binding.addr);
	p = _shared_ni_connection(&addr, rpres.ni_rparent_res_u.binding.tag, timeout);
	free(rpres.ni_rparent_res_u.binding.tag);

	s->parent = p;
	return p;
}

static shared_ref_t *
_shared_ni_relopen(shared_ref_t *s, const char *name, int timeout)
{
	ni_private *ni;
	ni_id root;
	shared_ref_t *x;
	int i;
	ni_status status;

	if (s == NULL) return NULL;

	if (!strcmp(name, ".")) return s;
	if (!strcmp(name, "..")) return _shared_ni_parent(s, timeout);

	if (!strcmp(name, "/"))
	{
		for (;;)
		{
			x = _shared_ni_parent(s, timeout);
			if (x == NULL) return s;
			s = x;
		}
	}

	for (i = 0; i < shared_ref_count; i++)
	{
		if (shared_ref[i] == NULL) continue;
		if (shared_ref[i]->relname == NULL) continue;
		if ((shared_ref[i]->parent == s) && (!strcmp(shared_ref[i]->relname, name))) return shared_ref[i];
	}

	ni = _shared_ni_csopen(s->domain, name, timeout);

	if (ni == NULL)
		return NULL;

	x = _shared_ni_find(&(ni->addrs[0]), ni->tags[0]);
	if (x != NULL)
	{
		ni_free(ni);
		if (x->parent == NULL) x->parent = s;
		if (x->relname == NULL) x->relname = strdup(name);
		return x;
	}

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(ni, &root);

	if (status != NI_OK) 
	{
		ni_free(ni);
		return NULL;
	}

	x = _shared_ni_new_ref(ni, timeout);
	if (x == NULL)
	{
		ni_free(ni);
		return NULL;
	}

	x->parent = s;
	x->relname = strdup(name);

	return x;
}

static ni_name
_shared_ni_escape_domain(ni_name name)
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
_shared_ni_find_domain(void *ni, struct in_addr addr, ni_name tag)
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

	status = ni_lookup(ni, &nid, "name", "machines", &idl);
	if (status != NI_OK) return NULL;

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookup(ni, &nid, "ip_address", inet_ntoa(addr), &idl);
	if (status != NI_OK) return NULL;

	nid.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);

	status = ni_lookupprop(ni, &nid, "serves", &nl);
	if (status != NI_OK) return NULL;

	for (i = 0; i < nl.ninl_len; i++)
	{
		slash = rindex(nl.ninl_val[i], '/');
		if (slash == NULL) continue;

		if (ni_name_match(slash + 1, tag))
		{
			*slash = 0;
			domain = _shared_ni_escape_domain(nl.ninl_val[i]);
			ni_namelist_free(&nl);
			return domain;
		}
	}

	ni_namelist_free(&nl);

	return NULL;
}

static int
_shared_ni_is_my_address(struct ifaddrs *ifa, struct in_addr *a)
{
	struct ifaddrs *p;

	if (ifa == NULL) return 0;
	if (a == NULL) return 0;

	for (p = ifa; p != NULL; p = p->ifa_next)
	{
		if (p->ifa_addr == NULL) continue;
		if ((p->ifa_flags & IFF_UP) == 0) continue;
		if (p->ifa_addr->sa_family != AF_INET) continue;

		if (a->s_addr == ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr) return 1;
	}

	return 0;
}

static char *
_shared_ni_domainof(shared_ref_t *ni, shared_ref_t *parent)
{
	struct sockaddr_in addr;
	char *dom;
	struct ifaddrs *ifa, *p;
	ni_private *nip;

	if (ni == NULL) return ni_name_dup(NAME_UNKNOWN);
	if (parent == NULL) return ni_name_dup(NAME_UNKNOWN);

	nip = (ni_private *)ni->domain;

	addr.sin_addr = nip->addrs[0];

	dom = _shared_ni_find_domain(parent->domain, nip->addrs[0], nip->tags[0]);
	if (dom != NULL) return dom;

	if (getifaddrs(&ifa) < 0) return ni_name_dup(NAME_UNKNOWN);

	if (_shared_ni_is_my_address(ifa, &(addr.sin_addr)))
	{
		/* Try all my non-loopback interfaces */
		for (p = ifa; p != NULL; p = p->ifa_next)
		{
			if (p->ifa_addr == NULL) continue;
			if ((p->ifa_flags & IFF_UP) == 0) continue;
			if (p->ifa_addr->sa_family != AF_INET) continue;
			addr.sin_addr.s_addr = ((struct sockaddr_in *)(p->ifa_addr))->sin_addr.s_addr;

			if (addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) continue;

			dom = _shared_ni_find_domain(parent->domain, addr.sin_addr, nip->tags[0]);
			if (dom != NULL)
			{
				freeifaddrs(ifa);
				return dom;
			}
		}
	}

	freeifaddrs(ifa);

	asprintf(&dom, "%s@%s", nip->tags[0], inet_ntoa(addr.sin_addr));
	return dom;
}

static const char *
_shared_ni_reldomainname(shared_ref_t *s)
{
	shared_ref_t *p;

	if (s == NULL) return NULL;
	if (s->relname != NULL) return (const char *)s->relname;

	p = _shared_ni_parent(s, s->timeout);
	if (p == NULL)
	{
		/* root domain, at least for the moment */
		return "/";
	}

	/* Get my name relative to my parent */
	s->relname = _shared_ni_domainof(s, p);

	return (const char *)s->relname;
}

static char *
_shared_ni_domainname(shared_ref_t *d)
{
	char *s, *t;
	const char *name;
	shared_ref_t *p;

	if (d == NULL) return strdup(NAME_UNKNOWN);

	s = NULL;

	for (p = d; p != NULL; p = _shared_ni_parent(p, p->timeout))
	{
		name = _shared_ni_reldomainname(p);
		if (name == NULL) name = NAME_UNKNOWN;

		if (!strcmp(name, "/"))
		{
			if (s == NULL) asprintf(&s, "/");
			return s;
		}

		if (s == NULL)
		{
			asprintf(&s, "/%s", name);
		}
		else
		{
			asprintf(&t, "/%s%s", name, s);
			free(s);
			s = t;
		}
	}

	return s;
}

#ifdef DEBUG
static void
_shared_ni_print(FILE *f)
{
	int i, delta;
	time_t now;

	fprintf(f, "shared_ref_count = %d\n", shared_ref_count);

	if (shared_ref_count == 0) return;

	now = time(NULL);

	fprintf(f, "    self       ni         parent     name\n");
	for (i = 0; i < shared_ref_count; i++)
	{
		fprintf(f, "%2d: 0x%08x 0x%08x 0x%08x", i, (unsigned int)shared_ref[i], (unsigned int)shared_ref[i]->domain, (unsigned int)shared_ref[i]->parent);
		if (shared_ref[i]->flags & SHARED_FLAGS_ISROOT)
		{
			delta = now - shared_ref[i]->isroot_time;
			fprintf(f, " / (%d)", delta);
		}
		else if (shared_ref[i]->relname != NULL) fprintf(f, " %s", shared_ref[i]->relname);
		fprintf(f, "\n");
	}
}
#endif

void
netinfo_clear(int flags)
{
	int i;

	for (i = 0; i < shared_ref_count; i++)
	{
		if (shared_ref[i] == shared_ref_local)
		{
			if (flags | NETINFO_CLEAR_PRESERVE_LOCAL) continue;
			else shared_ref_local = NULL;
		}

		if (shared_ref[i]->domain != NULL) ni_free(shared_ref[i]->domain);
		if (shared_ref[i]->relname != NULL) free(shared_ref[i]->relname);
	}

	if (shared_ref != NULL) free(shared_ref);
	shared_ref = NULL;
	shared_ref_count = 0;

	if (shared_ref_local != NULL)
	{
		if (shared_ref_local->relname != NULL) free(shared_ref_local->relname);
		shared_ref_local->relname = NULL;
		shared_ref_local->flags = 0;
		shared_ref_local->isroot_time = 0;
		shared_ref_local->parent = NULL;

		shared_ref = (shared_ref_t **)calloc(1, sizeof(shared_ref_t *));
		shared_ref[0] = shared_ref_local;
		shared_ref_count = 1;
	}
}

ni_status
netinfo_local(void **domain, int timeout)
{
	shared_ref_t *s;

	if (domain == NULL) return NI_INVALIDDOMAIN;
	*domain = NULL;

	s = _shared_ni_local(timeout);
	if (s == NULL) return NI_FAILED;

	*domain = s->domain;
	return NI_OK;
}

ni_status
netinfo_connect(struct in_addr *addr, const char *tag, void **ni, int timeout)
{
	shared_ref_t *s;

	if (addr == NULL) return NI_FAILED;
	if (tag == NULL) return NI_FAILED;
	if (ni == NULL) return NI_FAILED;

	*ni = NULL;

	s = _shared_ni_connection(addr, tag, timeout);
	if (s == NULL) return NI_FAILED;

	*ni = s->domain;
	return NI_OK;
}

ni_status
netinfo_open(void *ni, char *name, void **newni, int timeout)
{
	shared_ref_t *s;
	ni_private *p;
	char *cname, *x, *slash;

	if (name == NULL) return NI_FAILED;
	if (newni == NULL) return NI_FAILED;

	s = NULL;

	if (ni == NULL)
	{
		if (!strcmp(name, ".")) return netinfo_local(newni, timeout);

		s = _shared_ni_local(timeout);
	}
	else
	{
		p = (ni_private *)ni;
		s = _shared_ni_connection(&(p->addrs[0]), p->tags[0], timeout);
	}

	if (s == NULL) return NI_FAILED;

	cname = strdup(name);
	x = cname;
	if (x[0] == '/')
	{
		s = _shared_ni_relopen(s, "/", timeout);
		x++;
	}

	if (x[0] == '\0') x = NULL;

	while (x != NULL)
	{
		slash = strchr(x, '/');
		if (slash != NULL) *slash = '\0';

		s = _shared_ni_relopen(s, x, timeout);
		if (s == NULL)
		{
			free(cname);
			return NI_FAILED;
		}

		if (slash == NULL) x = NULL;
		else
		{
			if (x[0] == '\0') x = NULL;
			x = slash+1;
		}
	}

	free(cname);
	if (s == NULL) return NI_FAILED;
	*newni = s->domain;
	return NI_OK;
}

char *
netinfo_domainname(void *ni)
{
	shared_ref_t *s;

	s = _shared_ni_find_ni(ni);
	return _shared_ni_domainname(s);
}
