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
 * $FreeBSD$
 *
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <search.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>		/* For mmap()				 */
#include <rpc/rpc.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <libutil.h>
#include <libkern/OSByteOrder.h>

#include "statd.h"

FileHeader *status_info;	/* Pointer to the mmap()ed status file	 */
static int status_fd;		/* File descriptor for the open file	 */
static off_t status_file_len;	/* Current on-disc length of file	 */
static off_t status_info_len;	/* current length of mmap region	 */
void *mhroot = NULL;

/* map_file --------------------------------------------------------------- */
/*
   Purpose:	Set up or update status file memory mapping.
		Map with a generous size to allow file to grow without
		needing to remap often.
   Returns:	Nothing.  Errors to syslog.
*/
#define MAP_INCREMENT	(32*1024*1024)

void
map_file(void)
{
	off_t desired_map_len = (status_file_len + MAP_INCREMENT) & ~(MAP_INCREMENT - 1);
	int prot;

	if (status_info_len >= desired_map_len)
		return;
	if (status_info_len) {
		sync_file();
		munmap(status_info, status_info_len);
	}
	prot = PROT_READ;
	if (!list_only)
		prot |= PROT_WRITE;
	status_info_len = desired_map_len;
	status_info = mmap(NULL, status_info_len, prot, MAP_SHARED, status_fd, 0);
	if (status_info == (FileHeader *) MAP_FAILED) {
		log(LOG_ERR, "unable to mmap() status file");
		exit(1);
	}
}

/* sync_file --------------------------------------------------------------- */
/*
   Purpose:	Packaged call of msync() to flush changes to mmap()ed file
   Returns:	Nothing.  Errors to syslog.
*/

void
sync_file(void)
{
	if (msync((void *) status_info, status_file_len, MS_SYNC) < 0 &&
	    msync((void *) status_info, status_file_len, MS_SYNC) < 0) {
		log(LOG_ERR, "msync(%p,0x%llx) failed: %s", status_info, status_file_len, strerror(errno));
	}
}

/* mhcmp ------------------------------------------------------------------- */
/*
   Purpose:	comparing nodes in the MonitoredHost tree
   Returns:	-1, 0, 1
*/

int
mhcmp(const void *key1, const void *key2)
{
	const MonitoredHost *mhp1 = key1;
	const MonitoredHost *mhp2 = key2;
	char *s1, *s2;

	s1 = mhp1->mh_hostinfo_offset ?
		HOSTINFO(mhp1->mh_hostinfo_offset)->hi_name : mhp1->mh_name;
	s2 = mhp2->mh_hostinfo_offset ?
		HOSTINFO(mhp2->mh_hostinfo_offset)->hi_name : mhp2->mh_name;
	return strcmp(s1, s2);
}

#if 0				/* DEBUG */

/* mhdump/mhprint ---------------------------------------------------------- */
/*
   Purpose:	dumping the monitored host tree - for debugging purposes
   Returns:	Nothing
*/

void
mhprint(const void *node, VISIT v, __unused int level)
{
	const MonitoredHost *const * mhp = node;
	char *name;

	if ((v != postorder) && (v != leaf))
		return;

	name = (*mhp)->mh_hostinfo_offset ?
		HOSTINFO((*mhp)->mh_hostinfo_offset)->hi_name : (*mhp)->mh_name;
	log(LOG_ERR, "%s", name);
}

void
mhdump(void)
{
	log(LOG_ERR, "Monitored Host List:");
	twalk(mhroot, mhprint);
}

#endif

/* find_host -------------------------------------------------------------- */
/*
   Purpose:	Find the MonitoredHost and its entry in the status file for a given host
   Returns:	MonitoredHost structure for this host, or NULL.
   Notes:	Also creates structures/entries if requested.
		Failure to create also returns NULL.
*/

