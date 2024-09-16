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
 * Portions Copyright 2007-2017 Apple Inc.
 */

#pragma ident	"@(#)autod_nfs.c	1.126	05/06/08 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <string.h>
#include "deflt.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/signal.h>
#include <oncrpc/rpc.h>
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

#define MAXHOSTS        512

/*
 * host cache states
 */
#define NOHOST          0       /* host not found in the cache */
#define GOODHOST        1       /* host was OK last time we checked */
#define DEADHOST        2       /* host was dead last time we checked */
#define NXHOST          3       /* host didn't exist last time we checked */

struct cache_entry {
	struct  cache_entry *cache_next;
	char    *cache_host;
	time_t  cache_time;
	time_t  cache_max_time;
	int     cache_level;
	int     cache_state;
	rpcvers_t cache_reqvers;
	rpcvers_t cache_outvers;
	char    *cache_proto;
};

#define AUTOD_CACHE_TIME 2
#define AUTOD_MAX_CACHE_TIME 30

#define PINGNFS_DEBUG_LEVEL 4

static time_t entry_cache_time = AUTOD_CACHE_TIME;
static time_t entry_cache_max_time = AUTOD_MAX_CACHE_TIME;

static struct cache_entry *cache_head = NULL;
pthread_rwlock_t cache_lock;    /* protect the cache chain */

static int nfsmount(struct mapfs *, char *, char *, boolean_t, boolean_t,
    fsid_t, au_asid_t, fsid_t *, uint32_t *);
#ifdef HAVE_LOFS
static int is_nfs_port(char *);
#endif

static struct mapfs *enum_servers(struct mapent *, char *);
static struct mapfs *get_mysubnet_servers(struct mapfs *);
static int subnet_test(int, int, char *);

struct mapfs *add_mfs(struct mapfs *, int, struct mapfs **, struct mapfs **);
void free_mfs(struct mapfs *);
static void dump_mfs(struct mapfs *, char *, int);
static char *dump_distance(struct mapfs *);
static void cache_free(struct cache_entry *);
static int cache_check(const char *, rpcvers_t *, const char *, struct timeval *tv);
static void cache_enter(const char *, rpcvers_t, rpcvers_t, const char *, int);

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

int
mount_nfs(struct mapent *me, char *mntpnt, char *prevhost, boolean_t isdirect,
    fsid_t mntpnt_fsid, au_asid_t asid, fsid_t *fsidp,
    uint32_t *retflags)
{
#ifdef HAVE_LOFS
	struct mapfs *mfs, *mp;
#else
	struct mapfs *mfs;
#endif
	int err = -1;

	mfs = enum_servers(me, prevhost);
	if (mfs == NULL) {
		return ENOENT;
	}

#ifdef HAVE_LOFS
	/*
	 * Try loopback if we have something on localhost; if nothing
	 * works, we will fall back to NFS
	 */
	if (is_nfs_port(me->map_mntopts)) {
		for (mp = mfs; mp; mp = mp->mfs_next) {
			if (self_check(mp->mfs_host)) {
				err = loopbackmount(mp->mfs_dir,
				    mntpnt, me->map_mntopts);
				if (err) {
					mp->mfs_ignore = 1;
				} else {
					break;
				}
			}
		}
	}
#endif
	if (err) {
		err = nfsmount(mfs, mntpnt, me->map_mntopts, isdirect,
		    me->map_quarantine,
		    mntpnt_fsid, asid, fsidp, retflags);
		if (err && trace > 1) {
			trace_prt(1, "	Couldn't mount %s:%s, err=%d\n",
			    mfs->mfs_host, mfs->mfs_dir, err);
		}
	}
	free_mfs(mfs);
	return err;
}

struct aftype {
	int     afnum;
	char    *name;
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
#define N_AFS   (sizeof aflist / sizeof aflist[0])
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
			if (hp == NULL) {
				continue;
			}
			if (hp->h_addrtype != af) {
				freehostent(hp);
				continue;
			}

			/*
			 * For each address for this host see if it's on our
			 * local subnet.
			 */

