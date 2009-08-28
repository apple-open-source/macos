/*
 * Copyright (c) 2002-2008 Apple Inc.  All rights reserved.
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
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
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
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */



#include <rpcsvc/sm_inter.h>

/* ------------------------------------------------------------------------- */
/*
  Data structures for recording monitored hosts

  The information held by the status monitor comprises a list of hosts
  that we have been asked to monitor, and, associated with each monitored
  host, one or more clients to be called back if the monitored host crashes.

  The list of monitored hosts must be retained over a crash, so that upon
  re-boot we can call the SM_NOTIFY procedure in all those hosts so as to
  cause them to start recovery processing.  On the other hand, the client
  call-backs are not required to be preserved: they are assumed (in the
  protocol design) to be local processes which will have crashed when
  we did, and so are discarded on restart.

  We handle this by keeping the list of monitored hosts in a file
  (/var/db/statd.status) which is mmap()ed and whose format consists
  of the FileHeader followed by a packed list of HostInfo structures.
  For each currently-monitored host, a MonitoredHost structure associates
  the in-file HostInfo entry with a list of client notification callbacks.
  The MonitoredHost and Notify structures are held in normal memory and so
  will be lost on restart.

  All values stored in network byte order.
*/

typedef struct Notify_s {
	struct Notify_s *n_next;
	int32_t n_prog;		/* RPC program number to call		 */
	int32_t n_vers;		/* version number			 */
	int32_t n_proc;		/* procedure number			 */
	uint8_t n_data[16];	/* opaque data from caller		 */
	char n_host[1];		/* host to notify			 */
} Notify;

typedef struct MonitoredHost_s {
	off_t mh_hostinfo_offset;/* offset of HostInfo struct in file	 */
	char *mh_name;		/* alternate in-memory name pointer	 */
	Notify *mh_notify_list;	/* list of notifications		 */
} MonitoredHost;

/* file layout: a fixed-size header followed by packed HostInfo entries */

typedef struct {
	uint32_t fh_state;	/* State number as defined in statd protocol	 */
	uint32_t fh_reccnt;	/* # of HostInfo records			 */
	uint32_t fh_version;	/* version # of this file layout		 */
	char fh_reserved[244];	/* Reserved for future use			 */
} FileHeader;
#define STATUS_DB_VERSION_CONVERTED	0xffffffff
#define STATUS_DB_VERSION		0x4e534d31	/* "NSM1" */

#define NAMEINCR	64	/* must be power of 2 */

typedef struct {
	uint16_t hi_len;	/* length of this HostInfo record	 */
	uint16_t hi_monitored;	/* host is being monitored		 */
	uint16_t hi_notify;	/* host needs to be notified		 */
	uint16_t hi_namelen;	/* length of host name string		 */
	char hi_name[NAMEINCR];	/* name of monitored host		 */
	/* NULL terminated string allocated in NAMEINCR-byte increments */
} HostInfo;
#define RNDUP_NAMELEN(NL)	(((NL) + 1 + (NAMEINCR-1)) & (~(NAMEINCR-1)))
#define HOSTINFO(OFF)		((HostInfo*)((char*)status_info + (OFF)))

/* Original "version 0" file layout */
typedef struct {
	char hostname[SM_MAXSTRLEN + 1];/* Name of monitored host	 */
	int notifyReqd;			/* TRUE if we've crashed and not yet	 */
					/* informed the monitored host		 */
	uint32_t monList;		/* List of clients to inform if we	 */
					/* hear that the monitored host has	 */
					/* crashed, NULL if no longer monitored	 */
} HostInfo0;
typedef struct {
	uint32_t ourState;	/* State number as defined in statd protocol	 */
	uint32_t noOfHosts;	/* Number of elements in hosts[]		 */
	uint32_t version;	/* version # of this layout			 */
	char reserved[244];	/* Reserved for future use			 */
	HostInfo0 hosts[1];	/* vector of monitored hosts			 */
} FileLayout0;

/* ------------------------------------------------------------------------- */

#define _PATH_STATD_DATABASE		"/var/db/statd.status"
#define _PATH_STATD_PID			"/var/run/statd.pid"
#define _PATH_STATD_NOTIFY_PID		"/var/run/statd.notify.pid"
#define _PATH_NFS_CONF			"/etc/nfs.conf"
#define _PATH_LAUNCHCTL			"/bin/launchctl"
#define _PATH_STATD_NOTIFY_PLIST	"/System/Library/LaunchDaemons/com.apple.statd.notify.plist"
#define _STATD_NOTIFY_SERVICE_LABEL	"com.apple.statd.notify"

/* ------------------------------------------------------------------------- */

#define LIST_MODE_ONCE		1	/* -l */
#define LIST_MODE_WATCH		2	/* -L */

/*
 * structure holding statd config values
 */
struct nfs_conf_statd {
	int port;
	int simu_crash_allowed;
	int verbose;
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
void SYSLOG(int, const char *,...);
extern int log_to_stderr;

/* Global variables		 */

extern FileHeader *status_info;	/* The mmap()ed status file		 */
extern void *mhroot;		/* root of the MonitoredHost tree	 */

extern int statd_server;	/* are we the statd server (not notify, list...) */
extern int notify_only;		/* just send SM_NOTIFY messages */
extern int list_only;		/* just list status database entries */
extern struct pidfh *pfh;	/* pid file */
extern struct nfs_conf_statd config;	/* config settings */

/* Function prototypes		 */

extern int mhcmp(const void * /* key1 */ , const void * /* key2 */ );
extern MonitoredHost *find_host(char * /* hostname */ , int /* create */ );
extern int init_file(const char * /* filename */ );
extern int notify_hosts(void);
extern int do_unnotify_host(const char * /* hostname */ );
extern int list_hosts(int /* mode */ );
extern void sync_file(void);
extern int sm_check_hostname(struct svc_req * req, char *arg);
pid_t get_statd_pid(void);
pid_t get_statd_notify_pid(void);
int statd_notify_load(void);
int statd_notify_is_loaded(void);
int statd_notify_start(void);

#ifndef __unused
#define __unused
#endif
