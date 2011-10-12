/*
 * Copyright (c) 2008-2010 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This ds contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this ds except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * ds.
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

#include <mach/mach.h>

kern_return_t
libinfoDSmig_do_Response_async(mach_port_t server, char *reply, mach_msg_type_number_t replyCnt, vm_offset_t ooreply, mach_msg_type_number_t ooreplyCnt, mach_vm_address_t callbackAddr, security_token_t servertoken)
{
	return KERN_SUCCESS;
}

#ifdef DS_AVAILABLE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <ils.h>
#include "kvbuf.h"
#include <pwd.h>
#include <grp.h>
#include <fstab.h>
#include <netdb.h>
#include <notify.h>
#include <notify_keys.h>
#include <si_data.h>
#include <si_module.h>
#include <netdb_async.h>
#include <net/if.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>
#include "DSlibinfoMIG.h"
#include "DSmemberdMIG.h"
#ifdef DEBUG
#include <asl.h>
#endif

#define SOCK_UNSPEC 0
#define IPPROTO_UNSPEC 0

#define IPV6_ADDR_LEN 16
#define IPV4_ADDR_LEN 4

#define WANT_NOTHING 0
#define WANT_A4_ONLY 1
#define WANT_A6_ONLY 2
#define WANT_A6_PLUS_MAPPED_A4 3
#define WANT_MAPPED_A4_ONLY 4

/* ONLY TO BE USED BY getipv6nodebyaddr */
#define WANT_A6_OR_MAPPED_A4_IF_NO_A6 5

#define MAX_LOOKUP_ATTEMPTS 10

#define INET_NTOP_AF_INET_OFFSET 4
#define INET_NTOP_AF_INET6_OFFSET 8

mach_port_t _ds_port;
mach_port_t _mbr_port;

extern uint32_t gL1CacheEnabled;

static pthread_key_t _ds_serv_cache_key = 0;

static void
_ds_child(void)
{
	_ds_port = MACH_PORT_NULL;
	_mbr_port = MACH_PORT_NULL;
}

static int _si_opendirectory_disabled;

void
_si_disable_opendirectory(void)
{
	_si_opendirectory_disabled = 1;
	_ds_port = MACH_PORT_NULL;
	_mbr_port = MACH_PORT_NULL;
}