			res = 0;
			for (nb = &hp->h_addr_list[0]; *nb != NULL; nb++) {
				if ((res = subnet_test(af, hp->h_length, *nb)) != 0) {
					p = add_mfs(mfs, DIST_MYNET,
					    &mfs_head, &mfs_tail);
					if (!p) {
						freehostent(hp);
						return NULL;
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

	return mfs_head;
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
		if ((*a++ ^ *b++) & *mask) {
			break;
		}
	}
	return mask == masklim;
}

static int
subnet_test(int af, int len, char *addr)
{
	struct ifaddrs *ifalist, *ifa;
	char *if_inaddr, *if_inmask, *if_indstaddr;

	if (getifaddrs(&ifalist)) {
		return 0;
	}

	for (ifa = ifalist; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != af) {
			continue;
		}
		if_inaddr = (af == AF_INET) ?
		    (char *) &(((struct sockaddr_in *)(ifa->ifa_addr))->sin_addr) :
		    (char *) &(((struct sockaddr_in6 *)(ifa->ifa_addr))->sin6_addr);
		if (ifa->ifa_dstaddr) {
			if_indstaddr = (af == AF_INET) ?
			    (char *) &(((struct sockaddr_in *)(ifa->ifa_dstaddr))->sin_addr) :
			    (char *) &(((struct sockaddr_in6 *)(ifa->ifa_dstaddr))->sin6_addr);
		} else {
			if_indstaddr = NULL;
		}

		if (ifa->ifa_netmask == NULL) {
			if (memcmp(if_inaddr, addr, len) == 0 ||
			    (if_indstaddr && memcmp(if_indstaddr, addr, len) == 0)) {
				freeifaddrs(ifalist);
				return 1;
			}
		} else {
			if_inmask = (af == AF_INET) ?
			    (char *) &(((struct sockaddr_in *)(ifa->ifa_netmask))->sin_addr) :
			    (char *) &(((struct sockaddr_in6 *)(ifa->ifa_netmask))->sin6_addr);
			if (ifa->ifa_flags & IFF_POINTOPOINT) {
				if (if_indstaddr && memcmp(if_indstaddr, addr, len) == 0) {
					freeifaddrs(ifalist);
					return 1;
				}
			} else {
				if (masked_eq(if_inaddr, addr, if_inmask, len)) {
					freeifaddrs(ifalist);
					return 1;
				}
			}
		}
	}
	freeifaddrs(ifalist);
	return 0;
}

/*
 * ping a bunch of hosts at once and sort by who responds first
 */
static struct mapfs *
sort_servers(struct mapfs *mfs_in, int timeout)
{
	struct mapfs *m1 = NULL;
	enum clnt_stat clnt_stat;

	if (!mfs_in) {
		return NULL;
	}

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

	return m1;
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

	for (tmp = *mfs_head; tmp; tmp = tmp->mfs_next) {
		if ((strcmp(tmp->mfs_host, mfs->mfs_host) == 0 &&
		    strcmp(tmp->mfs_dir, mfs->mfs_dir) == 0) ||
		    mfs->mfs_ignore) {
			return *mfs_head;
		}
	}
	new = (struct mapfs *)malloc(sizeof(struct mapfs));
	if (!new) {
		syslog(LOG_ERR, "Memory allocation failed: %m");
		return NULL;
	}
	bcopy(mfs, new, sizeof(struct mapfs));
	new->mfs_next = NULL;
	if (distance) {
		new->mfs_distance = distance;
	}
	if (!*mfs_head) {
		*mfs_tail = *mfs_head = new;
	} else {
		(*mfs_tail)->mfs_next = new;
		*mfs_tail = new;
	}
	return *mfs_head;
}

static void
dump_mfs(struct mapfs *mfs, char *message, int level)
{
	struct mapfs *m1;

	if (trace <= level) {
		return;
	}

	trace_prt(1, "%s", message);
	if (!mfs) {
		trace_prt(0, "mfs is null\n");
		return;
	}
	for (m1 = mfs; m1; m1 = m1->mfs_next) {
		trace_prt(0, "\t%s[%s] ", m1->mfs_host, dump_distance(m1));
	}
	trace_prt(0, "\n");
}

static char *
dump_distance(struct mapfs *mfs)
{
	switch (mfs->mfs_distance) {
	case 0:                 return "zero";
	case DIST_SELF:         return "self";
	case DIST_MYSUB:        return "mysub";
	case DIST_MYNET:        return "mynet";
	case DIST_OTHER:        return "other";
	default:                return "other";
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

	if (!raw) {
		return NULL;
	}
	for (mfs = raw; mfs; mfs = mfs->mfs_next) {
		for (skip = 0, p = filter; p; p = p->mfs_next) {
			if (strcmp(p->mfs_host, mfs->mfs_host) == 0 &&
			    strcmp(p->mfs_dir, mfs->mfs_dir) == 0) {
				skip = 1;
				break;
			}
		}
		if (skip) {
			continue;
		}
		p = add_mfs(mfs, 0, &mfs_head, &mfs_tail);
		if (!p) {
			return NULL;
		}
	}
	return mfs_head;
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
		if (!p) {
			return NULL;
		}
		return mfs_head;
	}

	dump_mfs(me->map_fs, "	enum_servers: mapent: ", 2);

	/*
	 * get addresses & see if any are myself
	 * or were mounted from previously in a
	 * hierarchical mount.
	 */
	if (trace > 2) {
		trace_prt(1, "	enum_servers: looking for pref/self\n");
	}
	for (m1 = me->map_fs; m1; m1 = m1->mfs_next) {
		if (m1->mfs_ignore) {
			continue;
		}
		if (self_check(m1->mfs_host) ||
		    strcmp(m1->mfs_host, preferred) == 0) {
			if (trace > 2) {
				trace_prt(1, "	enum_servers: pref/self found, %s\n", m1->mfs_host);
			}
			p = add_mfs(m1, DIST_SELF, &mfs_head, &mfs_tail);
			if (!p) {
				return NULL;
			}
		}
	}

	/*
	 * look for entries on this subnet
	 */
	dump_mfs(me->map_fs, "	enum_servers: input of get_mysubnet_servers: ", 2);
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
		if (!p) {
			return NULL;
		}
	}
	if (m1) {
		free_mfs(m1);
	}

	/*
	 * add the rest of the entries at the end
	 */
	m1 = filter_mfs(me->map_fs, mfs_head);
	dump_mfs(m1, "	enum_servers: etc: output of filter_mfs: ", 3);
	m2 = sort_servers(m1, rpc_timeout / 2);
	dump_mfs(m2, "	enum_servers: etc: output of sort_servers: ", 3);
	if (m1) {
		free_mfs(m1);
	}
	m1 = m2;
	for (m2 = m1; m2; m2 = m2->mfs_next) {
		p = add_mfs(m2, DIST_OTHER, &mfs_head, &mfs_tail);
		if (!p) {
			return NULL;
		}
	}
	if (m1) {
		free_mfs(m1);
	}

	dump_mfs(mfs_head, "  enum_servers: output: ", 1);
	return mfs_head;
}