MonitoredHost *
find_host(char *hostname, int create)
{
	MonitoredHost mhtmp, **mhpp, *mhp;
	HostInfo *hip = NULL;
	HostInfo *spare_slot = NULL;
	off_t off, spare_off = status_file_len;
	int rv, namelen;
	uint i;
	uint16_t len, nlen;

	namelen = strlen(hostname);
	len = sizeof(HostInfo) - NAMEINCR + RNDUP_NAMELEN(namelen);

	/* try to find the host in the current set of monitored hosts	 */
	bzero(&mhtmp, sizeof(mhtmp));
	mhtmp.mh_name = hostname;
	mhpp = tfind(&mhtmp, &mhroot, mhcmp);

	/* Return if entry found, or if not asked to create one.	 */
	if (mhpp)
		return (*mhpp);
	else if (!create)
		return (NULL);

	/* not currently in the set of monitored hosts, allocate one	 */
	mhp = malloc(sizeof(*mhp));
	if (!mhp)
		return (NULL);
	bzero(mhp, sizeof(*mhp));

	/* find the host's slot in the status file			 */
	off = sizeof(FileHeader);
	for (i = 0; i < ntohl(status_info->fh_reccnt); i++) {
		hip = HOSTINFO(off);
		if ((ntohs(hip->hi_namelen) == namelen) && !strncasecmp(hostname, hip->hi_name, namelen))
			break;
		if (!spare_slot && !hip->hi_monitored && !hip->hi_notify && (len <= ntohs(hip->hi_len))) {
			spare_slot = hip;
			spare_off = off;
		}
		off += ntohs(hip->hi_len);
		hip = NULL;
	}

	/* Now create an entry, using the spare slot if one was found or	 */
	/* adding to the end of the list otherwise, extending file if reqd	 */
	if (!hip && !spare_slot) {
		off = spare_off = status_file_len;
		i = 0;
		rv = pwrite(status_fd, &i, 1, off + len - 1);
		if (rv < 1) {
			free(mhp);
			log(LOG_ERR, "Unable to extend status file");
			return (NULL);
		}
		nlen = htons(len);
		rv = pwrite(status_fd, &nlen, sizeof(nlen), off);
		if ((rv < 2) || (rv = fsync(status_fd)))
			log(LOG_ERR, "Unable to write extended status file record length: %d %s", rv, strerror(errno));
		status_file_len = off + len;
		map_file();
		spare_slot = HOSTINFO(off);
		status_info->fh_reccnt = htonl(ntohl(status_info->fh_reccnt) + 1);
	}
	if (!hip) {
		/* Initialise the spare slot that has been found/created		 */
		off = spare_off;
		spare_slot->hi_len = htons(len);
		spare_slot->hi_monitored = 0;
		spare_slot->hi_notify = 0;
		spare_slot->hi_namelen = htons(namelen);
		strlcpy(spare_slot->hi_name, hostname, namelen+1);
		bzero(&spare_slot->hi_name[namelen], RNDUP_NAMELEN(namelen) - namelen);
		hip = spare_slot;
	}
	mhp->mh_hostinfo_offset = off;

	/* insert new host into monitored host set */
	mhpp = tsearch(mhp, &mhroot, mhcmp);
	if (!mhpp) {
		/* insert failed */
		if (hip == spare_slot)
			sync_file();
		free(mhp);
		return (NULL);
	}
	if (!hip->hi_monitored) {
		/* update monitored status */
		hip->hi_monitored = htons(1);
		sync_file();
	}
	return (mhp);
}

