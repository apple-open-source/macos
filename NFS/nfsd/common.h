/*
 * Copyright (c) 1999-2010 Apple Inc. All rights reserved.
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
 * structure holding NFS server config values
 */
struct nfs_conf_server {
	int async;			/* sysctl */
	int bonjour;			/* nfsd */
	int bonjour_local_domain_only;	/* nfsd */
	int export_hash_size;		/* sysctl */
	int fsevents;			/* sysctl */
	int mount_port;			/* mountd */
	int mount_regular_files;	/* mountd */
	int mount_require_resv_port;	/* mountd */
	int nfsd_threads;		/* nfsd */
	int port;			/* nfsd */
	int reqcache_size;		/* sysctl */
	int request_queue_length;	/* sysctl */
	int require_resv_port;		/* sysctl */
	int tcp;			/* nfsd/mountd */
	int udp;			/* nfsd/mountd */
	int user_stats;			/* sysctl */
	int verbose;			/* nfsd/mountd */
	int wg_delay;			/* sysctl */
	int wg_delay_v3;		/* sysctl */
};

/*
 * verbose log levels:
 * 0 LOG_WARNING and higher
 * 1 LOG_NOTICE and higher
 * 2 LOG_INFO and higher
 * 3 LOG_DEBUG and higher
 * 4+ more LOG_DEBUG
 */
#define LOG_LEVEL	(LOG_WARNING + MIN(config.verbose, 3))
#define DEBUG(L, ...) \
 	do { \
		if ((L) <= (config.verbose - 2)) \
			SYSLOG(LOG_DEBUG, __VA_ARGS__); \
	} while (0)
#define log SYSLOG
void SYSLOG(int, const char *, ...);

void set_thread_sigmask(void);
int sysctl_get(const char *, int *);
int sysctl_set(const char *, int);

void nfsd(void);
void nfsd_start_server_threads(int);

void mountd_init(void);
void mountd(void);
int get_exportlist(void);
int check_for_mount_changes(void);
int clear_export_errors(uint32_t);

/* globals */
extern pthread_attr_t pattr;
extern const struct nfs_conf_server config_defaults;
extern struct nfs_conf_server config;
extern volatile int gothup, gotterm;
extern char exportsfilepath[MAXPATHLEN];
extern int checkexports;
extern int nfsudpport, nfstcpport;
extern int nfsudp6port, nfstcp6port;
extern int mountudpport, mounttcpport;
extern int mountudp6port, mounttcp6port;
extern time_t recheckexports_until;
extern int recheckexports;