static const struct mntopt mopts_nfs[] = {
	MOPT_NFS
};

static int
nfsmount(struct mapfs *mfs_in, char *mntpnt, char *opts, boolean_t isdirect,
    boolean_t quarantine, fsid_t mntpnt_fsid, au_asid_t asid,
    fsid_t *fsidp, uint32_t *retflags)
{
	mntoptparse_t mp;
	int flags, altflags;
	struct stat stbuf;
	rpcvers_t vers, versmin; /* used to negotiate nfs version in pingnfs */
	                         /* and mount version with mountd */
	rpcvers_t nfsvers;      /* version in map options, 0 if not there */
	long optval;
	static time_t prevmsg = 0;

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
	int mrlen = 0;

	dump_mfs(mfs_in, "  nfsmount: input: ", 2);
	replicated = (mfs_in->mfs_next != NULL);

	if (trace > 1) {
		trace_prt(1, "	nfsmount: mount on %s %s:\n",
		    mntpnt, opts);
		for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
			trace_prt(1, "	  %s:%s\n",
			    mfs->mfs_host, mfs->mfs_dir);
		}
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
		return ENOENT;
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
	} else {
		nfs_port = 0;   /* "unspecified" */
	}
	if (altflags & (NFS_MNT_VERS | NFS_MNT_NFSVERS)) {
		optval = get_nfs_vers(mp, altflags);
		if (optval == 0) {
			/* Error. */
			syslog(LOG_ERR, "%s: invalid NFS version number", mntpnt);
			freemntopts(mp);
			last_error = ENOENT;
			goto ret;
		}
		nfsvers = (rpcvers_t)optval;
	} else {
		nfsvers = 0;    /* "unspecified" */
	}
	if (set_versrange(nfsvers, &vers, &versmin) != 0) {
		syslog(LOG_ERR, "Incorrect NFS version specified for %s",
		    mntpnt);
		freemntopts(mp);
		last_error = ENOENT;
		goto ret;
	}
	freemntopts(mp);

	entries = 0;
	host = NULL;
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		if (!mfs->mfs_ignore) {
			entries++;
			host = mfs->mfs_host;
		}
	}

	if (entries == 1) {
		/*
		 * Make sure the server is responding before attempting a mount.
		 * This up-front check can potentially avoid a hang if a mount
		 * from this server is hierarchical and in the process of being
		 * force unmounted.
		 */
		i = pingnfs(host, &vers, versmin, 0, NULL, nfs_proto);
		if (i != RPC_SUCCESS) {
			if (prevmsg < time((time_t *) NULL)) {
				prevmsg = time((time_t *) NULL) + 5; // throttle these msgs
				if (i == RPC_PROGVERSMISMATCH) {
					syslog(LOG_ERR, "NFS server %s protocol version mismatch", host);
				} else {
					syslog(LOG_ERR, "NFS server %s not responding", host);
				}
			}
			last_error = ENOENT;
			goto out;
		}
	} else if (entries > 1) {
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
			if (mfs->mfs_ignore) {
				continue;
			}

			host = mfs->mfs_host;

			if (mfs->mfs_flags & MFS_URL) {
				char *path;
				int pathlen;

				if (nfs_port != 0 && mfs->mfs_port != 0 &&
				    (uint_t)nfs_port != mfs->mfs_port) {
					syslog(LOG_ERR, "nfsmount: port (%u) in nfs URL"
					    " not the same as port (%ld) in port "
					    "option\n", mfs->mfs_port, nfs_port);
					last_error = EIO;
					goto out;
				}

				dir = mfs->mfs_dir;

				/*
				 * Back off to a conventional mount.
				 *
				 * URL's can contain escape characters. Get
				 * rid of them.
				 */
				pathlen = (int) strlen(dir) + 2;
				path = malloc(pathlen);

				if (path == NULL) {
					syslog(LOG_ERR, "nfsmount: no memory");
					last_error = EIO;
					goto out;
				}

				strlcpy(path, dir, pathlen);
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
				if (nfs_proto == NULL) {
					syslog(LOG_ERR,
					    "NFS version %d "
					    "not supported by %s",
					    nfsvers, host);
				} else {
					syslog(LOG_ERR,
					    "NFS version %d "
					    "with proto %s "
					    "not supported by %s",
					    nfsvers, nfs_proto, host);
				}
				mfs->mfs_ignore = 1;
				last_error = ENOENT;
				continue;
			}

			entries++;

			switch (vers) {
#ifdef NFS_V4_DEFAULT
			case NFS_VER4: v4cnt++; break;
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
#ifdef NFS_V4_DEFAULT
				case NFS_VER4: v4near++; break;
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
			if (!replicated) {
				break;
			}
		}

		if (nfsvers == 0) {
			/*
			 * Choose the NFS version.
			 * We prefer higher versions, but will choose a one-
			 * version downgrade in service if we can use a local
			 * network interface and avoid a router.
			 */
#ifdef NFS_V4_DEFAULT
			if (v4cnt && v4cnt >= v3cnt && (v4near || !v3near)) {
				nfsvers = NFS_VER4;
			} else
#endif
			if (v3cnt && v3cnt >= v2cnt && (v3near || !v2near)) {
				nfsvers = NFS_VER3;
			} else {
				nfsvers = NFS_VER2;
			}
			if (trace > 2) {
				trace_prt(1,
				    "  nfsmount: v4=%d[%d],v3=%d[%d],v2=%d[%d] => v%u.\n",
				    v4cnt, v4near,
				    v3cnt, v3near,
				    v2cnt, v2near, nfsvers);
			}
		}
	}

	/*
	 * Find the first entry not marked as "ignore",
	 * and mount that.
	 */
	for (mfs = mfs_in; mfs; mfs = mfs->mfs_next) {
		if (!mfs->mfs_ignore) {
			break;
		}
	}

	/*
	 * Did we get through all possibilities without success?
	 */
	if (!mfs) {
		goto out;
	}

	/*
	 * Whew; do the mount, at last.
	 * We just call mount_generic() so it runs the NFS mount
	 * program; that way, we don't have to know the same
	 * stuff about mounting NFS that mount_nfs does.
	 */
	mrlen = (int) (strlen(mfs->mfs_host) + strlen(mfs->mfs_dir)) + 2;
	mount_resource = malloc(mrlen);
	if (mount_resource == NULL) {
		last_error = errno;
		goto out;
	}
	strlcpy(mount_resource, mfs->mfs_host, mrlen);
	strlcat(mount_resource, ":", mrlen);
	strlcat(mount_resource, mfs->mfs_dir, mrlen);
	/*
	 * Note we must mount as root for NFS because hierarchical mounts
	 * will almost certainly not work.
	 */
	last_error = mount_generic(mount_resource, "nfs", opts, nfsvers,
	    mntpnt, isdirect, FALSE, quarantine, mntpnt_fsid, 0, asid, fsidp,
	    retflags);

	free(mount_resource);

