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
 * svc_run() replacement for NetInfo
 * Copyright (C) 1989 by NeXT, Inc.
 */

/*
 * We provide our own svc_run() here instead of using the standard one
 * provided by the RPC library so that we can do a few useful things:
 *
 * 1. Close off connections on an LRU (least-recently-used) basis.
 * 2. Periodically send out resynchronization notices (master only).
 *
 * TODO: Clean up memory periodically too to avoid fragmentation problems.
 */

#include <NetInfo/config.h>
#include "ni_server.h"
#include "ni_globals.h"
#include "ni_notify.h"
#include "event.h"
#include <NetInfo/system_log.h>
#include <NetInfo/socket_lock.h>
#include "ni_dir.h"
#include "getstuff.h"
#include <sys/errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdlib.h>

extern SVCXPRT * svcfd_create(int, u_int, u_int);
extern int svc_maxfd;
extern int _rpc_dtablesize();

extern void readall_cleanup(void);
void log_connection_info(int fd);

int ni_svc_connections;		/* number of current NI connections */
int ni_svc_topconnections;	/* max (used) number of NI connections */
int ni_svc_maxconnections;	/* max (absolute) number of NI connections */
int ni_svc_lruclosed;		/* number of NI connections which we closed */

/*
 * Data structure used to keep track of file descriptors
 */
typedef struct lrustuff
{
	unsigned len;		/* number of descriptors stored here */
	int *val;		/* pointer to array of descriptors */
} lrustuff;


/*
 * perform the bitwise function c = a & b.
 * "max" is the max number of descriptors we expect.
 */
static void
fd_and(unsigned max, fd_set *a, fd_set *b, fd_set *c)
{
	int i;

	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		c->fds_bits[i] = a->fds_bits[i] & b->fds_bits[i];
	}
}


#ifdef notdef
/*
 * perform the bitwise function c = a | b.
 * "max" is the max number of descriptors we expect.
 */
static void
fd_or(unsigned max, fd_set *a, fd_set *b, fd_set *c)
{
	int i;

	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		c->fds_bits[i] = a->fds_bits[i] | b->fds_bits[i];
	}
}

#endif

/*
 * perform the bitwise function c = ~(a & b).
 * "max" is the max number of descriptors we expect.
 */
static void
fd_clr(unsigned max, fd_set *a, fd_set *b, fd_set *c)
{
	int i;

	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		c->fds_bits[i] = ~(a->fds_bits[i]) & b->fds_bits[i];
	}
}

/*
 * How many bits are set in the given word?
 */
static int
bitcount(unsigned u)
{
	int count;

	for (count = 0; u > 0; u >>=1) count += (u & 1);
	return count;
}

/*
 * How many bits are set in the given fd_set?
 */
static int
fd_count(unsigned max, fd_set *fds)
{
	int i, count;

	count = 0;
	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		count += bitcount((unsigned)fds->fds_bits[i]);
	}
	return count;
}

/*
 * Allocates and initializes an lru descriptor data structure
 */
static lrustuff
lru_init(unsigned max)
{
	lrustuff lru;

	lru.val = (int *)malloc(sizeof(int) * max);
	lru.len = 0;
	return lru;
}

/*
 * Mark a single descriptor as being recently used. If this is a new
 * descriptor, add it to the list.
 */
static void
lru_markone(lrustuff *lru, unsigned which)
{
	int i, j, mark, new;

	new = 1;
	mark = lru->len;
	for (i = 0; i < lru->len; i++)
	{
		if (lru->val[i] == which)
		{
			new = 0;
			mark = i;
			lru->len--; /* don't double count */
			break;
		}
	}

	for (j = mark; j > 0; j--)
	{
		lru->val[j] = lru->val[j - 1];
	}
	lru->val[0] = which;
	lru->len++;

	if (new == 1)
	{
		setsockopt(which, IPPROTO_TCP, TCP_NODELAY, &new, sizeof(int));
	}
}


/*
 * Mark each of the descriptors in the given set as being recently used.
 */