/* convert_version0_file  -------------------------------------------------- */
/*
   Purpose:	converts old-style status file to new style
   Returns:	Whether status file needs reinitialization (conversion failed)
*/
int
convert_version0_file(const char *filename0)
{
	int fd0, fd = -1, len, namelen, rv, nhosts0, nhosts, i, newcreated = 0;
	struct stat st;
	FileHeader fh;
	HostInfo0 *hi0p = NULL;
	HostInfo *hip = NULL;
	char *filename = NULL;
	int filenamelen;

	log(LOG_NOTICE, "converting old status file");

	/* open old file */
	fd0 = open(filename0, O_RDONLY);
	if (fd0 < 0) {
		log(LOG_ERR, "can't open status file: %s", strerror(errno));
		goto fail;
	}
	rv = fstat(fd0, &st);
	if (rv < 0) {
		log(LOG_ERR, "can't fstat open status file: %s", strerror(errno));
		goto fail;
	}
	/* read header0 */
	rv = read(fd0, &fh, sizeof(fh));
	if (rv != sizeof(fh)) {
		log(LOG_ERR, "can't read status file header: %d, %s", rv, strerror(errno));
		goto fail;
	}
	if (fh.fh_version != 0) {
		log(LOG_ERR, "invalid status file header version: 0x%x", ntohl(fh.fh_version));
		goto fail;
	}
	if (fh.fh_state > 2000000) {
		/* assume endianness changed */
		fh.fh_state = OSSwapInt32(fh.fh_state);
		fh.fh_reccnt = OSSwapInt32(fh.fh_reccnt);
	}
	nhosts0 = fh.fh_reccnt;
	/* verify nhosts0 vs file length */
	if (st.st_size < ((off_t) sizeof(FileHeader) + (off_t) sizeof(HostInfo0) * nhosts0)) {
		log(LOG_ERR, "status file header (state %d) host count %d exceeds file size: %lld (%lld)",
		    fh.fh_state, nhosts0, st.st_size, ((off_t) sizeof(FileHeader) + (off_t) sizeof(HostInfo0) * nhosts0));
		goto fail;
	}
	/* allocate maximally-sized hostinfo structs */
	hi0p = malloc(sizeof(*hi0p));
	hip = malloc(sizeof(*hip) + SM_MAXSTRLEN);

	/* create new file */
	len = strlen(filename0);
	filenamelen = len + 4 + 1;
	filename = malloc(filenamelen);
	if (!hi0p || !hip || !filename) {
		log(LOG_ERR, "can't allocate memory");
		goto fail;
	}
	strlcpy(filename, filename0, filenamelen);
	strlcat(filename, ".new", filenamelen);

	fd = open(filename, O_RDWR | O_CREAT, 0644);
	if (fd < 0) {
		log(LOG_ERR, "can't open new status file: %s", strerror(errno));
		goto fail;
	}
	newcreated = 1;

	/* write header */
	fh.fh_version = htonl(STATUS_DB_VERSION);
	fh.fh_state = htonl(fh.fh_state);
	fh.fh_reccnt = htonl(fh.fh_reccnt);
	rv = write(fd, &fh, sizeof(fh));
	if (rv != sizeof(fh)) {
		log(LOG_ERR, "can't write new status file header: %d %s", rv, strerror(errno));
		goto fail;
	}
	/* copy/convert all records in use */
	nhosts = 0;
	for (i = 0; i < nhosts0; i++) {
		/* read each hostinfo0 */
		rv = read(fd0, hi0p, sizeof(*hi0p));
		if (rv != sizeof(*hi0p)) {
			log(LOG_ERR, "can't read status file entry %d: %d %s", i, rv, strerror(errno));
			goto fail;
		}
		/* write each hostinfo */
		hip->hi_monitored = htons((hi0p->monList != 0) ? 1 : 0);
		hip->hi_notify = htons((hi0p->notifyReqd != 0) ? 1 : 0);
		if (!hip->hi_monitored && !hip->hi_notify)
			continue;
		namelen = strlen(hi0p->hostname);
		if (namelen > SM_MAXSTRLEN) {
			log(LOG_ERR, "status file entry %d, name too long: %d", i, namelen);
			goto fail;
		}
		strlcpy(hip->hi_name, hi0p->hostname, namelen+1);
		hip->hi_namelen = htons(namelen);
		len = sizeof(*hip) - NAMEINCR + RNDUP_NAMELEN(namelen);
		hip->hi_len = htons(len);
		bzero(&hip->hi_name[namelen], RNDUP_NAMELEN(namelen) - namelen);
		rv = write(fd, hip, len);
		if (rv != len) {
			log(LOG_ERR, "can't write new status file entry %d: %d %s", i, rv, strerror(errno));
			goto fail;
		}
		nhosts++;
	}

	/* update header */
	fh.fh_reccnt = htonl(nhosts);
	rv = pwrite(fd, &fh, sizeof(fh), 0);
	if ((rv != sizeof(fh)) || (rv = fsync(status_fd))) {
		log(LOG_ERR, "can't update new status file header: %d %s", rv, strerror(errno));
		goto fail;
	}
	free(hip);
	free(hi0p);
	close(fd);
	close(fd0);
	rv = rename(filename, filename0);
	if (rv < 0)
		log(LOG_ERR, "can't rename new status file into place: %d %s", rv, strerror(errno));
	free(filename);
	return (rv);
fail:
	if (hip)
		free(hip);
	if (hi0p)
		free(hi0p);
	if (fd >= 0)
		close(fd);
	if (newcreated)
		unlink(filename);
	if (filename)
		free(filename);
	if (fd0 >= 0)
		close(fd0);
	return (1);
}

