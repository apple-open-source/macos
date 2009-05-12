/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)autod_nfs.c	1.126	05/06/08 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include <deflt.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "mount.h"
#include <mntopts.h>
#include <locale.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <pthread.h>
#include <limits.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <assert.h>

#include "autofs_types.h"
#include "automount.h"
#include "auto_mntopts.h"
#include "replica.h"
#include "nfs.h"
#include "nfs_subr.h"

#include "umount_by_fsid.h"

#define	MAXHOSTS	512

/*
 * host cache states
 */
#define	NOHOST		0	/* host not found in the cache */
#define	GOODHOST	1	/* host was OK last time we checked */
#define	DEADHOST	2	/* host was dead last time we checked */
#define	NXHOST		3	/* host didn't exist last time we checked */

struct cache_entry {
	struct	cache_entry *cache_next;
	char	*cache_host;
	time_t	cache_time;
	int	cache_state;
	rpcvers_t cache_reqvers;
	rpcvers_t cache_outvers;
	char	*cache_proto;
};

static struct cache_entry *cache_head = NULL;
pthread_rwlock_t cache_lock;	/* protect the cache chain */

static int nfsmount(struct mapfs *, char *, char *, boolean_t, mach_port_t);
static int is_nfs_port(char *);

static struct mapfs *enum_servers(struct mapent *, char *);
static struct mapfs *get_mysubnet_servers(struct mapfs *);
static int subnet_test(int, int, char *);

struct mapfs *add_mfs(struct mapfs *, int, struct mapfs **, struct mapfs **);
void free_mfs(struct mapfs *);
static void dump_mfs(struct mapfs *, char *, int);
static char *dump_distance(struct mapfs *);
static void cache_free(struct cache_entry *);
static int cache_check(char *, rpcvers_t *, char *);
static void cache_enter(char *, rpcvers_t, rpcvers_t, char *, int);
static void destroy_auth_client_handle(CLIENT *cl);

#ifdef CACHE_DEBUG
static void trace_host_cache();
#endif /* CACHE_DEBUG */

static int rpc_timeout = 20;

#ifdef CACHE_DEBUG
/*
 * host cache counters. These variables do not need to be protected
 * by mutex's. They have been added to measure the utility of the
 * goodhost/deadhost cache in the lazy hierarchical mounting scheme.
 */
static int host_cache_accesses = 0;
static int host_cache_lookups = 0;
static int nxhost_cache_hits = 0;
static int deadhost_cache_hits = 0;
static int goodhost_cache_hits = 0;
#endif /* CACHE_DEBUG */

/*
 * There are the defaults (range) for the client when determining
 * which NFS version to use when probing the server (see above).
 * These will only be used when the vers mount option is not used and
 * these may be reset if /etc/default/nfs is configured to do so.
 */
static rpcvers_t vers_max_default = NFS_VER3;
static rpcvers_t vers_min_default = NFS_VER2;

static int is_v4_mount(char *);

int
mount_nfs(struct mapent *me, char *mntpnt, char *prevhost, boolean_t isdirect,
    mach_port_t gssd_port)
{
	struct mapfs *mfs, *mp;
	int err = -1;

	mfs = enum_servers(me, prevhost);
	if (mfs == NULL)
		return (ENOENT);

	/*
	 * Try loopback if we have something on localhost; if nothing
	 * works, we will fall back to NFS
	 */
	if (is_nfs_port(me->map_mntopts)) {
		for (mp = mfs; mp; mp = mp->mfs_next) {
			if (self_check(mp->mfs_host)) {
#if 0
				err = loopbackmount(mp->mfs_dir,
					mntpnt, me->map_mntopts);
#else
				/*
				 * XXX - no lofs yet.
				 */
				err = ENOENT;
#endif
				if (err) {
					mp->mfs_ignore = 1;
				} else {
					break;
				}
			}
		}
	}
	if (err) {
		err = nfsmount(mfs, mntpnt, me->map_mntopts, isdirect,
		    gssd_port);
		if (err && trace > 1) {
			trace_prt(1, "	Couldn't mount %s:%s, err=%d\n",
				mfs->mfs_host, mfs->mfs_dir, err);
		}
	}
	free_mfs(mfs);
	return (err);
}

struct aftype {
	int	afnum;
	char	*name;
};

static struct mapfs *
get_mysubnet_servers(struct mapfs *mfs_in)
{
	struct mapfs *mfs, *p, *mfs_head = NULL, *mfs_tail = NULL;

	static const struct aftype aflist[] = {
		{ AF_INET, "IPv4" },
#ifdef HAVE_IPV6_SUPPORT
		{ AF_INET6, "IPv6" }
#endif
	};
#define N_AFS	(sizeof aflist / sizeof aflist[0])
	struct hostent *hp;
	char **nb;
	int res;
	int af;
	int err;
	u_int i;

	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		for (i = 0; i < N_AFS; i++) {
			af = aflist[i].afnum;
			hp = getipnodebyname(mfs->mfs_host, af, AI_DEFAULT, &err);
			if (hp == NULL)
				continue;
			if (hp->h_addrtype != af) {
				freehostent(hp);
				continue;
			}

			/*
			 * For each address for this host see if it's on our
			 * local subnet.
			 */

			for (nb = &hp->h_addr_list[0]; *nb != NULL; nb++) {
				if ((res = subnet_test(af, hp->h_length, *nb)) != 0) {
					p = add_mfs(mfs, DIST_MYNET,
						&mfs_head, &mfs_tail);
					if (!p) {
						freehostent(hp);
						return (NULL);
					}
					break;
				}
			}  /* end of every host address */
			if (trace > 2) {
				trace_prt(1, "get_mysubnet_servers: host=%s "
					"netid=%s res=%s\n", mfs->mfs_host,
					aflist[i].name, res == 1?"SUC":"FAIL");
			}

			freehostent(hp);
		} /* end of while */

	} /* end of every map */

	return (mfs_head);

}

