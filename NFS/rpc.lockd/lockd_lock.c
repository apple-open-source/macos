/*
 * Copyright (c) 2002-2009 Apple Inc.  All rights reserved.
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
/*	$NetBSD: lockd_lock.c,v 1.5 2000/11/21 03:47:41 enami Exp $	*/
/*	$FreeBSD: src/usr.sbin/rpc.lockd/lockd_lock.c,v 1.10 2002/03/22 19:57:09 alfred Exp $ */

/*
 * Copyright (c) 2001 Andrew P. Lentvorski, Jr.
 * Copyright (c) 2000 Manuel Bouyer.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 */

#define LOCKD_DEBUG

#include <stdio.h>
#ifdef LOCKD_DEBUG
#include <stdarg.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <rpcsvc/sm_inter.h>
#include <rpcsvc/nlm_prot.h>

#include "lockd.h"
#include "lockd_lock.h"

#define MAXOBJECTSIZE 64
#define MAXBUFFERSIZE 1024

/*
 * A set of utilities for managing file locking
 *
 * XXX: All locks are in a linked list, a better structure should be used
 * to improve search/access effeciency.
 */

/* struct describing a lock */
struct file_lock {
	LIST_ENTRY(file_lock) nfslocklist;
	netobj filehandle; /* NFS filehandle */
	struct sockaddr *addr;
	struct nlm4_holder client; /* lock holder */
	u_int64_t granted_cookie;
	int nsm_status; /* status from the remote lock manager */
	int status; /* lock status, see below */
	int flags; /* lock flags, see lockd_lock.h */
	int blocking; /* blocking lock or not */
	char client_name[SM_MAXSTRLEN];	/* client_name is really variable length and must be last! */
};

LIST_HEAD(nfslocklist_head, file_lock);
struct nfslocklist_head nfslocklist_head = LIST_HEAD_INITIALIZER(nfslocklist_head);

LIST_HEAD(blockedlocklist_head, file_lock);
struct blockedlocklist_head blockedlocklist_head = LIST_HEAD_INITIALIZER(blockedlocklist_head);

/* struct describing a share reservation */
struct file_share {
	LIST_ENTRY(file_share) nfssharelist;
	netobj oh; /* share holder */
	short mode;
	short access;
	char client_name[SM_MAXSTRLEN]; /* name is really variable length and must be last! */
};
LIST_HEAD(nfssharelist_head, file_share);

/* Struct describing a file with share reservations */
struct sharefile {
	LIST_ENTRY(sharefile) sharefilelist;
	netobj filehandle; /* Local access filehandle */
	int fd; /* file descriptor: remains open until no more shares */
	int refcount;
	struct nfssharelist_head sharelist_head;
};
LIST_HEAD(nfssharefilelist_head, sharefile);
struct nfssharefilelist_head nfssharefilelist_head = LIST_HEAD_INITIALIZER(nfssharefilelist_head);

/* lock status */
#define LKST_LOCKED	1 /* lock is locked */
/* XXX: Is this flag file specific or lock specific? */
#define LKST_WAITING	2 /* file is already locked by another host */
#define LKST_PROCESSING	3 /* child is trying to aquire the lock */
#define LKST_DYING	4 /* must dies when we get news from the child */

/* struct describing a monitored host */
struct host {
	TAILQ_ENTRY(host) hostlst;
	int refcnt;
	time_t lastuse;
	struct sockaddr addr;
	char *name;	/* host name provided by client via caller_name */
	char *revname;	/* host name mapped from addr */
};
/* list of hosts we monitor */
TAILQ_HEAD(hostlst_head, host);
struct hostlst_head hostlst_head = TAILQ_HEAD_INITIALIZER(hostlst_head);
struct hostlst_head hostlst_unref = TAILQ_HEAD_INITIALIZER(hostlst_unref);

u_int64_t send_granted_cookie = 0;

/*
 * File monitoring handlers
 * XXX: These might be able to be removed when kevent support
 * is placed into the hardware lock/unlock routines.  (ie.
 * let the kernel do all the file monitoring)
 */

/* Struct describing a monitored file */
struct monfile {
	LIST_ENTRY(monfile) monfilelist;
	netobj filehandle; /* Local access filehandle */
	int fd; /* file descriptor: remains open until unlock! */
	int refcount;
	int exclusive;
};

/* List of files we monitor */
LIST_HEAD(monfilelist_head, monfile);
struct monfilelist_head monfilelist_head = LIST_HEAD_INITIALIZER(monfilelist_head);

static int debugdelay = 0;

enum nfslock_status { NFS_GRANTED = 0, NFS_GRANTED_DUPLICATE,
		      NFS_DENIED, NFS_DENIED_NOLOCK,
		      NFS_RESERR };

enum hwlock_status { HW_GRANTED = 0, HW_GRANTED_DUPLICATE,
		     HW_DENIED, HW_DENIED_NOLOCK, 
		     HW_STALEFH, HW_READONLY, HW_RESERR };

enum partialfilelock_status { PFL_GRANTED=0, PFL_GRANTED_DUPLICATE, PFL_DENIED,
			      PFL_NFSDENIED, PFL_NFSBLOCKED, PFL_NFSDENIED_NOLOCK, PFL_NFSRESERR, 
			      PFL_HWDENIED,  PFL_HWBLOCKED,  PFL_HWDENIED_NOLOCK, PFL_HWRESERR,
			      PFL_HWDENIED_STALEFH, PFL_HWDENIED_READONLY };

enum LFLAGS {LEDGE_LEFT, LEDGE_LBOUNDARY, LEDGE_INSIDE, LEDGE_RBOUNDARY, LEDGE_RIGHT};
enum RFLAGS {REDGE_LEFT, REDGE_LBOUNDARY, REDGE_INSIDE, REDGE_RBOUNDARY, REDGE_RIGHT};
/* XXX: WARNING! I HAVE OVERLOADED THIS STATUS ENUM!  SPLIT IT APART INTO TWO */
enum split_status {SPL_DISJOINT=0, SPL_LOCK1=1, SPL_LOCK2=2, SPL_CONTAINED=4, SPL_RESERR=8};

enum partialfilelock_status lock_partialfilelock(struct file_lock *fl);

int send_granted(struct file_lock *fl, int opcode);
void siglock(void);
void sigunlock(void);
void destroy_lock_host(struct host *ihp);
static void monitor_lock_host(const char *hostname, const struct sockaddr *addr);

void	copy_nlm4_lock_to_nlm4_holder(const struct nlm4_lock *src,
    const bool_t exclusive, struct nlm4_holder *dest);
struct file_lock *	allocate_file_lock(const netobj *lockowner,
    const netobj *filehandle, const struct sockaddr *addr,
    const char *caller_name);
void	deallocate_file_lock(struct file_lock *fl);
void	fill_file_lock(struct file_lock *fl,
    const bool_t exclusive, const int32_t svid,
    const u_int64_t offset, const u_int64_t len,
    const int state, const int status, const int flags, const int blocking);
int	regions_overlap(const u_int64_t start1, const u_int64_t len1,
    const u_int64_t start2, const u_int64_t len2);;
enum split_status  region_compare(const u_int64_t starte, const u_int64_t lene,
    const u_int64_t startu, const u_int64_t lenu,
    u_int64_t *start1, u_int64_t *len1, u_int64_t *start2, u_int64_t *len2);
int	same_netobj(const netobj *n0, const netobj *n1);
int	same_filelock_identity(const struct file_lock *fl0,
    const struct file_lock *fl2);

static void debuglog(char const *fmt, ...);
void dump_static_object(const unsigned char* object, const int sizeof_object,
                        unsigned char* hbuff, const int sizeof_hbuff,
                        unsigned char* cbuff, const int sizeof_cbuff);
void dump_netobj(const struct netobj *nobj);
void dump_filelock(const struct file_lock *fl);
struct file_lock *	get_lock_matching_unlock(const struct file_lock *fl, int cleanup);
enum nfslock_status	test_nfslock(const struct file_lock *fl,
    struct file_lock **conflicting_fl);
enum nfslock_status	lock_nfslock(struct file_lock *fl);
enum nfslock_status	delete_nfslock(struct file_lock *fl);
enum nfslock_status	unlock_nfslock(const struct file_lock *fl,
    struct file_lock **released_lock, struct file_lock **left_lock,
    struct file_lock **right_lock, int cleanup);
enum hwlock_status lock_hwlock(struct file_lock *fl);
enum split_status split_nfslock(const struct file_lock *exist_lock,
    const struct file_lock *unlock_lock, struct file_lock **left_lock,
    struct file_lock **right_lock);
void	add_blockingfilelock(struct file_lock *fl);
enum hwlock_status	unlock_hwlock(const struct file_lock *fl);
enum hwlock_status	test_hwlock(const struct file_lock *fl,
    struct file_lock **conflicting_fl); 
void	remove_blockingfilelock(struct file_lock *fl);
void	clear_blockingfilelock(const char *hostname);
void	retry_blockingfilelocklist(netobj *fh);
enum partialfilelock_status	unlock_partialfilelock(
    const struct file_lock *fl, int cleanup);
void	clear_partialfilelock(const char *hostname);
enum partialfilelock_status	test_partialfilelock(
    const struct file_lock *fl, struct file_lock **conflicting_fl);
enum nlm4_stats	do_test(struct file_lock *fl, struct file_lock **conflicting_fl);
enum nlm4_stats	do_unlock(struct file_lock *fl);
enum nlm4_stats	do_lock(struct file_lock *fl);
void	do_clear(const char *hostname);


void
debuglog(char const *fmt, ...)
{
	va_list ap;

	if (config.verbose < 1) {
		return;
	}

	sleep(debugdelay);

	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}

void
dump_static_object(object, size_object, hbuff, size_hbuff, cbuff, size_cbuff)
	const unsigned char *object;
	const int size_object;
	unsigned char *hbuff;
	const int size_hbuff;
	unsigned char *cbuff;
	const int size_cbuff;
{ 
	int i, objectsize;
	char *tmp = (char*)hbuff;

	if (config.verbose < 2) {
		return;
	}

	objectsize = size_object;

	if (objectsize == 0) {
		debuglog("object is size 0\n");
	} else {
		if (objectsize > MAXOBJECTSIZE) {
			debuglog("Object of size %d being clamped"
			    "to size %d\n", objectsize, MAXOBJECTSIZE);
			objectsize = MAXOBJECTSIZE;
		}
		
		if (hbuff != NULL) {
			if (size_hbuff < objectsize*2+1) {
				debuglog("Hbuff not large enough."
				    "  Increase size\n");
			} else {
				for(i=0;i<objectsize;i++,tmp+=2) {
					snprintf(tmp, size_hbuff - (2*i), "%02X", *(object+i));
				}
				*tmp = '\0';
			}
		}
		
		if (cbuff != NULL) {
			if (size_cbuff < objectsize+1) {
				debuglog("Cbuff not large enough."
				    "  Increase Size\n");
			}
			
			for(i=0;i<objectsize;i++) {
				if (*(object+i) >= 32 && *(object+i) <= 127) {
					*(cbuff+i) = *(object+i);
				} else {
					*(cbuff+i) = '.';
				}
			}
			*(cbuff+i) = '\0';
		}
	}
}

void
dump_netobj(const struct netobj *nobj)
{
	char hbuff[MAXBUFFERSIZE*2];
	char cbuff[MAXBUFFERSIZE];

	if (config.verbose < 2) {
		return;
	}

	if (nobj == NULL) {
		debuglog("Null netobj pointer\n");
	}
	else if (nobj->n_len == 0) {
		debuglog("Size zero netobj\n");
	} else {
		dump_static_object((const unsigned char *)nobj->n_bytes, nobj->n_len,
		    (unsigned char *)hbuff, sizeof(hbuff), (unsigned char *)cbuff, sizeof(cbuff));
		debuglog("netobj: len: %d  data: %s :::  %s\n",
		    nobj->n_len, hbuff, cbuff);
	}
}