out:
ret:
	if (nfs_proto) {
		free(nfs_proto);
	}

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

	return last_error;
}

int
get_nfs_vers(mntoptparse_t mp, int altflags)
{
	const char *optstrval;

	/*
	 * "vers=" takes precedence over "nfsvers="; arguably,
	 * we should let the last one specified in the option
	 * string win, but getmntopts() doesn't support that.
	 */
	if (altflags & NFS_MNT_VERS) {
		optstrval = getmntoptstr(mp, "vers");
	} else if (altflags & NFS_MNT_NFSVERS) {
		optstrval = getmntoptstr(mp, "nfsvers");
	} else {
		/*
		 * We shouldn't be called if neither of them are set.
		 */
		return 0;               /* neither vers= nor nfsvers= specified */
	}

	if (optstrval == NULL) {
		return 0;               /* no version specified */
	}
	if (strcmp(optstrval, "2") == 0) {
		return NFS_VER2;        /* NFSv2 */
	} else if (strcmp(optstrval, "3") == 0) {
		return NFS_VER3;        /* NFSv3 */
	} else if (strncmp(optstrval, "4", 1) == 0) {
		return NFS_VER4;        /* "4*" means NFSv4 */
	} else {
		return 0;               /* invalid version */
	}
}

/*
 * This routine has the same definition as clnt_create_vers(),
 * except it takes an additional timeout parameter - a pointer to
 * a timeval structure.  A NULL value for the pointer indicates
 * that the default timeout value should be used.
 */
static CLIENT *
clnt_create_vers_timed(const char *hostname, const rpcprog_t prog,
    rpcvers_t *vers_out, const rpcvers_t vers_low, const rpcvers_t vers_high,
    const char *proto, struct timeval *tp)
{
	CLIENT *clnt;
	struct timeval to;
	enum clnt_stat rpc_stat;
	struct rpc_err rpcerr;
	rpcvers_t v_low, v_high;

	if (tp == NULL) {
		to.tv_sec = 10;
		to.tv_usec = 0;
		tp = &to;
	}

	clnt = clnt_create_timeout(hostname, prog, vers_high, proto, tp);
	if (clnt == NULL) {
		return NULL;
	}
	clnt_control(clnt, CLSET_TIMEOUT, (void *)tp);
	rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void,
	    NULL, (xdrproc_t)xdr_void, NULL, *tp);
	if (rpc_stat == RPC_SUCCESS) {
		*vers_out = vers_high;
		return clnt;
	}
	v_low = vers_low;
	v_high = vers_high;
	while (rpc_stat == RPC_PROGVERSMISMATCH && v_high > v_low) {
		unsigned int minvers, maxvers;

		clnt_geterr(clnt, &rpcerr);
		minvers = rpcerr.re_vers.low;
		maxvers = rpcerr.re_vers.high;
		if (maxvers < v_high) {
			v_high = maxvers;
		} else {
			v_high--;
		}
		if (minvers > v_low) {
			v_low = minvers;
		}
		if (v_low > v_high) {
			goto error;
		}
		clnt_destroy(clnt);
		clnt = clnt_create_timeout(hostname, prog, v_high, proto, tp);
		if (clnt == NULL) {
			return NULL;
		}
		clnt_control(clnt, CLSET_TIMEOUT, tp);
		rpc_stat = clnt_call(clnt, NULLPROC, (xdrproc_t)xdr_void,
		    NULL, (xdrproc_t)xdr_void,
		    NULL, *tp);
		if (rpc_stat == RPC_SUCCESS) {
			*vers_out = v_high;
			return clnt;
		}
	}
	clnt_geterr(clnt, &rpcerr);

error:
	rpc_createerr.cf_stat = rpc_stat;
	rpc_createerr.cf_error = rpcerr;
	clnt_destroy(clnt);
	return NULL;
}

/*
 * Create a client handle for a well known service or a specific port on
 * host. This routine bypasses rpcbind and can be use to construct a client
 * handle to services that are not registered with rpcbind or where the remote
 * rpcbind is not available, e.g., the remote rpcbind port is blocked by a
 * firewall. We construct a client handle and then ping the service's NULL
 * proc to see that the service is really available. If the caller supplies
 * a non zero port number, the service name is ignored and the port will be
 * used. A non-zero port number limits the protocol family to inet or inet6.
 */