int
_ds_running(void)
{
	kern_return_t status;
	char *od_debug_mode = NULL;
	
	if (_ds_port != MACH_PORT_NULL) return 1;
	
	if (_si_opendirectory_disabled) return 0;
	pthread_atfork(NULL, NULL, _ds_child);
	
	if (!issetugid()) {
		od_debug_mode = getenv("OD_DEBUG_MODE");
	}
	
	if (od_debug_mode) {
		status = bootstrap_look_up(bootstrap_port, kDSStdMachDSLookupPortName "_debug", &_ds_port);
	} else {
		status = bootstrap_look_up2(bootstrap_port, kDSStdMachDSLookupPortName, 
									&_ds_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
	}
	if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;
	
	if (od_debug_mode) {
		status = bootstrap_look_up(bootstrap_port, kDSStdMachDSMembershipPortName "_debug", &_mbr_port);
	} else {
		status = bootstrap_look_up2(bootstrap_port, kDSStdMachDSMembershipPortName, 
									&_mbr_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
	}
	if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _mbr_port = MACH_PORT_NULL;
	
	return (_ds_port != MACH_PORT_NULL);
}

static void
_ds_serv_cache_free(void *x)
{
	if (x != NULL) si_item_release(x);
}

static kern_return_t
LI_DSLookupGetProcedureNumber(const char *name, int32_t *procno)
{
	kern_return_t status;
	security_token_t token;
	uint32_t n, len;

	if (name == NULL) return KERN_FAILURE;

	len = strlen(name) + 1;
	if (len == 1) return KERN_FAILURE;

	token.val[0] = -1;
	token.val[1] = -1;

	if (_ds_running() == 0) return KERN_FAILURE;
	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (n = 0; (_ds_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		status = libinfoDSmig_GetProcedureNumber(_ds_port, (char *)name, procno, &token);

		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1);
			status = bootstrap_look_up2(bootstrap_port, kDSStdMachDSLookupPortName, &_ds_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
			if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s status %u", name, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s auth failure uid=%d", name, token.val[0]);
#endif
		return KERN_FAILURE;
	}

#ifdef DEBUG
	asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupGetProcedureNumber %s = %d", name, *procno);
#endif
	return status;
}

static kern_return_t
LI_DSLookupQuery(int32_t procno, kvbuf_t *request, kvarray_t **reply)
{
	kern_return_t status;
	security_token_t token;
	uint32_t n;
	mach_msg_type_number_t illen, oolen;
	char ilbuf[MAX_MIG_INLINE_DATA];
	vm_offset_t oobuf;
	kvbuf_t *out;

	if (reply == NULL) return KERN_FAILURE;
	if ((request != NULL) && ((request->databuf == NULL) || (request->datalen == 0))) return KERN_FAILURE;

	token.val[0] = -1;
	token.val[1] = -1;
	*reply = NULL;

	if (_ds_running() == 0) return KERN_FAILURE;
	if (_ds_port == MACH_PORT_NULL) return KERN_FAILURE;

	status = MIG_SERVER_DIED;
	for (n = 0; (_ds_port != MACH_PORT_NULL) && (status == MIG_SERVER_DIED) && (n < MAX_LOOKUP_ATTEMPTS); n++)
	{
		illen = 0;
		oolen = 0;
		oobuf = 0;

		if (request != NULL)
		{
			status = libinfoDSmig_Query(_ds_port, procno, request->databuf, request->datalen, ilbuf, &illen, &oobuf, &oolen, &token);
		}
		else
		{
			status = libinfoDSmig_Query(_ds_port, procno, "", 0, ilbuf, &illen, &oobuf, &oolen, &token);
		}

		if (status == MACH_SEND_INVALID_DEST)
		{
			mach_port_mod_refs(mach_task_self(), _ds_port, MACH_PORT_RIGHT_SEND, -1);
			status = bootstrap_look_up2(bootstrap_port, kDSStdMachDSLookupPortName, &_ds_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
			if ((status != BOOTSTRAP_SUCCESS) && (status != BOOTSTRAP_UNKNOWN_SERVICE)) _ds_port = MACH_PORT_NULL;
			status = MIG_SERVER_DIED;
		}
	}

	if (status != KERN_SUCCESS)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d status %u", procno, status);
#endif
		return status;
	}

	if (token.val[0] != 0)
	{
#ifdef DEBUG
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d auth failure uid=%d", procno, token.val[0]);
#endif
		if ((oolen > 0) && (oobuf != 0)) vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
		return KERN_FAILURE;
	}

	out = (kvbuf_t *)calloc(1, sizeof(kvbuf_t));
	if (out == NULL)
	{
		if ((oolen > 0) && (oobuf != 0)) vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
		return KERN_FAILURE;
	}

	if ((oolen > 0) && (oobuf != 0))
	{
		out->datalen = oolen;
		out->databuf = malloc(oolen);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
			return KERN_FAILURE;
		}

		memcpy(out->databuf, (char *)oobuf, oolen);
		vm_deallocate(mach_task_self(), (vm_address_t)oobuf, oolen);
	}
	else if (illen > 0)
	{
		out->datalen = illen;
		out->databuf = malloc(illen);
		if (out->databuf == NULL)
		{
			free(out);
			*reply = NULL;
			return KERN_FAILURE;
		}

		memcpy(out->databuf, ilbuf, illen);
	}

	*reply = kvbuf_decode(out);
	if (*reply == NULL)
	{
		/* DS returned no data */
		free(out->databuf);
		free(out);
	}

#ifdef DEBUG
	asl_log(NULL, NULL, ASL_LEVEL_DEBUG, "_DSLookupQuery %d status OK", procno);
#endif
	return status;
}

/* notify SPI */
uint32_t notify_peek(int token, uint32_t *val);

typedef struct
{
	int notify_token_global;
	int notify_token_user;
	int notify_token_group;
	int notify_token_host;
	int notify_token_service;
} ds_si_private_t;

static uid_t
audit_token_uid(audit_token_t a)
{
	/*
	 * This should really call audit_token_to_au32,
	 * but that's in libbsm, not in a Libsystem library.
	 */
	return (uid_t)a.val[1];
}

static void
ds_get_validation(si_mod_t *si, uint64_t *a, uint64_t *b, int cat)
{
	ds_si_private_t *pp;
	uint32_t peek;
	int status;

	if (si == NULL) return;

	pp = (ds_si_private_t *)si->private;
	if (pp == NULL) return;

	if (a != NULL)
	{
		*a = 0;
		status = notify_peek(pp->notify_token_global, &peek);
		if (status == NOTIFY_STATUS_OK) *a = ntohl(peek);
	}

	if (b != NULL)
	{
		*b = 0;
		peek = 0;
		status = NOTIFY_STATUS_FAILED;

		if (cat == CATEGORY_USER) status = notify_peek(pp->notify_token_user, &peek);
		else if (cat == CATEGORY_GROUP) status = notify_peek(pp->notify_token_group, &peek);
		else if (cat == CATEGORY_GROUPLIST) status = notify_peek(pp->notify_token_group, &peek);
		else if (cat == CATEGORY_HOST_IPV4) status = notify_peek(pp->notify_token_host, &peek);
		else if (cat == CATEGORY_HOST_IPV6) status = notify_peek(pp->notify_token_host, &peek);
		else if (cat == CATEGORY_SERVICE) status = notify_peek(pp->notify_token_service, &peek);

		if (status == NOTIFY_STATUS_OK) *b = ntohl(peek);
	}
}

static si_list_t *
ds_list(si_mod_t *si, int cat, const char *procname, int *procnum, void *extra, si_item_t *(*extract)(si_mod_t *, kvarray_t *, void *, uint64_t, uint64_t), kvbuf_t *request)
{
	si_item_t *item;
	si_list_t *list;
	kvarray_t *reply;
	kern_return_t status;
	uint64_t va, vb;

	if (*procnum < 0)
	{
		status = LI_DSLookupGetProcedureNumber(procname, procnum);
		if (status != KERN_SUCCESS) return NULL;
	}

	reply = NULL;
	ds_get_validation(si, &va, &vb, cat);
	status = LI_DSLookupQuery(*procnum, request, &reply);

	if ((status != KERN_SUCCESS) || (reply == NULL)) return NULL;

	list = NULL;
	while (reply->curr < reply->count)
	{
		item = extract(si, reply, extra, va, vb);
		list = si_list_add(list, item);
		si_item_release(item);
	}

	kvarray_free(reply);

	return list;
}

static si_item_t *
ds_item(si_mod_t *si, int cat, const char *procname, int *procnum, void *extra, si_item_t *(*extract)(si_mod_t *, kvarray_t *, void *, uint64_t, uint64_t), kvbuf_t *request)
{
	si_item_t *item;
	kvarray_t *reply;
	kern_return_t status;
	uint64_t va, vb;

	if (*procnum < 0)
	{
		status = LI_DSLookupGetProcedureNumber(procname, procnum);
		if (status != KERN_SUCCESS) return NULL;
	}

	reply = NULL;
	ds_get_validation(si, &va, &vb, cat);
	status = LI_DSLookupQuery(*procnum, request, &reply);

	if ((status != KERN_SUCCESS) || (reply == NULL)) return NULL;

	item = extract(si, reply, extra, va, vb);
	kvarray_free(reply);

	return item;
}

/*
 * Extract the next user entry from a kvarray.
 */
static si_item_t *
extract_user(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct passwd tmp;
	uint32_t d, k, kcount;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	memset(&tmp, 0, sizeof(struct passwd));

	tmp.pw_uid = -2;
	tmp.pw_gid = -2;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "pw_name"))
		{
			if (tmp.pw_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "pw_passwd"))
		{
			if (tmp.pw_passwd != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_passwd = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "pw_uid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_uid = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "pw_gid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_gid = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "pw_change"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_change = atol(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "pw_expire"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.pw_expire = atol(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "pw_class"))
		{
			if (tmp.pw_class != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_class = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "pw_gecos"))
		{
			if (tmp.pw_gecos != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_gecos = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "pw_dir"))
		{
			if (tmp.pw_dir != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_dir = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "pw_shell"))
		{
			if (tmp.pw_shell != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.pw_shell = (char *)in->dict[d].val[k][0];
		}
	}

	if (tmp.pw_name == NULL) tmp.pw_name = "";
	if (tmp.pw_passwd == NULL) tmp.pw_passwd = "";
	if (tmp.pw_class == NULL) tmp.pw_class = "";
	if (tmp.pw_gecos == NULL) tmp.pw_gecos = "";
	if (tmp.pw_dir == NULL) tmp.pw_dir = "";
	if (tmp.pw_shell == NULL) tmp.pw_shell = "";

	return (si_item_t *)LI_ils_create("L4488ss44LssssL", (unsigned long)si, CATEGORY_USER, 1, valid_global, valid_cat, tmp.pw_name, tmp.pw_passwd, tmp.pw_uid, tmp.pw_gid, tmp.pw_change, tmp.pw_class, tmp.pw_gecos, tmp.pw_dir, tmp.pw_shell, tmp.pw_expire);
}

static si_item_t *
extract_group(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct group tmp;
	char *empty[1];
	uint32_t d, k, kcount;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct group));

	tmp.gr_gid = -2;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "gr_name"))
		{
			if (tmp.gr_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "gr_passwd"))
		{
			if (tmp.gr_passwd != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_passwd = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "gr_gid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.gr_gid = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "gr_mem"))
		{
			if (tmp.gr_mem != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_mem = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.gr_name == NULL) tmp.gr_name = "";
	if (tmp.gr_passwd == NULL) tmp.gr_passwd = "";
	if (tmp.gr_mem == NULL) tmp.gr_mem = empty;

	return (si_item_t *)LI_ils_create("L4488ss4*", (unsigned long)si, CATEGORY_GROUP, 1, valid_global, valid_cat, tmp.gr_name, tmp.gr_passwd, tmp.gr_gid, tmp.gr_mem);
}

static void
_free_addr_list(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++) free(l[i]);
	free(l);
}

/* map ipv4 addresses and append to v6 list */
static int 
_map_v4(char ***v6, uint32_t n6, char **v4, uint32_t n4)
{
	struct in6_addr a6;
	uint32_t i;

	a6.__u6_addr.__u6_addr32[0] = 0x00000000;
	a6.__u6_addr.__u6_addr32[1] = 0x00000000;
	a6.__u6_addr.__u6_addr32[2] = htonl(0x0000ffff);

	if (*v6 == NULL)
	{
		*v6 = (char **)calloc(n4 + 1, sizeof(char *));
	}
	else
	{
		*v6 = (char **)reallocf(*v6, (n6 + n4 + 1) * sizeof(char *));
	}

	if (*v6 == NULL) return -1;

	for (i = 0; i < n4; i++)
	{
		(*v6)[n6] = (char *)calloc(1, IPV6_ADDR_LEN);
		if ((*v6)[n6] == NULL) return -1;

		memcpy(&(a6.__u6_addr.__u6_addr32[3]), v4[i], IPV4_ADDR_LEN);
		memcpy((*v6)[n6], &(a6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);

		n6++;
	}

	return 0;
}

static si_item_t *
extract_netgroup(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	const char *host, *user, *domain;
	uint32_t d, k, kcount;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	kcount = in->dict[d].kcount;

	host = NULL;
	user = NULL;
	domain = NULL;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "host"))
		{
			if (host != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			host = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "user"))
		{
			if (user != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			user = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "domain"))
		{
			if (domain != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			domain = (char *)in->dict[d].val[k][0];
		}
	}

	if (host == NULL) host = "";
	if (user == NULL) user = "";
	if (domain == NULL) domain = "";

	return (si_item_t *)LI_ils_create("L4488sss", (unsigned long)si, CATEGORY_ALIAS, 1, valid_global, valid_cat, host, user, domain);
}