/* #define DUMP_FILELOCK_VERBOSE */
void
dump_filelock(const struct file_lock *fl)
{
#ifdef DUMP_FILELOCK_VERBOSE
	char hbuff[MAXBUFFERSIZE*2];
	char cbuff[MAXBUFFERSIZE];
#endif

	if (config.verbose < 2) {
		return;
	}

	if (fl != NULL) {
		debuglog("Dumping file lock structure @ %p\n", fl);

#ifdef DUMP_FILELOCK_VERBOSE
		dump_static_object((unsigned char *)&fl->filehandle.n_bytes,
		    fl->filehandle.n_len, hbuff, sizeof(hbuff),
		    cbuff, sizeof(cbuff));
		debuglog("Filehandle: %8s  :::  %8s\n", hbuff, cbuff);
#endif
		
		debuglog("Dumping nlm4_holder:\n"
		    "exc: %x  svid: %x  offset:len %llx:%llx\n",
		    fl->client.exclusive, fl->client.svid,
		    fl->client.l_offset, fl->client.l_len);

#ifdef DUMP_FILELOCK_VERBOSE
		debuglog("Dumping client identity:\n");
		dump_netobj(&fl->client.oh);
		
		debuglog("nsm: %d  status: %d  flags: %d  locker: %d"
		    "  fd:  %d\n", fl->nsm_status, fl->status,
		    fl->flags, fl->locker, fl->fd);
#endif
	} else {
		debuglog("NULL file lock structure\n");
	}
}

void
copy_nlm4_lock_to_nlm4_holder(src, exclusive, dest)
	const struct nlm4_lock *src;
	const bool_t exclusive;
	struct nlm4_holder *dest;
{

	dest->exclusive = exclusive;
	dest->oh.n_len = src->oh.n_len;
	dest->oh.n_bytes = src->oh.n_bytes;
	dest->svid = src->svid;
	dest->l_offset = src->l_offset;
	dest->l_len = src->l_len;
}


static size_t
strnlen(const char *s, size_t len)
{
    size_t n;

    for (n = 0;  s[n] != 0 && n < len; n++)
        ;
    return n;
}

/*
 * allocate_file_lock: Create a lock with the given parameters
 */

struct file_lock *
allocate_file_lock(const netobj *lockowner, const netobj *filehandle,
		   const struct sockaddr *addr, const char *caller_name)
{
	struct file_lock *newfl;
	size_t n;

	/* Beware of rubbish input! */
	n = strnlen(caller_name, SM_MAXSTRLEN);
	if (n == SM_MAXSTRLEN) {
		return NULL;
	}

	newfl = malloc(sizeof(*newfl) - sizeof(newfl->client_name) + n + 1);
	if (newfl == NULL) {
		return NULL;
	}
	bzero(newfl, sizeof(*newfl) - sizeof(newfl->client_name));
	memcpy(newfl->client_name, caller_name, n);
	newfl->client_name[n] = 0;

	newfl->client.oh.n_bytes = malloc(lockowner->n_len);
	if (newfl->client.oh.n_bytes == NULL) {
		free(newfl);
		return NULL;
	}
	newfl->client.oh.n_len = lockowner->n_len;
	bcopy(lockowner->n_bytes, newfl->client.oh.n_bytes, lockowner->n_len);

	newfl->filehandle.n_bytes = malloc(filehandle->n_len);
	if (newfl->filehandle.n_bytes == NULL) {
		free(newfl->client.oh.n_bytes);
		free(newfl);
		return NULL;
	}
	newfl->filehandle.n_len = filehandle->n_len;
	bcopy(filehandle->n_bytes, newfl->filehandle.n_bytes, filehandle->n_len);

	newfl->addr = malloc(addr->sa_len);
	if (newfl->addr == NULL) {
		free(newfl->client.oh.n_bytes);
		free(newfl);
		return NULL;
	}
	memcpy(newfl->addr, addr, addr->sa_len);

	return newfl;
}

/*
 * file_file_lock: Force creation of a valid file lock
 */
void
fill_file_lock(struct file_lock *fl,
    const bool_t exclusive, const int32_t svid,
    const u_int64_t offset, const u_int64_t len,
    const int state, const int status, const int flags, const int blocking)
{
	fl->client.exclusive = exclusive;
	fl->client.svid = svid;
	fl->client.l_offset = offset;
	fl->client.l_len = len;

	fl->nsm_status = state;
	fl->status = status;
	fl->flags = flags;
	fl->blocking = blocking;
}

/*
 * deallocate_file_lock: Free all storage associated with a file lock
 */
void
deallocate_file_lock(struct file_lock *fl)
{
	free(fl->addr);
	free(fl->client.oh.n_bytes);
	free(fl->filehandle.n_bytes);
	free(fl);
}

/*
 * regions_overlap(): This function examines the two provided regions for
 * overlap.
 */
int
regions_overlap(start1, len1, start2, len2)
	const u_int64_t start1, len1, start2, len2;
{
	u_int64_t d1,d2,d3,d4;
	enum split_status result;

	debuglog("Entering region overlap with vals: %llu:%llu--%llu:%llu\n",
		 start1, len1, start2, len2);

	result = region_compare(start1, len1, start2, len2,
	    &d1, &d2, &d3, &d4);

	debuglog("Exiting region overlap with val: %d\n",result);

	if (result == SPL_DISJOINT) {
		return 0;
	} else {
		return 1;
	}

	return (result);
}

/*
 * region_compare(): Examine lock regions and split appropriately
 *
 * XXX: Fix 64 bit overflow problems
 * XXX: Check to make sure I got *ALL* the cases. 
 * XXX: This DESPERATELY needs a regression test.
 */
enum split_status
region_compare(starte, lene, startu, lenu,
    start1, len1, start2, len2)
	const u_int64_t starte, lene, startu, lenu;
	u_int64_t *start1, *len1, *start2, *len2;
{
	/*
	 * Please pay attention to the sequential exclusions
	 * of the if statements!!!
	 */
	enum LFLAGS lflags = LEDGE_LEFT;
	enum RFLAGS rflags;
	enum split_status retval;

	retval = SPL_DISJOINT;

	if (lene == 0 && lenu == 0) {
		/* Examine left edge of locker */
		if (startu < starte) {
			lflags = LEDGE_LEFT;
		} else if (startu == starte) {
			lflags = LEDGE_LBOUNDARY;
		} else {
			lflags = LEDGE_INSIDE;
		}

		rflags = REDGE_RBOUNDARY; /* Both are infiinite */

		if (lflags == LEDGE_INSIDE) {
			*start1 = starte;
			*len1 = startu - starte;
		}

		if (lflags == LEDGE_LEFT || lflags == LEDGE_LBOUNDARY) {
			retval = SPL_CONTAINED;
		} else {
			retval = SPL_LOCK1;
		}
	} else if (lene == 0 && lenu != 0) {
		/* Established lock is infinite */
		/* Examine left edge of unlocker */
		if (startu < starte) {
			lflags = LEDGE_LEFT;
		} else if (startu == starte) {
			lflags = LEDGE_LBOUNDARY;
		} else if (startu > starte) {
			lflags = LEDGE_INSIDE;
		}

		/* Examine right edge of unlocker */
		if (startu + lenu < starte) {
			/* Right edge of unlocker left of established lock */
			rflags = REDGE_LEFT;
			return SPL_DISJOINT;
		} else if (startu + lenu == starte) {
			/* Right edge of unlocker on start of established lock */
			rflags = REDGE_LBOUNDARY;
			return SPL_DISJOINT;
		} else { /* Infinifty is right of finity */
			/* Right edge of unlocker inside established lock */
			rflags = REDGE_INSIDE;
		}

		if (lflags == LEDGE_INSIDE) {
			*start1 = starte;
			*len1 = startu - starte;
			retval |= SPL_LOCK1;
		}

		if (rflags == REDGE_INSIDE) {
			/* Create right lock */
			*start2 = startu+lenu;
			*len2 = 0;
			retval |= SPL_LOCK2;
		}
	} else if (lene != 0 && lenu == 0) {
		/* Unlocker is infinite */
		/* Examine left edge of unlocker */
		if (startu < starte) {
			lflags = LEDGE_LEFT;
			retval = SPL_CONTAINED;
			return retval;
		} else if (startu == starte) {
			lflags = LEDGE_LBOUNDARY;
			retval = SPL_CONTAINED;
			return retval;
		} else if ((startu > starte) && (startu < starte + lene - 1)) {
			lflags = LEDGE_INSIDE;
		} else if (startu == starte + lene - 1) {
			lflags = LEDGE_RBOUNDARY;
		} else { /* startu > starte + lene -1 */
			lflags = LEDGE_RIGHT;
			return SPL_DISJOINT;
		}

		rflags = REDGE_RIGHT; /* Infinity is right of finity */

		if (lflags == LEDGE_INSIDE || lflags == LEDGE_RBOUNDARY) {
			*start1 = starte;
			*len1 = startu - starte;
			retval |= SPL_LOCK1;
			return retval;
		}

	} else {
		/* Both locks are finite */

		/* Examine left edge of unlocker */
		if (startu < starte) {
			lflags = LEDGE_LEFT;
		} else if (startu == starte) {
			lflags = LEDGE_LBOUNDARY;
		} else if ((startu > starte) && (startu < starte + lene - 1)) {
			lflags = LEDGE_INSIDE;
		} else if (startu == starte + lene - 1) {
			lflags = LEDGE_RBOUNDARY;
		} else { /* startu > starte + lene -1 */
			lflags = LEDGE_RIGHT;
			return SPL_DISJOINT;
		}

		/* Examine right edge of unlocker */
		if (startu + lenu < starte) {
			/* Right edge of unlocker left of established lock */
			rflags = REDGE_LEFT;
			return SPL_DISJOINT;
		} else if (startu + lenu == starte) {
			/* Right edge of unlocker on start of established lock */
			rflags = REDGE_LBOUNDARY;
			return SPL_DISJOINT;
		} else if (startu + lenu < starte + lene) {
			/* Right edge of unlocker inside established lock */
			rflags = REDGE_INSIDE;
		} else if (startu + lenu == starte + lene) {
			/* Right edge of unlocker on right edge of established lock */
			rflags = REDGE_RBOUNDARY;
		} else { /* startu + lenu > starte + lene */
			/* Right edge of unlocker is right of established lock */
			rflags = REDGE_RIGHT;
		}

		if (lflags == LEDGE_INSIDE || lflags == LEDGE_RBOUNDARY) {
			/* Create left lock */
			*start1 = starte;
			*len1 = (startu - starte);
			retval |= SPL_LOCK1;
		}

		if (rflags == REDGE_INSIDE) {
			/* Create right lock */
			*start2 = startu+lenu;
			*len2 = starte+lene-(startu+lenu);
			retval |= SPL_LOCK2;
		}

		if ((lflags == LEDGE_LEFT || lflags == LEDGE_LBOUNDARY) &&
		    (rflags == REDGE_RBOUNDARY || rflags == REDGE_RIGHT)) {
			retval = SPL_CONTAINED;
		}
	}

	return retval;
}

/*
 * same_netobj: Compares the apprpriate bits of a netobj for identity
 */
int
same_netobj(const netobj *n0, const netobj *n1)
{
	int retval;

	retval = 0;

	debuglog("Entering netobj identity check\n");

	if (n0->n_len == n1->n_len) {
		debuglog("Preliminary length check passed\n");
		retval = !bcmp(n0->n_bytes, n1->n_bytes, n0->n_len);
		debuglog("netobj %smatch\n", retval ? "" : "mis");
	}
	
	return (retval);
}

/*
 * same_filelock_identity: Compares the appropriate bits of a file_lock
 */
int
same_filelock_identity(fl0, fl1)
	const struct file_lock *fl0, *fl1;
{
	int retval;

	retval = 0;

	debuglog("Checking filelock identity\n");

	/*
	 * Check process ids and host information.
	 */
	retval = (fl0->client.svid == fl1->client.svid &&
	    same_netobj(&(fl0->client.oh), &(fl1->client.oh)));

	debuglog("Exiting checking filelock identity: retval: %d\n",retval);

	return (retval);
}

/*
 * Below here are routines associated with manipulating the NFS
 * lock list.
 */

/*
 * get_lock_matching_unlock: Return a lock which matches the given unlock lock
 *                           or NULL otherwise
 * XXX: It is a shame that this duplicates so much code from test_nfslock.
 */