static CLIENT *
clnt_create_service_timed(const char *host, const char *service,
    const rpcprog_t prog, const rpcvers_t vers,
    const ushort_t port, const char *proto,
    struct timeval *tmout)
{
	CLIENT *clnt = NULL;
	struct timeval to;
	struct addrinfo hint, *res;
	char portstr[6];
	int sock;
	int gerror;

	if (tmout == NULL) {
		to.tv_sec = 10;
		to.tv_usec = 0;
		tmout = &to;
	}

	if (host == NULL || proto == NULL ||
	    (service == NULL && port == 0)) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EINVAL;
		return NULL;
	}
	rpc_createerr.cf_stat = RPC_SUCCESS;
	memset(&hint, 0, sizeof(struct addrinfo));
	if (netid2socparms(proto, &hint.ai_family, &hint.ai_socktype, &hint.ai_protocol, 1)) {
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EAFNOSUPPORT;
		return NULL;
	}
	hint.ai_flags = AI_DEFAULT;
	if (port) {
		snprintf(portstr, sizeof(portstr), "%u", port);
		portstr[sizeof(portstr) - 1] = '\0';
		service = portstr;
		hint.ai_flags |= AI_NUMERICSERV;
	}
	gerror = getaddrinfo(host, service, &hint, &res);
	if (gerror) {
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  clnt_create_service_timed: getadderinfo returned %d:%s\n",
			    gerror, gai_strerror(gerror));
		}
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return NULL;
	}
	sock = RPC_ANYSOCK;
	switch (res->ai_protocol) {
	case IPPROTO_UDP:
	{
		struct timeval retry_timeout;
		uint64_t rto = (tmout->tv_sec * 1000000 + tmout->tv_usec) / 5;
		retry_timeout.tv_sec = rto / 10000000;
		if (retry_timeout.tv_sec > 1) {
			retry_timeout.tv_sec = 1;
			retry_timeout.tv_usec = 0;
		} else {
			retry_timeout.tv_usec = rto > 2000 ? rto % 10000000 : 2000;
		}
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  clntudp_bufcreate_timeout service = %s, timeout = %ld.%d, retry_timeout = %ld.%d\n",
			    service, tmout->tv_sec, tmout->tv_usec, retry_timeout.tv_sec, retry_timeout.tv_usec);
		}
		clnt = clntudp_bufcreate_timeout(res->ai_addr, prog, vers, &sock, UDPMSGSIZE, UDPMSGSIZE, &retry_timeout, tmout);
		freeaddrinfo(res);
		if (clnt == NULL) {
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1, "  clnt_create_service: %s", clnt_spcreateerror("UDP failed"));
			}
			return NULL;
		}
	}
	break;
	case IPPROTO_TCP:
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  clnttcp_create_timeout service = %s, timeout = %ld.%d\n",
			    service, tmout->tv_sec, tmout->tv_usec);
		}
		clnt = clnttcp_create_timeout(res->ai_addr, prog, vers, &sock, 0, 0, NULL, tmout);
		freeaddrinfo(res);
		if (clnt == NULL) {
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1, "  clnt_create_service: %s", clnt_spcreateerror("TCP failed"));
			}
			return NULL;
		}
		break;
	default:
		freeaddrinfo(res);
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = EPFNOSUPPORT;
		return NULL;
	}

	/*
	 * timeout should now reflect the time remaining after setting up the client handle.
	 */
	clnt_control(clnt, CLSET_TIMEOUT, tmout);
	/*
	 * Check if we can reach the server with this clnt handle
	 * Other clnt_create calls do a ping by contacting the
	 * remote rpcbind, here will just try to execute the service's
	 * NULL proc.
	 */

	rpc_createerr.cf_stat = clnt_call(clnt, NULLPROC,
	    (xdrproc_t)xdr_void, 0,
	    (xdrproc_t)xdr_void, 0, *tmout);

	if (rpc_createerr.cf_stat != RPC_SUCCESS) {
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  clnt_create_service_timed: %s", clnt_sperror(clnt, "ping failed"));
		}
		clnt_geterr(clnt, &rpc_createerr.cf_error);
		clnt_destroy(clnt);
		return NULL;
	}

	if (trace > PINGNFS_DEBUG_LEVEL) {
		trace_prt(1, "  clnt_create_service_timed: Succeeded for %s %s\n",
		    host, service);
	}
	return clnt;
}

/*
 * Sends a null call to the remote host's (NFS program, versp). versp
 * may be "NULL" in which case the default maximum version is used.
 * Upon return, versp contains the maximum version supported iff versp!= NULL.
 */
