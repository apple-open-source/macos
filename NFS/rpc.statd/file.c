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
 * $FreeBSD$
 *
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>		/* For mmap()				*/
#include <rpc/rpc.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/param.h>
#include <libutil.h>

#include "statd.h"

FileLayout *status_info;	/* Pointer to the mmap()ed status file	*/
static int status_fd;		/* File descriptor for the open file	*/
static off_t status_file_len;	/* Current on-disc length of file	*/

/* sync_file --------------------------------------------------------------- */
/*
   Purpose:	Packaged call of msync() to flush changes to mmap()ed file
   Returns:	Nothing.  Errors to syslog.
*/

void sync_file(void)
{
  if (msync((void *)status_info, status_file_len, MS_SYNC) < 0 &&
      msync((void *)status_info, status_file_len, MS_SYNC) < 0)
  {
    syslog(LOG_ERR, "msync() failed: %s", strerror(errno));
  }
}

/* find_host -------------------------------------------------------------- */
/*
   Purpose:	Find the entry in the status file for a given host
   Returns:	Pointer to that entry in the mmap() region, or NULL.
   Notes:	Also creates entries if requested.
		Failure to create also returns NULL.
*/

HostInfo *find_host(char *hostname, int create)
{
  HostInfo *hp;
  HostInfo *spare_slot = NULL;
  HostInfo *result = NULL;
  int i;

  for (i = 0, hp = status_info->hosts; i < status_info->noOfHosts; i++, hp++)
  {
    if (!strncasecmp(hostname, hp->hostname, SM_MAXSTRLEN))
    {
      result = hp;
      break;
    }
    if (!spare_slot && !hp->monList && !hp->notifyReqd)
      spare_slot = hp;
  }

  /* Return if entry found, or if not asked to create one.		*/
  if (result || !create) return (result);

  /* Now create an entry, using the spare slot if one was found or	*/
  /* adding to the end of the list otherwise, extending file if reqd	*/
  if (!spare_slot)
  {
    off_t desired_size;
    spare_slot = &status_info->hosts[status_info->noOfHosts];
    desired_size = ((char*)spare_slot - (char*)status_info) + sizeof(HostInfo);
    if (desired_size > status_file_len)
    {
      /* Extend file by writing 1 byte of junk at the desired end pos	*/
      lseek(status_fd, desired_size - 1, SEEK_SET);
      i = write(status_fd, &i, 1);
      if (i < 1)
      {
	syslog(LOG_ERR, "Unable to extend status file");
	return (NULL);
      }
      status_file_len = desired_size;
    }
    status_info->noOfHosts++;
  }

  /* Initialise the spare slot that has been found/created		*/
  /* Note that we do not msync(), since the caller is presumed to be	*/
  /* about to modify the entry further					*/
  memset(spare_slot, 0, sizeof(HostInfo));
  strncpy(spare_slot->hostname, hostname, SM_MAXSTRLEN);
  return (spare_slot);
}

/* init_file -------------------------------------------------------------- */
/*
   Purpose:	Open file, create if necessary, initialise it.
   Returns:	Whether notifications are needed - exits on error
   Notes:	Opens the file, then mmap()s it for ease of access.
		Also performs initial clean-up of the file, zeroing
		monitor list pointers, setting the notifyReqd flag in
		all hosts that had a monitor list, and incrementing
		the state number to the next even value.
*/