struct file_lock *
get_lock_matching_unlock(const struct file_lock *fl, int cleanup)
{
	struct file_lock *ifl; /* Iterator */

	debuglog("Entering lock_matching_unlock\n");
	debuglog("********Dump of fl*****************\n");
	dump_filelock(fl);

	LIST_FOREACH(ifl, &nfslocklist_head, nfslocklist) {
		debuglog("Pointer to file lock: %p\n",ifl);

		debuglog("****Dump of ifl****\n");
		dump_filelock(ifl);
		debuglog("*******************\n");

		if (cleanup && (ifl == fl))  /* don't match the lock we're cleaning up under */
			continue;

		/*
		 * XXX: It is conceivable that someone could use the NLM RPC
		 * system to directly access filehandles.  This may be a
		 * security hazard as the filehandle code may bypass normal
		 * file access controls
		 */
		if (fl->filehandle.n_len != ifl->filehandle.n_len)
			continue;
		if (bcmp(fl->filehandle.n_bytes, ifl->filehandle.n_bytes,
			fl->filehandle.n_len))
			continue;

		debuglog("matching_unlock: Filehandles match, "
		    "checking regions\n");

		/* Filehandles match, check for region overlap */
		if (!regions_overlap(fl->client.l_offset, fl->client.l_len,
			ifl->client.l_offset, ifl->client.l_len))
			continue;
	       
		debuglog("matching_unlock: Region overlap"
		    " found %llu : %llu -- %llu : %llu\n",
		    fl->client.l_offset,fl->client.l_len,
		    ifl->client.l_offset,ifl->client.l_len);

		/* Regions overlap, check the identity */
		if (!same_filelock_identity(fl,ifl))
			continue;

		debuglog("matching_unlock: Duplicate lock id.  Granting\n");
		return (ifl);
	}

	debuglog("Exiting lock_matching_unlock\n");

	return (NULL);
}

/*
 * test_nfslock: check for NFS lock in lock list
 *
 * This routine makes the following assumptions:
 *    1) Nothing will adjust the lock list during a lookup
 *
 * This routine has an intersting quirk which bit me hard.
 * The conflicting_fl is the pointer to the conflicting lock.
 * However, to modify the "*pointer* to the conflicting lock" rather
 * that the "conflicting lock itself" one must pass in a "pointer to
 * the pointer of the conflicting lock".  Gross.
 */

enum nfslock_status
test_nfslock(const struct file_lock *fl, struct file_lock **conflicting_fl)
{
	struct file_lock *ifl; /* Iterator */
	enum nfslock_status retval;

	debuglog("Entering test_nfslock\n");

	retval = NFS_GRANTED;
	(*conflicting_fl) = NULL;

	debuglog("Entering lock search loop\n");

	debuglog("***********************************\n");
	debuglog("Dumping match filelock\n");
	debuglog("***********************************\n");
	dump_filelock(fl);
	debuglog("***********************************\n");

	LIST_FOREACH(ifl, &nfslocklist_head, nfslocklist) {
		if (retval == NFS_DENIED)
			break;

		debuglog("Top of lock loop\n");
		debuglog("Pointer to file lock: %p\n",ifl);
		
		debuglog("***********************************\n");
		debuglog("Dumping test filelock\n");
		debuglog("***********************************\n");
		dump_filelock(ifl);
		debuglog("***********************************\n");

		/*
		 * XXX: It is conceivable that someone could use the NLM RPC
		 * system to directly access filehandles.  This may be a
		 * security hazard as the filehandle code may bypass normal
		 * file access controls
		 */
		if (fl->filehandle.n_len != ifl->filehandle.n_len)
			continue;
		if (bcmp(fl->filehandle.n_bytes, ifl->filehandle.n_bytes,
			fl->filehandle.n_len))
			continue;

		debuglog("test_nfslock: filehandle match found\n");

		/* Filehandles match, check for region overlap */
		if (!regions_overlap(fl->client.l_offset, fl->client.l_len,
			ifl->client.l_offset, ifl->client.l_len))
			continue;

		debuglog("test_nfslock: Region overlap found"
		    " %llu : %llu -- %llu : %llu\n",
		    fl->client.l_offset,fl->client.l_len,
		    ifl->client.l_offset,ifl->client.l_len);

		/* Regions overlap, check the exclusivity */
		if (!(fl->client.exclusive || ifl->client.exclusive))
			continue;
	       
		debuglog("test_nfslock: Exclusivity failure: %d %d\n",
		    fl->client.exclusive,
		    ifl->client.exclusive);

		if (same_filelock_identity(fl,ifl)) {
			debuglog("test_nfslock: Duplicate id.  Granting\n");
			(*conflicting_fl) = ifl;
			retval = NFS_GRANTED_DUPLICATE;
		} else {
			/* locking attempt fails */
			debuglog("test_nfslock: Lock attempt failed\n");
			debuglog("Desired lock\n");
			dump_filelock(fl);
			debuglog("Conflicting lock\n");
			dump_filelock(ifl);
			(*conflicting_fl) = ifl;
			retval = NFS_DENIED;
		}
	}
	
	debuglog("Dumping file locks\n");
	debuglog("Exiting test_nfslock\n");
	
	return (retval);
}

/*
 * lock_nfslock: attempt to create a lock in the NFS lock list
 *
 * This routine tests whether the lock will be granted and then adds
 * the entry to the lock list if so.
 * 
 * Argument fl gets modified as its list housekeeping entries get modified
 * upon insertion into the NFS lock list
 *
 * This routine makes several assumptions:
 *    1) It is perfectly happy to grant a duplicate lock from the same pid.
 *       While this seems to be intuitively wrong, it is required for proper
 *       Posix semantics during unlock.  It is absolutely imperative to not
 *       unlock the main lock before the two child locks are established. Thus,
 *       one has be be able to create duplicate locks over an existing lock
 *    2) It currently accepts duplicate locks from the same id,pid
 */

enum nfslock_status
lock_nfslock(struct file_lock *fl)
{
	enum nfslock_status retval;
	struct file_lock *dummy_fl;

	dummy_fl = NULL;

	debuglog("Entering lock_nfslock...\n");

	retval = test_nfslock(fl,&dummy_fl);

	if (retval == NFS_GRANTED || retval == NFS_GRANTED_DUPLICATE) {
		debuglog("Inserting lock...\n");
		dump_filelock(fl);
		LIST_INSERT_HEAD(&nfslocklist_head, fl, nfslocklist);
	}

	debuglog("Exiting lock_nfslock...\n");

	return (retval);
}

/*
 * delete_nfslock: delete an NFS lock list entry
 *
 * This routine is used to delete a lock out of the NFS lock list
 * without regard to status, underlying locks, regions or anything else
 *
 * Note that this routine *does not deallocate memory* of the lock.
 * It just disconnects it from the list.  The lock can then be used
 * by other routines without fear of trashing the list.
 */

enum nfslock_status
delete_nfslock(struct file_lock *fl)
{

	LIST_REMOVE(fl, nfslocklist);

	return (NFS_GRANTED);
}

enum split_status
split_nfslock(exist_lock, unlock_lock, left_lock, right_lock)
	const struct file_lock *exist_lock, *unlock_lock;
	struct file_lock **left_lock, **right_lock;
{
	u_int64_t start1, len1, start2, len2;
	enum split_status spstatus;

	spstatus = region_compare(exist_lock->client.l_offset, exist_lock->client.l_len,
	    unlock_lock->client.l_offset, unlock_lock->client.l_len,
	    &start1, &len1, &start2, &len2);

	if ((spstatus & SPL_LOCK1) != 0) {
		*left_lock = allocate_file_lock(&exist_lock->client.oh, &exist_lock->filehandle, exist_lock->addr, exist_lock->client_name);
		if (*left_lock == NULL) {
			debuglog("Unable to allocate resource for split 1\n");
			return SPL_RESERR;
		}

		fill_file_lock(*left_lock,
		    exist_lock->client.exclusive, exist_lock->client.svid,
		    start1, len1,
		    exist_lock->nsm_status,
		    exist_lock->status, exist_lock->flags, exist_lock->blocking);
	}

	if ((spstatus & SPL_LOCK2) != 0) {
		*right_lock = allocate_file_lock(&exist_lock->client.oh, &exist_lock->filehandle, exist_lock->addr, exist_lock->client_name);
		if (*right_lock == NULL) {
			debuglog("Unable to allocate resource for split 1\n");
			if (*left_lock != NULL) {
				deallocate_file_lock(*left_lock);
			}
			return SPL_RESERR;
		}

		fill_file_lock(*right_lock,
		    exist_lock->client.exclusive, exist_lock->client.svid,
		    start2, len2,
		    exist_lock->nsm_status,
		    exist_lock->status, exist_lock->flags, exist_lock->blocking);
	}

	return spstatus;
}

enum nfslock_status
unlock_nfslock(fl, released_lock, left_lock, right_lock, cleanup)
	const struct file_lock *fl;
	struct file_lock **released_lock;
	struct file_lock **left_lock;
	struct file_lock **right_lock;
	int cleanup;
{
	struct file_lock *mfl; /* Matching file lock */
	enum nfslock_status retval;
	enum split_status spstatus;

	debuglog("Entering unlock_nfslock\n");

	*released_lock = NULL;
	*left_lock = NULL;
	*right_lock = NULL;

	retval = NFS_DENIED_NOLOCK;

	debuglog("Attempting to match lock...\n");
	mfl = get_lock_matching_unlock(fl, cleanup);

	if (mfl != NULL) {
		debuglog("Unlock matched.  Querying for split\n");

		spstatus = split_nfslock(mfl, fl, left_lock, right_lock);

		debuglog("Split returned %d %p %p %p %p\n",spstatus,mfl,fl,*left_lock,*right_lock);
		debuglog("********Split dumps********");
		dump_filelock(mfl);
		dump_filelock(fl);
		dump_filelock(*left_lock);
		dump_filelock(*right_lock);
		debuglog("********End Split dumps********");

		if (spstatus == SPL_RESERR) {
			if (*left_lock != NULL) {
				deallocate_file_lock(*left_lock);
				*left_lock = NULL;
			}

			if (*right_lock != NULL) {
				deallocate_file_lock(*right_lock);
				*right_lock = NULL;
			}

			return NFS_RESERR;
		}

		/* Insert new locks from split if required */
		if (*left_lock != NULL) {
			debuglog("Split left activated\n");
			LIST_INSERT_HEAD(&nfslocklist_head, *left_lock, nfslocklist);
		}

		if (*right_lock != NULL) {
			debuglog("Split right activated\n");
			LIST_INSERT_HEAD(&nfslocklist_head, *right_lock, nfslocklist);
		}

		/* Unlock the lock since it matches identity */
		LIST_REMOVE(mfl, nfslocklist);
		*released_lock = mfl;
		retval = NFS_GRANTED;
	}

	debuglog("Exiting unlock_nfslock\n");

	return retval;
}

/*
 * Below here are the routines for manipulating the file lock directly
 * on the disk hardware itself
 */