static void
lru_mark(unsigned max, lrustuff *lru, fd_set *fds)
{
	int i, j;
	fd_mask mask, mask2;

	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		mask = fds->fds_bits[i];
		for (j = 0, mask2 = 1; mask && j < NFDBITS; j++, mask2 <<= 1)
		{
			if (mask & mask2)
			{
				lru_markone(lru, NFDBITS * i + j);
				mask ^= mask2;
			}
		}
	}
}

/*
 * The given descriptor has been closed. Delete it from the list.
 */
static void
lru_unmarkone(lrustuff *lru, unsigned which)
{
	int i;

	for (i = 0; i < lru->len; i++)
	{
		if (lru->val[i] == which)
		{
			while (i < lru->len)
			{
				lru->val[i] = lru->val[i + 1];
				i++;
			}

			lru->len--;
			return;
		}
	}
}

/*
 * The given descriptors have been closed. Delete them from the list.
 */
static void
lru_unmark(unsigned max, lrustuff *lru, fd_set *fds)
{
	int i, j;
	fd_mask mask, mask2;

	for (i = 0; i < howmany(max, NFDBITS); i++)
	{
		mask = fds->fds_bits[i];
		for (j = 0, mask2 = 1; mask && j < NFDBITS; j++, mask2 <<= 1)
		{
			if (mask & mask2)
			{
				lru_unmarkone(lru, NFDBITS * i + j);
				mask ^= mask2;
			}
		}
	}
}

/*
 * Close off the LRU descriptor.
 */
static void
lru_close(lrustuff *lru)
{
	fd_set mask;
	int fd;

	fd = lru->val[lru->len - 1];
	log_connection_info(fd);
	lru_unmarkone(lru, fd);
	FD_ZERO(&mask);
	FD_SET(fd, &mask);

	socket_lock();
	close(fd);
	svc_getreqset(&mask);
	socket_unlock();

	if (FD_ISSET(fd, &svc_fdset))
	{
		system_log(LOG_ERR, "closed descriptor is still set");
	}

	ni_svc_lruclosed++;
}

#if CONNECTION_CHECK

void
open_connections(unsigned maxfds, fd_set *fds)
{
	int newfd;
	struct sockaddr_in from;
	int fromlen;
	SVCXPRT *transp;

	if (FD_ISSET(tcp_sock, fds))
	{
		FD_CLR(tcp_sock, fds);
		fromlen = sizeof(from);

		socket_lock();
		newfd = accept(tcp_sock, (struct sockaddr *)&from, &fromlen);
		socket_unlock();

		if (newfd >= 0)
		{
			if (is_trusted_network(db_ni, &from))
			{
				transp = (SVCXPRT *)svcfd_create(newfd, NI_SENDSIZE, NI_RECVSIZE);
				if (transp != NULL)
				{
					transp->xp_raddr = from;
					transp->xp_addrlen = fromlen;
					if (!FD_ISSET(newfd, &svc_fdset))
					{
						system_log(LOG_ERR, "new descriptor is not set");
					}
				}
				else
				{
					socket_lock();
					close(newfd);
					socket_unlock();

					if (FD_ISSET(newfd, &svc_fdset))
					{
						system_log(LOG_ERR, "closed descriptor is still set");
					}
				}
			}
			else
			{
				/*
				 * We don't trust the network that this
				 * guy is from. Close him off.
				 */
				socket_lock();
				close(newfd);
				socket_unlock();
			}
		}
	} 

	/*
	 * Now handle UDP socket
	 */
	if (FD_ISSET(udp_sock, fds))
	{
		socket_lock();
		svc_getreqset(fds);
		socket_unlock();
	}
}


#endif

/*
 * The replacement for the standard svc_run() provided by the RPC library
 * so that we can keep track of the LRU descriptors and perform periodic
 * actions.
 */