static si_item_t *
extract_alias(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct aliasent tmp;
	char *empty[1];
	uint32_t d, k, kcount;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct group));

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "alias_name"))
		{
			if (tmp.alias_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.alias_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "alias_members"))
		{
			if (tmp.alias_members != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.alias_members_len = in->dict[d].vcount[k];
			tmp.alias_members = (char **)in->dict[d].val[k];
		}
		else if (string_equal(in->dict[d].key[k], "alias_local"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.alias_local = atoi(in->dict[d].val[k][0]);
		}
	}

	return (si_item_t *)LI_ils_create("L4488s4*4", (unsigned long)si, CATEGORY_ALIAS, 1, valid_global, valid_cat, tmp.alias_name, tmp.alias_members_len, tmp.alias_members, tmp.alias_local);
}

static si_item_t *
extract_host(si_mod_t *si, kvarray_t *in, void *extra, uint64_t valid_global, uint64_t valid_cat)
{
	struct hostent tmp;
	si_item_t *out;
	uint32_t i, d, k, kcount, vcount, v4count, v6count, want;
	int status, addr_len;
	int family, addr_count;
	struct in_addr a4;
	struct in6_addr a6;
	char **v4addrs, **v6addrs;
	char *empty[1];

	v4addrs = NULL;
	v6addrs = NULL;
	v4count = 0;
	v6count = 0;
	addr_count = 0;
	addr_len = sizeof(void *);

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct hostent));

	family = AF_INET;
	tmp.h_length = IPV4_ADDR_LEN;

	want = WANT_A4_ONLY;
	if (extra != NULL) want = *(uint32_t *)extra;

	if (want != WANT_A4_ONLY)
	{
		family = AF_INET6;
		tmp.h_length = IPV6_ADDR_LEN;
	}

	tmp.h_addrtype = family;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "h_name"))
		{
			if (tmp.h_name != NULL) continue;

			vcount = in->dict[d].vcount[k];
			if (vcount == 0) continue;

			tmp.h_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "h_aliases"))
		{
			if (tmp.h_aliases != NULL) continue;

			vcount = in->dict[d].vcount[k];
			if (vcount == 0) continue;

			tmp.h_aliases = (char **)in->dict[d].val[k];
		}
		else if (string_equal(in->dict[d].key[k], "h_ipv4_addr_list"))
		{
			if (v4addrs != NULL) continue;

			v4count = in->dict[d].vcount[k];
			if (v4count == 0) continue;

			v4addrs = (char **)calloc(v4count + 1, sizeof(char *));
			if (v4addrs == NULL)
			{
				_free_addr_list(v6addrs);
				return NULL;
			}

			for (i = 0; i < v4count; i++)
			{
				v4addrs[i] = calloc(1, IPV4_ADDR_LEN);
				if (v4addrs[i] == NULL)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memset(&a4, 0, sizeof(struct in_addr));
				status = inet_pton(AF_INET, in->dict[d].val[k][i], &a4);
				if (status != 1)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memcpy(v4addrs[i], &a4, IPV4_ADDR_LEN);
			}
		}
		else if (string_equal(in->dict[d].key[k], "h_ipv6_addr_list"))
		{
			if (v6addrs != NULL) continue;

			v6count = in->dict[d].vcount[k];
			if (v6count == 0) continue;

			v6addrs = (char **)calloc(v6count + 1, sizeof(char *));
			if (v6addrs == NULL)
			{
				_free_addr_list(v4addrs);
				return NULL;
			}

			for (i = 0; i < v6count; i++)
			{
				v6addrs[i] = calloc(1, IPV6_ADDR_LEN);
				if (v6addrs[i] == NULL)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memset(&a6, 0, sizeof(struct in6_addr));
				status = inet_pton(AF_INET6, in->dict[d].val[k][i], &a6);
				if (status != 1)
				{
					_free_addr_list(v4addrs);
					_free_addr_list(v6addrs);
					return NULL;
				}

				memcpy(v6addrs[i], &(a6.__u6_addr.__u6_addr32[0]), IPV6_ADDR_LEN);
			}
		}
	}

	if (tmp.h_name == NULL) tmp.h_name = "";
	if (tmp.h_aliases == NULL) tmp.h_aliases = empty;

	if (want == WANT_A4_ONLY)
	{
		_free_addr_list(v6addrs);
		if (v4addrs == NULL) return NULL;

		tmp.h_addr_list = v4addrs;
		out = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, valid_global, valid_cat, tmp.h_name, tmp.h_aliases, tmp.h_addrtype, tmp.h_length, tmp.h_addr_list);
		_free_addr_list(v4addrs);
		return out;
	}
	else if ((want == WANT_A6_ONLY) || ((want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) && (v6count > 0)))
	{
		_free_addr_list(v4addrs);
		if (v6addrs == NULL) return NULL;

		tmp.h_addr_list = v6addrs;
		out = (si_item_t *)LI_ils_create("L4488s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, valid_global, valid_cat, tmp.h_name, tmp.h_aliases, tmp.h_addrtype, tmp.h_length, tmp.h_addr_list);
		_free_addr_list(v6addrs);
		return out;
	}

	/*
	 * At this point, want is WANT_A6_PLUS_MAPPED_A4, WANT_MAPPED_A4_ONLY,
	 * or WANT_A6_OR_MAPPED_A4_IF_NO_A6.  In the last case, there are no ipv6
	 * addresses, so that case degenerates into WANT_MAPPED_A4_ONLY.
	 */
	if (want == WANT_A6_OR_MAPPED_A4_IF_NO_A6) want = WANT_MAPPED_A4_ONLY;

	if (want == WANT_MAPPED_A4_ONLY)
	{
		_free_addr_list(v6addrs);
		v6addrs = NULL;
		v6count = 0;
	}

	status = _map_v4(&v6addrs, v6count, v4addrs, v4count);
	_free_addr_list(v4addrs);
	if (status != 0)
	{
		_free_addr_list(v6addrs);
		return NULL;
	}

	if (v6addrs == NULL) return NULL;

	tmp.h_addr_list = v6addrs;
	out = (si_item_t *)LI_ils_create("L4488s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, valid_global, valid_cat, tmp.h_name, tmp.h_aliases, tmp.h_addrtype, tmp.h_length, tmp.h_addr_list);
	_free_addr_list(v6addrs);
	return out;
}

static si_item_t *
extract_network(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct netent tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct netent));

	tmp.n_addrtype = AF_INET;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "n_name"))
		{
			if (tmp.n_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.n_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "n_net"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.n_net = inet_network(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "n_addrtype"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.n_addrtype = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "n_aliases"))
		{
			if (tmp.n_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.n_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.n_name == NULL) tmp.n_name = "";
	if (tmp.n_aliases == NULL) tmp.n_aliases = empty;

	return (si_item_t *)LI_ils_create("L4488s*44", (unsigned long)si, CATEGORY_NETWORK, 1, valid_global, valid_cat, tmp.n_name, tmp.n_aliases, tmp.n_addrtype, tmp.n_net);
}

static si_item_t *
extract_service(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct servent tmp;
	char *empty[1];
	uint32_t d, k, kcount;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct servent));

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "s_name"))
		{
			if (tmp.s_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "s_aliases"))
		{
			if (tmp.s_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_aliases = (char **)in->dict[d].val[k];
		}
		else if (string_equal(in->dict[d].key[k], "s_port"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.s_port = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "s_proto"))
		{
			if (tmp.s_proto != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.s_proto = (char *)in->dict[d].val[k][0];
		}
	}

	if (tmp.s_name == NULL) tmp.s_name = "";
	if (tmp.s_proto == NULL) tmp.s_proto = "";
	if (tmp.s_aliases == NULL) tmp.s_aliases = empty;

	/* strange but correct */
	tmp.s_port = htons(tmp.s_port);

	return (si_item_t *)LI_ils_create("L4488s*4s", (unsigned long)si, CATEGORY_SERVICE, 1, valid_global, valid_cat, tmp.s_name, tmp.s_aliases, tmp.s_port, tmp.s_proto);
}

