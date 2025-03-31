/*
 * Copyright (c) 1999-2018 Apple Inc.  All rights reserved.
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
/*
 * Copyright (c) 1989, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <oncrpc/rpc.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <sys/syslog.h>
#include <CoreAnalytics/CoreAnalytics.h>
#include <CoreFoundation/CoreFoundation.h>

#include "common.h"
#include "nfsd_analytics.h"

#define DISABLE_NFSD_ANALYTICS 1

#if DISABLE_NFSD_ANALYTICS

void
nfsd_analytics_config_send(void)
{
};
void
nfsd_analytics_mount_send(__unused SVCXPRT *transp, __unused int mountver)
{
};
void
nfsd_analytics_statistics_send(void)
{
};
void
nfsd_analytics_statistics_regsiter(void)
{
};
void
nfsd_analytics_statistics_unregsiter(void)
{
};
void
nfsd_analytics_exports_send(__unused int hostcount, __unused int hostcount_non_loopback, __unused int opt_flags)
{
};

#else /* DISABLE_NFSD_ANALYTICS */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

static int
is_xctest(void)
{
	static int xctest = -1;

	if (xctest == -1) {
		xctest = getenv("XCTestSessionIdentifier") ? 1 : 0;
	}

	return xctest;
}

#define SKIP_XCTEST if (is_xctest()) return;

void
dict_add_int_as_string(xpc_object_t dict, const char *key, int value)
{
	char str[NAME_MAX] = {};
	snprintf(str, sizeof(str), "%d", value);
	xpc_dictionary_set_string(dict, key, str);
}

/* Config event */

typedef enum {
	BOOLEAN,
	INT,
	STRING,
} type_t;

typedef struct nac_param {
	const char *nac_name;
	size_t     nac_in_size;
	type_t     nac_out_type;
	size_t     nac_offset;
} nac_param_t;

nac_param_t nac_params[] = {
	{ "async", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, async) },
	{ "bonjour", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, bonjour) },
	{ "bonjour_local_domain_only", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, bonjour_local_domain_only) },
	{ "export_hash_size", sizeof(int), STRING, offsetof(struct nfs_conf_server, export_hash_size) },
	{ "fsevents", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, fsevents) },
	{ "materialize_dataless_files", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, materialize_dataless_files) },
	{ "mount_port", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, mount_port) },
	{ "mount_regular_files", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, mount_regular_files) },
	{ "mount_require_resv_port", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, mount_require_resv_port) },
	{ "nfsd_threads", sizeof(int), STRING, offsetof(struct nfs_conf_server, nfsd_threads) },
	{ "port", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, port) },
	{ "reqcache_size", sizeof(int), STRING, offsetof(struct nfs_conf_server, reqcache_size) },
	{ "request_queue_length", sizeof(int), STRING, offsetof(struct nfs_conf_server, request_queue_length) },
	{ "require_resv_port", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, require_resv_port) },
	{ "tcp", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, tcp) },
	{ "udp", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, udp) },
	{ "user_stats", sizeof(int), BOOLEAN, offsetof(struct nfs_conf_server, user_stats) },
	{ "verbose", sizeof(int), STRING, offsetof(struct nfs_conf_server, verbose) },
	{ "wg_delay", sizeof(int), STRING, offsetof(struct nfs_conf_server, wg_delay) },
	{ "wg_delay_v3", sizeof(int), STRING, offsetof(struct nfs_conf_server, wg_delay_v3) },
};

void
nfsd_analytics_config_send(void)
{
	SKIP_XCTEST;

	/* No need to send event if config was not modified from default */
	if (memcmp(&config, &config_defaults, sizeof(struct nfs_conf_server)) == 0) {
		return;
	}

	struct nfs_conf_server _config = config;
	struct nfs_conf_server _config_defaults = config_defaults;
	const void *config_addr = (void *)&_config;
	const void *config_defaults_addr = (void *)&_config_defaults;

	log(LOG_DEBUG, "nfsd_analytics:com.apple.nfs.server.config sending event");
	analytics_send_event_lazy("com.apple.nfs.server.config", ^xpc_object_t (void) {
		xpc_object_t dict = xpc_dictionary_create_empty();
		for (int i = 0; i < ARRAY_SIZE(nac_params); i++) {
		        nac_param_t *param = &nac_params[i];
		        size_t offset = param->nac_offset;
		        type_t type = param->nac_out_type;
		        if (memcmp(config_addr + offset, config_defaults_addr + offset, param->nac_in_size) != 0) {
		                if (type == BOOLEAN) {
		                        xpc_dictionary_set_bool(dict, param->nac_name, *((int *)(config_addr + offset)) ? true : false);
				} else if (type == INT) {
		                        xpc_dictionary_set_int64(dict, param->nac_name, *((int *)(config_addr + offset)));
				} else if (type == STRING) {
		                        dict_add_int_as_string(dict, param->nac_name, *((int *)(config_addr + offset)));
				} else {
		                        log(LOG_ERR, "nfsd_analytics:com.apple.nfs.server.config invalid out type");
				}
			}
		}
		return dict;
	});
}