enum clnt_stat
pingnfs(
	const char *hostpart,
	rpcvers_t *versp,
	rpcvers_t versmin,
	ushort_t port,                  /* may be zeor */
	const char *path,
	const char *proto)
{
	CLIENT *cl = NULL;
	enum clnt_stat clnt_stat;
	rpcvers_t versmax;      /* maximum version to try against server */
	rpcvers_t outvers;      /* version supported by host on last call */
	rpcvers_t vers_to_try;  /* to try different versions against host */
	const char *hostname = hostpart;
	char *hostcopy = NULL;
	char *pathcopy = NULL;
	struct timeval tv;

	if (trace > PINGNFS_DEBUG_LEVEL - 1) {
		trace_prt(1, " pingnfs: enter %s vers = %d versmin = %d port = %d proto = %s, path = %s\n",
		    hostpart, versp ? *versp : -1, versmin, port,
		    proto ? proto : "NULL", path ? path : "NULL");
	}
	if (path != NULL && strcmp(hostname, "nfs") == 0 &&
	    strncmp(path, "//", 2) == 0) {
		char *sport;

		hostcopy = strdup(path + 2);

		if (hostcopy == NULL) {
			syslog(LOG_ERR, "pingnfs: memory allocation failed");
			return RPC_SYSTEMERROR;
		}

		pathcopy = strchr(hostcopy, '/');

		/*
		 * This cannot happen. If it does, give up
		 * on the ping as this is obviously a corrupt
		 * entry.
		 */
		if (pathcopy == NULL) {
			free(hostcopy);
			return RPC_SUCCESS;
		}

		/*
		 * Probable end point of host string.
		 */
		*pathcopy = '\0';

		sport = strchr(hostname, ':');

		if (sport != NULL && sport < pathcopy) {
			/*
			 * Actual end point of host string.
			 */
			*sport = '\0';
			port = htons((ushort_t)atoi(sport + 1));
		}
		hostname = hostcopy;
		path = pathcopy;
	}

	/* Pick up the default versions and then set them appropriately */
	if (versp) {
		versmax = *versp;
		/* use versmin passed in */
	} else {
		set_versrange(0, &versmax, &versmin);
	}

	if (proto &&
	    strcasecmp(proto, "udp") == 0 &&
	    versmax == NFS_VER4) {
		/*
		 * No V4-over-UDP for you.
		 */
		if (versmin == NFS_VER4) {
			if (versp) {
				*versp = versmax - 1;
				if (path != NULL && path == pathcopy) {
					free(pathcopy);
				}
				if (hostname != NULL && hostname == hostcopy) {
					free(hostcopy);
				}
				return RPC_SUCCESS;
			}
			if (path != NULL && path == pathcopy) {
				free(pathcopy);
			}
			if (hostname != NULL && hostname == hostcopy) {
				free(hostcopy);
			}
			return RPC_PROGUNAVAIL;
		} else {
			versmax--;
		}
	}

	if (versp) {
		*versp = versmax;
	}

	switch (cache_check(hostname, versp, proto, &tv)) {
	case GOODHOST:
		if (hostcopy != NULL) {
			free(hostcopy);
		}
		return RPC_SUCCESS;
	case DEADHOST:
		if (hostcopy != NULL) {
			free(hostcopy);
		}
		return RPC_TIMEDOUT;
	case NXHOST:
		if (hostcopy != NULL) {
			free(hostcopy);
		}
		return RPC_UNKNOWNHOST;
	case NOHOST:
	default:
		break;
	}

	vers_to_try = versmax;

	/*
	 * check the host's version within the timeout
	 */
	if (trace > PINGNFS_DEBUG_LEVEL) {
		trace_prt(1, "  ping: %s request vers=%d min=%d\n",
		    hostname, versmax, versmin);
	}

	do {
		outvers = vers_to_try;
		/*
		 * If NFSv4, we give the port number explicitly so that we
		 * avoid talking to the portmapper.
		 */
		if (vers_to_try == NFS_VER4) {
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1, "  pingnfs: Trying ping via TCP\n");
			}

			if ((cl = clnt_create_service_timed(hostname, "nfs",
			    NFS_PROG,
			    vers_to_try,
			    port, "tcp",
			    &tv))
			    != NULL) {
				outvers = vers_to_try;
				break;
			}
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1, "  pingnfs: Can't ping via TCP"
				    " %s: RPC error=%d\n",
				    hostname, rpc_createerr.cf_stat);
			}
		} else {
			const char *proto_to_try;

			proto_to_try =  (proto == NULL) ? "tcp" : proto;
			if (proto_to_try && (strcmp(proto_to_try, "tcp") == 0)) {
				/* Turn off portmap legacy behaviour, use TCP for the portmap call */
				rpc_control(RPC_PORTMAP_NETID_SET, NULL);
				if (trace > PINGNFS_DEBUG_LEVEL) {
					trace_prt(1, "  pingnfs: RPC_PORTMAP_NETID_SET is set to NULL");
				}
			}
			if ((cl = clnt_create_vers_timed(hostname, NFS_PROG,
			    &outvers, versmin, vers_to_try,
			    proto_to_try, &tv))
			    != NULL) {
				break;
			}
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1, "  pingnfs: Can't ping via %s"
				    " %s: RPC error=%d\n",
				    proto_to_try, hostname, rpc_createerr.cf_stat);
			}
			if (rpc_createerr.cf_stat == RPC_UNKNOWNHOST ||
			    rpc_createerr.cf_stat == RPC_TIMEDOUT) {
				break;
			}
			if (proto == NULL && rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED) {
				if (trace > PINGNFS_DEBUG_LEVEL) {
					trace_prt(1, "  pingnfs: Trying ping "
					    "via UDP\n");
				}
				if ((cl = clnt_create_vers_timed(hostname,
				    NFS_PROG, &outvers,
				    versmin, vers_to_try,
				    "udp", &tv)) != NULL) {
					break;
				}
				if (trace > PINGNFS_DEBUG_LEVEL) {
					trace_prt(1, "  pingnfs: Can't ping "
					    "via  %s: "
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
		if (vers_to_try < versmin) {
			break;
		}
		if (versp != NULL) {    /* recheck the cache */
			*versp = vers_to_try;
			if (trace > PINGNFS_DEBUG_LEVEL) {
				trace_prt(1,
				    "  pingnfs: check cache: vers=%d\n",
				    *versp);
			}
			switch (cache_check(hostname, versp, proto, &tv)) {
			case GOODHOST:
				if (hostcopy != NULL) {
					free(hostcopy);
				}
				return RPC_SUCCESS;
			case DEADHOST:
				if (hostcopy != NULL) {
					free(hostcopy);
				}
				return RPC_TIMEDOUT;
			case NXHOST:
				if (hostcopy != NULL) {
					free(hostcopy);
				}
				return RPC_UNKNOWNHOST;
			case NOHOST:
			default:
				break;
			}
		}
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  pingnfs: Try version=%d\n",
			    vers_to_try);
		}
	} while (cl == NULL);


	if (cl == NULL) {
		if (verbose) {
			syslog(LOG_ERR, "pingnfs: %s%s",
			    hostname, clnt_spcreateerror(""));
		}
		clnt_stat = rpc_createerr.cf_stat;
	} else {
		clnt_destroy(cl);
		clnt_stat = RPC_SUCCESS;
	}

	if (trace > PINGNFS_DEBUG_LEVEL) {
		clnt_stat == RPC_SUCCESS ?
		trace_prt(1, "	pingnfs OK: nfs version=%d\n", outvers):
		trace_prt(1, "	pingnfs FAIL: can't get nfs version\n");
	}

	switch (clnt_stat) {
	case RPC_SUCCESS:
		cache_enter(hostname, versmax, outvers, proto, GOODHOST);
		if (versp != NULL) {
			*versp = outvers;
		}
		break;

	case RPC_UNKNOWNHOST:
		cache_enter(hostname, versmax, versmax, proto, NXHOST);
		break;

	default:
		cache_enter(hostname, versmax, versmax, proto, DEADHOST);
		break;
	}

	if (hostcopy != NULL) {
		free(hostcopy);
	}

	return clnt_stat;
}