static si_item_t *
extract_protocol(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct protoent tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct protoent));

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "p_name"))
		{
			if (tmp.p_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.p_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "p_proto"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.p_proto = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "p_aliases"))
		{
			if (tmp.p_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.p_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.p_name == NULL) tmp.p_name = "";
	if (tmp.p_aliases == NULL) tmp.p_aliases = empty;

	return (si_item_t *)LI_ils_create("L4488s*4", (unsigned long)si, CATEGORY_PROTOCOL, 1, valid_global, valid_cat, tmp.p_name, tmp.p_aliases, tmp.p_proto);
}

static si_item_t *
extract_rpc(si_mod_t *si, kvarray_t *in, void *ignored, uint64_t valid_global, uint64_t valid_cat)
{
	struct rpcent tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, sizeof(struct rpcent));

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "r_name"))
		{
			if (tmp.r_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.r_name = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "r_number"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.r_number = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "r_aliases"))
		{
			if (tmp.r_aliases != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.r_aliases = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.r_name == NULL) tmp.r_name = "";
	if (tmp.r_aliases == NULL) tmp.r_aliases = empty;

	return (si_item_t *)LI_ils_create("L4488s*4", (unsigned long)si, CATEGORY_RPC, 1, valid_global, valid_cat, tmp.r_name, tmp.r_aliases, tmp.r_number);
}

static si_item_t *
extract_fstab(si_mod_t *si, kvarray_t *in, void *extra, uint64_t valid_global, uint64_t valid_cat)
{
	struct fstab tmp;
	uint32_t d, k, kcount;
	char *file;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;

	file = NULL;
	if (extra != NULL) file = (char *)extra;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	memset(&tmp, 0, sizeof(struct fstab));

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "fs_spec"))
		{
			if (tmp.fs_spec != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_spec = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "fs_file"))
		{
			if (tmp.fs_file != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_file = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "fs_vfstype"))
		{
			if (tmp.fs_vfstype != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_vfstype = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "fs_mntops"))
		{
			if (tmp.fs_mntops != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_mntops = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "fs_type"))
		{
			if (tmp.fs_type != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.fs_type = (char *)in->dict[d].val[k][0];
		}
		else if (string_equal(in->dict[d].key[k], "fs_freq"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.fs_freq = atoi(in->dict[d].val[k][0]);
		}
		else if (string_equal(in->dict[d].key[k], "fs_passno"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.fs_passno = atoi(in->dict[d].val[k][0]);
		}
	}

	if (tmp.fs_spec == NULL) tmp.fs_spec = "";
	if (tmp.fs_file == NULL) tmp.fs_file = "";
	if (tmp.fs_vfstype == NULL) tmp.fs_vfstype = "";
	if (tmp.fs_mntops == NULL) tmp.fs_mntops = "";
	if (tmp.fs_type == NULL) tmp.fs_type = "";

	if ((file != NULL) && string_not_equal(file, tmp.fs_file)) return NULL;

	return (si_item_t *)LI_ils_create("L4488sssss44", (unsigned long)si, CATEGORY_FS, 1, valid_global, valid_cat, tmp.fs_spec, tmp.fs_file, tmp.fs_vfstype, tmp.fs_mntops, tmp.fs_type, tmp.fs_freq, tmp.fs_passno);
}

static si_item_t *
extract_mac_mac(si_mod_t *si, kvarray_t *in, void *extra, uint64_t valid_global, uint64_t valid_cat)
{
	uint32_t d, k, kcount;
	char *cmac;
	si_item_t *out;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;
	if (extra == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	kcount = in->dict[d].kcount;

	cmac = NULL;
	for (k = 0; k < kcount; k++)
	{
		if ((cmac == NULL) && (string_equal(in->dict[d].key[k], "mac")))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			cmac = si_standardize_mac_address(in->dict[d].val[k][0]);
			if (cmac == NULL) return NULL;
		}
	}

	if (cmac == NULL) return NULL;

	out = (si_item_t *)LI_ils_create("L4488ss", (unsigned long)si, CATEGORY_MAC, 1, valid_global, valid_cat, extra, cmac);
	free(cmac);
	return out;
}

static si_item_t *
extract_mac_name(si_mod_t *si, kvarray_t *in, void *extra, uint64_t valid_global, uint64_t valid_cat)
{
	uint32_t d, k, kcount;
	const char *name;
	si_item_t *out;

	if (si == NULL) return NULL;
	if (in == NULL) return NULL;
	if (extra == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	kcount = in->dict[d].kcount;

	name = NULL;
	for (k = 0; k < kcount; k++)
	{
		if ((name == NULL) && (string_equal(in->dict[d].key[k], "name")))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			name = in->dict[d].val[k][0];
		}
	}

	if (name == NULL) return NULL;

	out = (si_item_t *)LI_ils_create("L4488ss", (unsigned long)si, CATEGORY_MAC, 1, valid_global, valid_cat, name, extra);
	return out;
}

static si_item_t *
ds_user_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("login", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_USER, "getpwnam", &proc, NULL, extract_user, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_user_byuid(si_mod_t *si, uid_t uid)
{
	static int proc = -1;
	char val[16];
	kvbuf_t *request;
	si_item_t *item;

	snprintf(val, sizeof(val), "%d", (int)uid);
	request = kvbuf_query_key_val("uid", val);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_USER, "getpwuid", &proc, NULL, extract_user, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_user_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_USER, "getpwent", &proc, NULL, extract_user, NULL);
}

static si_item_t *
ds_group_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_GROUP, "getgrnam", &proc, NULL, extract_group, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_group_bygid(si_mod_t *si, gid_t gid)
{
	static int proc = -1;
	char val[16];
	kvbuf_t *request;
	si_item_t *item;

	snprintf(val, sizeof(val), "%d", (int)gid);
	request = kvbuf_query_key_val("gid", val);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_GROUP, "getgrgid", &proc, NULL, extract_group, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_group_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_GROUP, "getgrent", &proc, NULL, extract_group, NULL);
}