enum hwlock_status
lock_hwlock(struct file_lock *fl)
{
	struct monfile *imf,*nmf;
	int lflags, flerror;
	fhandle_t fh;

	/* Scan to see if filehandle already present */
	LIST_FOREACH(imf, &monfilelist_head, monfilelist) {
		if ((fl->filehandle.n_len == imf->filehandle.n_len) &&
		    (bcmp(fl->filehandle.n_bytes, imf->filehandle.n_bytes,
			fl->filehandle.n_len) == 0)) {
			/* imf is the correct filehandle */
			break;
		}
	}

	/*
	 * Filehandle already exists (we control the file)
	 * *AND* NFS has already cleared the lock for availability
	 * Grant it and bump the refcount.
	 */
	if (imf != NULL) {
		++(imf->refcount);
		return (HW_GRANTED);
	}

	/* No filehandle found, create and go */
	nmf = malloc(sizeof(struct monfile));
	if (nmf == NULL) {
		debuglog("hwlock resource allocation failure\n");
		return (HW_RESERR);
	}
	nmf->filehandle.n_bytes = malloc(fl->filehandle.n_len);
	if (nmf->filehandle.n_bytes == NULL) {
		debuglog("hwlock resource allocation failure\n");
		free(nmf);
		return (HW_RESERR);
	}

	if (fl->filehandle.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("hwlock: bad fh length %d (from %16s): %32s\n",
		    fl->filehandle.n_len, fl->client_name, strerror(errno));
		free(nmf->filehandle.n_bytes);
		free(nmf);
		return (HW_STALEFH);
	}
	fh.fh_len = fl->filehandle.n_len;
	bcopy(fl->filehandle.n_bytes, fh.fh_data, fh.fh_len);

	/* O_RDWR may not work if file system is read-only */
	nmf->fd = fhopen(&fh, O_RDWR);
	if ((nmf->fd < 0) && (errno == EROFS) && !fl->client.exclusive)
		nmf->fd = fhopen(&fh, O_RDONLY);
	if (nmf->fd < 0) {
		debuglog("fhopen failed (from %16s): %32s\n",
		    fl->client_name, strerror(errno));
		free(nmf->filehandle.n_bytes);
		free(nmf);
		switch (errno) {
		case ESTALE:
			return (HW_STALEFH);
		case EROFS:
			return (HW_READONLY);
		default:
			return (HW_RESERR);
		}
	}

	/* File opened correctly, fill the monitor struct */
	nmf->filehandle.n_len = fl->filehandle.n_len;
	bcopy(fl->filehandle.n_bytes, nmf->filehandle.n_bytes, fl->filehandle.n_len);
	nmf->refcount = 1;
	nmf->exclusive = fl->client.exclusive;

	lflags = (nmf->exclusive == 1) ?
	    (LOCK_EX | LOCK_NB) : (LOCK_SH | LOCK_NB);

	flerror = flock(nmf->fd, lflags);

	if (flerror != 0) {
		debuglog("flock failed (from %16s): %32s\n",
		    fl->client_name, strerror(errno));
		close(nmf->fd);
		free(nmf->filehandle.n_bytes);
		free(nmf);
		switch (errno) {
		case EAGAIN:
			return (HW_DENIED);
		case ESTALE:
			return (HW_STALEFH);
		case EROFS:
			return (HW_READONLY);
		default:
			return (HW_RESERR);
			break;
		}
	}

	/* File opened and locked */
	LIST_INSERT_HEAD(&monfilelist_head, nmf, monfilelist);

	debuglog("flock succeeded (from %16s)\n", fl->client_name);
	return (HW_GRANTED);
}
		
enum hwlock_status
unlock_hwlock(const struct file_lock *fl)
{
	struct monfile *imf;

	debuglog("Entering unlock_hwlock\n");
	debuglog("Entering loop interation\n");

	/* Scan to see if filehandle already present */
	LIST_FOREACH(imf, &monfilelist_head, monfilelist) {
		if ((fl->filehandle.n_len == imf->filehandle.n_len) &&
		    (bcmp(fl->filehandle.n_bytes, imf->filehandle.n_bytes,
			fl->filehandle.n_len) == 0)) {
			/* imf is the correct filehandle */
			break;
		}
	}

	debuglog("Completed iteration.  Proceeding\n");

	if (imf == NULL) {
		/* No lock found */
		debuglog("Exiting unlock_hwlock (HW_DENIED_NOLOCK)\n");
		return (HW_DENIED_NOLOCK);
	}

	/* Lock found */
	--imf->refcount;

	if (imf->refcount < 0) {
		debuglog("Negative hardware reference count\n");
	}

	if (imf->refcount <= 0) {
		close(imf->fd);
		LIST_REMOVE(imf, monfilelist);
		free(imf->filehandle.n_bytes);
		free(imf);
	}
	debuglog("Exiting unlock_hwlock (HW_GRANTED)\n");
	return (HW_GRANTED);
}

enum hwlock_status
test_hwlock(fl, conflicting_fl)
	const struct file_lock *fl __unused;
	struct file_lock **conflicting_fl __unused;
{

	/*
	 * XXX: lock tests on hardware are not required until
	 * true partial file testing is done on the underlying file
	 */
	return (HW_RESERR);
}



/*
 * Below here are routines for manipulating blocked lock requests
 * They should only be called from the XXX_partialfilelock routines
 * if at all possible
 */

void
add_blockingfilelock(struct file_lock *fl)
{
	struct file_lock *ifl, *nfl;

	debuglog("Entering add_blockingfilelock\n");

	/*
	 * Check for a duplicate lock request.
	 * If found, deallocate the older request.
	 */
	ifl = LIST_FIRST(&blockedlocklist_head);
	for (; ifl != NULL; ifl = nfl) {
		debuglog("Pointer to file lock: %p\n",ifl);
		debuglog("****Dump of ifl****\n");
		dump_filelock(ifl);
		debuglog("*******************\n");

		nfl = LIST_NEXT(ifl, nfslocklist);

		if (fl->filehandle.n_len != ifl->filehandle.n_len)
			continue;
		if (bcmp(fl->filehandle.n_bytes, ifl->filehandle.n_bytes,
			fl->filehandle.n_len))
			continue;

		/* Filehandles match, check region */
		if ((fl->client.l_offset != ifl->client.l_offset) ||
		    (fl->client.l_len != ifl->client.l_len))
			continue;

		/* Regions match, check the identity */
		if (!same_filelock_identity(fl,ifl))
			continue;

		debuglog("add_blockingfilelock: removing duplicate lock request.\n");
		remove_blockingfilelock(ifl);
		deallocate_file_lock(ifl);
		break;
	}

	/*
	 * Clear the blocking flag so that it can be reused without
	 * adding it to the blocking queue a second time
	 */

	fl->blocking = 0;
	LIST_INSERT_HEAD(&blockedlocklist_head, fl, nfslocklist);

	debuglog("Exiting add_blockingfilelock\n");
}

void
remove_blockingfilelock(struct file_lock *fl)
{

	debuglog("Entering remove_blockingfilelock\n");

	LIST_REMOVE(fl, nfslocklist);

	debuglog("Exiting remove_blockingfilelock\n");
}

void
clear_blockingfilelock(const char *hostname)
{
	struct file_lock *ifl,*nfl;

	/*
	 * Normally, LIST_FOREACH is called for, but since
	 * the current element *is* the iterator, deleting it
	 * would mess up the iteration.  Thus, a next element
	 * must be used explicitly
	 */

	ifl = LIST_FIRST(&blockedlocklist_head);

	while (ifl != NULL) {
		nfl = LIST_NEXT(ifl, nfslocklist);

		if (strncmp(hostname, ifl->client_name, SM_MAXSTRLEN) == 0) {
			remove_blockingfilelock(ifl);
			deallocate_file_lock(ifl);
		}

		ifl = nfl;
	}
}

int need_retry_blocked_locks = 0; /* need to call retry_blockingfilelocklist() */

void
retry_blockingfilelocklist(netobj *fh)
{
	/*
	 * If fh is given, then retry just the locks with the
	 * same filehandle in the blocked list.
	 * Otherwise, simply retry all locks in the blocked list.
	 */
	struct file_lock *ifl, *nfl, *pfl; /* Iterator */
	enum partialfilelock_status pflstatus;
	int rv;

	debuglog("Entering retry_blockingfilelocklist\n");

	need_retry_blocked_locks = 0;

	pfl = NULL;
	ifl = LIST_FIRST(&blockedlocklist_head);
	debuglog("Iterator choice %p\n",ifl);

	while (ifl != NULL) {
		/*
		 * SUBTLE BUG: The next element must be worked out before the
		 * current element has been moved
		 */
		nfl = LIST_NEXT(ifl, nfslocklist);
		debuglog("Iterator choice %p\n",ifl);
		debuglog("Prev iterator choice %p\n",pfl);
		debuglog("Next iterator choice %p\n",nfl);

		/* if given a filehandle, only retry locks for the same filehandle */
		if (fh && !same_netobj(fh, &ifl->filehandle)) {
			ifl = nfl;
			continue;
		}

		/*
		 * SUBTLE BUG: The file_lock must be removed from the
		 * old list so that it's list pointers get disconnected
		 * before being allowed to participate in the new list
		 * which will automatically add it in if necessary.
		 */

		LIST_REMOVE(ifl, nfslocklist);
		pflstatus = lock_partialfilelock(ifl);
		
		if (pflstatus == PFL_GRANTED || pflstatus == PFL_GRANTED_DUPLICATE) {
			debuglog("Granted blocked lock\n");
			/* lock granted and is now being used */
			rv = send_granted(ifl, 0);
			if (rv) {
				/*
				 * Uh oh... the NLM_GRANTED message failed.
				 * About the only thing we can do is drop the lock.
				 * Note: this could be bad if the error was only
				 * transient.  Hopefully, if the client is still
				 * waiting for the lock, they will resend the request.
				 */
				do_unlock(ifl);
				/* ifl is NO LONGER VALID AT THIS POINT */
			}
		} else if (pflstatus == PFL_HWDENIED_STALEFH) {
			/*
			 * Uh oh...
			 * It would be nice if we could inform the client of
			 * this error.  Unfortunately, there's no way to do
			 * that in the NLM protocol (can't send "granted"
			 * message with an error and there's no "never going
			 * to be granted" message).
			 *
			 * Since there's no chance of this blocked request ever
			 * succeeding, we drop the lock request rather than
			 * needlessly keeping it around just to rot forever in
			 * the blocked lock list.
			 *
			 * Hopefully, if the client is still waiting for the lock,
			 * they will resend the request (and get an error then).
			 *
			 * XXX Note: PFL_HWDENIED_READONLY could potentially
			 * be handled this way as well, although that would
			 * only be an issue if a file system changed from
			 * read-write to read-only out from under a blocked
			 * lock request, and that's far less likely than a
			 * file disappearing out from under such a request.
			 */
			deallocate_file_lock(ifl);
			/* ifl is NO LONGER VALID AT THIS POINT */
		} else {
			/* Reinsert lock back into same place in blocked list */
			debuglog("Replacing blocked lock\n");
			if (pfl != NULL)
				LIST_INSERT_AFTER(pfl, ifl, nfslocklist);
			else
				/* ifl is the only elem. in the list */
				LIST_INSERT_HEAD(&blockedlocklist_head, ifl, nfslocklist);
		}

		if (pflstatus == PFL_GRANTED || pflstatus == PFL_GRANTED_DUPLICATE ||
		    pflstatus == PFL_HWDENIED_STALEFH) {
			/* If ifl was permanently removed from the list, (e.g it */
			/* was granted or dropped), pfl should remain where it's at. */
		} else {
			/* If ifl was left in the list, (e.g it was reinserted back */
			/* in place), pfl should simply be moved forward to be ifl */
			pfl = ifl;
		}
		/* Valid increment behavior regardless of state of ifl */
		ifl = nfl;
	}

	debuglog("Exiting retry_blockingfilelocklist\n");
}

/*
 * Below here are routines associated with manipulating all
 * aspects of the partial file locking system (list, hardware, etc.)
 */

/*
 * Please note that lock monitoring must be done at this level which
 * keeps track of *individual* lock requests on lock and unlock
 *
 * XXX: Split unlocking is going to make the unlock code miserable
 */

/*
 * lock_partialfilelock:
 *
 * Argument fl gets modified as its list housekeeping entries get modified
 * upon insertion into the NFS lock list
 *
 * This routine makes several assumptions:
 * 1) It (will) pass locks through to flock to lock the entire underlying file
 *     and then parcel out NFS locks if it gets control of the file.
 *         This matches the old rpc.lockd file semantics (except where it
 *         is now more correct).  It is the safe solution, but will cause
 *         overly restrictive blocking if someone is trying to use the
 *         underlying files without using NFS.  This appears to be an
 *         acceptable tradeoff since most people use standalone NFS servers.
 * XXX: The right solution is probably kevent combined with fcntl
 *
 *    2) Nothing modifies the lock lists between testing and granting
 *           I have no idea whether this is a useful assumption or not
 */

