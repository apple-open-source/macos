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

#include <netinfo/ni.h>
#include <NetInfo/system.h>
#include <NetInfo/syslock.h>

/*
 * Constants
 */
const char NAME_NAME[] =  "name";
const char NAME_MACHINES[] = "machines";
const char NAME_IP_ADDRESS[] = "ip_address";
const char NAME_SERVES[] = "serves";
const char NAME_DOT[] = ".";
const char NAME_DOTDOT[] = "..";
const char NAME_MASTER[] = "master";
#ifdef notdef
const char NAME_LOOPBACK[] = "127.0.0.1";
#endif
const char NAME_UID[] = "uid";
const char NAME_PASSWD[] = "passwd";
const char NAME_USERS[] = "users";
const char NAME_GROUPS[] = "groups";
const char NAME_ADMIN[] = "admin";
const char NAME_NETWORKS[] = "networks";
const char NAME_ADDRESS[] = "address";
const char NAME_TRUSTED_NETWORKS[] = "trusted_networks";
const char NAME_AUTHENTICATION_AUTHORITY[] = "authentication_authority";
const char NAME_GUID[] = "generateduid";
const char ACCESS_USER_SUPER[] = "root";
const char ACCESS_USER_ANYBODY[] = "*";
const char ACCESS_NAME_PREFIX[] = "_writers_";
const char ACCESS_DIR_KEY[] = "_writers";

/*
 * Variables
 */
void *db_ni;			/* handle to the database we serve */
char *db_tag;			/* tag of the database we serve */
int shutdown_server;	/* flag to signal time to shutdown server */
int i_am_clone;			/* on if server is clone */
unsigned master_addr;	/* address of master, if clone server */
char *master_tag;		/* tag of master, if clone server */
int cleanupwait;		/* time to wait before cleaning up */
int debug = 0;
int standalone = 0;

/* 
 * for clone: have done transfer in last time period
 */
unsigned have_transferred = 0;

int tcp_sock = -1;
int udp_sock = -1;

int max_readall_proxies = 0;
bool_t strict_proxies = FALSE;
bool_t db_lockup = FALSE;
int readall_proxies = 0;
int sending_all = 0;
syslock *readall_syslock;
syslock *lockup_syslock;
bool_t i_am_proxy = FALSE;
int process_group = 0;

char *db_tagname = NULL;	/* tagname (for logging purposes) */

int max_subthreads = 0;

int update_latency_secs = 0;

int cleanupwait = -2;	/* -1 has some meaning */
int cleanuptime = -1;	/* time to wait before cleaning up */

/* Include blown authentication count in statistics */
#define N_AUTH_COUNT 4	/* Better equal what's in ni_globals.h! */
unsigned auth_count[N_AUTH_COUNT];

/* Avoid needless readalls (and race condition handling same) */
bool_t readall_done = FALSE;

/* Promote members of the admin group to root access */
bool_t promote_admins = TRUE;

/* Force the domain to be root... */
bool_t forcedIsRoot = FALSE;

/* Allow clones to reply to readall request... */
bool_t cloneReadallResponseOK = FALSE;

/* Report reading all in statistics ... */
bool_t reading_all = FALSE;

/* File descriptor bit mask which reflects those sockets
 * associated with "client" RPC operations.
 */
fd_set clnt_fdset = { { 0 } };

/* Keep track of current parent binding... */
int latestParentStatus = 16;	/* NI_NOTMASTER (from <netinfo/ni_prot.h>) */
char *latestParentInfo = NULL;

/* RPCGEN needs these */

int _rpcpmstart=0;	/* Started by a port monitor ? */
int _rpcfdtype=0;	/* Whether Stream or Datagram ? */
int _rpcsvcdirty=0;	/* Still serving ? */