static si_item_t *
ds_grouplist(si_mod_t *si, const char *name)
{
	struct passwd *pw;
	kern_return_t kstatus, ks2;
	uint32_t i, j, count, uid, basegid, gidptrCnt;
	int32_t *gidp;
	gid_t *gidptr;
	audit_token_t token;
	si_item_t *user, *item;
	char **gidlist;
	uint64_t va, vb;
	size_t gidptrsz;
	int n;

	if (name == NULL) return NULL;

	user = ds_user_byname(si, name);
	if (user == NULL) return NULL;

	pw = (struct passwd *)((uintptr_t)user + sizeof(si_item_t));
	uid = pw->pw_uid;
	basegid = pw->pw_gid;

	free(user);

	count = 0;
	gidptr = NULL;
	gidptrCnt = 0;
	gidptrsz = 0;
	memset(&token, 0, sizeof(audit_token_t));

	if (_mbr_port == MACH_PORT_NULL)
	{
		kstatus = bootstrap_look_up2(bootstrap_port, kDSStdMachDSMembershipPortName, &_mbr_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
	}

	for (n = 0; n < MAX_LOOKUP_ATTEMPTS; n++)
	{
		kstatus = memberdDSmig_GetAllGroups(_mbr_port, uid, &count, &gidptr, &gidptrCnt, &token);
		if (kstatus != MACH_SEND_INVALID_DEST) break;

		mach_port_mod_refs(mach_task_self(), _mbr_port, MACH_PORT_RIGHT_SEND, -1);

		ks2 = bootstrap_look_up2(bootstrap_port, kDSStdMachDSMembershipPortName, &_mbr_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
		if ((ks2 != BOOTSTRAP_SUCCESS) && (ks2 != BOOTSTRAP_UNKNOWN_SERVICE))
		{
			_mbr_port = MACH_PORT_NULL;
			break;
		}
	}

	if (kstatus != KERN_SUCCESS) return NULL;
	if (gidptr == NULL) return NULL;

	/* gidptrCnt is the size, but it was set to number of groups (by DS) in 10.6 and earlier */
	gidptrsz = gidptrCnt;
	if (count == gidptrCnt) gidptrsz = gidptrCnt * sizeof(gid_t);

	if ((audit_token_uid(token) != 0) || (count == 0))
	{
		if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);
		return NULL;
	}

	gidlist = (char **)calloc(count + 1, sizeof(char *));
	if (gidlist == NULL)
	{
		if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);
		return NULL;
	}

	for (i = 0; i < count; i++) 
	{
		gidp = (int32_t *)calloc(1, sizeof(int32_t));
		if (gidp == NULL)
		{
			for (j = 0; j < i; j++) free(gidlist[j]);
			free(gidlist);
			count = 0;
			break;
		}

		*gidp = gidptr[i];
		gidlist[i] = (char *)gidp;
	}

	if (count == 0)
	{
		if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);
		return NULL;
	}

	va = 0;
	vb = 0;
	ds_get_validation(si, &va, &vb, CATEGORY_GROUPLIST);

	item = (si_item_t *)LI_ils_create("L4488s44a", (unsigned long)si, CATEGORY_GROUPLIST, 1, va, vb, name, basegid, count, gidlist);

	if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);

	for (i = 0; i <= count; i++) free(gidlist[i]);
	free(gidlist);

	return item;
}

static si_list_t *
ds_netgroup_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_list_t *list;

	request = kvbuf_query_key_val("netgroup", name);
	if (request == NULL) return NULL;

	list = ds_list(si, CATEGORY_NETGROUP, "getnetgrent", &proc, NULL, extract_netgroup, request);

	kvbuf_free(request);

	return list;
}

static int
check_innetgr(kvarray_t *in)
{
	uint32_t d, k, kcount;

	if (in == NULL) return 0;

	d = in->curr;
	if (d >= in->count) return 0;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (string_equal(in->dict[d].key[k], "result"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			return atoi(in->dict[d].val[k][0]);
		}
	}

	return 0;
}

static int
ds_in_netgroup(si_mod_t *si, const char *group, const char *host, const char *user, const char *domain)
{
	int is_innetgr;
	kvbuf_t *request;
	kvarray_t *reply;
	kern_return_t status;
	static int proc = -1;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("innetgr", &proc);
		if (status != KERN_SUCCESS) return 0;
	}

	/* Encode NULL */
	if (group == NULL) group = "";
	if (host == NULL) host = "";
	if (user == NULL) user = "";
	if (domain == NULL) domain = "";

	request = kvbuf_query("ksksksks", "netgroup", group, "host", host, "user", user, "domain", domain);
	if (request == NULL) return 0;

	reply = NULL;
	status = LI_DSLookupQuery(proc, request, &reply);
	kvbuf_free(request);

	if ((status != KERN_SUCCESS) || (reply == NULL)) return 0;

	is_innetgr = check_innetgr(reply);

	kvarray_free(reply);

	return is_innetgr;
}