enum partialfilelock_status
lock_partialfilelock(struct file_lock *fl)
{
	enum partialfilelock_status retval;
	enum nfslock_status lnlstatus;
	enum hwlock_status hwstatus;

	debuglog("Entering lock_partialfilelock\n");

	retval = PFL_DENIED;

	/*
	 * Execute the NFS lock first, if possible, as it is significantly
	 * easier and less expensive to undo than the filesystem lock
	 */

	lnlstatus = lock_nfslock(fl);

	switch (lnlstatus) {
	case NFS_GRANTED:
	case NFS_GRANTED_DUPLICATE:
		/*
		 * At this point, the NFS lock is allocated and active.
		 * Remember to clean it up if the hardware lock fails
		 */
		hwstatus = lock_hwlock(fl);

		switch (hwstatus) {
		case HW_GRANTED:
		case HW_GRANTED_DUPLICATE:
			debuglog("HW GRANTED\n");
			/*
			 * XXX: Fixme: Check hwstatus for duplicate when
			 * true partial file locking and accounting is
			 * done on the hardware 
			 */
			if (lnlstatus == NFS_GRANTED_DUPLICATE) {
				retval = PFL_GRANTED_DUPLICATE;
			} else {
				retval = PFL_GRANTED;
			}
			if (fl->flags & LOCK_MON)
				monitor_lock_host_by_name(fl->client_name, fl->addr);
			break;
		case HW_RESERR:
			debuglog("HW RESERR\n");
			retval = PFL_HWRESERR;
			break;
		case HW_DENIED:
			debuglog("HW DENIED\n");
			retval = PFL_HWDENIED;
			break;
		case HW_DENIED_NOLOCK:
			debuglog("HW DENIED NOLOCK\n");
			retval = PFL_HWDENIED_NOLOCK;
			break;
		case HW_STALEFH:
			debuglog("HW STALE FH\n");
			retval = PFL_HWDENIED_STALEFH;
			break;
		case HW_READONLY:
			debuglog("HW READONLY\n");
			retval = PFL_HWDENIED_READONLY;
			break;
		default:
			debuglog("Unmatched hwstatus %d\n",hwstatus);
			break;
		}

		if (retval != PFL_GRANTED &&
		    retval != PFL_GRANTED_DUPLICATE) {
			/* Clean up the NFS lock */
			debuglog("Deleting trial NFS lock\n");
			delete_nfslock(fl);
		}
		if (retval == PFL_GRANTED_DUPLICATE) {
			/* Clean up locks replaced by this lock */
			debuglog("Cleaning up replaced locks\n");
			unlock_partialfilelock(fl, 1);
		}
		break;
	case NFS_DENIED:
		retval = PFL_NFSDENIED;
		break;
	case NFS_RESERR:
		retval = PFL_NFSRESERR;
	default:
		debuglog("Unmatched lnlstatus %d\n");
		retval = PFL_NFSDENIED_NOLOCK;
		break;
	}

	/*
	 * By the time fl reaches here, it is completely free again on
	 * failure.  The NFS lock done before attempting the
	 * hardware lock has been backed out
	 */

	if (retval == PFL_NFSDENIED || retval == PFL_HWDENIED) {
		/* Once last chance to check the lock */
		if (fl->blocking == 1) {
			if (retval == PFL_NFSDENIED) {
				/* Queue the lock */
				debuglog("BLOCKING LOCK RECEIVED\n");
				retval = PFL_NFSBLOCKED;
				add_blockingfilelock(fl);
				dump_filelock(fl);
			} else {
				/* retval is okay as PFL_HWDENIED */
				debuglog("BLOCKING LOCK DENIED IN HARDWARE\n");
				dump_filelock(fl);
			}
		} else {
			/* Leave retval alone, it's already correct */
			debuglog("Lock denied.  Non-blocking failure\n");
			dump_filelock(fl);
		}
	}

	debuglog("Exiting lock_partialfilelock\n");

	return retval;
}

/*
 * unlock_partialfilelock:
 *
 * Given a file_lock, unlock all locks which match.
 *
 * Note that a given lock might have to unlock ITSELF!  See
 * clear_partialfilelock for example.
 *
 * If cleanup is set, we will NOT unlock the lock passed in.
 * We will merely clean up all other locks that the given lock
 * has replaced.
 */

enum partialfilelock_status
unlock_partialfilelock(const struct file_lock *fl, int cleanup)
{
	struct file_lock *lfl,*rfl,*releasedfl,*selffl;
	enum partialfilelock_status retval;
	enum nfslock_status unlstatus;
	enum hwlock_status unlhwstatus, lhwstatus;

	debuglog("Entering unlock_partialfilelock\n");

	selffl = NULL;
	lfl = NULL;
	rfl = NULL;
	releasedfl = NULL;
	retval = PFL_DENIED;

	/*
	 * There are significant overlap and atomicity issues
	 * with partially releasing a lock.  For example, releasing
	 * part of an NFS shared lock does *not* always release the
	 * corresponding part of the file since there is only one
	 * rpc.lockd UID but multiple users could be requesting it
	 * from NFS.  Also, an unlock request should never allow
	 * another process to gain a lock on the remaining parts.
	 * ie. Always apply the new locks before releasing the
	 * old one
	 */

	/*
	 * Loop is required since multiple little locks
	 * can be allocated and then deallocated with one
	 * big unlock.
	 *
	 * The loop is required to be here so that the nfs &
	 * hw subsystems do not need to communicate with one
	 * one another
	 */

	do {
		debuglog("Value of releasedfl: %p\n",releasedfl);
		/* lfl&rfl are created *AND* placed into the NFS lock list if required */
		unlstatus = unlock_nfslock(fl, &releasedfl, &lfl, &rfl, cleanup);
		debuglog("Value of releasedfl: %p\n",releasedfl);


		/* XXX: This is grungy.  It should be refactored to be cleaner */
		if (lfl != NULL) {
			lhwstatus = lock_hwlock(lfl);
			if (lhwstatus != HW_GRANTED &&
			    lhwstatus != HW_GRANTED_DUPLICATE) {
				debuglog("HW duplicate lock failure for left split\n");
			}
			if (lfl->flags & LOCK_MON)
				monitor_lock_host_by_name(lfl->client_name, lfl->addr);
		}

		if (rfl != NULL) {
			lhwstatus = lock_hwlock(rfl);
			if (lhwstatus != HW_GRANTED &&
			    lhwstatus != HW_GRANTED_DUPLICATE) {
				debuglog("HW duplicate lock failure for right split\n");
			}
			if (rfl->flags & LOCK_MON)
				monitor_lock_host_by_name(rfl->client_name, rfl->addr);
		}

		switch (unlstatus) {
		case NFS_GRANTED:
			/* Attempt to unlock on the hardware */
			debuglog("NFS unlock granted.  Attempting hardware unlock\n");

			/* This call *MUST NOT* unlock the two newly allocated locks */
			unlhwstatus = unlock_hwlock(fl);
			debuglog("HW unlock returned with code %d\n",unlhwstatus);
			
			switch (unlhwstatus) {
			case HW_GRANTED:
				debuglog("HW unlock granted\n");
				if (releasedfl->flags & LOCK_MON)
					unmonitor_lock_host(releasedfl->client_name);
				retval = PFL_GRANTED;
				break;
			case HW_DENIED_NOLOCK:
				/* Huh?!?!  This shouldn't happen */
				debuglog("HW unlock denied no lock\n");
				retval = PFL_HWRESERR;
				/* Break out of do-while */
				unlstatus = NFS_RESERR;
				break;
			default:
				debuglog("HW unlock failed\n");
				retval = PFL_HWRESERR;
				/* Break out of do-while */
				unlstatus = NFS_RESERR;
				break;
			}
			
			debuglog("Exiting with status retval: %d\n",retval);
			
			// XXX sending granted messages before unlock response
			// XXX causes unlock response to be corrupted?
			// XXX Workaround is to move this to nlm_prot_svc.c
			// XXX after the unlock response is sent.
			// retry_blockingfilelocklist();
			need_retry_blocked_locks = 1;
			break;
		case NFS_DENIED_NOLOCK:
			retval = PFL_GRANTED;
			debuglog("All locks cleaned out\n");
			break;
		default:
			retval = PFL_NFSRESERR;
			debuglog("NFS unlock failure\n");
			dump_filelock(fl);
			break;
		}

		if (releasedfl != NULL) {
			if (fl == releasedfl) {
				/*
				 * XXX: YECHHH!!! Attempt to unlock self succeeded
				 * but we can't deallocate the space yet.  This is what
				 * happens when you don't write malloc and free together
				 */
				if (!cleanup) {
					debuglog("Attempt to unlock self\n");
					selffl = releasedfl;
				}
			} else {
				/*
				 * XXX: this deallocation *still* needs to migrate closer
				 * to the allocation code way up in get_lock or the allocation
				 * code needs to migrate down (violation of "When you write
				 * malloc you must write free")
				 */
			
				deallocate_file_lock(releasedfl);
			}
		}

	} while (unlstatus == NFS_GRANTED);

	if (!cleanup && (selffl != NULL)) {
		/*
		 * This statement wipes out the incoming file lock (fl)
		 * in spite of the fact that it is declared const
		 */
		debuglog("WARNING!  Destroying incoming lock pointer\n");
		deallocate_file_lock(selffl);
	}

	debuglog("Exiting unlock_partialfilelock\n");

	return retval;
}

/*
 * clear_partialfilelock
 *
 * Normally called in response to statd state number change.
 * Wipe out all locks held by a host.  As a bonus, the act of
 * doing so should automatically clear their statd entries and
 * unmonitor the host.
 */

void 
clear_partialfilelock(const char *hostname)
{
	struct file_lock *ifl, *nfl;
	enum partialfilelock_status pfsret;
	struct host *ihp;

	/*
	 * Check if the name we got from statd is
	 * actually one reverse-mapped from the client's
	 * address.  If so, use the name provided as
	 * the caller_name in lock requests instead so that
	 * we can correctly identify the client's locks.
	 */
	TAILQ_FOREACH(ihp, &hostlst_head, hostlst) {
		if (ihp->revname && strncmp(hostname, ihp->revname,
			SM_MAXSTRLEN) == 0) {
			hostname = ihp->name;
			debuglog("Clearing locks for %s (%s)\n",
				hostname, ihp->revname);
			break;
		}
	}

	/* Clear blocking file lock list */
	clear_blockingfilelock(hostname);

	/* do all required unlocks */
	/* Note that unlock can smash the current pointer to a lock */

	/*
	 * Normally, LIST_FOREACH is called for, but since
	 * the current element *is* the iterator, deleting it
	 * would mess up the iteration.  Thus, a next element
	 * must be used explicitly
	 */
restart:
	ifl = LIST_FIRST(&nfslocklist_head);

	while (ifl != NULL) {
		nfl = LIST_NEXT(ifl, nfslocklist);

		if (strncmp(hostname, ifl->client_name, SM_MAXSTRLEN) == 0) {
			/* Unlock destroys ifl out from underneath */
			pfsret = unlock_partialfilelock(ifl, 0);
			if (pfsret != PFL_GRANTED) {
				/* Uh oh... there was some sort of problem. */
				/* If we restart the loop, we may get */
				/* stuck here forever getting errors. */
				/* So, let's just abort the whole scan. */
				syslog(LOG_WARNING, "lock clearing for %s failed: %d",
					hostname, pfsret);
				break;
			}
			/* ifl is NO LONGER VALID AT THIS POINT */
			/* Note: the unlock may deallocate several existing locks. */
			/* Therefore, we need to restart the scanning of the list, */
			/* because nfl could be pointing to a freed lock. */
			goto restart;
		}
		ifl = nfl;
	}
}

/*
 * test_partialfilelock:
 */
enum partialfilelock_status
test_partialfilelock(const struct file_lock *fl,
    struct file_lock **conflicting_fl)
{
	enum partialfilelock_status retval;
	enum nfslock_status teststatus;

	debuglog("Entering testpartialfilelock...\n");

	retval = PFL_DENIED;

	teststatus = test_nfslock(fl, conflicting_fl);
	debuglog("test_partialfilelock: teststatus %d\n",teststatus);