#ifdef HAVE_LOFS
#define MNTTYPE_LOFS    "lofs"

int
loopbackmount(char *fsname;             /* Directory being mounted */
    char *dir;                          /* Directory being mounted on */
    char *mntopts)
{
	struct mnttab mnt;
	int flags = 0;
	char fstype[] = MNTTYPE_LOFS;
	int dirlen;
	struct stat st;
	char optbuf[AUTOFS_MAXOPTSLEN];

	dirlen = strlen(dir);
	if (dir[dirlen - 1] == ' ') {
		dirlen--;
	}

	if (dirlen == strlen(fsname) &&
	    strncmp(fsname, dir, dirlen) == 0) {
		syslog(LOG_ERR,
		    "Mount of %s on %s would result in deadlock, aborted\n",
		    fsname, dir);
		return RET_ERR;
	}
	mnt.mnt_mntopts = mntopts;
	if (hasmntopt(&mnt, MNTOPT_RO) != NULL) {
		flags |= MS_RDONLY;
	}

	(void) strlcpy(optbuf, mntopts, sizeof(optbuf));

	if (trace > 1) {
		trace_prt(1,
		    "  loopbackmount: fsname=%s, dir=%s, flags=%d\n",
		    fsname, dir, flags);
	}

	if (mount(fsname, dir, flags | MS_DATA | MS_OPTIONSTR, fstype,
	    NULL, 0, optbuf, sizeof(optbuf)) < 0) {
		syslog(LOG_ERR, "Mount of %s on %s: %m", fsname, dir);
		return RET_ERR;
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

	return 0;
}
#endif

/*
 * Find cache entry matching host, vers, and proto.
 *
 * Assumes cache_lock is held for reading.
 */
static struct cache_entry *
cache_find(const char *host, rpcvers_t vers, const char *proto)
{
	struct cache_entry *ce, *prev;
	timenow = time(NULL);

	for (ce = cache_head; ce; ce = ce->cache_next) {
		if (timenow > ce->cache_max_time) {
			(void) pthread_rwlock_unlock(&cache_lock);
			(void) pthread_rwlock_wrlock(&cache_lock);
			for (prev = NULL, ce = cache_head; ce;
			    prev = ce, ce = ce->cache_next) {
				if (timenow > ce->cache_max_time) {
					if (trace > PINGNFS_DEBUG_LEVEL) {
						trace_prt(1, "  cache_find: removing entry for %s remaining = %ld (%ld)\n",
						    ce->cache_host, ce->cache_time - timenow, ce->cache_max_time - timenow);
					}
					cache_free(ce);
					if (prev) {
						prev->cache_next = NULL;
					} else {
						cache_head = NULL;
					}
					break;
				}
			}
			(void) pthread_rwlock_unlock(&cache_lock);
			(void) pthread_rwlock_rdlock(&cache_lock);
			return NULL;
		}
		if (strcmp(host, ce->cache_host) != 0) {
			continue;
		}
		if ((proto == NULL && ce->cache_proto != NULL) ||
		    (proto != NULL && ce->cache_proto == NULL)) {
			continue;
		}
		if (proto != NULL &&
		    strcmp(proto, ce->cache_proto) != 0) {
			continue;
		}

		if (vers == (rpcvers_t)-1 ||
		    (vers == ce->cache_reqvers) ||
		    (vers == ce->cache_outvers)) {
			break;
		}
	}
	return ce;
}

/*
 * Put a new entry in the cache chain by prepending it to the front.
 * If there isn't enough memory then just give up.
 */
static void
cache_enter(const char *host, rpcvers_t reqvers, rpcvers_t outvers, const char *proto, int state)
{
	struct cache_entry *entry;

	timenow = time(NULL);

	(void) pthread_rwlock_rdlock(&cache_lock);
	entry = cache_find(host, reqvers, proto);
	if (entry) {
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  cache_enter: FOUND entry state = %d, level = %d time = %ld remain = %ld\n",
			    entry->cache_state, entry->cache_level, entry->cache_time,
			    entry->cache_time - timenow);
		}
		entry->cache_state = state;
		entry->cache_level++;
		entry->cache_time = timenow + (entry_cache_time << entry->cache_level);
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1, "  cache_enter: UPDATED entry state = %d, level = %d time = %ld remain = %ld\n",
			    entry->cache_state, entry->cache_level, entry->cache_time,
			    entry->cache_time - timenow);
		}
		(void) pthread_rwlock_unlock(&cache_lock);
		return;
	}
	(void) pthread_rwlock_unlock(&cache_lock);
	entry = (struct cache_entry *)malloc(sizeof(struct cache_entry));
	if (entry == NULL) {
		return;
	}
	(void) memset((caddr_t)entry, 0, sizeof(struct cache_entry));
	entry->cache_host = strdup(host);
	if (entry->cache_host == NULL) {
		cache_free(entry);
		return;
	}
	entry->cache_reqvers = reqvers;
	entry->cache_outvers = outvers;
	entry->cache_proto = (proto == NULL ? NULL : strdup(proto));
	entry->cache_state = state;
	entry->cache_time = timenow + entry_cache_time;
	entry->cache_max_time = timenow + entry_cache_max_time;
	if (trace > PINGNFS_DEBUG_LEVEL) {
		trace_prt(1, "  cache_enter: NEW entry state = %d, level = %d time = %ld remain = %ld\n",
		    entry->cache_state, entry->cache_level, entry->cache_time,
		    entry->cache_time - timenow);
	}
	(void) pthread_rwlock_wrlock(&cache_lock);