/* Mount event */

typedef struct nam_convert {
	int        namc_value;
	const char *namc_name;
} nam_convert_t;

nam_convert_t nam_families[] = {
	{ AF_INET, "AF_INET" },
	{ AF_INET6, "AF_INET6" },
	{ AF_LOCAL, "AF_LOCAL" },
};

nam_convert_t nam_types[] = {
	{ SOCK_STREAM, "SOCK_STREAM" },
	{ SOCK_DGRAM, "SOCK_DGRAM" },
	{ SOCK_RAW, "SOCK_RAW" },
};

nam_convert_t nam_versions[] = {
	{ RPCMNT_VER1, "NFSv2" },
	{ RPCMNT_VER3, "NFSv3" },
};

static const char *
nam_convert(nam_convert_t *array, size_t array_size, int value)
{
	for (int i = 0; i < array_size; i++) {
		if (array[i].namc_value == value) {
			return array[i].namc_name;
		}
	}
	return "Unknown";
}

#define nam_convert_family(family) nam_convert(nam_families, ARRAY_SIZE(nam_families), family)
#define nam_convert_type(type) nam_convert(nam_types, ARRAY_SIZE(nam_types), type)
#define nam_convert_version(version) nam_convert(nam_versions, ARRAY_SIZE(nam_versions), version)

void
nfsd_analytics_mount_send(SVCXPRT *transp, int mountver)
{
	SKIP_XCTEST;

	int family, type;
	const char *netid = svc_getnetid(transp);

	if (netid == NULL) {
		log(LOG_ERR, "nfsd_analytics:com.apple.nfs.server.config invalid netid");
		return;
	}

	if (netid2socparms(netid, &family, &type, NULL, false)) {
		log(LOG_ERR, "nfsd_analytics:com.apple.nfs.server.config netid2socparms failure");
		return;
	}

	log(LOG_DEBUG, "nfsd_analytics:com.apple.nfs.server.mount sending event");
	analytics_send_event_lazy("com.apple.nfs.server.mount", ^xpc_object_t (void) {
		xpc_object_t dict = xpc_dictionary_create_empty();
		xpc_dictionary_set_string(dict, "Socket_Family", nam_convert_family(family));
		xpc_dictionary_set_string(dict, "Socket_Type", nam_convert_type(type));
		xpc_dictionary_set_string(dict, "NFS_Version", nam_convert_version(mountver));
		return dict;
	});
}

/* Statistics event */

typedef struct nas_value {
	const char *nas_name;
	size_t     nas_offset;
} nas_value_t;