	if (teststatus == NFS_GRANTED || teststatus == NFS_GRANTED_DUPLICATE) {
		/* XXX: Add the underlying filesystem locking code */
		retval = (teststatus == NFS_GRANTED) ?
		    PFL_GRANTED : PFL_GRANTED_DUPLICATE;
		debuglog("Dumping locks...\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		debuglog("Done dumping locks...\n");
	} else {
		retval = PFL_NFSDENIED;
		debuglog("NFS test denied.\n");
		dump_filelock(fl);
		debuglog("Conflicting.\n");
		dump_filelock(*conflicting_fl);
	}

	debuglog("Exiting testpartialfilelock...\n");

	return retval;
}

/*
 * Below here are routines associated with translating the partial file locking
 * codes into useful codes to send back to the NFS RPC messaging system
 */

/*
 * These routines translate the (relatively) useful return codes back onto
 * the few return codes which the nlm subsystems wishes to trasmit
 */

enum nlm4_stats
do_test(struct file_lock *fl, struct file_lock **conflicting_fl)
{
	enum partialfilelock_status pfsret;
	enum nlm4_stats retval;

	debuglog("Entering do_test...\n");

	pfsret = test_partialfilelock(fl,conflicting_fl);

	switch (pfsret) {
	case PFL_GRANTED:
		debuglog("PFL test lock granted\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_GRANTED_DUPLICATE:
		debuglog("PFL test lock granted--duplicate id detected\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		debuglog("Clearing conflicting_fl for call semantics\n");
		*conflicting_fl = NULL;
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_NFSDENIED:
	case PFL_HWDENIED:
		debuglog("PFL test lock denied\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
		break;
	case PFL_NFSRESERR:
	case PFL_HWRESERR:
		debuglog("PFL test lock resource fail\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
		break;
	default:
		debuglog("PFL test lock *FAILED*\n");
		dump_filelock(fl);
		dump_filelock(*conflicting_fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
		break;
	}

	debuglog("Exiting do_test...\n");

	return retval;
}

/*
 * do_lock: Try to acquire a lock
 *
 * This routine makes a distinction between NLM versions.  I am pretty
 * convinced that this should be abstracted out and bounced up a level
 */

enum nlm4_stats
do_lock(struct file_lock *fl)
{
	enum partialfilelock_status pfsret;
	enum nlm4_stats retval;

	debuglog("Entering do_lock...\n");

	pfsret = lock_partialfilelock(fl);

	switch (pfsret) {
	case PFL_GRANTED:
		debuglog("PFL lock granted");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_GRANTED_DUPLICATE:
		debuglog("PFL lock granted--duplicate id detected");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_NFSDENIED:
	case PFL_HWDENIED:
		debuglog("PFL_NFS lock denied");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
		break;
	case PFL_NFSBLOCKED:
	case PFL_HWBLOCKED:
		debuglog("PFL_NFS blocking lock denied.  Queued.\n");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_blocked : nlm_blocked;
		break;
	case PFL_NFSRESERR:
	case PFL_HWRESERR:
	case PFL_NFSDENIED_NOLOCK:
	case PFL_HWDENIED_NOLOCK:
		debuglog("PFL lock resource alocation fail\n");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
		break;
	case PFL_HWDENIED_STALEFH:
		debuglog("PFL_NFS lock denied STALEFH");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_stale_fh : nlm_denied;
		break;
	case PFL_HWDENIED_READONLY:
		debuglog("PFL_NFS lock denied READONLY");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_rofs : nlm_denied;
		break;
	default:
		debuglog("PFL lock *FAILED*");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
		break;
	}

	debuglog("Exiting do_lock...\n");

	return retval;
}

enum nlm4_stats
do_unlock(struct file_lock *fl)
{
	enum partialfilelock_status pfsret;
	enum nlm4_stats retval;

	debuglog("Entering do_unlock...\n");
	pfsret = unlock_partialfilelock(fl, 0);

	switch (pfsret) {
	case PFL_GRANTED:
		debuglog("PFL unlock granted");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_NFSDENIED:
	case PFL_HWDENIED:
		debuglog("PFL_NFS unlock denied");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied : nlm_denied;
		break;
	case PFL_NFSDENIED_NOLOCK:
	case PFL_HWDENIED_NOLOCK:
		debuglog("PFL_NFS no lock found\n");
		retval = (fl->flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	case PFL_NFSRESERR:
	case PFL_HWRESERR:
		debuglog("PFL unlock resource failure");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
		break;
	default:
		debuglog("PFL unlock *FAILED*");
		dump_filelock(fl);
		retval = (fl->flags & LOCK_V4) ? nlm4_failed : nlm_denied;
		break;
	}

	debuglog("Exiting do_unlock...\n");

	return retval;
}

/*
 * do_clear
 *
 * This routine is non-existent because it doesn't have a return code.
 * It is here for completeness in case someone *does* need to do return
 * codes later.  A decent compiler should optimize this away.
 */

void
do_clear(const char *hostname)
{

	clear_partialfilelock(hostname);
}

/*
 * The following routines are all called from the code which the
 * RPC layer invokes
 */

/*
 * testlock(): inform the caller if the requested lock would be granted
 *
 * returns NULL if lock would granted
 * returns pointer to a conflicting nlm4_holder if not
 */

struct nlm4_holder *
testlock(struct nlm4_lock *lock, bool_t exclusive, int flags __unused)
{
	struct file_lock test_fl, *conflicting_fl;

	if (lock->fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    lock->fh.n_len, NFSV3_MAX_FH_SIZE);
		return NULL;
	}

	bzero(&test_fl, sizeof(test_fl));

	test_fl.filehandle.n_len = lock->fh.n_len;
	test_fl.filehandle.n_bytes = lock->fh.n_bytes;
	copy_nlm4_lock_to_nlm4_holder(lock, exclusive, &test_fl.client);

	siglock();
	do_test(&test_fl, &conflicting_fl);
	
	if (conflicting_fl == NULL) {
		debuglog("No conflicting lock found\n");
		sigunlock();
		return NULL;
	} else {
		debuglog("Found conflicting lock\n");
		dump_filelock(conflicting_fl);
		sigunlock();
		return (&conflicting_fl->client);
	}
}

/*
 * getlock: try to aquire the lock. 
 * If file is already locked and we can sleep, put the lock in the list with
 * status LKST_WAITING; it'll be processed later.
 * Otherwise try to lock. If we're allowed to block, fork a child which
 * will do the blocking lock.
 */

enum nlm4_stats
getlock(nlm4_lockargs *lckarg, struct svc_req *rqstp, const int flags)
{
	struct file_lock *newfl;
	enum nlm4_stats retval;

	debuglog("Entering getlock...\n");

	if (grace_expired == 0 && lckarg->reclaim == 0)
		return (flags & LOCK_V4) ?
		    nlm4_denied_grace_period : nlm_denied_grace_period;

	if (lckarg->alock.fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    lckarg->alock.fh.n_len, NFSV3_MAX_FH_SIZE);
		return (flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	/* allocate new file_lock for this request */
	newfl = allocate_file_lock(&lckarg->alock.oh, &lckarg->alock.fh,
				   (struct sockaddr *)svc_getcaller(rqstp->rq_xprt),
				   lckarg->alock.caller_name);
	if (newfl == NULL) {
		syslog(LOG_NOTICE, "lock allocate failed: %s", strerror(errno));
		/* failed */
		return (flags & LOCK_V4) ?
		    nlm4_denied_nolocks : nlm_denied_nolocks;
	}

	fill_file_lock(newfl,
	    lckarg->exclusive, lckarg->alock.svid, lckarg->alock.l_offset,
	    lckarg->alock.l_len,
	    lckarg->state, 0, flags, lckarg->block);
	
	/*
	 * newfl is now fully constructed and deallocate_file_lock
	 * can now be used to delete it
	 */
	
	siglock();
	debuglog("Pointer to new lock is %p\n",newfl);

	retval = do_lock(newfl);

	debuglog("Pointer to new lock is %p\n",newfl);
	sigunlock();
	
	switch (retval)
		{
		case nlm4_granted:
			/* case nlm_granted: is the same as nlm4_granted */
			/* do_mon(lckarg->alock.caller_name); */
			break;
		case nlm4_blocked:
			/* case nlm_blocked: is the same as nlm4_blocked */
			/* do_mon(lckarg->alock.caller_name); */
			break;
		default:
			deallocate_file_lock(newfl);
			break;
		}

	debuglog("Exiting getlock...\n");

	return retval;
}


/* unlock a filehandle */
enum nlm4_stats
unlock(nlm4_lock *lock, const int flags)
{
	struct file_lock fl;
	enum nlm4_stats err;
	
	debuglog("Entering unlock...\n");

	if (lock->fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    lock->fh.n_len, NFSV3_MAX_FH_SIZE);
		return (flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}
	
	siglock();
	
	bzero(&fl,sizeof(struct file_lock));
	fl.filehandle.n_len = lock->fh.n_len;
	fl.filehandle.n_bytes = lock->fh.n_bytes;
	
	copy_nlm4_lock_to_nlm4_holder(lock, 0, &fl.client);
	
	err = do_unlock(&fl);
	
	sigunlock();
	
	debuglog("Exiting unlock...\n");
	
	return err;
}

/* cancel a blocked lock request */
enum nlm4_stats
cancellock(nlm4_cancargs *args, const int flags)
{
	struct file_lock *ifl, *nfl;
	enum nlm4_stats err;

	debuglog("Entering cancellock...\n");

	if (args->alock.fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    args->alock.fh.n_len, NFSV3_MAX_FH_SIZE);
		return (flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	siglock();

	err = (flags & LOCK_V4) ? nlm4_denied : nlm_denied;

	/*
	 * scan blocked lock list for matching request and remove/destroy
	 */
	ifl = LIST_FIRST(&blockedlocklist_head);
	for ( ; ifl != NULL; ifl = nfl) {
		nfl = LIST_NEXT(ifl, nfslocklist);

		/* compare lock fh - filehandle */
		if (!same_netobj(&args->alock.fh, &ifl->filehandle))
			continue;

		/* compare lock caller_name - client_name */
		if (strncmp(args->alock.caller_name, ifl->client_name, SM_MAXSTRLEN))
			continue;

		/* Note: done't compare cookie - client_cookie */
		/* The cookie may be specific to the cancel request */
		/* and not be the same as the one in the original lock request. */

		/* compare lock oh - client.oh */
		if (!same_netobj(&args->alock.oh, &ifl->client.oh))
			continue;

		/* compare lock svid - client.svid */
		if (args->alock.svid != ifl->client.svid)
			continue;

		/* compare lock l_offset - client.l_offset */
		if (args->alock.l_offset != ifl->client.l_offset)
			continue;

		/* compare lock l_len - client.l_len */
		if (args->alock.l_len != ifl->client.l_len)
			continue;

		/* compare exclusive - client.exclusive */
		if (args->exclusive != ifl->client.exclusive)
			continue;

		/* got it */
		remove_blockingfilelock(ifl);
		deallocate_file_lock(ifl);
		err = (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		break;
	}

	sigunlock();

	debuglog("Exiting cancellock...\n");

	return err;
}


/*
 * XXX: The following monitor/unmonitor routines 
 * have not been extensively tested (ie. no regression
 * script exists like for the locking sections)
 */

/*
 * Find a lock host on a queue.  If found:
 *    bump the ref,
 *    bump the access time,
 *    dequeue it from the queue it was found on,
 *    enqueue it at the front of the "in use" queue.
 */
static struct host *
get_lock_host(struct hostlst_head *hd, const char *hostname, const struct sockaddr *saddr)
{
	struct host *ihp;

	if (!hostname && !saddr)
		return (NULL);

	debuglog("get_lock_host %s\n", hostname ? hostname : "addr");
	TAILQ_FOREACH(ihp, hd, hostlst) {
		if (hostname && (strncmp(hostname, ihp->name, SM_MAXSTRLEN) != 0))
			continue;
		if (saddr && addrcmp(saddr, &ihp->addr))
			continue;
		TAILQ_REMOVE(hd, ihp, hostlst);
		/*
		 * Host is already monitored, so just bump the
		 * reference count.  But don't bump the reference
		 * count if we're adding additional client-side
		 * references.  Client-side monitors are done by
		 * address, are never unmonitored, and should only
		 * take one refcount.  Otherwise, repeated calls
		 * could cause the refcount to wrap.
		 */
		if (!saddr || !ihp->addr.sa_len)
			++ihp->refcnt;
		ihp->lastuse = currsec;
		/* Host should only be in the monitor list once */
		TAILQ_INSERT_HEAD(&hostlst_head, ihp, hostlst);
		break;
	}
	debuglog("get_lock_host %s %s\n",
	    ihp == NULL ? "did not find" : "found", hostname ? hostname : "addr");
	return (ihp);
}

/*
 * monitor_lock_host: monitor lock hosts locally with a ref count and
 * inform statd
 */
void
monitor_lock_host_by_name(const char *hostname, const struct sockaddr *saddr)
{
	struct host *ihp;

	debuglog("monitor_lock_host: %s\n", hostname);
	ihp = get_lock_host(&hostlst_head, hostname, NULL);
	if (ihp == NULL)
		ihp = get_lock_host(&hostlst_unref, hostname, NULL);
	if (ihp != NULL) {
		if (ihp->revname)
			debuglog("Monitor_lock_host: %s - %s (cached)\n",
				ihp->name, ihp->revname);
		else
			debuglog("Monitor_lock_host: %s (cached)\n", ihp->name);
		return;
	}

	monitor_lock_host(hostname, saddr);
}

void
monitor_lock_host_by_addr(const struct sockaddr *saddr)
{
	struct host *ihp;
	struct hostent *hp;
	char hostaddr[SM_MAXSTRLEN];
	struct sockaddr_in *sin = (struct sockaddr_in *)saddr;

	if (getnameinfo(saddr, saddr->sa_len, hostaddr, sizeof(hostaddr),
			NULL, 0, NI_NUMERICHOST)) {
		debuglog("monitor_lock_host: bad address\n");
		return;
	}
	debuglog("monitor_lock_host: %s\n", hostaddr);
	ihp = get_lock_host(&hostlst_head, NULL, saddr);
	if (ihp == NULL)
		ihp = get_lock_host(&hostlst_unref, NULL, saddr);
	if (ihp != NULL) {
		debuglog("Monitor_lock_host: %s (cached)\n", ihp->name);
		return;
	}

	hp = gethostbyaddr((char*)&sin->sin_addr, sizeof(sin->sin_addr), AF_INET);
	if (hp) {
		monitor_lock_host(hp->h_name, saddr);
	} else {
		// herror(hostaddr);
		monitor_lock_host(hostaddr, saddr);
	}
}

static void
monitor_lock_host(const char *hostname, const struct sockaddr *saddr)
{
	struct host *nhp;
	struct hostent *hp = NULL;
	struct mon smon;
	struct sm_stat_res sres;
	int rpcret;
	int retrying = 0;
	size_t n;
	struct sockaddr_in *sin = (struct sockaddr_in *) saddr;

	rpcret = 0;

	/* Host is not yet monitored, add it */
	debuglog("Monitor_lock_host: %s (creating)\n", hostname);
	n = strnlen(hostname, SM_MAXSTRLEN);
	if (n == SM_MAXSTRLEN) {
		debuglog("monitor_lock_host: hostname too long\n");
		return;
	}
	nhp = (struct host *) malloc(sizeof(struct host));
	if (nhp == NULL) {
		debuglog("Unable to allocate entry for statd mon\n");
		return;
	}

	/* Allocated new host entry, now fill the fields */
	nhp->name = strdup(hostname);
	if (nhp->name == NULL) {
		debuglog("Unable to allocate entry name for statd mon\n");
		free(nhp);
		return;
	}
	nhp->revname = NULL;
	nhp->refcnt = 1;
	nhp->lastuse = currsec;
	if (saddr) {
		bcopy(saddr, &nhp->addr, saddr->sa_len);
	} else {
		nhp->addr.sa_len = 0;
	}
	debuglog("Locally Monitoring host '%s'\n", hostname);

	debuglog("Attempting to tell statd\n");

	bzero(&smon,sizeof(smon));

	smon.mon_id.mon_name = nhp->name;
	smon.mon_id.my_id.my_name = "localhost\0";

	smon.mon_id.my_id.my_prog = NLM_PROG;
	smon.mon_id.my_id.my_vers = NLM_SM;
	smon.mon_id.my_id.my_proc = NLM_SM_NOTIFY;

retry:
	rpcret = callrpc("localhost", SM_PROG, SM_VERS, SM_MON, (xdrproc_t)xdr_mon,
	    &smon, (xdrproc_t)xdr_sm_stat_res, &sres);

	if (rpcret == 0) {
		if (sres.res_stat == stat_fail && !retrying) {
			debuglog("Statd call failed\n");
			/*
			 * It's possible that the hostname provided
			 * by the client isn't valid. Retry with a
			 * a hostname reverse-mapped from the client's
			 * address (if provided) and stash this name
			 * in the host entry so that we can identify it
			 * with this name later when we ask statd to
			 * unmonitor it.
			 */
			if (saddr)
				hp = gethostbyaddr((char *) &sin->sin_addr,
					sizeof(sin->sin_addr), AF_INET);
			if (hp != NULL && strcmp(nhp->name, hp->h_name) != 0) {
				debuglog("Statd retry with '%s'\n", hp->h_name);
				smon.mon_id.mon_name = hp->h_name;
				nhp->revname = strdup(hp->h_name);
				if (nhp->revname == NULL) {
					debuglog("No memory for revname\n");
					free(nhp->name);
					free(nhp);
					return;
				}
				retrying = 1;
				goto retry;
			}
		}
	} else {
		debuglog("Rpc call to statd failed with return value: %d\n",
		    rpcret);
	}

	/*
	 * Insert host in the monitor list, even if statd
	 * doesn't like it.  If the name is unacceptable
	 * to statd, at least we'll avoid subsequent rejection
	 * on every lock request.
	 */
	TAILQ_INSERT_HEAD(&hostlst_head, nhp, hostlst);
}

/*
 * unmonitor_lock_host: clear monitor ref counts and inform statd when gone
 */
void
unmonitor_lock_host(const char *hostname)
{
	struct host *ihp;

	TAILQ_FOREACH(ihp, &hostlst_head, hostlst) {
		if (strncmp(hostname, ihp->name, SM_MAXSTRLEN) == 0) {
			/* Host is unmonitored, drop refcount */
			--ihp->refcnt;
			/* Host should only be in the monitor list once */
			break;
		}
	}

	if (ihp == NULL) {
		debuglog("Could not find host %16s in mon list\n", hostname);
		return;
	}

	if (ihp->refcnt > 0)
		return;

	if (ihp->refcnt < 0) {
		debuglog("Negative refcount!: %d\n", ihp->refcnt);
	}

	TAILQ_REMOVE(&hostlst_head, ihp, hostlst);
	TAILQ_INSERT_HEAD(&hostlst_unref, ihp, hostlst);
	if (config.host_monitor_cache_timeout <= 0)
		destroy_lock_host(ihp);
}

void
destroy_lock_host(struct host *ihp)
{
	struct mon_id smon_id;
	struct sm_stat smstat;
	int rpcret;
	char *name;

	/*
	 * If the client was monitored with a hostname obtained from
	 * its address, then use that instead of the possibly invalid
	 * hostname provided in caller_name.
	 */
	name = ihp->revname ? ihp->revname : ihp->name;
	debuglog("Attempting to unmonitor host %16s\n", name);

	bzero(&smon_id,sizeof(smon_id));
	smon_id.mon_name = name;
	smon_id.my_id.my_name = "localhost";
	smon_id.my_id.my_prog = NLM_PROG;
	smon_id.my_id.my_vers = NLM_SM;
	smon_id.my_id.my_proc = NLM_SM_NOTIFY;
			  
	rpcret = callrpc("localhost", SM_PROG, SM_VERS, SM_UNMON, (xdrproc_t)xdr_mon_id,
	    &smon_id, (xdrproc_t)xdr_sm_stat, &smstat);
			  
	if (rpcret != 0) {
		debuglog("Rpc call to unmonitor statd failed with "
		   " return value: %d: %s", rpcret, clnt_sperrno(rpcret));
	} else {
		debuglog("Succeeded unmonitoring %16s\n", ihp->name);
	}

	TAILQ_REMOVE(&hostlst_unref, ihp, hostlst);
	if (ihp->name)
		free(ihp->name);
	if (ihp->revname)
		free(ihp->revname);
	free(ihp);
}

/*
 * returns 1 if there are hosts to expire or 0 if there are none.
 */
int
expire_lock_hosts(void)
{
	struct host *ihp;

	for ( ;; ) {
		ihp = TAILQ_LAST(&hostlst_unref, hostlst_head);
		if (ihp == NULL)
			break;
		if ((config.host_monitor_cache_timeout > 0) &&
		    (ihp->lastuse >= (currsec - config.host_monitor_cache_timeout)))
			break;
		debuglog("expire_lock_hosts: expiring %s %d %d %d\n",
		    ihp->name, (int)ihp->lastuse,
		    (int)currsec, (int)currsec - config.host_monitor_cache_timeout);
		destroy_lock_host(ihp);
	}
	return (TAILQ_LAST(&hostlst_unref, hostlst_head) != NULL);
}

/*
 * notify: Clear all locks from a host if statd complains
 *
 * XXX: This routine has not been thoroughly tested.  However, neither
 * had the old one been.  It used to compare the statd crash state counter
 * to the current lock state.  The upshot of this was that it basically
 * cleared all locks from the specified host 99% of the time (with the
 * other 1% being a bug).  Consequently, the assumption is that clearing
 * all locks from a host when notified by statd is acceptable.
 *
 * Please note that this routine skips the usual level of redirection
 * through a do_* type routine.  This introduces a possible level of
 * error and might better be written as do_notify and take this one out.

 */

void
notify(const char *hostname, const int state)
{
	debuglog("notify from %s, new state %d", hostname, state);
	
	siglock();
	do_clear(hostname);
	sigunlock();

	debuglog("Leaving notify\n");
}

int
send_granted(fl, opcode)
	struct file_lock *fl;
	int opcode __unused;
{
	CLIENT *cli;
	static char dummy;
	struct timeval timeo;
	enum clnt_stat rv;
	static struct nlm_res retval;
	static struct nlm4_res retval4;

	debuglog("About to send granted on blocked lock\n");

	cli = get_client(fl->addr, (fl->flags & LOCK_V4) ? NLM_VERS4 : NLM_VERS, 0);
	if (cli == NULL) {
		syslog(LOG_NOTICE, "failed to get CLIENT for %s",
		    fl->client_name);
		/*
		 * We fail to notify remote that the lock has been granted.
		 * The client will timeout and retry, the lock will be
		 * granted at this time.
		 */
		return -1;
	}
	timeo.tv_sec = 0;
	timeo.tv_usec = (fl->flags & LOCK_ASYNC) ? 0 : 500000; /* 0.5s */

	fl->granted_cookie = ++send_granted_cookie;
	if (!send_granted_cookie)
		send_granted_cookie++;

	if (fl->flags & LOCK_V4) {
		static nlm4_testargs res;
		res.cookie.n_len = sizeof(fl->granted_cookie);
		res.cookie.n_bytes = (char*)&fl->granted_cookie;
		res.exclusive = fl->client.exclusive;
		res.alock.caller_name = fl->client_name;
		res.alock.fh.n_len = fl->filehandle.n_len;
		res.alock.fh.n_bytes = fl->filehandle.n_bytes;
		res.alock.oh = fl->client.oh;
		res.alock.svid = fl->client.svid;
		res.alock.l_offset = fl->client.l_offset;
		res.alock.l_len = fl->client.l_len;
		debuglog("sending v4 reply%s",
			 (fl->flags & LOCK_ASYNC) ? " (async)":"");
		if (fl->flags & LOCK_ASYNC) {
			rv = clnt_call(cli, NLM4_GRANTED_MSG,
			    (xdrproc_t)xdr_nlm4_testargs, &res, (xdrproc_t)xdr_void, &dummy, timeo);
		} else {
			rv = clnt_call(cli, NLM4_GRANTED,
			    (xdrproc_t)xdr_nlm4_testargs, &res, (xdrproc_t)xdr_nlm4_res,
			    &retval4, timeo);
		}
	} else {
		static nlm_testargs res;

		res.cookie.n_len = sizeof(fl->granted_cookie);
		res.cookie.n_bytes = (char*)&fl->granted_cookie;
		res.exclusive = fl->client.exclusive;
		res.alock.caller_name = fl->client_name;
		res.alock.fh.n_len = fl->filehandle.n_len;
		res.alock.fh.n_bytes = fl->filehandle.n_bytes;
		res.alock.oh = fl->client.oh;
		res.alock.svid = fl->client.svid;
		res.alock.l_offset = fl->client.l_offset;
		res.alock.l_len = fl->client.l_len;
		debuglog("sending v1 reply%s",
			 (fl->flags & LOCK_ASYNC) ? " (async)":"");
		if (fl->flags & LOCK_ASYNC) {
			rv = clnt_call(cli, NLM_GRANTED_MSG,
			    (xdrproc_t)xdr_nlm_testargs, &res, (xdrproc_t)xdr_void, &dummy, timeo);
		} else {
			rv = clnt_call(cli, NLM_GRANTED,
			    (xdrproc_t)xdr_nlm_testargs, &res, (xdrproc_t)xdr_nlm_res,
			    &retval, timeo);
		}
	}
	if (config.verbose > 2)
		debuglog("clnt_call returns %d(%s) for granted",
			 rv, clnt_sperrno(rv));

	if ((rv != RPC_SUCCESS) &&
	    !((fl->flags & LOCK_ASYNC) && (rv == RPC_TIMEDOUT)))
		return -1;
	return 0;
}

/*
 * granted_failed: remove a granted lock that wasn't successfully
 * accepted by the client
 */
void
granted_failed(nlm4_res *arg)
{
	u_int64_t cookie;
	struct file_lock *ifl;

	debuglog("Entering granted_failed, status %d\n", arg->stat.stat);

	if (arg->cookie.n_len != sizeof(cookie)) {
		debuglog("Exiting granted_failed: bogus cookie size %d\n",
			arg->cookie.n_len);
		return;
	}
	bcopy(arg->cookie.n_bytes, &cookie, sizeof(cookie));
	debuglog("granted_failed, cookie 0x%llx\n", cookie);

	LIST_FOREACH(ifl, &nfslocklist_head, nfslocklist) {
		debuglog("Pointer to file lock: %p\n",ifl);

		debuglog("****Dump of ifl****\n");
		dump_filelock(ifl);

		if (ifl->granted_cookie != cookie)
			continue;

		debuglog("granted_failed: cookie found\n");
		break;
	}

	if (ifl) {
		do_unlock(ifl);
		/* ifl is NO LONGER VALID AT THIS POINT */
	} else {
		debuglog("granted_failed: cookie NOT FOUND\n");
	}

	debuglog("Exiting granted_failed\n");
}

/*
 * getshare: try to acquire a share reservation
 */
enum nlm4_stats
getshare(nlm_shareargs *shrarg, struct svc_req *rqstp __unused, const int flags)
{
	struct sharefile *shrfile;
	struct file_share *sh;
	size_t n;

	debuglog("Entering getshare...\n");

	if (grace_expired == 0 && shrarg->reclaim == 0) {
		debuglog("getshare denied - grace period\n");
		return (flags & LOCK_V4) ?
			nlm4_denied_grace_period :
			nlm_denied_grace_period;
	}

	if (shrarg->share.fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    shrarg->share.fh.n_len, NFSV3_MAX_FH_SIZE);
		return (flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	/* find file in list of share files */
	LIST_FOREACH(shrfile, &nfssharefilelist_head, sharefilelist) {
		if ((shrarg->share.fh.n_len == shrfile->filehandle.n_len) &&
		    (bcmp(shrarg->share.fh.n_bytes, shrfile->filehandle.n_bytes,
			shrarg->share.fh.n_len) == 0)) {
			/* shrfile is the correct file */
			break;
		}
	}

	/* if share file not found, create a new share file */
	if (!shrfile) {
		fhandle_t fh;
		int fd;
		fh.fh_len = shrarg->share.fh.n_len;
		bcopy(shrarg->share.fh.n_bytes, fh.fh_data, fh.fh_len);
		fd = fhopen(&fh, O_RDONLY);
		if (fd < 0) {
			debuglog("fhopen failed (from %16s): %32s\n",
			    shrarg->share.caller_name, strerror(errno));
			if ((flags & LOCK_V4) == 0)
				return nlm_denied;
			switch (errno) {
			case ESTALE:
				return nlm4_stale_fh;
			default:
				return nlm4_failed;
			}
		}
		shrfile = malloc(sizeof(struct sharefile));
		if (!shrfile) {
			debuglog("getshare failed: can't allocate sharefile\n");
			close(fd);
			return (flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
		}
		shrfile->filehandle.n_len = shrarg->share.fh.n_len;
		shrfile->filehandle.n_bytes = malloc(shrarg->share.fh.n_len);
		if (!shrfile->filehandle.n_bytes) {
			debuglog("getshare failed: can't allocate sharefile filehandle\n");
			free(shrfile);
			close(fd);
			return (flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
		}
		bcopy(shrarg->share.fh.n_bytes, shrfile->filehandle.n_bytes,
			shrarg->share.fh.n_len);
		shrfile->fd = fd;
		shrfile->refcount = 0;
		shrfile->sharelist_head.lh_first = NULL;
		LIST_INSERT_HEAD(&nfssharefilelist_head, shrfile, sharefilelist);
	}

	/* compare request mode/access to current shares */
	LIST_FOREACH(sh, &shrfile->sharelist_head, nfssharelist) {
		/* if request host/owner matches a current share... */
		if ((strncmp(shrarg->share.caller_name, sh->client_name, SM_MAXSTRLEN) == 0) &&
			same_netobj(&shrarg->share.oh, &sh->oh)) {
			/* ...then just update share mode/access */
			sh->mode = shrarg->share.mode;
			sh->access = shrarg->share.access;
			debuglog("getshare: updated existing share\n");
			return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
		}
		if (((shrarg->share.mode & sh->access) != 0) ||
		    ((shrarg->share.access & sh->mode) != 0)) {
			/* share request conflicts with existing share */
			debuglog("getshare: conflicts with existing share\n");
			return (flags & LOCK_V4) ? nlm4_denied : nlm_denied;
		}
	}

	/* create/init new share */
	n = strnlen(shrarg->share.caller_name, SM_MAXSTRLEN);
	if (n < SM_MAXSTRLEN) {
		sh = malloc(sizeof(*sh) - sizeof(sh->client_name) + n + 1);
	} else {
		debuglog("getshare failed: hostname too long\n");
		sh = NULL;
	}
	if (!sh) {
		debuglog("getshare failed: can't allocate share\n");
		if (!shrfile->refcount) {
			LIST_REMOVE(shrfile, sharefilelist);
			close(shrfile->fd);
			free(shrfile->filehandle.n_bytes);
			free(shrfile);
		}
		return (flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
	}
	bzero(sh, sizeof(*sh) - sizeof(sh->client_name));
	sh->oh.n_len = shrarg->share.oh.n_len;
	sh->oh.n_bytes = malloc(sh->oh.n_len);
	if (!sh->oh.n_bytes) {
		debuglog("getshare failed: can't allocate share owner handle\n");
		free(sh);
		if (!shrfile->refcount) {
			LIST_REMOVE(shrfile, sharefilelist);
			close(shrfile->fd);
			free(shrfile->filehandle.n_bytes);
			free(shrfile);
		}
		return (flags & LOCK_V4) ? nlm4_denied_nolocks : nlm_denied_nolocks;
	}
	memcpy(sh->client_name, shrarg->share.caller_name, n);
	sh->client_name[n] = 0;
	sh->mode = shrarg->share.mode;
	sh->access = shrarg->share.access;

	/* insert new share into file's share list */
	LIST_INSERT_HEAD(&shrfile->sharelist_head, sh, nfssharelist);
	shrfile->refcount++;

	debuglog("Exiting getshare...\n");

	return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
}


/* remove a share reservation */
enum nlm4_stats
unshare(nlm_shareargs *shrarg, struct svc_req *rqstp __unused, const int flags)
{
	struct sharefile *shrfile;
	struct file_share *sh;

	debuglog("Entering unshare...\n");

	if (shrarg->share.fh.n_len > NFSV3_MAX_FH_SIZE) {
		debuglog("received fhandle size %d, max size %d",
		    shrarg->share.fh.n_len, NFSV3_MAX_FH_SIZE);
		return (flags & LOCK_V4) ? nlm4_failed : nlm_denied;
	}

	/* find file in list of share files */
	LIST_FOREACH(shrfile, &nfssharefilelist_head, sharefilelist) {
		if ((shrarg->share.fh.n_len == shrfile->filehandle.n_len) &&
		    (bcmp(shrarg->share.fh.n_bytes, shrfile->filehandle.n_bytes,
			shrarg->share.fh.n_len) == 0)) {
			/* shrfile is the correct file */
			break;
		}
	}

	/* if share file not found, return success (per spec) */
	if (!shrfile) {
		debuglog("unshare: no such share file\n");
		return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	}

	/* find share */
	LIST_FOREACH(sh, &shrfile->sharelist_head, nfssharelist) {
		/* if request host/owner matches a current share... */
		if ((strncmp(shrarg->share.caller_name, sh->client_name, SM_MAXSTRLEN) == 0) &&
			same_netobj(&shrarg->share.oh, &sh->oh))
			break;
	}

	/* if share not found, return success (per spec) */
	if (!sh) {
		debuglog("unshare: no such share\n");
		return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
	}

	/* remove share from file and deallocate */
	shrfile->refcount--;
	LIST_REMOVE(sh, nfssharelist);
	free(sh->oh.n_bytes);
	free(sh);

	/* if file has no more shares, deallocate share file */
	if (!shrfile->refcount) {
		debuglog("unshare: file has no more shares\n");
		LIST_REMOVE(shrfile, sharefilelist);
		close(shrfile->fd);
		free(shrfile->filehandle.n_bytes);
		free(shrfile);
	}

	debuglog("Exiting unshare...\n");

	return (flags & LOCK_V4) ? nlm4_granted : nlm_granted;
}

/*
 * do_free_all
 *
 * Wipe out all non-monitored locks and shares held by a host.
 */

void 
do_free_all(const char *hostname)
{
	struct file_lock *ifl, *nfl;
	struct sharefile *shrfile, *nshrfile;
	struct file_share *ifs, *nfs;
	enum partialfilelock_status pfsret;

	/* clear non-monitored blocking file locks */
	ifl = LIST_FIRST(&blockedlocklist_head);
	while (ifl != NULL) {
		nfl = LIST_NEXT(ifl, nfslocklist);

		if (((ifl->flags & LOCK_MON) == 0) &&
		    (strncmp(hostname, ifl->client_name, SM_MAXSTRLEN) == 0)) {
			remove_blockingfilelock(ifl);
			deallocate_file_lock(ifl);
		}

		ifl = nfl;
	}

	/* clear non-monitored file locks */
restart:
	ifl = LIST_FIRST(&nfslocklist_head);
	while (ifl != NULL) {
		nfl = LIST_NEXT(ifl, nfslocklist);

		if (((ifl->flags & LOCK_MON) == 0) &&
		    (strncmp(hostname, ifl->client_name, SM_MAXSTRLEN) == 0)) {
			/* Unlock destroys ifl out from underneath */
			pfsret = unlock_partialfilelock(ifl, 0);
			if (pfsret != PFL_GRANTED) {
				/* Uh oh... there was some sort of problem. */
				/* If we restart the loop, we may get */
				/* stuck here forever getting errors. */
				/* So, let's just abort the whole scan. */
				syslog(LOG_WARNING, "unmonitored lock clearing for %s failed: %d",
					hostname, pfsret);
				break;
			}
			/* ifl is NO LONGER VALID AT THIS POINT */
			/* Note: the unlock may deallocate several existing locks. */
			/* Therefore, we need to restart the scanning of the list, */
			/* because nfl could be pointing to a freed lock. */
			goto restart;
		}

		ifl = nfl;
	}

	/* clear shares */
	shrfile = LIST_FIRST(&nfssharefilelist_head);
	while (shrfile != NULL) {
		nshrfile = LIST_NEXT(shrfile, sharefilelist);

		ifs = LIST_FIRST(&shrfile->sharelist_head);
		while (ifs != NULL) {
			nfs = LIST_NEXT(ifs, nfssharelist);

			if (strncmp(hostname, ifs->client_name, SM_MAXSTRLEN) == 0) {
				shrfile->refcount--;
				LIST_REMOVE(ifs, nfssharelist);
				free(ifs->oh.n_bytes);
				free(ifs);
			}

			ifs = nfs;
		}

		if (!shrfile->refcount) {
			LIST_REMOVE(shrfile, sharefilelist);
			close(shrfile->fd);
			free(shrfile->filehandle.n_bytes);
			free(shrfile);
		}

		shrfile = nshrfile;
	}

}



/*
 * Routines below here have not been modified in the overhaul
 */

/*
 * Are these two routines still required since lockd is not spawning off
 * children to service locks anymore?  Presumably they were originally
 * put in place to prevent a one child from changing the lock list out
 * from under another one.
 */

void
siglock(void)
{
  sigset_t block;
	
  sigemptyset(&block);
  sigaddset(&block, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &block, NULL) < 0) {
    syslog(LOG_WARNING, "siglock failed: %s", strerror(errno));
  }
}

void
sigunlock(void)
{
  sigset_t block;
	
  sigemptyset(&block);
  sigaddset(&block, SIGCHLD);
  
  if (sigprocmask(SIG_UNBLOCK, &block, NULL) < 0) {
    syslog(LOG_WARNING, "sigunlock failed: %s", strerror(errno));
  }
}