#ifdef CACHE_DEBUG
	host_cache_accesses++;          /* up host cache access counter */
#endif /* CACHE DEBUG */
	entry->cache_next = cache_head;
	cache_head = entry;
	(void) pthread_rwlock_unlock(&cache_lock);
}

static int
cache_check(const char *host, rpcvers_t *versp, const char *proto, struct timeval *tv)
{
	int state = NOHOST;
	struct cache_entry *ce;
	rpcvers_t ver = versp ? *versp : -1;

	timenow = time(NULL);

	if (tv) {
		tv->tv_sec = entry_cache_time;
		tv->tv_usec = 0;
	}
	(void) pthread_rwlock_rdlock(&cache_lock);
#ifdef CACHE_DEBUG
	/* Increment the lookup and access counters for the host cache */
	host_cache_accesses++;
	host_cache_lookups++;
	if ((host_cache_lookups % 1000) == 0) {
		trace_host_cache();
	}
#endif /* CACHE DEBUG */

	ce = cache_find(host, versp ? *versp : -1, proto);
	if (ce) {
		if (tv) {
			tv->tv_sec = (entry_cache_time << ce->cache_level);
		}
		state = ce->cache_state;
		if (state != GOODHOST && timenow > ce->cache_time) {
			state = NOHOST;
		} else if (versp != NULL) {
			*versp = ce->cache_outvers;
		}
		if (trace > PINGNFS_DEBUG_LEVEL) {
			trace_prt(1,
			    "  cache_check: found cache entry tv = %ld, state = %d level = %d vers = %d\n",
			    tv->tv_sec, state, ce->cache_level, ver);
		}

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
		}
#endif /* CACHE_DEBUG */
	} else if (trace > PINGNFS_DEBUG_LEVEL) {
		trace_prt(1, "  cache_check: Cache miss\n");
	}
	(void) pthread_rwlock_unlock(&cache_lock);

	return state;
}

/*
 * Free a cache entry and all entries
 * further down the chain since they
 * will also be expired.
 */
static void
cache_free(struct cache_entry *entry)
{
	struct cache_entry *ce, *next = NULL;

	for (ce = entry; ce; ce = next) {
		if (ce->cache_host) {
			free(ce->cache_host);
		}
		if (ce->cache_proto) {
			free(ce->cache_proto);
		}
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
	flush_host_name_cache();
}

#ifdef HAVE_LOFS
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
		return 0;
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
			return 0;
		}
		if (nfs_port > USHRT_MAX) {
			syslog(LOG_ERR, "Invalid port number %ld in \"%s\"",
			    nfs_port, opts);
			freemntopts(mp);
			return 0;
		}
	}
	freemntopts(mp);

	/*
	 * if no port specified or it is same as NFS_PORT return nfs
	 * To use any other daemon the port number should be different
	 */
	if (!got_port || nfs_port == NFS_PORT) {
		return 1;
	}
#if 0
	/*
	 * If daemon is nfsd, return nfs
	 * XXX - we don't have getservbyport_r(), and it's not clear
	 * that this does anything useful - the only port that should
	 * map to "nfsd" is 2049, i.e. NFS_PORT.
	 */
	if (getservbyport_r(nfs_port, NULL, &sv, buf, 256) == &sv &&
	    strcmp(sv.s_name, "nfsd") == 0) {
		return 1;
	}
#endif

	/*
	 * daemon is not nfs
	 */
	return 0;
}
#endif


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
	case NFS_VER4:
		*vers = NFS_VER4;
		*versmin = NFS_VER4;
		break;
	case NFS_VER3:
		*vers = NFS_VER3;
		*versmin = NFS_VER3;
		break;
	case NFS_VER2:
		*vers = NFS_VER2;
		*versmin = NFS_VER2;
		break;
	default:
		return -1;
	}
	return 0;
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