/*
 * XXX - there's no SIOC to get at in_localaddr() or in6_localaddr();
 * we might have to do getaddrlist() and reimplement it ourselves.
 * Note that the answer can change over time....
 */
static int
masked_eq(char *a, char *b, char *mask, int len)
{
	char *masklim;

	masklim = mask + len;

	for (; mask < masklim; mask++) {
		if ((*a++ ^ *b++) & *mask)
			break;
	}
	return (mask == masklim);
}
	
static int
subnet_test(int af, int len, char *addr)
{
	struct ifaddrs *ifalist, *ifa;

	if (getifaddrs(&ifalist))
		return (0);

	for (ifa = ifalist; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != af)
			continue;
		if (ifa->ifa_addr->sa_len != len)
			continue;
		if (ifa->ifa_netmask == 0) {
			if (memcmp(ifa->ifa_addr->sa_data, addr, len) == 0 ||
			    (ifa->ifa_dstaddr && memcmp(ifa->ifa_dstaddr->sa_data, addr, len) == 0)) {
				freeifaddrs(ifalist);
				return (1);
			}
		} else {
			if (ifa->ifa_flags & IFF_POINTOPOINT) {
				if (ifa->ifa_dstaddr && memcmp(ifa->ifa_dstaddr->sa_data, addr, len) == 0) {
					freeifaddrs(ifalist);
					return (1);
				}
			} else {
				if (masked_eq(ifa->ifa_addr->sa_data, addr, ifa->ifa_netmask->sa_data, len)) {
					freeifaddrs(ifalist);
					return (1);
				}
			}
		}
	}
	freeifaddrs(ifalist);
	return (0);
}

/*
 * ping a bunch of hosts at once and sort by who responds first
 */
static struct mapfs *
sort_servers(struct mapfs *mfs_in, int timeout)
{
	struct mapfs *m1 = NULL;
	enum clnt_stat clnt_stat;

	if (!mfs_in)
		return (NULL);

	clnt_stat = nfs_cast(mfs_in, &m1, timeout);

	if (!m1) {
		char buff[2048] = {'\0'};
		const char *ellipsis = "";

		for (m1 = mfs_in; m1; m1 = m1->mfs_next) {
			if (strlcat(buff, m1->mfs_host, sizeof buff) >=
			    sizeof buff) {
				ellipsis = "...";
				break;
			}
			if (m1->mfs_next) {
				if (strlcat(buff, ",", sizeof buff) >=
				    sizeof buff) {
					ellipsis = "...";
					break;
				}
			}
		}

		syslog(LOG_ERR, "servers %s%s not responding: %s",
			buff, ellipsis, clnt_sperrno(clnt_stat));
	}

	return (m1);
}

/*
 * Add a mapfs entry to the list described by *mfs_head and *mfs_tail,
 * provided it is not marked "ignored" and isn't a dupe of ones we've
 * already seen.
 */
struct mapfs *
add_mfs(struct mapfs *mfs, int distance, struct mapfs **mfs_head,
	struct mapfs **mfs_tail)
{
	struct mapfs *tmp, *new;
	void bcopy();

	for (tmp = *mfs_head; tmp; tmp = tmp->mfs_next)
		if ((strcmp(tmp->mfs_host, mfs->mfs_host) == 0 &&
		    strcmp(tmp->mfs_dir, mfs->mfs_dir) == 0) ||
			mfs->mfs_ignore)
			return (*mfs_head);
	new = (struct mapfs *)malloc(sizeof (struct mapfs));
	if (!new) {
		syslog(LOG_ERR, "Memory allocation failed: %m");
		return (NULL);
	}
	bcopy(mfs, new, sizeof (struct mapfs));
	new->mfs_next = NULL;
	if (distance)
		new->mfs_distance = distance;
	if (!*mfs_head)
		*mfs_tail = *mfs_head = new;
	else {
		(*mfs_tail)->mfs_next = new;
		*mfs_tail = new;
	}
	return (*mfs_head);
}

static void
dump_mfs(struct mapfs *mfs, char *message, int level)
{
	struct mapfs *m1;

	if (trace <= level)
		return;

	trace_prt(1, "%s", message);
	if (!mfs) {
		trace_prt(0, "mfs is null\n");
		return;
	}
	for (m1 = mfs; m1; m1 = m1->mfs_next)
		trace_prt(0, "%s[%s] ", m1->mfs_host, dump_distance(m1));
	trace_prt(0, "\n");
}

static char *
dump_distance(struct mapfs *mfs)
{
	switch (mfs->mfs_distance) {
	case 0:			return ("zero");
	case DIST_SELF:		return ("self");
	case DIST_MYSUB:	return ("mysub");
	case DIST_MYNET:	return ("mynet");
	case DIST_OTHER:	return ("other");
	default:		return ("other");
	}
}

/*
 * Walk linked list "raw", building a new list consisting of members
 * NOT found in list "filter", returning the result.
 */
