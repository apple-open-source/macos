/*
 * Copyright (c) 2002-2007 Apple Inc.  All rights reserved.
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
  (/var/db/statd.status) which is mmap()ed and whose format is described
  by the typedef FileLayout.  The lists of client callbacks are chained
  off this structure, but are held in normal memory and so will be
  lost after a re-boot.  Hence the actual values of MonList * pointers
  in the copy on disc have no significance, but their NULL/non-NULL
  status indicates whether this host is actually being monitored or if it
  is an empty slot in the file.
*/

typedef struct MonList_s
{
  struct MonList_s *next;	/* Next in list or NULL			*/
  char notifyHost[SM_MAXSTRLEN + 1];	/* Host to notify		*/
  int notifyProg;		/* RPC program number to call		*/
  int notifyVers;		/* version number			*/
  int notifyProc;		/* procedure number			*/
  unsigned char notifyData[16];	/* Opaque data from caller		*/
} MonList;

typedef struct
{
  char hostname[SM_MAXSTRLEN + 1];	/* Name of monitored host	*/
  int notifyReqd;		/* TRUE if we've crashed and not yet	*/
				/* informed the monitored host		*/
  MonList *monList;		/* List of clients to inform if we	*/
				/* hear that the monitored host has	*/
				/* crashed, NULL if no longer monitored	*/
} HostInfo;


/* Overall file layout.  						*/

typedef struct
{
  int ourState;		/* State number as defined in statd protocol	*/
  int noOfHosts;	/* Number of elements in hosts[]		*/
  char reserved[248];	/* Reserved for future use			*/
  HostInfo hosts[1];	/* vector of monitored hosts			*/
} FileLayout;

#define	HEADER_LEN (sizeof(FileLayout) - sizeof(HostInfo))

/* ------------------------------------------------------------------------- */

#define _PATH_STATD_DATABASE		"/var/db/statd.status"
#define _PATH_STATD_PID			"/var/run/statd.pid"
#define _PATH_STATD_NOTIFY_PID		"/var/run/statd.notify.pid"
#define _PATH_NFS_CONF			"/etc/nfs.conf"
#define _PATH_LAUNCHCTL			"/bin/launchctl"
#define _PATH_STATD_NOTIFY_PLIST	"/System/Library/LaunchDaemons/com.apple.statd.notify.plist"
#define _STATD_NOTIFY_SERVICE_LABEL	"com.apple.statd.notify"

/* ------------------------------------------------------------------------- */

/*
 * structure holding statd config values
 */
struct nfs_conf_statd {
	int port;
	int verbose;
};

/* Global variables		*/

extern FileLayout *status_info;	/* The mmap()ed status file		*/

extern int debug;		/* =1 to enable diagnostics to syslog	*/
extern int notify_only;		/* only send SM_NOTIFY messages */
extern struct pidfh *pfh;	/* pid file */
extern struct nfs_conf_statd config;	/* config settings */

/* Function prototypes		*/

extern HostInfo *find_host(char * /*hostname*/, int /*create*/);
extern int init_file(const char * /*filename*/);
extern int notify_hosts(void);
extern void sync_file(void);
extern int sm_check_hostname(struct svc_req *req, char *arg);
pid_t get_statd_pid(void);

#ifndef __unused
#define __unused
#endif