void
ni_svc_run(int maxlisteners)
{
	fd_set readfds;
	fd_set orig;
	fd_set fds;
	fd_set save;
	fd_set shut;
	unsigned maxfds;
	unsigned lastfd;
	extern int errno;
	int event_fd;
	lrustuff lru;
	struct timeval now;
	bool_t saw_ebadf = FALSE;	/* Two straight EBADFs flush LRU */

	FD_ZERO(&readfds);
	FD_ZERO(&orig);
	FD_ZERO(&fds);
	FD_ZERO(&save);
	FD_ZERO(&shut);

	gettimeofday(&now, NULL);
	cleanuptime = now.tv_sec + cleanupwait;
	orig = svc_fdset;

	maxfds = _rpc_dtablesize();
	if (FD_SETSIZE < maxfds)
	{
		/* if netinfod compiled with less than libSystem */
		maxfds = FD_SETSIZE;
	}

	lru = lru_init(maxfds);
	ni_svc_connections = 0;
	ni_svc_topconnections = 0;
	ni_svc_maxconnections = maxlisteners;
	ni_svc_lruclosed = 0;

	while (!shutdown_server)
	{
		/*
		 * If we need to clean up proxies, do it here to
		 * avoid possible race conditions with ourself, due
		 * to calling free() in [HashTable removeKey:] in
		 * remove_proxy in the signal catcher, which might be
		 * invoked just after some other piece of this thread
		 * called the malloc library.
		 *
		 * Check once ere the select().
		 * XXX Even though we also check if we're interrupted
		 * out of the select(), readall_done might be false,
		 * then between the check and the call of select() if
		 * might become true.  If this is a problem (highly
		 * unlikely: the child will just sit around, and it
		 * might miss an update, and we'll be no worse off,
		 * but we'll even check this when select() completes),
		 * we could always just have select timeout, right?
		 */
		if (readall_done) readall_cleanup();

		/*
		 * Protect the main thread from using the set of ALL known
		 * RPC file descriptors (svc_fdset) by excluding those FDs
		 * associated with the client side operations.
		 */
		fd_clr(maxfds, &clnt_fdset, &svc_fdset, &readfds);

		event_fd = event_pipe[0];
		if (event_fd >= 0) FD_SET(event_fd, &readfds);

		/*
		 * There's a race condition here.  If svc_fdset changed
		 * between when readfds is set, above, and here, and if
		 * the change is that some other thread closed one of
		 * the FDs, then we might have a list with a bad fd in it.
		 */
		lastfd = svc_maxfd;
		if ((event_fd >= 0) && (event_fd > lastfd)) lastfd = event_fd;

		switch (select(lastfd+1, &readfds, NULL, NULL, NULL))
		{
		case -1:
			if (errno == EBADF)
			{
				/*
			 	* If we get in this state, there's no way out.
			 	* Somehow (perhaps in the race condition, above),
			 	* some FD got into the set we want, and it likely
			 	* won't go away.  Just flush the LRU cache.
			 	*/
				if (saw_ebadf)
				{
					/*
					 * Two straight EBADFs and we flush the
					 * LRU cache.
					 */
					system_log(LOG_WARNING, "2nd straight bad file number in readfds; flushing LRU cache (%d entr%s)", ni_svc_connections, ni_svc_connections != 1 ? "ies" : "y");
					for (; ni_svc_connections > 0; ni_svc_connections--)
					{
				 	   lru_close(&lru);
					}

					saw_ebadf = FALSE;
			 	   }
				   else
				   {
						system_log(LOG_WARNING, "Bad file number in readfds; cocking...");
						saw_ebadf = TRUE;
					}
				}
				else if (errno != EINTR)
				{
			 	   saw_ebadf = FALSE;
			 	   system_log(LOG_ERR, "unexpected errno: %m");
			 	   sleep(10);
				}
				else
				{
					saw_ebadf = FALSE;
					if (readall_done) readall_cleanup();
				}
			break;

		case 0:
			saw_ebadf = FALSE;
			break;

		default:
			saw_ebadf = FALSE;
			if (readall_done) readall_cleanup();

			if (event_fd >= 0 && FD_ISSET(event_fd, &readfds))
			{
				FD_CLR(event_fd, &readfds);
				event_handle();
			}

			fd_clr(maxfds, &clnt_fdset, &svc_fdset, &save);

			/*
			 * First find out if any of our listener
			 * sockets want to open another socket
			 */
			fd_and(maxfds, &orig, &readfds, &fds);
#if CONNECTION_CHECK
			open_connections(maxfds, &fds);
#else
			socket_lock();
			svc_getreqset(&fds);
			socket_unlock();
#endif

			/*
			 * Now see if we have any newly opened 
			 * descriptors and then put them in the
			 * lru list if we do.
			 */
			fd_clr(maxfds, &save, &svc_fdset, &fds);
			fd_clr(maxfds, &clnt_fdset, &fds, &fds);

			ni_svc_connections += fd_count(maxfds, &fds);
			if (ni_svc_connections > ni_svc_topconnections)
			{
				ni_svc_topconnections = ni_svc_connections;
			}

			lru_mark(maxfds, &lru, &fds);

			/*
			 * Update the lru list with any sockets that
			 * need service and service them.
			 */
			fd_clr(maxfds, &orig, &readfds, &fds);
			lru_mark(maxfds, &lru, &fds);

			socket_lock();
			svc_getreqset(&fds);
			socket_unlock();

			/*
			 * Discover if any sockets were shut and
			 * clear them from the lru list
			 */
			fd_clr(maxfds, &svc_fdset, &save, &shut);
			fd_clr(maxfds, &clnt_fdset, &shut, &shut);
			ni_svc_connections -= fd_count(maxfds, &shut);
			lru_unmark(maxfds, &lru, &shut);
			if (ni_svc_connections > ni_svc_maxconnections)
			{
				system_log(LOG_INFO, "Over max FDs; closing LRU (used %d, max %d)", ni_svc_connections, ni_svc_maxconnections);
			}
	
			while (ni_svc_connections > ni_svc_maxconnections)
			{
				/*
				 * If we have reached the maximum number
				 * of descriptors, close the lru ones off
				 */
				lru_close(&lru);
				ni_svc_connections--;
			}
		}

		/*
		 * Turn off periodic resync if cleanupwait is not positive.
		 * XXX Note that the cache flush, which is turned off,
		 * should be done regardless of the resyncs.  Also,
		 * recall that when cleanupwait is set in main() and
		 * _ni_resync_2(), we force master cleanupwait to be
		 * more than 0.
		 */
		gettimeofday(&now, NULL);
		if ((0 < cleanupwait) && (now.tv_sec > cleanuptime))
		{
#ifdef FLUSHCACHE
			/*
			 * Clean out memory
			 * XXX: Turned off for now
			 * because this may take a long time and can
			 * cause clients to be locked out. We need a better
			 * strategy for cleaning up memory.
			 */
			ni_forget(db_ni);
#endif
			cleanuptime = now.tv_sec + cleanupwait;
			system_log(LOG_DEBUG, "cleaning up...");
			/*
			 * Check for database synchronization on both
			 * master and clone.
			 */
			if (i_am_clone)
			{
				/*
				 * If clone, check to see if still in sync
				 */
				dir_clonecheck();
				/*
				 * Clear have_transferred flag to allow
				 * possible transfers in next cleanup 
				 * period.
				 */
				have_transferred = 0;
			}
			else
			{
				/*
				 * If master, force resynchronization, in case 
				 * any clones are out of date.
				 */
				notify_resync();
			} 
		}
	}
}

/*
 * Log when we lru_close something.  Might generate tons of messages...
 */
void
log_connection_info(int fd)
{
	struct sockaddr_in us;
	struct sockaddr_in them;
	int count;

	count = sizeof(us);
	if (0 != getsockname(fd, (struct sockaddr *)&us, &count))
	{
		system_log(LOG_DEBUG, "lru_close %d; can't getsockname - %m", fd);
		return;
	}

	count = sizeof(them);

	if (0 != getpeername(fd, (struct sockaddr *)&them, &count))
	{
		system_log(LOG_DEBUG, "lru_close %d: can't getpeername - %m", fd);
		return;
	}

	system_log(LOG_DEBUG, "lru_close %d from %s:%hu to %s:%hu", fd,
	   inet_ntoa(us.sin_addr), ntohs(us.sin_port),
	   inet_ntoa(them.sin_addr), ntohs(them.sin_port));
}