static struct mapfs *
filter_mfs(struct mapfs *raw, struct mapfs *filter)
{
	struct mapfs *mfs, *p, *mfs_head = NULL, *mfs_tail = NULL;
	int skip;

	if (!raw)
		return (NULL);
	for (mfs = raw; mfs; mfs = mfs->mfs_next) {
		for (skip = 0, p = filter; p; p = p->mfs_next) {
			if (strcmp(p->mfs_host, mfs->mfs_host) == 0 &&
			    strcmp(p->mfs_dir, mfs->mfs_dir) == 0) {
				skip = 1;
				break;
			}
		}
		if (skip)
			continue;
		p = add_mfs(mfs, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	return (mfs_head);
}

/*
 * Walk a linked list of mapfs structs, freeing each member.
 */
void
free_mfs(struct mapfs *mfs)
{
	struct mapfs *tmp;

	while (mfs) {
		tmp = mfs->mfs_next;
		free(mfs);
		mfs = tmp;
	}
}

/*
 * New code for NFS client failover: we need to carry and sort
 * lists of server possibilities rather than return a single
 * entry.  It preserves previous behaviour of sorting first by
 * locality (loopback-or-preferred/subnet/net/other) and then
 * by ping times.  We'll short-circuit this process when we
 * have ENOUGH or more entries.
 */
static struct mapfs *
enum_servers(struct mapent *me, char *preferred)
{
	struct mapfs *p, *m1, *m2, *mfs_head = NULL, *mfs_tail = NULL;

	/*
	 * Short-circuit for simple cases.
	 */
	if (!me->map_fs->mfs_next) {
		p = add_mfs(me->map_fs, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
		return (mfs_head);
	}

	dump_mfs(me->map_fs, "	enum_servers: mapent: ", 2);

	/*
	 * get addresses & see if any are myself
	 * or were mounted from previously in a
	 * hierarchical mount.
	 */
	if (trace > 2)
		trace_prt(1, "	enum_servers: looking for pref/self\n");
	for (m1 = me->map_fs; m1; m1 = m1->mfs_next) {
		if (m1->mfs_ignore)
			continue;
		if (self_check(m1->mfs_host) ||
		    strcmp(m1->mfs_host, preferred) == 0) {
			p = add_mfs(m1, DIST_SELF, &mfs_head, &mfs_tail);
			if (!p)
				return (NULL);
		}
	}
	if (trace > 2 && m1)
		trace_prt(1, "	enum_servers: pref/self found, %s\n",
			m1->mfs_host);

	/*
	 * look for entries on this subnet
	 */
	dump_mfs(m1, "	enum_servers: input of get_mysubnet_servers: ", 2);
	m1 = get_mysubnet_servers(me->map_fs);
	dump_mfs(m1, "	enum_servers: output of get_mysubnet_servers: ", 3);
	if (m1 && m1->mfs_next) {
		m2 = sort_servers(m1, rpc_timeout / 2);
		dump_mfs(m2, "	enum_servers: output of sort_servers: ", 3);
		free_mfs(m1);
		m1 = m2;
	}

	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, 0, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

	/*
	 * add the rest of the entries at the end
	 */
	m1 = filter_mfs(me->map_fs, mfs_head);
	dump_mfs(m1, "	enum_servers: etc: output of filter_mfs: ", 3);
	m2 = sort_servers(m1, rpc_timeout / 2);
	dump_mfs(m2, "	enum_servers: etc: output of sort_servers: ", 3);
	if (m1)
		free_mfs(m1);
	m1 = m2;
	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p)
			return (NULL);
	}
	if (m1)
		free_mfs(m1);

	dump_mfs(mfs_head, "  enum_servers: output: ", 1);
	return (mfs_head);
}

static const struct mntopt mopts_nfs[] = {
	MOPT_NFS
};

static int
nfsmount(struct mapfs *mfs_in, char *mntpnt, char *opts, boolean_t isdirect,
    mach_port_t gssd_port)
{
	mntoptparse_t mp;
	int flags, altflags;
	struct stat stbuf;
	rpcvers_t vers, versmin; /* used to negotiate nfs version in pingnfs */
				/* and mount version with mountd */
	long nfsvers;		/* version in map options, 0 if not there */

	/* used to negotiate nfs version using webnfs */
	rpcvers_t pubversmin, pubversmax;
	int i;
	char *nfs_proto = NULL;
	long nfs_port = 0;
	char *host, *dir;
	struct mapfs *mfs = NULL;
	int last_error = 0;
	int replicated;
	int entries = 0;
	int v2cnt = 0, v3cnt = 0, v4cnt = 0;
	int v2near = 0, v3near = 0, v4near = 0;
	char *mount_resource = NULL;
	ushort_t thisport;

	dump_mfs(mfs_in, "  nfsmount: input: ", 2);

	if (trace > 1) {
		trace_prt(1, "	nfsmount: mount on %s %s:\n",
			mntpnt, opts);
		for (mfs = mfs_in; mfs; mfs = mfs->mfs_next)
			trace_prt(1, "	  %s:%s\n",
				mfs->mfs_host, mfs->mfs_dir);
	}

	/*
	 * Make sure mountpoint is safe to mount on
	 *
	 * XXX - if we do a stat() on the mount point of a direct
	 * mount, that'll trigger the mount, so do that only for
	 * an indirect mount.
	 *
	 * XXX - why bother doing it at all?  Won't the program
	 * we run just fail if it doesn't exist?
	 */
	if (!isdirect && lstat(mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (ENOENT);
	}

	/*
	 * Parse mount options.
	 */
	flags = altflags = 0;
	getmnt_silent = 1;
	mp = getmntopts(opts, mopts_nfs, &flags, &altflags);
	if (mp == NULL) {
		syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
		    opts);
		last_error = ENOENT;
		goto ret;
	}

	/*
	 * Get protocol specified in options list, if any.
	 * XXX - process NFS_MNT_TCP and NFS_MNT_UDP?
	 */
	if (altflags & NFS_MNT_PROTO) {
		const char *nfs_proto_opt;

		nfs_proto_opt = getmntoptstr(mp, "proto");
		if (nfs_proto_opt == NULL) {
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
		nfs_proto = strdup(nfs_proto_opt);
	}

	/*
	 * Get port specified in options list, if any.
	 */
	if (altflags & NFS_MNT_PORT) {
		nfs_port = getmntoptnum(mp, "port");
		if (nfs_port < 1) {
			syslog(LOG_ERR, "%s: invalid port number", mntpnt);
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
		if (nfs_port > USHRT_MAX) {
			syslog(LOG_ERR, "%s: invalid port number %ld", mntpnt, nfs_port);
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
	} else
		nfs_port = 0;	/* "unspecified" */

	if (altflags & NFS_MNT_VERS) {
		nfsvers = getmntoptnum(mp, "vers");
		if (nfsvers < 1) {
			syslog(LOG_ERR, "%s: invalid NFS version number", mntpnt);
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
		if (set_versrange(nfsvers, &vers, &versmin) != 0) {
			syslog(LOG_ERR, "Incorrect NFS version specified for %s",
				mntpnt);
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
	} else
		nfsvers = 0;	/* "unspecified" */
	freemntopts(mp);

	if (nfsvers != 0) {
		pubversmax = pubversmin = nfsvers;
	} else {
		pubversmax = vers;
		pubversmin = versmin;
	}

	entries = 0;
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		if (!mfs->mfs_ignore)
			entries++;
	}

	if (entries > 1) {
		/*
		 * We have more than one resource.
		 * Walk the whole list of resources, pinging and
		 * collecting version info, and choose one to
		 * mount.
		 *
		 * If we have a version preference, this is easy; we'll
		 * just reject anything that doesn't match.
		 *
		 * If not, we want to try to provide the best compromise
		 * that considers proximity, preference for a higher version,
		 * sorted order, and number of replicas.  We will count
		 * the number of V2 and V3 replicas and also the number
		 * which are "near", i.e. the localhost or on the same
		 * subnet.
		 *
		 * XXX - this really belongs in mount_nfs.
		 */
		entries = 0;
		for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
			if (mfs->mfs_ignore)
				continue;

			host = mfs->mfs_host;

			if (mfs->mfs_flags & MFS_URL) {
				char *path;

				if (nfs_port != 0 && mfs->mfs_port != 0 &&
				    (uint_t)nfs_port != mfs->mfs_port) {

					syslog(LOG_ERR, "nfsmount: port (%u) in nfs URL"
						" not the same as port (%d) in port "
						"option\n", mfs->mfs_port, nfs_port);
					last_error = EIO;
					goto out;
				} else if (nfs_port != 0)
					thisport = nfs_port;
				else
					thisport = mfs->mfs_port;

				dir = mfs->mfs_dir;

				/*
				 * Back off to a conventional mount.
				 *
				 * URL's can contain escape characters. Get
				 * rid of them.
				 */
				path = malloc(strlen(dir) + 2);

				if (path == NULL) {
					syslog(LOG_ERR, "nfsmount: no memory");
					last_error = EIO;
					goto out;
				}

				strcpy(path, dir);
				URLparse(path);
				mfs->mfs_dir = path;
				mfs->mfs_flags |= MFS_ALLOC_DIR;
				mfs->mfs_flags &= ~MFS_URL;
			}

			i = pingnfs(host, &vers, versmin, 0, NULL, nfs_proto);
			if (i != RPC_SUCCESS) {
				if (i == RPC_PROGVERSMISMATCH) {
					syslog(LOG_ERR, "server %s: NFS "
						"protocol version mismatch",
						host);
				} else {
					syslog(LOG_ERR, "server %s not "
						"responding", host);
				}
				mfs->mfs_ignore = 1;
				last_error = ENOENT;
				continue;
			}
			if (nfsvers != 0 && (rpcvers_t)nfsvers != vers) {
				if (nfs_proto == NULL)
					syslog(LOG_ERR,
						"NFS version %d "
						"not supported by %s",
						nfsvers, host);
				else
					syslog(LOG_ERR,
						"NFS version %d "
						"with proto %s "
						"not supported by %s",
						nfsvers, nfs_proto, host);
				mfs->mfs_ignore = 1;
				last_error = ENOENT;
				continue;
			}

			entries++;

			switch (vers) {
#ifdef NFS_V4
			case NFS_V4: v4cnt++; break;
#endif
			case NFS_VER3: v3cnt++; break;
			case NFS_VER2: v2cnt++; break;
			default: break;
			}

			/*
			 * It's not clear how useful this stuff is if
			 * we are using webnfs across the internet, but it
			 * can't hurt.
			 */
			if (mfs->mfs_distance &&
			    mfs->mfs_distance <= DIST_MYSUB) {
				switch (vers) {
#ifdef NFS_V4
				case NFS_V4: v4near++; break;
#endif
				case NFS_VER3: v3near++; break;
				case NFS_VER2: v2near++; break;
				default: break;
				}
			}

			/*
			 * If the mount is not replicated, we don't want to
			 * ping every entry, so we'll stop here.  This means
			 * that we may have to go back to "nextentry" above
			 * to consider another entry if there we can't get
			 * all the way to mount(2) with this one.
			 */
			if (!replicated)
				break;
		}

		if (nfsvers == 0) {
			/*
			 * Choose the NFS version.
			 * We prefer higher versions, but will choose a one-
			 * version downgrade in service if we can use a local
			 * network interface and avoid a router.
			 */
#ifdef NFS_V4
			if (v4cnt && v4cnt >= v3cnt && (v4near || !v3near))
				nfsvers = NFS_V4;
			else
#endif
			if (v3cnt && v3cnt >= v2cnt && (v3near || !v2near))
				nfsvers = NFS_VER3;
			else
				nfsvers = NFS_VER2;
			if (trace > 2)
				trace_prt(1,
				"  nfsmount: v4=%d[%d],v3=%d[%d],v2=%d[%d] => v%d.\n",
				v4cnt, v4near,
				v3cnt, v3near,
				v2cnt, v2near, nfsvers);
		}

		/*
		 * Since we don't support different NFS versions in replicated
		 * mounts, take the opportunity to set
		 * the mount protocol version as appropriate.
		 */
		switch (nfsvers) {
#ifdef NFS_V4
		case NFS_V4:
			break;
#endif
		case NFS_VER3:
			versmin = MOUNTVERS3;
			break;
		case NFS_VER2:
			versmin = MOUNTVERS;
			break;
		}
	}

	/*
	 * Find the first entry not marked as "ignore",
	 * and mount that.
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		if (!mfs->mfs_ignore)
			break;
	}

	/*
	 * Did we get through all possibilities without success?
	 */
	if (!mfs)
		goto out;

	/*
	 * Whew; do the mount, at last.
	 * We just call mount_generic() so it runs the NFS mount
	 * program; that way, we don't have to know the same
	 * stuff about mounting NFS that mount_nfs does.
	 */
	mount_resource = malloc(strlen(mfs->mfs_host) + strlen(mfs->mfs_dir) + 2);
	if (mount_resource == NULL) {
		last_error = errno;
		goto out;
	}
	strcpy(mount_resource, mfs->mfs_host);
	strcat(mount_resource, ":");
	strcat(mount_resource, mfs->mfs_dir);
	last_error = mount_generic(mount_resource, "nfs", opts, mntpnt,
	    isdirect, 0, gssd_port);
	free(mount_resource);

out:
ret:
	if (nfs_proto)
		free(nfs_proto);

	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {

		if (mfs->mfs_flags & MFS_ALLOC_DIR) {
			free(mfs->mfs_dir);
			mfs->mfs_dir = NULL;
			mfs->mfs_flags &= ~MFS_ALLOC_DIR;
		}

		if (mfs->mfs_args != NULL) {
			free(mfs->mfs_args);
			mfs->mfs_args = NULL;
		}
	}

	return (last_error);
}

/*
 * Sends a null call to the remote host's (NFS program, versp). versp
 * may be "NULL" in which case the default maximum version is used.
 * Upon return, versp contains the maximum version supported iff versp!= NULL.
 */
enum clnt_stat
pingnfs(
	char *hostpart,
	rpcvers_t *versp,
	rpcvers_t versmin,
	ushort_t port,			/* may be zeor */
	char *path,
	char *proto)
{
	CLIENT *cl = NULL;
	enum clnt_stat clnt_stat;
	rpcvers_t versmax;	/* maximum version to try against server */
	rpcvers_t outvers;	/* version supported by host on last call */
	rpcvers_t vers_to_try;	/* to try different versions against host */
	char *hostname = hostpart;

	if (path != NULL && strcmp(hostname, "nfs") == 0 &&
	    strncmp(path, "//", 2) == 0) {
		char *sport;

		hostname = strdup(path+2);

		if (hostname == NULL)
			return (RPC_SYSTEMERROR);

		path = strchr(hostname, '/');

		/*
		 * This cannot happen. If it does, give up
		 * on the ping as this is obviously a corrupt
		 * entry.
		 */
		if (path == NULL) {
			free(hostname);
			return (RPC_SUCCESS);
		}

		/*
		 * Probable end point of host string.
		 */
		*path = '\0';

		sport = strchr(hostname, ':');

		if (sport != NULL && sport < path) {

			/*
			 * Actual end point of host string.
			 */
			*sport = '\0';
			port = htons((ushort_t)atoi(sport+1));
		}
	}

	/* Pick up the default versions and then set them appropriately */
	if (versp) {
		versmax = *versp;
		/* use versmin passed in */
	} else {
		set_versrange(0, &versmax, &versmin);
	}

	if (versp)
		*versp = versmax;

	switch (cache_check(hostname, versp, proto)) {
	case GOODHOST:
		if (hostname != hostpart)
			free(hostname);
		return (RPC_SUCCESS);
	case DEADHOST:
		if (hostname != hostpart)
			free(hostname);
		return (RPC_TIMEDOUT);
	case NXHOST:
		if (hostname != hostpart)
			free(hostname);
		return (RPC_UNKNOWNHOST);
	case NOHOST:
	default:
		break;
	}

	vers_to_try = versmax;

	/*
	 * check the host's version within the timeout
	 */
	if (trace > 1)
		trace_prt(1, "	ping: %s request vers=%d min=%d\n",
				hostname, versmax, versmin);

	do {
#ifdef HAVE_V4
		/*
		 * If NFSv4, we give the port number explicitly so that we
		 * avoid talking to the portmapper.  (XXX - necessary?)
		 */
		if (vers_to_try == NFS_V4) {
			if (trace > 4) {
				trace_prt(1, "  pingnfs: Trying ping via TCP\n");
			}

			hp = gethostbyname(hostname);
			if (h == NULL) {
				rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
				cl = NULL;
			} else if (h->h_addrtype != AF_INET) {
				/*
				 * Only support INET for now
				 */
				rpc_createerr.cf_stat = RPC_SYSTEMERROR;
				rpc_createerr.cf_error.re_errno = EAFNOSUPPORT;
			} else {
				bzero((char *)&sin, sizeof sin);
				sin.sin_family = h->h_addrtype;
				sin.sin_port = port;
				bcopy(h->h_addr, (char*)&sin.sin_addr, h->h_length);
				sock = RPC_ANYSOCK;
				cl = clnttcp_create(&sin, NFS_PROG, vers_to_try,
				    &sock, 0, 0);
				if (cl != NULL) {
					outvers = vers_to_try;
					break;
				}
			}
			if (trace > 4) {
				trace_prt(1, "  pingnfs: Can't ping via TCP"
					" %s: RPC error=%d\n",
					hostname, rpc_createerr.cf_stat);
			}

		} else
#endif
		{
			if ((cl = clnt_create(hostname, NFS_PROG,
				vers_to_try, "udp"))
				!= NULL)
				break;
			if (trace > 4) {
				trace_prt(1, "  pingnfs: Can't ping via UDP"
					" %s: RPC error=%d\n",
					hostname, rpc_createerr.cf_stat);
			}
			if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST ||
				rpc_createerr.cf_stat == RPC_TIMEDOUT)
				break;
			if (rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED) {
				if (trace > 4) {
					trace_prt(1, "  pingnfs: Trying ping "
						"via TCP\n");
				}
				if ((cl = clnt_create(hostname,
					NFS_PROG, vers_to_try, "tcp")) != NULL)
					break;
				if (trace > 4) {
					trace_prt(1, "  pingnfs: Can't ping "
						"via TCP %s: "
						"RPC error=%d\n",
						hostname,
						rpc_createerr.cf_stat);
				}
			}
		}

		/*
		 * backoff and return lower version to retry the ping.
		 * XXX we should be more careful and handle
		 * RPC_PROGVERSMISMATCH here, because that error is handled
		 * in clnt_create_vers(). It's not done to stay in sync
		 * with the nfs mount command.
		 */
		vers_to_try--;
		if (vers_to_try < versmin)
			break;
		if (versp != NULL) {	/* recheck the cache */
			*versp = vers_to_try;
			if (trace > 4) {
				trace_prt(1,
				    "  pingnfs: check cache: vers=%d\n",
				    *versp);
			}
			switch (cache_check(hostname, versp, proto)) {
			case GOODHOST:
				if (hostname != hostpart)
					free(hostname);
				return (RPC_SUCCESS);
			case DEADHOST:
				if (hostname != hostpart)
					free(hostname);
				return (RPC_TIMEDOUT);
			case NXHOST:
				if (hostname != hostpart)
					free(hostname);
				return (RPC_UNKNOWNHOST);
			case NOHOST:
			default:
				break;
			}
		}
		if (trace > 4) {
			trace_prt(1, "  pingnfs: Try version=%d\n",
				vers_to_try);
		}
	} while (cl == NULL);


	if (cl == NULL) {
		if (verbose)
			syslog(LOG_ERR, "pingnfs: %s%s",
				hostname, clnt_spcreateerror(""));
		clnt_stat = rpc_createerr.cf_stat;
	} else {
		clnt_destroy(cl);
		clnt_stat = RPC_SUCCESS;
	}

	if (trace > 1)
		clnt_stat == RPC_SUCCESS ?
			trace_prt(1, "	pingnfs OK: nfs version=%d\n", outvers):
			trace_prt(1, "	pingnfs FAIL: can't get nfs version\n");

	switch (clnt_stat) {

	case RPC_SUCCESS:
		cache_enter(hostname, versmax, outvers, proto, GOODHOST);
		if (versp != NULL)
			*versp = outvers;
		break;

	case RPC_UNKNOWNHOST:
		cache_enter(hostname, versmax, versmax, proto, NXHOST);
		break;

	default:
		cache_enter(hostname, versmax, versmax, proto, DEADHOST);
		break;
	}

	if (hostpart != hostname)
		free(hostname);

	return (clnt_stat);
}

#if 0
#define	MNTTYPE_LOFS	"lofs"

int
loopbackmount(fsname, dir, mntopts)
	char *fsname;		/* Directory being mounted */
	char *dir;		/* Directory being mounted on */
	char *mntopts;
{
	struct mnttab mnt;
	int flags = 0;
	char fstype[] = MNTTYPE_LOFS;
	int dirlen;
	struct stat st;
	char optbuf[AUTOFS_MAXOPTSLEN];

	dirlen = strlen(dir);
	if (dir[dirlen-1] == ' ')
		dirlen--;

	if (dirlen == strlen(fsname) &&
		strncmp(fsname, dir, dirlen) == 0) {
		syslog(LOG_ERR,
			"Mount of %s on %s would result in deadlock, aborted\n",
			fsname, dir);
		return (RET_ERR);
	}
	mnt.mnt_mntopts = mntopts;
	if (hasmntopt(&mnt, MNTOPT_RO) != NULL)
		flags |= MS_RDONLY;

	(void) strlcpy(optbuf, mntopts, sizeof (optbuf));

	if (trace > 1)
		trace_prt(1,
			"  loopbackmount: fsname=%s, dir=%s, flags=%d\n",
			fsname, dir, flags);

	if (mount(fsname, dir, flags | MS_DATA | MS_OPTIONSTR, fstype,
	    NULL, 0, optbuf, sizeof (optbuf)) < 0) {
		syslog(LOG_ERR, "Mount of %s on %s: %m", fsname, dir);
		return (RET_ERR);
	}

	if (stat(dir, &st) == 0) {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s dev=%x rdev=%x OK\n",
			    fsname, dir, st.st_dev, st.st_rdev);
		}
	} else {
		if (trace > 1) {
			trace_prt(1,
			    "  loopbackmount of %s on %s OK\n", fsname, dir);
			trace_prt(1, "	stat of %s failed\n", dir);
		}
	}

	return (0);
}
#endif

#if 0
/*
 * Look for the value of a numeric option of the form foo=x.  If found, set
 * *valp to the value and return non-zero.  If not found or the option is
 * malformed, return zero.
 */

int
nopt(mnt, opt, valp)
	struct mnttab *mnt;
	char *opt;
	int *valp;			/* OUT */
{
	char *equal;
	char *str;

	/*
	 * We should never get a null pointer, but if we do, it's better to
	 * ignore the option than to dump core.
	 */

	if (valp == NULL) {
		syslog(LOG_DEBUG, "null pointer for %s option", opt);
		return (0);
	}

	if (str = hasmntopt(mnt, opt)) {
		if (equal = strchr(str, '=')) {
			*valp = atoi(&equal[1]);
			return (1);
		} else {
			syslog(LOG_ERR, "Bad numeric option '%s'", str);
		}
	}
	return (0);
}
#endif

int
nfsunmount(fsid_t *fsid, struct mnttab *mnt)
{
	struct timeval timeout;
	CLIENT *cl;
	enum clnt_stat rpc_stat;
	char *host, *path;
	struct replica *list;
	int i, count = 0;
	int isv4mount = is_v4_mount(mnt->mnt_mountp);

	if (trace > 1)
		trace_prt(1, "	nfsunmount: umount %s\n", mnt->mnt_mountp);

	if (umount_by_fsid(fsid, 0) < 0) {
		if (trace > 1)
			trace_prt(1, "	nfsunmount: umount %s FAILED\n",
				mnt->mnt_mountp);
		if (errno)
			return (errno);
	}

	/*
	 * If this is a NFSv4 mount, the mount protocol was not used
	 * so we just return.
	 */
	if (isv4mount) {
		if (trace > 1)
			trace_prt(1, "	nfsunmount: umount %s OK\n",
				mnt->mnt_mountp);
		return (0);
	}

	/*
	 * XXX - we don't do WebNFS.
	 */
#if 0
	/*
	 * If mounted with -o public, then no need to contact server
	 * because mount protocol was not used.
	 */
	if (hasmntopt(mnt, MNTOPT_PUBLIC) != NULL) {
		return (0);
	}
#endif

	/*
	 * The rest of this code is advisory to the server.
	 * If it fails return success anyway.
	 */

	list = parse_replica(mnt->mnt_special, &count);
	if (!list) {
		if (count >= 0)
			syslog(LOG_ERR,
			    "Memory allocation failed: %m");
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {

		host = list[i].host;
		path = list[i].path;

		/*
		 * Skip file systems mounted using WebNFS, because mount
		 * protocol was not used.
		 */
		if (strcmp(host, "nfs") == 0 && strncmp(path, "//", 2) == 0)
			continue;

		/*
		 * We assume this binds to a reserved port, if possible.
		 */
		cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "udp");
		if (cl == NULL)
			break;
#ifdef MALLOC_DEBUG
		add_alloc("CLNT_HANDLE", cl, 0, __FILE__, __LINE__);
		add_alloc("AUTH_HANDLE", cl->cl_auth, 0,
			__FILE__, __LINE__);
#endif
#ifdef MALLOC_DEBUG
		drop_alloc("AUTH_HANDLE", cl->cl_auth, __FILE__, __LINE__);
#endif
		AUTH_DESTROY(cl->cl_auth);
		if ((cl->cl_auth = authunix_create_default()) == NULL) {
			if (verbose)
				syslog(LOG_ERR, "umount %s:%s: %s",
					host, path,
					"Failed creating default auth handle");
			destroy_auth_client_handle(cl);
			continue;
		}
#ifdef MALLOC_DEBUG
		add_alloc("AUTH_HANDLE", cl->cl_auth, 0, __FILE__, __LINE__);
#endif
		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		rpc_stat = clnt_call(cl, MOUNTPROC_UMNT, (xdrproc_t)xdr_dirpath,
			    (caddr_t)&path, (xdrproc_t)xdr_void, (char *)NULL,
			    timeout);
		if (verbose && rpc_stat != RPC_SUCCESS)
			syslog(LOG_ERR, "%s: %s",
				host, clnt_sperror(cl, "unmount"));
		destroy_auth_client_handle(cl);
	}

	free_replica(list, count);

	if (trace > 1)
		trace_prt(1, "	nfsunmount: umount %s OK\n", mnt->mnt_mountp);

	return (0);
}

/*
 * Put a new entry in the cache chain by prepending it to the front.
 * If there isn't enough memory then just give up.
 */
static void
cache_enter(host, reqvers, outvers, proto, state)
	char *host;
	rpcvers_t reqvers;
	rpcvers_t outvers;
	char *proto;
	int state;
{
	struct cache_entry *entry;
	int cache_time = 30;	/* sec */

	timenow = time(NULL);

	entry = (struct cache_entry *)malloc(sizeof (struct cache_entry));
	if (entry == NULL)
		return;
	(void) memset((caddr_t)entry, 0, sizeof (struct cache_entry));
	entry->cache_host = strdup(host);
	if (entry->cache_host == NULL) {
		cache_free(entry);
		return;
	}
	entry->cache_reqvers = reqvers;
	entry->cache_outvers = outvers;
	entry->cache_proto = (proto == NULL ? NULL : strdup(proto));
	entry->cache_state = state;
	entry->cache_time = timenow + cache_time;
	(void) pthread_rwlock_wrlock(&cache_lock);
#ifdef CACHE_DEBUG
	host_cache_accesses++;		/* up host cache access counter */
#endif /* CACHE DEBUG */
	entry->cache_next = cache_head;
	cache_head = entry;
	(void) pthread_rwlock_unlock(&cache_lock);
}

static int
cache_check(host, versp, proto)
	char *host;
	rpcvers_t *versp;
	char *proto;
{
	int state = NOHOST;
	struct cache_entry *ce, *prev;

	timenow = time(NULL);

	(void) pthread_rwlock_rdlock(&cache_lock);

#ifdef CACHE_DEBUG
	/* Increment the lookup and access counters for the host cache */
	host_cache_accesses++;
	host_cache_lookups++;
	if ((host_cache_lookups%1000) == 0)
		trace_host_cache();
#endif /* CACHE DEBUG */

	for (ce = cache_head; ce; ce = ce->cache_next) {
		if (timenow > ce->cache_time) {
			(void) pthread_rwlock_unlock(&cache_lock);
			(void) pthread_rwlock_wrlock(&cache_lock);
			for (prev = NULL, ce = cache_head; ce;
				prev = ce, ce = ce->cache_next) {
				if (timenow > ce->cache_time) {
					cache_free(ce);
					if (prev)
						prev->cache_next = NULL;
					else
						cache_head = NULL;
					break;
				}
			}
			(void) pthread_rwlock_unlock(&cache_lock);
			return (state);
		}
		if (strcmp(host, ce->cache_host) != 0)
			continue;
		if ((proto == NULL && ce->cache_proto != NULL) ||
		    (proto != NULL && ce->cache_proto == NULL))
			continue;
		if (proto != NULL &&
		    strcmp(proto, ce->cache_proto) != 0)
			continue;

		if (versp == NULL ||
			(versp != NULL && *versp == ce->cache_reqvers) ||
			(versp != NULL && *versp == ce->cache_outvers)) {
				if (versp != NULL)
					*versp = ce->cache_outvers;
				state = ce->cache_state;

				/* increment the host cache hit counters */
#ifdef CACHE_DEBUG
				switch (state) {

				case GOODHOST:
					goodhost_cache_hits++;
					break;

				case DEADHOST:
					deadhost_cache_hits++;
					break;

				case NXHOST:
					nxhost_cache_hits++;
					break;
#endif /* CACHE_DEBUG */
				(void) pthread_rwlock_unlock(&cache_lock);
				return (state);
		}
	}
	(void) pthread_rwlock_unlock(&cache_lock);
	return (state);
}

/*
 * Free a cache entry and all entries
 * further down the chain since they
 * will also be expired.
 */
static void
cache_free(entry)
	struct cache_entry *entry;
{
	struct cache_entry *ce, *next = NULL;

	for (ce = entry; ce; ce = next) {
		if (ce->cache_host)
			free(ce->cache_host);
		if (ce->cache_proto)
			free(ce->cache_proto);
		next = ce->cache_next;
		free(ce);
	}
}

static void
cache_flush(void)
{
	(void) pthread_rwlock_wrlock(&cache_lock);
	cache_free(cache_head);
	cache_head = NULL;
	(void) pthread_rwlock_unlock(&cache_lock);
}

void
flush_caches(void)
{
	pthread_mutex_lock(&cleanup_lock);
	pthread_cond_signal(&cleanup_start_cv);
	(void) pthread_cond_wait(&cleanup_done_cv, &cleanup_lock);
	pthread_mutex_unlock(&cleanup_lock);
	cache_flush();
}

/*
 * Returns 1, if port option is NFS_PORT or
 *	nfsd is running on the port given
 * Returns 0, if both port is not NFS_PORT and nfsd is not
 *	running on the port.
 */

static int
is_nfs_port(char *opts)
{
	mntoptparse_t mp;
	int flags, altflags;
	long nfs_port = 0;
#if 0
	struct servent sv;
	char buf[256];
#endif
	int got_port;

	/*
	 * Parse mount options.
	 */
	flags = altflags = 0;
	getmnt_silent = 1;
	mp = getmntopts(opts, mopts_nfs, &flags, &altflags);
	if (mp == NULL) {
		syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
		    opts);
		return (0);
	}

	/*
	 * Get port specified in options list, if any.
	 */
	got_port = (altflags & NFS_MNT_PORT);
	if (got_port) {
		nfs_port = getmntoptnum(mp, "port");
		if (nfs_port == -1) {
			syslog(LOG_ERR, "Invalid port number in \"%s\"",
			    opts);
			freemntopts(mp);
			return (0);
		}
		if (nfs_port > USHRT_MAX) {
			syslog(LOG_ERR, "Invalid port number %ld in \"%s\"",
			    nfs_port, opts);
			freemntopts(mp);
			return (0);
		}
	}
	freemntopts(mp);

	/*
	 * if no port specified or it is same as NFS_PORT return nfs
	 * To use any other daemon the port number should be different
	 */
	if (!got_port || nfs_port == NFS_PORT)
		return (1);
#if 0
	/*
	 * If daemon is nfsd, return nfs
	 * XXX - we don't have getservbyport_r(), and it's not clear
	 * that this does anything useful - the only port that should
	 * map to "nfsd" is 2049, i.e. NFS_PORT.
	 */
	if (getservbyport_r(nfs_port, NULL, &sv, buf, 256) == &sv &&
		strcmp(sv.s_name, "nfsd") == 0)
		return (1);
#endif

	/*
	 * daemon is not nfs
	 */
	return (0);
}


/*
 * destroy_auth_client_handle(cl)
 * destroys the created client handle
 */
static void
destroy_auth_client_handle(CLIENT *cl)
{
	if (cl) {
		if (cl->cl_auth) {
#ifdef MALLOC_DEBUG
			drop_alloc("AUTH_HANDLE", cl->cl_auth,
				__FILE__, __LINE__);
#endif
			AUTH_DESTROY(cl->cl_auth);
			cl->cl_auth = NULL;
		}
#ifdef MALLOC_DEBUG
		drop_alloc("CLNT_HANDLE", cl,
			__FILE__, __LINE__);
#endif
		clnt_destroy(cl);
	}
}


/*
 * Attempt to figure out which version of NFS to use in pingnfs().  If
 * the version number was specified (i.e., non-zero), then use it.
 * Otherwise, default to the compiled-in default or the default as set
 * by the /etc/default/nfs configuration (as read by read_default().
 */
int
set_versrange(rpcvers_t nfsvers, rpcvers_t *vers, rpcvers_t *versmin)
{
	switch (nfsvers) {
	case 0:
		*vers = vers_max_default;
		*versmin = vers_min_default;
		break;
#ifdef NFS_V4
	case NFS_V4:
		*vers = NFS_V4;
		*versmin = NFS_V4;
		break;
#endif
	case NFS_VER3:
		*vers = NFS_VER3;
		*versmin = NFS_VER3;
		break;
	case NFS_VER2:
		*vers = NFS_VER2;
		*versmin = NFS_VER2;
		break;
	default:
		return (-1);
	}
	return (0);
}

#ifdef CACHE_DEBUG
/*
 * trace_host_cache()
 * traces the host cache values at desired points
 */
static void
trace_host_cache()
{
	syslog(LOG_ERR,
		"host_cache: accesses=%d lookups=%d deadhits=%d goodhits=%d\n",
		host_cache_accesses, host_cache_lookups, deadhost_cache_hits,
		goodhost_cache_hits);
}
#endif /* CACHE_DEBUG */

/*
 * We don't support NFSv4.
 */
static int
is_v4_mount(__unused char *mntpath)
{
	return (FALSE);
}