static si_item_t *
ds_alias_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_ALIAS, "alias_getbyname", &proc, NULL, extract_alias, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_alias_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_ALIAS, "alias_getent", &proc, NULL, extract_alias, NULL);
}

static si_item_t *
ds_host_byname(si_mod_t *si, const char *name, int af, const char *ignored, uint32_t *err)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;
	uint32_t want4, want6;
	int cat;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (name == NULL)
	{
		*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	want4 = 0;
	want6 = 0;

	cat = CATEGORY_HOST_IPV4;

	if (af == AF_INET)
	{
		want4 = 1;
	}
	else if (af == AF_INET6)
	{
		want6 = 1;
		cat = CATEGORY_HOST_IPV6;
	}
	else
	{
		*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	request = kvbuf_query("kskuku", "name", name, "ipv4", want4, "ipv6", want6);
	if (request == NULL)
	{
		*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	item = ds_item(si, cat, "gethostbyname", &proc, NULL, extract_host, request);

	if ((item == NULL) && (err != NULL)) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_host_byaddr(si_mod_t *si, const void *addr, int af, const char *ignored, uint32_t *err)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;
	struct in_addr addr4;
	struct in6_addr addr6;
	char val[64 + 1 + IF_NAMESIZE];
	int cat;
	uint32_t want;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	cat = CATEGORY_HOST_IPV4;

	memset(&addr4, 0, sizeof(struct in_addr));
	memset(&addr6, 0, sizeof(struct in6_addr));
	memset(val, 0, sizeof(val));

	want = WANT_A4_ONLY;

	if (af == AF_INET)
	{
		memcpy(&addr4.s_addr, addr, IPV4_ADDR_LEN);
		if (inet_ntop(af, &addr4, val, sizeof(val)) == NULL)
		{
			*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
			return NULL;
		}
	}
	else if (af == AF_INET6)
	{
		want = WANT_A6_ONLY;
		cat = CATEGORY_HOST_IPV6;
		memcpy(&addr6, addr, IPV6_ADDR_LEN);
		if (inet_ntop(af, &addr6, val, sizeof(val)) == NULL)
		{
			*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
			return NULL;
		}
	}
	else
	{
		*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	request = kvbuf_query("ksku", "address", val, "family", af);
	if (request == NULL)
	{
		*err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	item = ds_item(si, cat, "gethostbyaddr", &proc, &want, extract_host, request);

	if ((item == NULL) && (err != NULL)) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_host_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_HOST_IPV4, "gethostent", &proc, NULL, extract_host, NULL);
}

static si_item_t *
ds_network_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_NETWORK, "getnetbyname", &proc, NULL, extract_network, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_network_byaddr(si_mod_t *si, uint32_t addr)
{
	static int proc = -1;
	unsigned char f1, f2, f3;
	char val[64];
	kvbuf_t *request;
	si_item_t *item;

	f1 = addr & 0xff;
	addr >>= 8;
	f2 = addr & 0xff;
	addr >>= 8;
	f3 = addr & 0xff;

	if (f3 != 0) snprintf(val, sizeof(val), "%u.%u.%u", f3, f2, f1);
	else if (f2 != 0) snprintf(val, sizeof(val), "%u.%u", f2, f1);
	else snprintf(val, sizeof(val), "%u", f1);

	request = kvbuf_query_key_val("net", val);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_NETWORK, "getnetbyaddr", &proc, NULL, extract_network, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_network_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_NETWORK, "getnetent", &proc, NULL, extract_network, NULL);
}

static si_item_t *
ds_service_byname(si_mod_t *si, const char *name, const char *proto)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;
	struct servent *s;

	if (name == NULL) name = "";
	if (proto == NULL) proto = "";

	/* Check our local service cache (see ds_addrinfo). */
	item = pthread_getspecific(_ds_serv_cache_key);
	if (item != NULL)
	{
		s = (struct servent *)((uintptr_t)item + sizeof(si_item_t));
		if (string_equal(name, s->s_name)) return si_item_retain(item);
	}

	request = kvbuf_query("ksks", "name", name, "proto", proto);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_SERVICE, "getservbyname", &proc, NULL, extract_service, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_service_byport(si_mod_t *si, int port, const char *proto)
{
	static int proc = -1;
	uint16_t sport;
	char val[16];
	kvbuf_t *request;
	si_item_t *item;

	if (proto == NULL) proto = "";

	sport = port;
	snprintf(val, sizeof(val), "%d", ntohs(sport));

	request = kvbuf_query("ksks", "port", val, "proto", proto);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_SERVICE, "getservbyport", &proc, NULL, extract_service, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_service_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_SERVICE, "getservent", &proc, NULL, extract_service, NULL);
}

static si_item_t *
ds_protocol_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_PROTOCOL, "getprotobyname", &proc, NULL, extract_protocol, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_protocol_bynumber(si_mod_t *si, int number)
{
	static int proc = -1;
	char val[16];
	kvbuf_t *request;
	si_item_t *item;

	snprintf(val, sizeof(val), "%d", number);
	request = kvbuf_query_key_val("number", val);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_PROTOCOL, "getprotobynumber", &proc, NULL, extract_protocol, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_protocol_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_PROTOCOL, "getprotoent", &proc, NULL, extract_protocol, NULL);
}

static si_item_t *
ds_rpc_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_RPC, "getrpcbyname", &proc, NULL, extract_rpc, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_rpc_bynumber(si_mod_t *si, int number)
{
	static int proc = -1;
	char val[16];
	kvbuf_t *request;
	si_item_t *item;

	snprintf(val, sizeof(val), "%u", (uint32_t)number);
	request = kvbuf_query_key_val("number", val);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_RPC, "getrpcbynumber", &proc, NULL, extract_rpc, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_rpc_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_RPC, "getrpcent", &proc, NULL, extract_rpc, NULL);
}

static si_item_t *
ds_fs_byspec(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_FS, "getfsbyname", &proc, NULL, extract_fstab, request);

	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_fs_all(si_mod_t *si)
{
	static int proc = -1;

	return ds_list(si, CATEGORY_FS, "getfsent", &proc, NULL, extract_fstab, NULL);
}

static si_item_t *
ds_fs_byfile(si_mod_t *si, const char *name)
{
	si_item_t *item;
	si_list_t *list;
	uint32_t i;
	struct fstab *f;

	if (name == NULL) return NULL;

	list = ds_fs_all(si);
	if (list == NULL) return NULL;

	item = NULL;
	for (i = 0; (i < list->count) && (item == NULL); i++)
	{
		f = (struct fstab *)((uintptr_t)(list->entry[i]) + sizeof(si_item_t));
		if (string_equal(name, f->fs_file)) item = si_item_retain(list->entry[i]);
	}

	si_list_release(list);
	return item;
}

static si_item_t *
ds_mac_byname(si_mod_t *si, const char *name)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;

	request = kvbuf_query_key_val("name", name);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_MAC, "getmacbyname", &proc, (void *)name, extract_mac_mac, request);

	kvbuf_free(request);
	return item;
}