nas_value_t nas_values[] = {
	{ "NULL", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_NULL]) },
	{ "GETATTR", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_GETATTR]) },
	{ "SETATTR", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_SETATTR]) },
	{ "LOOKUP", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_LOOKUP]) },
	{ "ACCESS", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_ACCESS]) },
	{ "READLINK", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_READLINK]) },
	{ "READ", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_READ]) },
	{ "WRITE", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_WRITE]) },
	{ "CREATE", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_CREATE]) },
	{ "MKDIR", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_MKDIR]) },
	{ "SYMLINK", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_SYMLINK]) },
	{ "MKNOD", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_MKNOD]) },
	{ "REMOVE", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_REMOVE]) },
	{ "RMDIR", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_RMDIR]) },
	{ "RENAME", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_RENAME]) },
	{ "LINK", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_LINK]) },
	{ "READDIR", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_READDIR]) },
	{ "READDIRPLUS", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_READDIRPLUS]) },
	{ "FSSTAT", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_FSSTAT]) },
	{ "FSINFO", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_FSINFO]) },
	{ "PATHCONF", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_PATHCONF]) },
	{ "COMMIT", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_COMMIT]) },
	{ "SVC_RPC_ERRS", offsetof(struct nfsrvstats, srvrpc_errs) },
	{ "SVC_ERRS", offsetof(struct nfsrvstats, srv_errs) },
	{ "SRVCACHE_INPROG_HITS", offsetof(struct nfsrvstats, srvcache_inproghits) },
	{ "SRVCACHE_IDEMONE_HITS", offsetof(struct nfsrvstats, srvcache_idemdonehits) },
	{ "SRVCACHE_NON_IDEMONE_HITS", offsetof(struct nfsrvstats, srvcache_nonidemdonehits) },
	{ "SRVCACHE_MISSES", offsetof(struct nfsrvstats, srvcache_misses) },
	{ "SRVWRITE_WriteOps", offsetof(struct nfsrvstats, srvvop_writes) },
	{ "SRVWRITE_WriteRPC", offsetof(struct nfsrvstats, srvrpccntv3[NFSPROC_WRITE]) },
};

#define STATISTICS_DATA "/var/run/nfs.stats"
#define STATISTICS_FORMAT "%s %llu"
#define STATISTICS_FORMAT_NL STATISTICS_FORMAT "\n"

static uint64_t
stats_get_val(struct nfsrvstats *stp, int index)
{
	return *((uint64_t *)((void *)stp + nas_values[index].nas_offset));
}

static uint64_t *
stats_get_ptr(struct nfsrvstats *stp, int index)
{
	return (uint64_t *)((void *)stp + nas_values[index].nas_offset);
}

static int
nfsd_analytics_event_statistics_write_file(struct nfsrvstats *stp)
{
	FILE *file = fopen(STATISTICS_DATA, "w");
	if (file == NULL) {
		log(LOG_ERR, "%s: Failed to open the file", __FUNCTION__);
		return 1;
	}

	for (int i = 0; i < ARRAY_SIZE(nas_values); i++) {
		fprintf(file, STATISTICS_FORMAT_NL, nas_values[i].nas_name, stats_get_val(stp, i));
	}

	fclose(file);
	return 0;
}

static int
nfsd_analytics_event_statistics_read_file(struct nfsrvstats *stp)
{
	char line[NAME_MAX], key[NAME_MAX];

	FILE *file = fopen(STATISTICS_DATA, "r");
	if (file == NULL) {
		log(LOG_ERR, "%s: Failed to open the file", __FUNCTION__);
		return 1;
	}

	for (int i = 0; i < ARRAY_SIZE(nas_values); i++) {
		fgets(line, sizeof(line), file);
		sscanf(line, STATISTICS_FORMAT, key, stats_get_ptr(stp, i));

		if (strncmp(key, nas_values[i].nas_name, strlen(nas_values[i].nas_name))) {
			log(LOG_ERR, "%s: Incorrect key: got <%s>, expected <%s>", __FUNCTION__, key, nas_values[i].nas_name);
			return 1;
		}
	}

	fclose(file);
	return 0;
}

static void
nfssvc_init_vec(struct iovec *vec, void *buf, size_t *buflen)
{
	vec[0].iov_base = buf;
	vec[0].iov_len = *buflen;
	vec[1].iov_base = buflen;
	vec[1].iov_len = sizeof(*buflen);
}

static int
readsrvstats(struct nfsrvstats *stp)
{
	size_t buflen = sizeof(*stp);
	struct iovec vec[2];

	nfssvc_init_vec(vec, stp, &buflen);
	return nfssvc(NFSSVC_SRVSTATS, vec);
}

