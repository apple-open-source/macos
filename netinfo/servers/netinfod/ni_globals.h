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
 * Globals used by the NetInfo server. Most are just constants
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <sys/types.h>
#include <NetInfo/syslock.h>

/*
 * #defines
 */

#define CONNECTION_CHECK 1	/* turns on connection security checking */

#define NI_RECVSIZE 512
#define NI_SENDSIZE 1024

#ifdef _UNIX_BSD_43_
#define VAR_DIR "/tmp"
#define CONFIG_FILE_NAME "/etc/netinfo/niconfig_%s.xml"
#else
#define VAR_DIR "/var/run"
#define CONFIG_FILE_NAME "/var/run/niconfig_%s.xml"
#endif

/*
 * Constants
 */
extern const char NAME_NAME[];
extern const char NAME_MACHINES[];
extern const char NAME_IP_ADDRESS[];
extern const char NAME_SERVES[];
extern const char NAME_DOT[];
extern const char NAME_DOTDOT[];
extern const char NAME_MASTER[];
#ifdef notdef
extern const char NAME_LOOPBACK[];
#endif
extern const char NAME_UID[];
extern const char NAME_PASSWD[];
extern const char NAME_USERS[];
extern const char NAME_GROUPS[];
extern const char NAME_ADMIN[];
extern const char NAME_NETWORKS[];
extern const char NAME_ADDRESS[];
extern const char NAME_TRUSTED_NETWORKS[];
extern const char NAME_AUTHENTICATION_AUTHORITY[];
extern const char NAME_GUID[];
extern const char ACCESS_USER_SUPER[];
extern const char ACCESS_USER_ANYBODY[];
extern const char ACCESS_NAME_PREFIX[];
extern const char ACCESS_DIR_KEY[];
extern const char SHADOW_NAME_PREFIX[];
extern const char SECURE_NAME_PREFIX[];
extern const char SHADOW_DIR_KEY[];
extern const char SECURE_DIR_KEY[];
extern const char VALUE_SHADOW[];


/*
 * Variables
 */
extern void *db_ni;				/* handle to the database we serve */
extern char *db_tag;			/* tag of the database we serve */

extern int shutdown_server;	/* flag to signal time to shutdown server */

extern int debug;
extern int standalone;

extern int i_am_clone;			/* on if server is clone */
extern unsigned master_addr;	/* address of master, if clone server */
extern char *master_tag;		/* tag of master, if clone server */
/* 
 * for clone: have done transfer in last time period
 */
extern unsigned have_transferred;	

/*
 * Keep track of listener sockets
 */
extern int udp_sock;
extern int tcp_sock;

/* Ensure we've a definition of bool_t (stolen from <rpc/types.h>) */
#ifndef bool_t
#define bool_t int
#endif bool_t
#define MAX_READALL_PROXIES (1024 - 600)	/* Number of privileged ports...*/
extern int max_readall_proxies;
extern bool_t strict_proxies;
extern volatile int readall_proxies;
extern volatile int sending_all;
extern volatile bool_t db_lockup;
extern syslock *lockup_syslock;
extern syslock *readall_syslock;
extern bool_t i_am_proxy;
extern int process_group;

/* Make number of notify threads configurable */
extern volatile int max_subthreads;

/* Following moved from notify.c */
#define MAX_SUBTHREADS 5 /* Max # of update threads, not including main */

/* Make update latency configurable */
extern volatile int update_latency_secs;
#define UPDATE_LATENCY_SECS 2 /* seconds to wait before distributing update */

extern int cleanupwait;	/* Make the inter-resync period configurable */
extern int cleanuptime;	/* Include time 'til cleanup in statistics */
#define CLEANUPWAIT (30*60)	/* # of seconds to wait before cleaning up */

/* Number of times for local to try to bind to to a parent */
#define LOCAL_BIND_ATTEMPTS 2

/* Include blown authentication count in statistics */
#define N_AUTH_COUNT 4
extern unsigned auth_count[N_AUTH_COUNT];
#define GOOD 0
#define BAD 1
#define WGOOD 2
#define WBAD 3

extern bool_t readall_done;

/* Promote members of the admin group to root access */
extern bool_t promote_admins;

/* Force the domain to be root... */
extern bool_t forcedIsRoot;

/* Allow clones to reply to readall request... */
extern bool_t cloneReadallResponseOK;

/* Report reading all in statistics ... */
extern bool_t reading_all;

/*
 * File descriptor bit mask which reflects those sockets
 * associated with "client" RPC operations.
 */
extern fd_set clnt_fdset;

/* Keep track of current parent binding... */
extern char *latestParentInfo;	/* xxx.xxx.xxx.xxx/tag of parent */