static si_item_t *
ds_mac_bymac(si_mod_t *si, const char *mac)
{
	static int proc = -1;
	kvbuf_t *request;
	si_item_t *item;
	char *cmac;

	cmac = si_standardize_mac_address(mac);
	if (cmac == NULL) return NULL;

	request = kvbuf_query_key_val("mac", cmac);
	if (request == NULL) return NULL;

	item = ds_item(si, CATEGORY_MAC, "gethostbymac", &proc, cmac, extract_mac_name, request);

	free(cmac);
	kvbuf_free(request);
	return item;
}

static si_list_t *
ds_addrinfo(si_mod_t *si, const void *node, const void *serv, uint32_t family, uint32_t socktype, uint32_t proto, uint32_t flags, const char *ignored, uint32_t *err)
{
	static int proc = -1;
	si_list_t *list, *out;
	si_item_t *item;
	kvbuf_t *request = NULL;
	kvdict_t *dict;
	kvarray_t *reply = NULL;
	kern_return_t status = 0;
	uint16_t scope = 0;
	int i, k, kcount, d;
	uint32_t port;

	struct hostent *h;
	char *h_name = NULL;
	int h_aliases_cnt = 0;
	const char **h_aliases = NULL;
	struct in_addr *a4 = NULL;
	struct in6_addr *a6 = NULL;
	int a4_cnt = 0;
	int a6_cnt = 0;
	struct servent *s;
	const char *s_name = NULL;
	int s_aliases_cnt = 0;
	const char **s_aliases = NULL;
	uint16_t s_port = 0;
	const char *s_proto = NULL;
	const char *protoname;
	int wantv4, wantv6;

	int numericserv = ((flags & AI_NUMERICSERV) != 0);
	int numerichost = ((flags & AI_NUMERICHOST) != 0);

	wantv4 = 0;
	wantv6 = 0;

	if (node != NULL)
	{
		wantv4 = ((family != AF_INET6) || (flags & AI_V4MAPPED));
		wantv6 = (family != AF_INET);
	}

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if (proc < 0)
	{
		status = LI_DSLookupGetProcedureNumber("gethostbyname_service", &proc);
		if (status != KERN_SUCCESS)
		{
			if (err != NULL) *err = SI_STATUS_EAI_SYSTEM;
			return NULL;
		}
	}

	/* look up canonical name of numeric host */
	if ((numerichost == 1) && (flags & AI_CANONNAME) && (node != NULL))
	{
		item = si_host_byaddr(si, node, family, NULL, NULL);
		if (item != NULL)
		{
			h = (struct hostent *)((uintptr_t)item + sizeof(si_item_t));
			h_name = strdup(h->h_name);
			si_item_release(item);
		}
	}

	if (numericserv == 1)
	{
		s_port = *(int16_t *)serv;
	}

	if ((numericserv == 0) || (numerichost == 0))
	{
		request = kvbuf_new();
		if (request != NULL)
		{
			kvbuf_add_dict(request);

			if (numerichost == 0)
			{
				kvbuf_add_key(request, "name");
				kvbuf_add_val(request, node);
				kvbuf_add_key(request, "ipv4");
				kvbuf_add_val(request, wantv4 ? "1" : "0");
				kvbuf_add_key(request, "ipv6");
				kvbuf_add_val(request, wantv6 ? "1" : "0");
			}

			if (numericserv == 0)
			{
				protoname = NULL;   
				if (proto == IPPROTO_UDP) protoname = "udp";
				if (proto == IPPROTO_TCP) protoname = "tcp";

				kvbuf_add_key(request, "s_name");
				kvbuf_add_val(request, serv);
				if (protoname != NULL)
				{
					kvbuf_add_key(request, "s_proto");
					kvbuf_add_val(request, protoname);
				}
			}

			status = LI_DSLookupQuery(proc, request, &reply);
			kvbuf_free(request);
		}
		else
		{
			if (err != NULL) *err = SI_STATUS_EAI_SYSTEM;
		}
	}

	if ((status != KERN_SUCCESS) || (reply == NULL))
	{
		free(h_name);
		return NULL;
	}

	for (d = 0; d < reply->count; d++)
	{
		dict = reply->dict + d;
		kcount = dict->kcount;

		for (k = 0; k < kcount; k++)
		{
			if (string_equal(dict->key[k], "h_name"))
			{
				if (dict->vcount[k] == 0) continue;
				h_name = strdup(dict->val[k][0]);
			}
			else if (string_equal(dict->key[k], "h_aliases"))
			{
				h_aliases_cnt = dict->vcount[k];
				h_aliases = (const char **)calloc(h_aliases_cnt, sizeof(char *));
				if (h_aliases == NULL) h_aliases_cnt = 0;

				for (i = 0; i < h_aliases_cnt; ++i)
				{
					h_aliases[i] = dict->val[k][i];
				}
			}
			else if (wantv4 && (string_equal(dict->key[k], "h_ipv4_addr_list")))
			{
				a4_cnt = dict->vcount[k];
				a4 = calloc(a4_cnt, sizeof(struct in_addr));
				if (a4 == NULL) a4_cnt = 0;

				for (i = 0; i < a4_cnt; ++i)
				{
					memset(&a4[i], 0, sizeof(struct in_addr));
					inet_pton(AF_INET, dict->val[k][i], &a4[i]);
				}
			}
			else if (wantv6 && (string_equal(dict->key[k], "h_ipv6_addr_list")))
			{
				a6_cnt = dict->vcount[k];
				a6 = calloc(a6_cnt, sizeof(struct in6_addr));
				if (a6 == NULL) a6_cnt = 0;

				for (i = 0; i < a6_cnt; ++i)
				{
					memset(&a6[i], 0, sizeof(struct in6_addr));
					inet_pton(AF_INET6, dict->val[k][i], &a6[i]);
				}
			}
			else if (string_equal(dict->key[k], "s_name"))
			{
				if (dict->vcount[k] == 0) continue;
				s_name = dict->val[k][0];
			}
			else if (string_equal(dict->key[k], "s_port"))
			{
				if (dict->vcount[k] == 0) continue;
				s_port = atoi(dict->val[k][0]);
			}
			else if (string_equal(dict->key[k], "s_aliases"))
			{
				s_aliases_cnt = dict->vcount[k];
				s_aliases = (const char **)calloc(s_aliases_cnt+1, sizeof(char *));
				if (s_aliases == NULL) s_aliases_cnt = 0;

				for (i = 0; i < s_aliases_cnt; ++i)
				{
					s_aliases[i] = dict->val[k][i];
				}
			}
			else if (string_equal(dict->key[k], "s_proto"))
			{
				if (dict->vcount[k] == 0) continue;
				s_proto = dict->val[k][0];
			}
		}
	}

	kvarray_free(reply);

	/* check if we actually got back what we wanted */
	if (((wantv4 || wantv6) && (a4_cnt == 0) && (a6_cnt == 0)) || ((serv != NULL) && (s_port == 0)))
	{
		if (err != NULL) *err = SI_STATUS_EAI_NONAME;

		free(h_name);
		free(h_aliases);
		free(s_aliases);
		free(a4);
		free(a6);

		return NULL;
	}

	/*
	 * Cache the service entry regardless of whether there is a host match.
	 * This allows later modules to get the service entry quickly.
	 * This should really be part of the general cache mechanism, but that's
	 * not currently visible outside of the search module.
	 */
	if ((s_name != NULL) && (s_port != 0))
	{
		port = htons(s_port);

		item = pthread_getspecific(_ds_serv_cache_key);
		if (item != NULL)
		{
			s = (struct servent *)((uintptr_t)item + sizeof(si_item_t));
			if ((port != s->s_port) || string_not_equal(s_name, s->s_name))
			{
				si_item_release(item);
				item = NULL;
			}
		}

		if (item == NULL)
		{
			item = LI_ils_create("L4488s*4s", (unsigned long)si, CATEGORY_SERVICE, 1, (uint64_t)1, (uint64_t)1, s_name, s_aliases, port, s_proto);
			pthread_setspecific(_ds_serv_cache_key, item);
		}
	}

	/* Construct the addrinfo list from the returned addresses (if found). */
	out = NULL;
	for (i = 0; i < a6_cnt; i++)
	{
		list = si_addrinfo_list(si, flags, socktype, proto, NULL, &a6[i], s_port, scope, NULL, h_name);
		out = si_list_concat(out, list);
		si_list_release(list);
	}

	for (i = 0; i < a4_cnt; i++)
	{
		list = si_addrinfo_list(si, flags, socktype, proto, &a4[i], NULL, s_port, 0, h_name, NULL);
		out = si_list_concat(out, list);
		si_list_release(list);
	}

	free(h_name);
	free(h_aliases);
	free(s_aliases);
	free(a4);
	free(a6);

	return out;
}