void
nfsd_analytics_event_statistics(void)
{
	SKIP_XCTEST;

	int err;
	struct nfsrvstats stats_cur = {}, stats_last = {}, stats_gap = {}, *stats = &stats_cur;
	static struct nfsrvstats stats_zero = {};

	err = readsrvstats(&stats_cur);
	if (err) {
		log(LOG_ERR, "nfsd_analytics:com.apple.nfs.server.statistics failure %d", err);
		return;
	}

	err = nfsd_analytics_event_statistics_read_file(&stats_last);
	if (!err) {
		/* Managed to open the last statistics file */
		int valid = 1;

		/* First check if kernel statistics were not zeroed */
		for (int i = 0; i < ARRAY_SIZE(nas_values); i++) {
			if (stats_get_val(&stats_cur, i) < stats_get_val(&stats_last, i)) {
				valid = 0;
				log(LOG_INFO, "nfsd_analytics:com.apple.nfs.server.statistics kernel statistics were zeroed");
				break;
			}
		}

		/* Substruct with last sample if statistics were not zeroed */
		if (valid) {
			for (int i = 0; i < ARRAY_SIZE(nas_values); i++) {
				*(stats_get_ptr(&stats_gap, i)) = stats_get_val(&stats_cur, i) - stats_get_val(&stats_last, i);
			}
			stats = &stats_gap;
		}
	}

	/* Update stats file with current values */
	nfsd_analytics_event_statistics_write_file(&stats_cur);

	/* No need to send event if statistics were not changed */
	if (memcmp(stats, &stats_zero, sizeof(struct nfsrvstats)) == 0) {
		log(LOG_DEBUG, "nfsd_analytics:com.apple.nfs.server.statistics statistics were not changed ");
		return;
	}

	log(LOG_DEBUG, "nfsd_analytics:com.apple.nfs.server.statistics sending event");
	analytics_send_event_lazy("com.apple.nfs.server.statistics", ^xpc_object_t (void) {
		xpc_object_t dict = xpc_dictionary_create_empty();
		for (int i = 0; i < ARRAY_SIZE(nas_values); i++) {
		        xpc_dictionary_set_uint64(dict, nas_values[i].nas_name, stats_get_val(stats, i));
		}

		/* Server Write Gathering */
		xpc_dictionary_set_uint64(dict, "SRVWRITE_Opsaved", stats->srvrpccntv3[NFSPROC_WRITE] - stats->srvvop_writes);

		return dict;
	});
}

dispatch_source_t statisticsTimer = NULL;

void
nfsd_analytics_statistics_regsiter(void)
{
	SKIP_XCTEST;

	statisticsTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, NULL);
	dispatch_source_set_timer(statisticsTimer, DISPATCH_TIME_NOW /* start now */, 60 * 60 * NSEC_PER_SEC /* 1 hour interval */, 10 * NSEC_PER_SEC /* 10 seconds leeway */);
	dispatch_source_set_event_handler(statisticsTimer, ^{
		nfsd_analytics_event_statistics();
	});
	dispatch_resume(statisticsTimer);
}

void
nfsd_analytics_statistics_unregsiter(void)
{
	SKIP_XCTEST;

	dispatch_cancel(statisticsTimer);
	nfsd_analytics_event_statistics();
}

/* Exports event */

typedef struct nae_flag {
	const char *nae_name;
	int        nae_value;
} nae_flag_t;

nae_flag_t nae_flags[] = {
	{ "MAPROOT", OP_MAPROOT },
	{ "MAPALL", OP_MAPALL },
	{ "SECFLAV", OP_SECFLAV },
	{ "MASK", OP_MASK },
	{ "NET", OP_NET },
	{ "MANGLEDNAMES", OP_MANGLEDNAMES },
	{ "ALLDIRS", OP_ALLDIRS },
	{ "READONLY", OP_READONLY },
	{ "32BITCLIENTS", OP_32BITCLIENTS },
	{ "FSPATH", OP_FSPATH },
	{ "FSUUID", OP_FSUUID },
	{ "OFFLINE", OP_OFFLINE },
};

void
nfsd_analytics_exports_send(int hostcount, int hostcount_non_loopback, int opt_flags)
{
	SKIP_XCTEST;

	log(LOG_DEBUG, "nfsd_analytics:com.apple.nfs.server.exports sending event");
	analytics_send_event_lazy("com.apple.nfs.server.exports", ^xpc_object_t (void) {
		xpc_object_t dict = xpc_dictionary_create_empty();
		for (int i = 0; i < ARRAY_SIZE(nae_flags); i++) {
		        if (nae_flags[i].nae_value & opt_flags) {
		                xpc_dictionary_set_bool(dict, nae_flags[i].nae_name, true);
			}
		}
		dict_add_int_as_string(dict, "HOST_COUNT", hostcount);
		dict_add_int_as_string(dict, "HOST_COUNT_LOOPBACK", hostcount - hostcount_non_loopback);
		return dict;
	});
}

#endif /* DISABLE_NFSD_ANALYTICS */