int init_file(const char *filename)
{
  int new_file = FALSE;
  char buf[HEADER_LEN];
  struct flock lock = {0};
  int i, err, do_cleanup, need_notify = FALSE;

  /* try to open existing file - if not present, create one		*/
  status_fd = open(filename, O_RDWR);
  if ((status_fd < 0) && (errno == ENOENT))
  {
    status_fd = open(filename, O_RDWR | O_CREAT, 0644);
    new_file = TRUE;
  }
  if (status_fd < 0) {
    syslog(LOG_ERR, "unable to open status file %s", filename);
    exit(1);
  }

  /* grab a lock on the file to protect initialization */
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = HEADER_LEN;
  err = fcntl(status_fd, F_SETLKW, &lock);
  if (err == -1) 
    syslog(LOG_ERR, "fcntl lock error(%d): %s\n", errno, strerror(errno));

  /* File now open.  mmap() it, with a generous size to allow for	*/
  /* later growth, where we will extend the file but not re-map it.	*/
  status_info = (FileLayout *)
    mmap(NULL, 0x10000000, PROT_READ | PROT_WRITE, MAP_SHARED, status_fd, 0);

  if (status_info == (FileLayout *) MAP_FAILED) {
    syslog(LOG_ERR, "unable to mmap() status file");
    exit(1);
  }

  status_file_len = lseek(status_fd, 0L, SEEK_END);

  /* If the file was not newly created, validate the contents, and if	*/
  /* defective, re-create from scratch.					*/
  if (!new_file)
  {
    if ((status_file_len < HEADER_LEN) || (status_file_len
      < (HEADER_LEN + sizeof(HostInfo) * status_info->noOfHosts)) )
    {
      syslog(LOG_WARNING, "status file is corrupt");
      new_file = TRUE;
    }
  }

  /* Initialisation of a new, empty file.				*/
  if (new_file)
  {
    memset(buf, 0, sizeof(buf));
    lseek(status_fd, 0L, SEEK_SET);
    write(status_fd, buf, HEADER_LEN);
    status_file_len = HEADER_LEN;
  }
  else
  {
    if (notify_only && get_statd_pid())
    {
      /* Don't attempt to do clean up because statd is already running */
      syslog(LOG_INFO, "statd.notify not initting database because statd's running");
      do_cleanup = 0;
    } else {
      do_cleanup = 1;
    }
    /* Clean-up of existing file - monitored hosts will have a pointer	*/
    /* to a list of clients, which refers to memory in the previous	*/
    /* incarnation of the program and so are meaningless now.  These	*/
    /* pointers are zeroed and the fact that the host was previously	*/
    /* monitored is recorded by setting the notifyReqd flag, which will	*/
    /* in due course cause a SM_NOTIFY to be sent.			*/
    /* Note that if we crash twice in quick succession, some hosts may	*/
    /* already have notifyReqd set, where we didn't manage to notify	*/
    /* them before the second crash occurred.				*/
    for (i = 0; i < status_info->noOfHosts; i++)
    {
      HostInfo *this_host = &status_info->hosts[i];

      if (do_cleanup && this_host->monList)
      {
	this_host->notifyReqd = TRUE;
	this_host->monList = NULL;
      }
      if (this_host->notifyReqd == TRUE) {
        if (debug) syslog(LOG_DEBUG, "need notify: %s", this_host->hostname);
      	need_notify = TRUE;
      }
    }
    if (do_cleanup) {
      /* Select the next higher even number for the state counter		*/
      status_info->ourState = (status_info->ourState + 2) & 0xfffffffe;
      status_info->ourState++;
    }
  }
  sync_file();

  /* unlock header */
  lock.l_type = F_UNLCK;
  err = fcntl(status_fd, F_SETLKW, &lock);
  if (err == -1) 
    syslog(LOG_ERR, "fcntl unlock error(%d): %s\n", errno, strerror(errno));
  return (need_notify);
}

/* notify_one_host --------------------------------------------------------- */
/*
   Purpose:	Perform SM_NOTIFY procedure at specified host
   Returns:	TRUE if success, FALSE if failed.
*/