static int
ds_is_valid(si_mod_t *si, si_item_t *item)
{
	si_mod_t *src;
	ds_si_private_t *pp;
	int status;
	uint32_t oldval, newval;

	if (si == NULL) return 0;
	if (item == NULL) return 0;
	if (si->name == NULL) return 0;
	if (item->src == NULL) return 0;

	pp = (ds_si_private_t *)si->private;
	if (pp == NULL) return 0;

	src = (si_mod_t *)item->src;

	if (src->name == NULL) return 0;
	if (string_not_equal(si->name, src->name)) return 0;

	/* check global invalidation */
	oldval = item->validation_a;
	newval = -1;
	status = notify_peek(pp->notify_token_global, &newval);
	if (status != NOTIFY_STATUS_OK) return 0;

	newval = ntohl(newval);
	if (oldval != newval) return 0;

	oldval = item->validation_b;
	newval = -1;
	if (item->type == CATEGORY_USER) status = notify_peek(pp->notify_token_user, &newval);
	else if (item->type == CATEGORY_GROUP) status = notify_peek(pp->notify_token_group, &newval);
	else if (item->type == CATEGORY_HOST_IPV4) status = notify_peek(pp->notify_token_host, &newval);
	else if (item->type == CATEGORY_HOST_IPV6) status = notify_peek(pp->notify_token_host, &newval);
	else if (item->type == CATEGORY_SERVICE) status = notify_peek(pp->notify_token_service, &newval);
	else return 0;

	if (status != NOTIFY_STATUS_OK) return 0;

	newval = ntohl(newval);
	if (oldval != newval) return 0;

	return 1;
}

si_mod_t *
si_module_static_ds(void)
{
	static const struct si_mod_vtable_s ds_vtable =
	{
		.sim_is_valid = &ds_is_valid,

		.sim_user_byname = &ds_user_byname,
		.sim_user_byuid = &ds_user_byuid,
		.sim_user_all = &ds_user_all,

		.sim_group_byname = &ds_group_byname,
		.sim_group_bygid = &ds_group_bygid,
		.sim_group_all = &ds_group_all,

		.sim_grouplist = &ds_grouplist,

		.sim_netgroup_byname = &ds_netgroup_byname,
		.sim_in_netgroup = &ds_in_netgroup,

		.sim_alias_byname = &ds_alias_byname,
		.sim_alias_all = &ds_alias_all,

		.sim_host_byname = &ds_host_byname,
		.sim_host_byaddr = &ds_host_byaddr,
		.sim_host_all = &ds_host_all,

		.sim_network_byname = &ds_network_byname,
		.sim_network_byaddr = &ds_network_byaddr,
		.sim_network_all = &ds_network_all,

		.sim_service_byname = &ds_service_byname,
		.sim_service_byport = &ds_service_byport,
		.sim_service_all = &ds_service_all,

		.sim_protocol_byname = &ds_protocol_byname,
		.sim_protocol_bynumber = &ds_protocol_bynumber,
		.sim_protocol_all = &ds_protocol_all,

		.sim_rpc_byname = &ds_rpc_byname,
		.sim_rpc_bynumber = &ds_rpc_bynumber,
		.sim_rpc_all = &ds_rpc_all,

		.sim_fs_byspec = &ds_fs_byspec,
		.sim_fs_byfile = &ds_fs_byfile,
		.sim_fs_all = &ds_fs_all,

		.sim_mac_byname = &ds_mac_byname,
		.sim_mac_bymac = &ds_mac_bymac,

		/* si_mac_all not supported */
		.sim_mac_all = NULL,

		/* si_addrinfo not supported */
		.sim_wants_addrinfo = NULL,
		.sim_addrinfo = NULL,
	};

	static si_mod_t si =
	{
		.vers = 1,
		.refcount = 1,
		.flags = SI_MOD_FLAG_STATIC,

		.private = NULL,
		.vtable = &ds_vtable,
	};

	static dispatch_once_t once;
	dispatch_once(&once, ^{
		pthread_key_create(&_ds_serv_cache_key, _ds_serv_cache_free);

		si.name = strdup("ds");
		ds_si_private_t *pp = calloc(1, sizeof(ds_si_private_t));

		if (pp != NULL)
		{
			pp->notify_token_global = -1;
			pp->notify_token_user = -1;
			pp->notify_token_group = -1;
			pp->notify_token_host = -1;
			pp->notify_token_service = -1;
		}

		/*
		 * Don't register for notifications if the cache is disabled.
		 * notifyd (notably) disables the cache to prevent deadlocks.
		 */
		if (gL1CacheEnabled != 0)
		{
			/*
			 * Errors in registering for cache invalidation notifications are ignored.
			 * If there are failures, the tokens remain set to -1 which just causes 
			 * cached items to be invalidated.
			 */
			notify_register_check(kNotifyDSCacheInvalidation, &(pp->notify_token_global));
			notify_register_check(kNotifyDSCacheInvalidationUser, &(pp->notify_token_user));
			notify_register_check(kNotifyDSCacheInvalidationGroup, &(pp->notify_token_group));
			notify_register_check(kNotifyDSCacheInvalidationHost, &(pp->notify_token_host));
			notify_register_check(kNotifyDSCacheInvalidationService, &(pp->notify_token_service));
		}

		si.private = pp;
	});

	return &si;
}

#endif /* DS_AVAILABLE */