/* init_file -------------------------------------------------------------- */
/*
   Purpose:	Open file, create if necessary, initialise it.
   Returns:	Whether notifications are needed - exits on error
   Notes:	Opens the status file, verifies its integrity, and
   		sets up the mmap() access.
		Also performs initial cleanup of the file, clearing
		monitor status, setting the notify flag for hosts
		that were previously monitored, and advancing the
		state number.
*/

int
init_file(const char *filename)
{
	int new_file = FALSE;
	struct flock lock = {0};
	int rv, len, err, update, need_notify = FALSE;
	uint i;
	FileHeader fh;
	HostInfo *hip = NULL;
	off_t off;

	/*
	 * only update if we're statd, or if we're statd.notify and statd
	 * isn't running
	 */
	if (statd_server || (notify_only && !get_statd_pid()))
		update = 1;
	else
		update = 0;

reopen:
	/* try to open existing file - if not present, create one		 */
	status_fd = open(filename, list_only ? O_RDONLY : O_RDWR);
	if ((status_fd < 0) && (errno == ENOENT) && statd_server) {
		status_fd = open(filename, O_RDWR | O_CREAT, 0644);
		new_file = TRUE;
	}
	if (status_fd < 0) {
		if (notify_only)
			return (need_notify);
		log(LOG_ERR, "unable to open status file %s - error %d: %s", filename, errno, strerror(errno));
		exit(1);
	}
	off = sizeof(fh);

	if (!list_only) {
		/* grab a lock on the file to protect initialization */
		lock.l_type = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start = 0;
		lock.l_len = sizeof(FileHeader);
		err = fcntl(status_fd, F_SETLKW, &lock);
		if (err == -1)
			log(LOG_ERR, "fcntl lock error(%d): %s", errno, strerror(errno));
	}
	/* read header */
	rv = read(status_fd, &fh, sizeof(fh));
	if (rv != sizeof(fh)) {
		if (rv < 0) {
			log(LOG_ERR, "can't read status file header: %d, %s", rv, strerror(errno));
			exit(1);
		}
		/* note: we may have just created an empty file above */
		new_file = TRUE;
		goto newfile;
	}
	if (fh.fh_version == ntohl(STATUS_DB_VERSION_CONVERTED)) {
		/* Lost conversion race.  Just reopen. */
		close(status_fd);
		goto reopen;
	}
	if (fh.fh_version == 0) {
		/* status file needs to be converted */
		if (list_only) {
			log(LOG_ERR, "status file needs conversion");
			exit(1);
		}
		rv = convert_version0_file(filename);
		if (rv) {
			/* conversion failed */
			log(LOG_ERR, "status file conversion failed, creating new file");
			new_file = TRUE;
			goto newfile;
		}
		/* update old file's version to alert anyone who may */
		/* already have it opened (waiting for the lock) */
		fh.fh_version = htonl(STATUS_DB_VERSION_CONVERTED);
		rv = pwrite(status_fd, &fh, sizeof(fh), 0);
		if ((rv != sizeof(fh)) || (rv = fsync(status_fd)))
			log(LOG_ERR, "failed to update old status file header version: %d, %s", rv, strerror(errno));
		close(status_fd);
		goto reopen;
	}
	/* Scan the whole status file.  Along the way, we'll verify the	 */
	/* integrity of the file and clean up monitored/notification fields.	 */
	if (!update && notify_only)
		DEBUG(1, "notify_only: skipping database initialization");

	if (!hip)
		hip = malloc(sizeof(*hip) + SM_MAXSTRLEN);
	if (!hip) {
		log(LOG_ERR, "can't allocate memory");
		exit(1);
	}
	for (i = 0; i < ntohl(fh.fh_reccnt); i++) {
		rv = pread(status_fd, hip, sizeof(*hip), off);
		if (rv != sizeof(*hip)) {
			log(LOG_ERR, "error reading status file host info entry # %d, %d %s", i, rv, strerror(errno));
			break;
		}
		hip->hi_len = ntohs(hip->hi_len);
		hip->hi_monitored = ntohs(hip->hi_monitored);
		hip->hi_notify = ntohs(hip->hi_notify);
		hip->hi_namelen = ntohs(hip->hi_namelen);
		if (hip->hi_len > (sizeof(*hip) + SM_MAXSTRLEN)) {
			log(LOG_ERR, "status file host info entry # %d, has bogus length %d", i, hip->hi_len);
			break;
		}
		if (hip->hi_namelen > SM_MAXSTRLEN) {
			log(LOG_ERR, "status file host info entry # %d, has bogus name length %d", i, hip->hi_namelen);
			break;
		}
		if (hip->hi_namelen >= (hip->hi_len - sizeof(*hip) + NAMEINCR)) {
			log(LOG_ERR, "status file host info entry # %d, name length %d exceeds record length %d", i, hip->hi_namelen, hip->hi_len);
			break;
		}
		if (hip->hi_namelen >= NAMEINCR) {
			len = RNDUP_NAMELEN(hip->hi_namelen) - NAMEINCR;
			rv = pread(status_fd, &hip->hi_name[NAMEINCR], len, off + sizeof(*hip));
			if (rv != len) {
				log(LOG_ERR, "error reading status file host info entry # %d name, %d %s", i, rv, strerror(errno));
				break;
			}
		}
		if (hip->hi_name[hip->hi_namelen] != 0) {
			log(LOG_ERR, "status file host info entry # %d, name not terminated", i);
			break;
		}
		if (hip->hi_monitored && update) {
			hip->hi_monitored = 0;
			hip->hi_notify = htons(1);
			hip->hi_namelen = htons(hip->hi_namelen);
			hip->hi_len = htons(hip->hi_len);
			rv = pwrite(status_fd, hip, sizeof(*hip), off);
			if ((rv != sizeof(*hip)) || (rv = fsync(status_fd))) {
				log(LOG_ERR, "error updating status file host info entry # %d, %d %s", i, rv, strerror(errno));
				break;
			}
			hip->hi_len = ntohs(hip->hi_len);
		}
		if (hip->hi_notify && (statd_server || notify_only)) {
			DEBUG(1, "need notify: %s", hip->hi_name);
			need_notify = TRUE;
		}
		off += hip->hi_len;
	}
	if (i != ntohl(fh.fh_reccnt)) {
		log(LOG_ERR, "status file failed verification, reinitializing file");
		new_file = TRUE;
	}
newfile:
	if (new_file)
		need_notify = FALSE;
	if (update) {
		if (new_file) {
			rv = ftruncate(status_fd, 0);
			if (rv < 0) {
				log(LOG_ERR, "unable to truncate status file, aborting");
				exit(1);
			}
			bzero(&fh, sizeof(fh));
			fh.fh_version = htonl(STATUS_DB_VERSION);
			fh.fh_state = htonl(statd_server ? 1 : 2);
			off = sizeof(fh);
		} else {
			/* advance to next even number (next odd if we're "up" again) */
			fh.fh_state = ntohl(fh.fh_state);
			if (statd_server)	/* advance to next odd */
				fh.fh_state += (fh.fh_state & 1) ? 2 : 1;
			else if (notify_only && (fh.fh_state & 1))	/* if odd, advance to even */
				fh.fh_state++;
			fh.fh_state = htonl(fh.fh_state);
			/* make sure the file ends where we think it should */
			status_file_len = lseek(status_fd, 0L, SEEK_END);
			if (status_file_len != off) {
				log(LOG_ERR, "status file is longer than expected, %lld vs %lld, truncating", status_file_len, off);
				rv = ftruncate(status_fd, off);
				if (rv < 0) {
					log(LOG_ERR, "unable to truncate status file, aborting");
					exit(1);
				}
			}
		}
		rv = pwrite(status_fd, &fh, sizeof(fh), 0);
		if ((rv < 0) || (rv = fsync(status_fd))) {
			log(LOG_ERR, "unable to initialize status file header, aborting");
			exit(1);
		}
	}
	status_file_len = off;
	status_info_len = 0;
	map_file();
	sync_file();

	if (!list_only) {
		/* unlock header */
		lock.l_type = F_UNLCK;
		err = fcntl(status_fd, F_SETLKW, &lock);
		if (err == -1)
			log(LOG_ERR, "fcntl unlock error(%d): %s", errno, strerror(errno));
	}
	if (hip)
		free(hip);
	return (need_notify);
}