static int notify_one_host(char *hostname)
{
  struct timeval timeout = { 20, 0 };	/* 20 secs timeout		*/
  CLIENT *cli;
  char dummy; 
  stat_chge arg;
  char our_hostname[SM_MAXSTRLEN+1];

  gethostname(our_hostname, sizeof(our_hostname));
  our_hostname[SM_MAXSTRLEN] = '\0';
  arg.mon_name = our_hostname;
  arg.state = status_info->ourState;

  if (debug) syslog (LOG_DEBUG, "Sending SM_NOTIFY to host %s from %s", hostname, our_hostname);

  cli = clnt_create(hostname, SM_PROG, SM_VERS, "udp");
  if (!cli)
  {
    syslog(LOG_ERR, "Failed to contact host %s%s", hostname,
      clnt_spcreateerror(""));
    return (FALSE);
  }

  if (clnt_call(cli, SM_NOTIFY, (xdrproc_t)xdr_stat_chge, &arg, (xdrproc_t)xdr_void, &dummy, timeout)
    != RPC_SUCCESS)
  {
    syslog(LOG_ERR, "Failed to contact rpc.statd at host %s", hostname);
    clnt_destroy(cli);
    return (FALSE);
  }

  clnt_destroy(cli);
  return (TRUE);
}

/* notify_hosts ------------------------------------------------------------ */
/*
   Purpose:	Send SM_NOTIFY to all hosts marked as requiring it
   Returns:	Nothing.
   Notes:	Does nothing if there are no monitored hosts.
		Called after all the initialisation has been done - 
		logs to syslog.
*/

int notify_hosts(void)
{
  int i;
  int attempts;
  int work_to_do = FALSE;
  HostInfo *hp;
  pid_t pid;

  /* claim PID file */
  pfh = pidfile_open(_PATH_STATD_NOTIFY_PID, 0644, &pid);
  if (pfh == NULL) {
    syslog(LOG_ERR, "can't open statd.notify pidfile: %s (%d)", strerror(errno), errno);
    if (errno == EEXIST) {
      syslog(LOG_ERR, "statd.notify already running, pid: %d", pid);
      return (0);
    }
    return (2);
  }
  if (pidfile_write(pfh) == -1)
    syslog(LOG_WARNING, "can't write to statd.notify pidfile: %s (%d)", strerror(errno), errno);

  work_to_do = init_file(_PATH_STATD_DATABASE);

  if (!work_to_do) {
    /* No work found */
    syslog(LOG_NOTICE, "statd.notify - no notifications needed");
    pidfile_remove(pfh);
    return (0);
  }

  syslog(LOG_INFO, "statd.notify starting%s",
	debug ? " - debug enabled" : "");

  /* Here in the child process.  We continue until all the hosts marked	*/
  /* as requiring notification have been duly notified.			*/
  /* If one of the initial attempts fails, we sleep for a while and	*/
  /* have another go.  This is necessary because when we have crashed,	*/
  /* (eg. a power outage) it is quite possible that we won't be able to	*/
  /* contact all monitored hosts immediately on restart, either because	*/
  /* they crashed too and take longer to come up (in which case the	*/
  /* notification isn't really required), or more importantly if some	*/
  /* router etc. needed to reach the monitored host has not come back	*/
  /* up yet.  In this case, we will be a bit late in re-establishing	*/
  /* locks (after the grace period) but that is the best we can do.	*/
  /* We try 10 times at 5 sec intervals, 10 more times at 1 minute	*/
  /* intervals, then 24 more times at hourly intervals, finally		*/
  /* giving up altogether if the host hasn't come back to life after	*/
  /* 24 hours.								*/

  for (attempts = 0; attempts < 44; attempts++)
  {
    work_to_do = FALSE;		/* Unless anything fails		*/
    for (i = status_info->noOfHosts, hp = status_info->hosts; i ; i--, hp++)
    {
      if (hp->notifyReqd)
      {
        if (notify_one_host(hp->hostname))
	{
	  hp->notifyReqd = FALSE;
          sync_file();
	}
	else work_to_do = TRUE;
      }
    }
    if (!work_to_do) break;
    if (attempts < 10) sleep(5);
    else if (attempts < 20) sleep(60);
    else sleep(60*60);
  }
  pidfile_remove(pfh);
  return(0);
}