/* notify_one_host --------------------------------------------------------- */
/*
   Purpose:	Perform SM_NOTIFY procedure at specified host
   Returns:	TRUE if success, FALSE if failed.
*/

static int
notify_one_host(char *hostname, int warn)
{
	struct timeval timeout = {20, 0};	/* 20 secs timeout		 */
	CLIENT *cli;
	char dummy, proto[] = "udp", empty[] = "";
	stat_chge arg;
	char our_hostname[SM_MAXSTRLEN + 1];

	gethostname(our_hostname, sizeof(our_hostname));
	our_hostname[SM_MAXSTRLEN] = '\0';
	arg.mon_name = our_hostname;
	arg.state = ntohl(status_info->fh_state);

	DEBUG(1, "Sending SM_NOTIFY to host %s from %s", hostname, our_hostname);

	cli = clnt_create(hostname, SM_PROG, SM_VERS, proto);
	if (!cli) {
		if (warn)
			log(LOG_WARNING, "Failed to contact host %s%s", hostname, clnt_spcreateerror(empty));
		return (FALSE);
	}
	if (clnt_call(cli, SM_NOTIFY, (xdrproc_t) xdr_stat_chge, &arg, (xdrproc_t) xdr_void, &dummy, timeout)
	    != RPC_SUCCESS) {
		if (warn)
			log(LOG_WARNING, "Failed to contact rpc.statd at host %s", hostname);
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

int
notify_hosts(void)
{
	int i, reccnt;
	int attempts, warn;
	int work_to_do = FALSE;
	HostInfo *hip;
	off_t off;
	pid_t pid;

	/* claim PID file */
	pfh = pidfile_open(_PATH_STATD_NOTIFY_PID, 0644, &pid);
	if (pfh == NULL) {
		log(LOG_ERR, "can't open statd.notify pidfile: %s (%d)", strerror(errno), errno);
		if (errno == EEXIST) {
			log(LOG_ERR, "statd.notify already running, pid: %d", pid);
			return (0);
		}
		return (2);
	}
	if (pidfile_write(pfh) == -1)
		log(LOG_WARNING, "can't write to statd.notify pidfile: %s (%d)", strerror(errno), errno);

	work_to_do = init_file(_PATH_STATD_DATABASE);

	if (!work_to_do) {
		/* No work found */
		log(LOG_NOTICE, "statd.notify - no notifications needed");
		pidfile_remove(pfh);
		return (0);
	}
	log(LOG_INFO, "statd.notify starting");

	/* Here in the child process.  We continue until all the hosts marked	 */
	/* as requiring notification have been duly notified.			 */
	/* If one of the initial attempts fails, we sleep for a while and	 */
	/* have another go.  This is necessary because when we have crashed,	 */
	/* (eg. a power outage) it is quite possible that we won't be able to	 */
	/* contact all monitored hosts immediately on restart, either because	 */
	/* they crashed too and take longer to come up (in which case the	 */
	/* notification isn't really required), or more importantly if some	 */
	/* router etc. needed to reach the monitored host has not come back	 */
	/* up yet.  In this case, we will be a bit late in re-establishing	 */
	/* locks (after the grace period) but that is the best we can do.	 */
	/* We try 10 times at 5 sec intervals, 10 more times at 1 minute	 */
	/* intervals, then 24 more times at hourly intervals, finally		 */
	/* giving up altogether if the host hasn't come back to life after	 */
	/* 24 hours.								 */

	for (attempts = 0; attempts < 44; attempts++) {
		work_to_do = FALSE;	/* Unless anything fails		 */
		warn = !(attempts % 10) || (attempts >= 20); /* limit warning frequency */

		/* update status_file_len and mmap */
		i = lseek(status_fd, 0L, SEEK_END);
		if (i > 0) {
			status_file_len = i;
			map_file();
		}
		/* iterate through all entries */
		off = sizeof(FileHeader);
		reccnt = ntohl(status_info->fh_reccnt);
		for (i = 0; i < reccnt; i++) {
			hip = HOSTINFO(off);
			if (hip->hi_notify) {
				if (notify_one_host(hip->hi_name, warn)) {
					hip->hi_notify = 0;
					sync_file();
					if (attempts)	/* log success if we've logged errors */
						log(LOG_WARNING, "Contacted rpc.statd at host %s", hip->hi_name);
				} else
					work_to_do = TRUE;
			}
			off += ntohs(hip->hi_len);
		}
		if (!work_to_do)
			break;
		if (attempts < 10)
			sleep(5);
		else if (attempts < 20)
			sleep(60);
		else
			sleep(60 * 60);
	}
	pidfile_remove(pfh);
	return (0);
}

/* do_unnotify_host ------------------------------------------------------ */
/*
   Purpose:	Clear the notify flag for the given host.
   Returns:	Nothing.
*/

int
do_unnotify_host(const char *hostname)
{
	uint i;
	int found = 0, dosync = 0;
	off_t off;
	HostInfo *hip;

	init_file(_PATH_STATD_DATABASE);

	/* iterate through all entries */
	off = sizeof(FileHeader);
	for (i = 0; i < status_info->fh_reccnt && off < status_file_len; i++, off += ntohs(hip->hi_len)) {
		hip = HOSTINFO(off);
		if (strncmp(hip->hi_name, hostname, SM_MAXSTRLEN))
			continue;
		found = 1;
		if (!hip->hi_notify)
			continue;
		hip->hi_notify = 0;
		dosync = 1;
	}
	if (dosync)
		sync_file();
	return (!found);
}

/* list_hosts ------------------------------------------------------------ */
/*
   Purpose:	Lists all hosts and their status.
   Returns:	Nothing.
*/

struct hoststate {
	TAILQ_ENTRY(hoststate) hs_list;
	uint16_t hs_monitor;	/* host being monitored */
	uint16_t hs_notify;	/* host needs notification */
	char hs_flag;		/* temporary flag */
#define HS_CHANGED	1
#define HS_OLD		2
#define HS_NEW		3
	char hs_name[1];	/* host's name */
};
static
TAILQ_HEAD(, hoststate) hosts;
	static uint32_t prev_state, prev_reccnt;
	static off_t prev_status_file_len;

	int list_hosts(int mode)
{
	uint i;
	int watchmode = (mode == LIST_MODE_WATCH);
	uint32_t state, reccnt;
	HostInfo *hip;
	struct hoststate *hsp, *nhsp;
	off_t off;

	init_file(_PATH_STATD_DATABASE);

	if (watchmode) {
		prev_state = 0xffffffff;
		TAILQ_INIT(&hosts);
	}
	/* print header/legend */
	printf("Status DB: %s\n", _PATH_STATD_DATABASE);
	printf("Status DB Length: %lld\n", status_file_len);
	printf("State: %d\n", ntohl(status_info->fh_state));
	printf("#Hosts: %d\n", ntohl(status_info->fh_reccnt));
	printf("  +----- Being Monitored\n");
	printf("  |  +-- Notification Required\n");
	printf("  |  |\n");
	printf("  v  v\n");
loop:
	state = ntohl(status_info->fh_state);
	reccnt = ntohl(status_info->fh_reccnt);
	status_file_len = lseek(status_fd, 0L, SEEK_END);
	map_file();

	if (watchmode && (prev_state != 0xffffffff)) {
		if (status_file_len != prev_status_file_len)
			printf("Status DB Length: %lld -> %lld\n", prev_status_file_len, status_file_len);
		if (state != prev_state)
			printf("State: %d -> %d\n", prev_state, state);
		if (reccnt != prev_reccnt)
			printf("#Hosts: %d -> %d\n", prev_reccnt, reccnt);
	}
	/* iterate through all entries */
	off = sizeof(FileHeader);
	for (i = 0; i < reccnt && off < status_file_len; i++, off += ntohs(hip->hi_len)) {
		hip = HOSTINFO(off);
		if (!hip->hi_namelen || !hip->hi_name[0] ||
		    (!hip->hi_monitored && !hip->hi_notify && (!config.verbose || watchmode)))
			continue;
		if (!watchmode) {
			printf("  %c  %c   %s\n", (hip->hi_monitored ? 'M' : ' '), (hip->hi_notify ? 'N' : ' '), hip->hi_name);
			continue;
		}
		TAILQ_FOREACH(hsp, &hosts, hs_list)
			if (!strcmp(hsp->hs_name, hip->hi_name))
			break;
		if (!hsp) {
			if (!(hsp = calloc(1, sizeof(*hsp) + ntohs(hip->hi_namelen))))
				continue;
			strncpy(hsp->hs_name, hip->hi_name, ntohs(hip->hi_namelen));
			TAILQ_INSERT_TAIL(&hosts, hsp, hs_list);
			hsp->hs_flag = HS_NEW;
		} else {
			hsp->hs_flag = ((hsp->hs_monitor == hip->hi_monitored) && (hsp->hs_notify == hip->hi_notify)) ? 0 : HS_CHANGED;
		}
		hsp->hs_monitor = hip->hi_monitored;
		hsp->hs_notify = hip->hi_notify;
	}

	if (!watchmode)
		return (0);

	TAILQ_FOREACH_SAFE(hsp, &hosts, hs_list, nhsp) {
		if (!hsp->hs_flag)
			continue;
		printf("%c %c  %c   %s\n", ((hsp->hs_flag == HS_OLD) ? '-' : ((hsp->hs_flag == HS_NEW) ? '+' : ' ')),
		       (hsp->hs_monitor ? 'M' : ' '), (hsp->hs_notify ? 'N' : ' '), hsp->hs_name);
		if (hsp->hs_flag == HS_OLD) {
			TAILQ_REMOVE(&hosts, hsp, hs_list);
			free(hsp);
		}
	}
	prev_state = state;
	prev_reccnt = reccnt;
	prev_status_file_len = status_file_len;
	TAILQ_FOREACH(hsp, &hosts, hs_list)
		hsp->hs_flag = HS_OLD;
	sleep(2);
	goto loop;
}
