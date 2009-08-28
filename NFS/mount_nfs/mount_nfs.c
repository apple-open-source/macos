/*
 * Copyright (c) 1999-2009 Apple Inc.  All rights reserved.
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
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 */


#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <libutil.h>
#include <mntopts.h>

#define SIMON_SAYS_USE_NFSV4	"4.0alpha"

#define _PATH_NFS_CONF	"/etc/nfs.conf"
struct nfs_conf_client {
	int access_cache_timeout;
	int allow_async;
	int initialdowndelay;
	int iosize;
	int nextdowndelay;
	int nfsiod_thread_max;
	int statfs_rate_limit;
};
/* init to invalid values so we will only set values specified in nfs.conf */
struct nfs_conf_client config =
{
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
};

/* mount options */

/* values */
#define ALTF_ATTRCACHE_VAL	0x00000001
#define ALTF_DSIZE		0x00000002
#define ALTF_MAXGROUPS		0x00000004
#define ALTF_MOUNTPORT		0x00000008
#define ALTF_PORT		0x00000010
#define ALTF_PROTO		0x00000020
#define ALTF_READAHEAD		0x00000040
#define ALTF_RETRANS		0x00000080
#define ALTF_RETRYCNT		0x00000100
#define ALTF_RSIZE		0x00000200
#define ALTF_SEC		0x00000400
#define ALTF_TIMEO		0x00000800
#define ALTF_VERS		0x00001000
#define ALTF_VERS2		0x00002000 /* deprecated */
#define ALTF_VERS3		0x00004000 /* deprecated */
#define ALTF_VERS4		0x00008000 /* deprecated */
#define ALTF_WSIZE		0x00010000
#define ALTF_DEADTIMEOUT	0x00020000

/* switches */
#define ALTF_ATTRCACHE		0x00000001
#define ALTF_BG			0x00000002
#define ALTF_CONN		0x00000004
#define ALTF_DUMBTIMR		0x00000008
#define ALTF_HARD		0x00000010
#define ALTF_INTR		0x00000020
#define ALTF_LOCALLOCKS		0x00000040
#define ALTF_LOCKS		0x00000080
#define ALTF_MNTUDP		0x00000100
#define ALTF_NEGNAMECACHE	0x00000200
#define ALTF_RDIRPLUS		0x00000400
#define ALTF_RESVPORT		0x00000800
#define ALTF_SOFT		0x00001000
#define ALTF_TCP		0x00002000
#define ALTF_UDP		0x00004000
#define ALTF_MUTEJUKEBOX	0x00008000

/* standard and value-setting options */
struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	MOPT_ASYNC,
	MOPT_SYNC,
	{ "acdirmax", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acdirmin", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acregmax", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acregmin", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "actimeo", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "dsize", 0, ALTF_DSIZE, 1 },
	{ "maxgroups", 0, ALTF_MAXGROUPS, 1 },
	{ "mountport", 0, ALTF_MOUNTPORT, 1 },
	{ "port", 0, ALTF_PORT, 1 },
	{ "proto", 0, ALTF_PROTO, 1 },
	{ "readahead", 0, ALTF_READAHEAD, 1 },
	{ "retrans", 0, ALTF_RETRANS, 1 },
	{ "retrycnt", 0, ALTF_RETRYCNT, 1 },
	{ "rsize", 0, ALTF_RSIZE, 1 },
	{ "rwsize", 0, ALTF_RSIZE|ALTF_WSIZE, 1 },
	{ "sec", 0, ALTF_SEC, 1 },
	{ "timeo", 0, ALTF_TIMEO, 1 },
	{ "vers", 0, ALTF_VERS, 1 },
	{ "wsize", 0, ALTF_WSIZE, 1 },
	{ "deadtimeout", 0, ALTF_DEADTIMEOUT, 1 },
	/**/
	{ "nfsv2", 0, ALTF_VERS2, 1 }, /* deprecated, use vers=# */
	{ "nfsv3", 0, ALTF_VERS3, 1 }, /* deprecated, use vers=# */
	{ "nfsv4", 0, ALTF_VERS4, 1 }, /* deprecated, use vers=# */
	{ "nfsvers", 0, ALTF_VERS, 1 }, /* deprecated, use vers=# */
	{ NULL }
};
/* on/off switching options */
struct mntopt mopts_switches[] = {
	{ "ac", 0, ALTF_ATTRCACHE, 1 },
	{ "bg", 0, ALTF_BG, 1 },
	{ "conn", 0, ALTF_CONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "hard", 0, ALTF_HARD, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
	{ "locallocks", 0, ALTF_LOCALLOCKS, 1 },
	{ "lock", 0, ALTF_LOCKS, 1 },
	{ "lockd", 0, ALTF_LOCKS, 1 },
	{ "locks", 0, ALTF_LOCKS, 1 },
	{ "mntudp", 0, ALTF_MNTUDP, 1 },
	{ "mutejukebox", 0, ALTF_MUTEJUKEBOX, 1 },
	{ "negnamecache", 0, ALTF_NEGNAMECACHE, 1 },
	{ "nlm", 0, ALTF_LOCKS, 1 },
	{ "rdirplus", 0, ALTF_RDIRPLUS, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ "udp", 0, ALTF_UDP, 1 },
	{ NULL }
};
/* inverse of mopts_switches (set up at runtime) */
struct mntopt *mopts_switches_no = NULL;

/* default NFS mount args */
struct nfs_args nfsdefargs = {
	NFS_ARGSVERSION,			/* struct version */
	NULL,					/* server address */
	sizeof (struct sockaddr_in),		/* server address size */
	SOCK_STREAM,				/* socket type */
	0,					/* socket protocol */
	NULL,					/* file handle */
	0,					/* file handle size */
	NFSMNT_NFSV3,				/* NFS mount flags */
	NFS_WSIZE,				/* write size */
	NFS_RSIZE,				/* read size */
	NFS_READDIRSIZE,			/* directory read size */
	10,					/* initial timeout */
	NFS_RETRANS,				/* times to retry send */
	NFS_MAXGRPS,				/* max size of group list */
	NFS_DEFRAHEAD,				/* #blocks of readahead to do */
	0,					/* obsolete */
	0,					/* obsolete */
	NULL,					/* server's name (mntfromname) */
	NFS_MINATTRTIMO,			/* reg file min attr cache timeout */
	NFS_MAXATTRTIMO,			/* reg file max attr cache timeout */
	NFS_MINDIRATTRTIMO,			/* dir min attr cache timeout */
	NFS_MAXDIRATTRTIMO,			/* dir max attr cache timeout */
	0,					/* security mechanism flavor */
	0					/* dead timeout */
};

int nfsproto = IPPROTO_TCP;
int forceproto = 0;
int forceport = 0;
int mountport = 0;
int mnttcp_ok = 1;
int force2 = 0;
int force3 = 0;
int force4 = 0;
int nolocallocks = 0;

#define PMAP_TIMEOUT_SHORT	10		// set when retrycnt=0 (automounter)
#define PMAP_TIMEOUT_LONG	60
int pmap_timeout = PMAP_TIMEOUT_LONG;		// seconds

/* flags controlling mount_nfs behavior */
#define BGRND		0x1
#define ISBGRND		0x2
int opflags = 0;
int verbose = 0;

#define DEF_RETRY_BG	10000
#define DEF_RETRY_FG	1
int retrycnt = -1;

uid_t real_uid, eff_uid;

/* mount options */
int mntflags = 0;
struct nfs_args nfsargs, *nfsargsp = &nfsargs;

#ifdef __LP64__
typedef unsigned int	xdr_ulong_t;
typedef int		xdr_long_t;
#else
typedef unsigned long	xdr_ulong_t;
typedef long		xdr_long_t;
#endif

struct nfhret {
	xdr_ulong_t	stat;
	xdr_long_t	vers;
	xdr_long_t	auth;
	bool_t		sysok;
	xdr_long_t	fhsize;
	uint8_t		nfh[NFSX_V3FHMAX];
};


/* function prototypes */
void	checkNFSVersionOptions(void);
void	dump_mount_options(char *);
void	handle_mntopts(char *);
void	warn_badoptions(char *);
int	config_read(struct nfs_conf_client *);
void	config_sysctl(void);
int	pmap_getport_timely(struct sockaddr_in *, int, int, int, int);
int	getnfsargs(char *, struct nfs_args *);
void	set_rpc_maxgrouplist(int);
__dead	void usage(void);
int	xdr_dir(XDR *, char *);
int	xdr_fh(XDR *, struct nfhret *);


int
main(int argc, char *argv[])
{
	uint i;
	int c, num, rv;
	char name[MAXPATHLEN], *p, *spec;

	/* drop setuid root privs asap */
	eff_uid = geteuid();
	real_uid = getuid();
	seteuid(real_uid); 

	/* set up mopts_switches_no from mopts_switches */
	mopts_switches_no = malloc(sizeof(mopts_switches));
	if (!mopts_switches_no)
		errx(ENOMEM, "memory allocation failed");
	bcopy(mopts_switches, mopts_switches_no, sizeof(mopts_switches));
	for (i=0; i < sizeof(mopts_switches)/sizeof(struct mntopt); i++)
		mopts_switches_no[i].m_inverse = (mopts_switches[i].m_inverse == 0);

	/* set up defaults and process options */
	nfsargs = nfsdefargs;
	config_read(&config);

	while ((c = getopt(argc, argv, "234a:bcdg:I:iLlo:PR:r:sTt:Uvw:x:")) != EOF)
		switch (c) {
		case '4':
			errx(EPERM, "sorry, you must specify vers=%s to use the alpha-quality NFSv4 support", SIMON_SAYS_USE_NFSV4);
			force4 = 1;
			checkNFSVersionOptions();
			break;
		case '3':
			force3 = 1;
			checkNFSVersionOptions();
			break;
		case '2':
			force2 = 1;
			checkNFSVersionOptions();
			nfsargsp->flags &= ~NFSMNT_NFSV3;
			break;
		case 'a':
			num = strtol(optarg, &p, 10);
			if (*p || num < 0) {
				warnx("illegal -a value -- %s", optarg);
			} else {
				nfsargsp->readahead = num;
				nfsargsp->flags |= NFSMNT_READAHEAD;
			}
			break;
		case 'b':
			opflags |= BGRND;
			break;
		case 'c':
			nfsargsp->flags |= NFSMNT_NOCONN;
			break;
		case 'd':
			nfsargsp->flags |= NFSMNT_DUMBTIMR;
			break;
		case 'g':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0) {
				warnx("illegal maxgroups value -- %s", optarg);
			} else {
				set_rpc_maxgrouplist(num);
				nfsargsp->maxgrouplist = num;
				nfsargsp->flags |= NFSMNT_MAXGRPS;
			}
			break;
		case 'I':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0) {
				warnx("illegal -I value -- %s", optarg);
			} else {
				nfsargsp->readdirsize = num;
				nfsargsp->flags |= NFSMNT_READDIRSIZE;
			}
			break;
		case 'i':
			nfsargsp->flags |= NFSMNT_INT;
			break;
		case 'L':
			nfsargsp->flags |= NFSMNT_NOLOCKS;
			nfsargsp->flags &= ~NFSMNT_LOCALLOCKS;
			break;
		case 'l':
			nfsargsp->flags |= NFSMNT_RDIRPLUS;
			break;
		case 'o':
			handle_mntopts(optarg);
			break;
		case 'P':
			nfsargsp->flags |= NFSMNT_RESVPORT;
			break;
		case 'R':
			num = strtol(optarg, &p, 10);
			if (*p)
				warnx("illegal -R value -- %s", optarg);
			else {
				retrycnt = num;
				if (retrycnt == 0)
					pmap_timeout = PMAP_TIMEOUT_SHORT;
			}
			break;
		case 'r':
			num = strtol(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0) {
				warnx("illegal -r value -- %s", optarg);
			} else {
				nfsargsp->rsize = num;
				nfsargsp->flags |= NFSMNT_RSIZE;
			}
			break;
		case 's':
			nfsargsp->flags |= NFSMNT_SOFT;
			break;
		case 'T':
			nfsargsp->sotype = SOCK_STREAM;
			nfsproto = IPPROTO_TCP;
			forceproto++;
			break;
		case 't':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0) {
				warnx("illegal -t value -- %s", optarg);
			} else {
				nfsargsp->timeo = num;
				nfsargsp->flags |= NFSMNT_TIMEO;
			}
			break;
		case 'w':
			num = strtol(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0) {
				warnx("illegal -w value -- %s", optarg);
			} else {
				nfsargsp->wsize = num;
				nfsargsp->flags |= NFSMNT_WSIZE;
			}
			break;
		case 'x':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0) {
				warnx("illegal -x value -- %s", optarg);
			} else {
				nfsargsp->retrans = num;
				nfsargsp->flags |= NFSMNT_RETRANS;
			}
			break;
		case 'U':
			mnttcp_ok = 0;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	/* soft, read-only mount implies ... */
	if ((nfsargsp->flags & NFSMNT_SOFT) && (mntflags & MNT_RDONLY)) {
		/* ...use of local locks */
		if (!(nfsargsp->flags & NFSMNT_NOLOCKS) && !nolocallocks)
			nfsargsp->flags |= NFSMNT_LOCALLOCKS;
		/* ... dead timeout */
		if (!(nfsargsp->flags & NFSMNT_DEADTIMEOUT)) {
			nfsargsp->flags |= NFSMNT_DEADTIMEOUT;
			nfsargsp->deadtimeout = 60;
		}
	}

	if ((argc == 1) && !strcmp(argv[0], "configupdate")) {
		config_sysctl();
		exit(0);
	}

	if (argc != 2)
		usage();

	spec = *argv++;

	if (realpath(*argv, name) == NULL)
		err(errno ? errno : 1, "realpath %s", name);

	if ((rv = getnfsargs(spec, nfsargsp)))
		exit(rv);

	config_sysctl();

	if (verbose)
		dump_mount_options(name);

	if (mount("nfs", name, mntflags, nfsargsp))
		err(errno, "%s", name);
	exit(0);
}

void
checkNFSVersionOptions(void)
{
	if ((force2 + force3 + force4) > 1)
		errx(EINVAL,"conflicting NFS version options:%s%s%s",
			force2 ? " v2" : "", force3 ? " v3" : "",
			force4 ? " v4" : "");
}

void
handle_mntopts(char *opts)
{
	mntoptparse_t mop;
	char *p;
	const char *p2;
	int num, altflags = 0, dummyflags;

	getmnt_silent = 1;

	/* do standard and value-setting options first */
	altflags = 0;
	mop = getmntopts(opts, mopts, &mntflags, &altflags);
	if (mop == NULL)
		errx(EINVAL, "getmntops failed: %s", opts);

	if (altflags & ALTF_ATTRCACHE_VAL) {
		if ((p2 = getmntoptstr(mop, "actimeo"))) {
			num = getmntoptnum(mop, "actimeo");
			if (num < 0) {
				warnx("illegal actimeo value -- %s", p2);
			} else {
				nfsargsp->acregmin = num;
				nfsargsp->acregmax = nfsargsp->acregmin;
				nfsargsp->acdirmin = nfsargsp->acregmin;
				nfsargsp->acdirmax = nfsargsp->acregmin;
				nfsargsp->flags |= NFSMNT_ACREGMIN;
				nfsargsp->flags |= NFSMNT_ACREGMAX;
				nfsargsp->flags |= NFSMNT_ACDIRMIN;
				nfsargsp->flags |= NFSMNT_ACDIRMAX;
			}
		}
		if ((p2 = getmntoptstr(mop, "acregmin"))) {
			num = getmntoptnum(mop, "acregmin");
			if (num < 0) {
				warnx("illegal acregmin value -- %s", p2);
			} else {
				nfsargsp->acregmin = num;
				nfsargsp->flags |= NFSMNT_ACREGMIN;
			}
		}
		if ((p2 = getmntoptstr(mop, "acregmax"))) {
			num = getmntoptnum(mop, "acregmax");
			if (num < 0) {
				warnx("illegal acregmax value -- %s", p2);
			} else {
				nfsargsp->acregmax = num;
				nfsargsp->flags |= NFSMNT_ACREGMAX;
			}
		}
		if ((p2 = getmntoptstr(mop, "acdirmin"))) {
			num = getmntoptnum(mop, "acdirmin");
			if (num < 0) {
				warnx("illegal acdirmin value -- %s", p2);
			} else {
				nfsargsp->acdirmin = num;
				nfsargsp->flags |= NFSMNT_ACDIRMIN;
			}
		}
		if ((p2 = getmntoptstr(mop, "acdirmax"))) {
			num = getmntoptnum(mop, "acdirmax");
			if (num < 0) {
				warnx("illegal acdirmax value -- %s", p2);
			} else {
				nfsargsp->acdirmax = num;
				nfsargsp->flags |= NFSMNT_ACDIRMAX;
			}
		}
	}
	if (altflags & ALTF_DEADTIMEOUT) {
		if ((p2 = getmntoptstr(mop, "deadtimeout"))) {
			num = strtol(p2, &p, 10);
			if (num < 0) {
				warnx("illegal deadtimeout value -- %s", p2);
			} else {
				nfsargsp->deadtimeout = num;
				nfsargsp->flags |= NFSMNT_DEADTIMEOUT;
			}
		}
	}
	if (altflags & ALTF_DSIZE) {
		if ((p2 = getmntoptstr(mop, "dsize"))) {
			num = strtol(p2, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if ((num <= 0) || *p) {
				warnx("illegal dsize value -- %s", p2);
			} else {
				nfsargsp->readdirsize = num;
				nfsargsp->flags |= NFSMNT_READDIRSIZE;
			}
		}
	}
	if (altflags & ALTF_MAXGROUPS) {
		num = getmntoptnum(mop, "maxgroups");
		if (num <= 0) {
			warnx("illegal maxgroups value -- %s", getmntoptstr(mop, "maxgroups"));
		} else {
			set_rpc_maxgrouplist(num);
			nfsargsp->maxgrouplist = num;
			nfsargsp->flags |= NFSMNT_MAXGRPS;
		}
	}
	if (altflags & ALTF_MOUNTPORT) {
		num = getmntoptnum(mop, "mountport");
		if (num < 0)
			warnx("illegal mountport number -- %s", getmntoptstr(mop, "mountport"));
		else
			mountport = num;
	}
	if (altflags & ALTF_PORT) {
		num = getmntoptnum(mop, "port");
		if (num < 0)
			warnx("illegal port number -- %s", getmntoptstr(mop, "port"));
		else
			forceport = num;
	}
	if (altflags & ALTF_PROTO) {
		p2 = getmntoptstr(mop, "proto");
		if (p2) {
			if (!strcasecmp(p2, "tcp")) {
				nfsargsp->sotype = SOCK_STREAM;
				nfsproto = IPPROTO_TCP;
				forceproto++;
			} else if (!strcasecmp(p2, "udp")) {
				nfsargsp->sotype = SOCK_DGRAM;
				nfsproto = IPPROTO_UDP;
				forceproto++;
			} else {
				warnx("unknown protocol -- %s", p2);
			}
		}
	}
	if (altflags & ALTF_READAHEAD) {
		num = getmntoptnum(mop, "readahead");
		if (num < 0) {
			warnx("illegal readahead value -- %s", getmntoptstr(mop, "readahead"));
		} else {
			nfsargsp->readahead = num;
			nfsargsp->flags |= NFSMNT_READAHEAD;
		}
	}
	if (altflags & ALTF_RETRANS) {
		num = getmntoptnum(mop, "retrans");
		if (num <= 0) {
			warnx("illegal retrans value -- %s", getmntoptstr(mop, "retrans"));
		} else {
			nfsargsp->retrans = num;
			nfsargsp->flags |= NFSMNT_RETRANS;
		}
	}
	if (altflags & ALTF_RETRYCNT) {
		retrycnt = getmntoptnum(mop, "retrycnt");
		if (retrycnt == 0)
			pmap_timeout = PMAP_TIMEOUT_SHORT;
	}
	if (altflags & ALTF_RSIZE) {
		p2 = getmntoptstr(mop, "rwsize");
		if (!p2)
			p2 = getmntoptstr(mop, "rsize");
		if (p2) {
			num = strtol(p2, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if ((num <= 0) || *p) {
				warnx("illegal rsize value -- %s", p2);
			} else {
				nfsargsp->rsize = num;
				nfsargsp->flags |= NFSMNT_RSIZE;
			}
		}
	}
	if (altflags & ALTF_SEC) {
		p2 = getmntoptstr(mop, "sec");
		if (!p2)
			warnx("missing security value for sec= option");
		else if (!strcmp("krb5p", p2))
			nfsargsp->auth = RPCAUTH_KRB5P;
		else if (!strcmp("krb5i", p2))
			nfsargsp->auth = RPCAUTH_KRB5I;
		else if (!strcmp("krb5", p2))
			nfsargsp->auth = RPCAUTH_KRB5;
		else if (!strcmp("sys", p2))
			nfsargsp->auth = RPCAUTH_SYS;
		else
			warnx("illegal security value -- %s", p2);

		if (nfsargsp->auth == RPCAUTH_SYS)
			nfsargsp->flags |= NFSMNT_SECSYSOK;
	}
	if (altflags & ALTF_TIMEO) {
		num = getmntoptnum(mop, "timeo");
		if (num <= 0) {
			warnx("illegal timeout value -- %s", getmntoptstr(mop, "timeo"));
		} else {
			nfsargsp->timeo = num;
			nfsargsp->flags |= NFSMNT_TIMEO;
		}
	}
	if (altflags & ALTF_VERS) {
		if ((p2 = getmntoptstr(mop, "vers")))
			num = getmntoptnum(mop, "vers");
		else if ((p2 = getmntoptstr(mop, "nfsvers")))
			num = getmntoptnum(mop, "nfsvers");
		if (p2) {
			if (!strcmp(p2, SIMON_SAYS_USE_NFSV4))
				num = 4;
			switch (num) {
			case 2:
				force2 = 1;
				nfsargsp->flags &= ~NFSMNT_NFSV3;
				nfsargsp->flags &= ~NFSMNT_NFSV4;
				break;
			case 3:
				force3 = 1;
				nfsargsp->flags |= NFSMNT_NFSV3;
				nfsargsp->flags &= ~NFSMNT_NFSV4;
				break;
			case 4:
				if (strcmp(p2, SIMON_SAYS_USE_NFSV4))
					errx(EPERM, "sorry, you must specify vers=%s to use the alpha-quality NFSv4 support", SIMON_SAYS_USE_NFSV4);
				force4 = 1;
				nfsargsp->flags &= ~NFSMNT_NFSV3;
				nfsargsp->flags |= NFSMNT_NFSV4;
				break;
			default:
				warnx("illegal NFS version value -- %s", p2);
				break;
			}
		}
	}
	if (altflags & ALTF_VERS2) {
		warnx("option nfsv2 deprecated, use vers=#");
		force2 = 1;
		nfsargsp->flags &= ~NFSMNT_NFSV3;
		nfsargsp->flags &= ~NFSMNT_NFSV4;
		checkNFSVersionOptions();
	}
	if (altflags & ALTF_VERS3) {
		warnx("option nfsv3 deprecated, use vers=#");
		force3 = 1;
		nfsargsp->flags |= NFSMNT_NFSV3;
		nfsargsp->flags &= ~NFSMNT_NFSV4;
		checkNFSVersionOptions();
	}
	if (altflags & ALTF_VERS4) {
		warnx("option nfsv4 deprecated, use vers=#");
		errx(EPERM, "sorry, you must specify vers=%s to use the alpha-quality NFSv4 support", SIMON_SAYS_USE_NFSV4);
		force4 = 1;
		nfsargsp->flags &= ~NFSMNT_NFSV3;
		nfsargsp->flags |= NFSMNT_NFSV4;
		checkNFSVersionOptions();
	}
	if (altflags & ALTF_WSIZE) {
		p2 = getmntoptstr(mop, "rwsize");
		if (!p2)
			p2 = getmntoptstr(mop, "wsize");
		if (p2) {
			num = strtol(p2, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if ((num <= 0) || *p) {
				warnx("illegal wsize value -- %s", p2);
			} else {
				nfsargsp->wsize = num;
				nfsargsp->flags |= NFSMNT_WSIZE;
			}
		}
	}
	freemntopts(mop);

	/* next do positive form of switch options */
	altflags = 0;
	mop = getmntopts(opts, mopts_switches, &dummyflags, &altflags);
	if (mop == NULL)
		errx(EINVAL, "getmntops failed: %s", opts);
	if (verbose > 2)
		printf("altflags=0x%x\n", altflags);

	if (altflags & ALTF_ATTRCACHE) {
		nfsargsp->acregmin = nfsdefargs.acregmin;
		nfsargsp->acregmax = nfsdefargs.acregmax;
		nfsargsp->acdirmin = nfsdefargs.acdirmin;
		nfsargsp->acdirmax = nfsdefargs.acdirmax;
		nfsargsp->flags &= ~NFSMNT_ACREGMIN;
		nfsargsp->flags &= ~NFSMNT_ACREGMAX;
		nfsargsp->flags &= ~NFSMNT_ACDIRMIN;
		nfsargsp->flags &= ~NFSMNT_ACDIRMAX;
	}
	if (altflags & ALTF_BG)
		opflags |= BGRND;
	if (altflags & ALTF_CONN)
		nfsargsp->flags &= ~NFSMNT_NOCONN;
	if (altflags & ALTF_DUMBTIMR)
		nfsargsp->flags |= NFSMNT_DUMBTIMR;
	if (altflags & ALTF_HARD)
		nfsargsp->flags &= ~NFSMNT_SOFT;
	if (altflags & ALTF_INTR)
		nfsargsp->flags |= NFSMNT_INT;
	if (altflags & ALTF_LOCALLOCKS) {
		nfsargsp->flags |= NFSMNT_LOCALLOCKS;
		nfsargsp->flags &= ~NFSMNT_NOLOCKS;
	}
	if (altflags & ALTF_LOCKS)
		nfsargsp->flags &= ~NFSMNT_NOLOCKS;
	if (altflags & ALTF_MNTUDP)
		mnttcp_ok = 0;
	if (altflags & ALTF_MUTEJUKEBOX)
		nfsargsp->flags |= NFSMNT_MUTEJUKEBOX;
	if (altflags & ALTF_NEGNAMECACHE)
		nfsargsp->flags &= ~NFSMNT_NONEGNAMECACHE;
	if (altflags & ALTF_RDIRPLUS)
		nfsargsp->flags |= NFSMNT_RDIRPLUS;
	if (altflags & ALTF_RESVPORT)
		nfsargsp->flags |= NFSMNT_RESVPORT;
	if (altflags & ALTF_SOFT)
		nfsargsp->flags |= NFSMNT_SOFT;
	if (altflags & ALTF_TCP) {
		nfsargsp->sotype = SOCK_STREAM;
		nfsproto = IPPROTO_TCP;
		forceproto++;
	}
	if (altflags & ALTF_UDP) {
		nfsargsp->sotype = SOCK_DGRAM;
		nfsproto = IPPROTO_UDP;
		forceproto++;
	}
	freemntopts(mop);

	/* finally do negative form of switch options */
	altflags = 0;
	mop = getmntopts(opts, mopts_switches_no, &dummyflags, &altflags);
	if (mop == NULL)
		errx(EINVAL, "getmntops failed: %s", opts);
	if (verbose > 2)
		printf("altflags=0x%x\n", altflags);

	if (altflags & ALTF_ATTRCACHE) {
		nfsargsp->acregmin = 0;
		nfsargsp->acregmax = 0;
		nfsargsp->acdirmin = 0;
		nfsargsp->acdirmax = 0;
		nfsargsp->flags |= NFSMNT_ACREGMIN;
		nfsargsp->flags |= NFSMNT_ACREGMAX;
		nfsargsp->flags |= NFSMNT_ACDIRMIN;
		nfsargsp->flags |= NFSMNT_ACDIRMAX;
	}
	if (altflags & ALTF_BG)
		opflags &= ~BGRND;
	if (altflags & ALTF_CONN)
		nfsargsp->flags |= NFSMNT_NOCONN;
	if (altflags & ALTF_DUMBTIMR)
		nfsargsp->flags &= ~NFSMNT_DUMBTIMR;
	if (altflags & ALTF_HARD)
		nfsargsp->flags |= NFSMNT_SOFT;
	if (altflags & ALTF_INTR)
		nfsargsp->flags &= ~NFSMNT_INT;
	if (altflags & ALTF_LOCALLOCKS) {
		nfsargsp->flags &= ~NFSMNT_LOCALLOCKS;
		nolocallocks = 1;
	}
	if (altflags & ALTF_LOCKS) {
		nfsargsp->flags &= ~NFSMNT_LOCALLOCKS;
		nfsargsp->flags |= NFSMNT_NOLOCKS;
	}
	if (altflags & ALTF_MNTUDP)
		mnttcp_ok = 1;
	if (altflags & ALTF_MUTEJUKEBOX)
		nfsargsp->flags &= ~NFSMNT_MUTEJUKEBOX;
	if (altflags & ALTF_NEGNAMECACHE)
		nfsargsp->flags |= NFSMNT_NONEGNAMECACHE;
	if (altflags & ALTF_RDIRPLUS)
		nfsargsp->flags &= ~NFSMNT_RDIRPLUS;
	if (altflags & ALTF_RESVPORT)
		nfsargsp->flags &= ~NFSMNT_RESVPORT;
	if (altflags & ALTF_SOFT)
		nfsargsp->flags &= ~NFSMNT_SOFT;
	if (altflags & ALTF_TCP) {
		/* notcp = udp */
		nfsargsp->sotype = SOCK_DGRAM;
		nfsproto = IPPROTO_UDP;
		forceproto++;
	}
	if (altflags & ALTF_UDP) {
		/* noudp = tcp */
		nfsargsp->sotype = SOCK_STREAM;
		nfsproto = IPPROTO_TCP;
		forceproto++;
	}
	freemntopts(mop);

	warn_badoptions(opts);
}

/*
 * Compare the supplied option string with the lists
 * of known mount options and issue a warning for any
 * unknown options.
 */
void
warn_badoptions(char *opts)
{
	char *p, *opt, *saveopt, *myopts;
	struct mntopt *k, *known;

	myopts = strdup(opts);
	if (myopts == NULL)
		return;

	for (opt = myopts; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		saveopt = opt;
		if (strncmp(opt, "no", 2) == 0)	// negative option
			opt += 2;
		p = strchr(opt, '=');		// value option
                if (p)
                         *p = '\0';

		known = &mopts[0];
		for (k = known; k->m_option != NULL; ++k)
                        if (strcasecmp(opt, k->m_option) == 0)
                                break;
		if (k->m_option != NULL)	// known option
			continue;

		known = &mopts_switches[0];
		for (k = known; k->m_option != NULL; ++k)
                        if (strcasecmp(opt, k->m_option) == 0)
                                break;
		if (k->m_option != NULL)	// known option
			continue;

		warnx("warning: option \"%s\" not known", saveopt);	
	}

	free(myopts);
}

/*
 * read the NFS client values from nfs.conf
 */
int
config_read(struct nfs_conf_client *conf)
{
	FILE *f;
	size_t len, linenum = 0;
	char *line, *p, *key, *value;
	long val;

	if (!(f = fopen(_PATH_NFS_CONF, "r"))) {
		if (errno != ENOENT)
			warn("%s", _PATH_NFS_CONF);
		return (1);
	}
	for (; (line = fparseln(f, &len, &linenum, NULL, 0)); free(line)) {
		if (len <= 0)
			continue;
		/* trim trailing whitespace */
		p = line + len - 1;
		while ((p > line) && isspace(*p))
			*p-- = '\0';
		/* find key start */
		key = line;
		while (isspace(*key))
			key++;
		/* find equals/value */
		value = p = strchr(line, '=');
		if (p) /* trim trailing whitespace on key */
			do { *p-- = '\0'; } while ((p > line) && isspace(*p));
		/* find value start */
		if (value)
			do { value++; } while (isspace(*value));

		/* all client keys start with "nfs.client." */
		if (strncmp(key, "nfs.client.", 11)) {
			if (verbose)
				printf("%4ld %s=%s\n", linenum, key, value ? value : "");
			continue;
		}
		val = !value ? 1 : strtol(value, NULL, 0);
		if (verbose)
			printf("%4ld %s=%s (%ld)\n", linenum, key, value ? value : "", val);

		if (!strcmp(key, "nfs.client.access_cache_timeout")) {
			conf->access_cache_timeout = val;
		} else if (!strcmp(key, "nfs.client.allow_async")) {
			conf->allow_async = val;
		} else if (!strcmp(key, "nfs.client.initialdowndelay")) {
			conf->initialdowndelay = val;
		} else if (!strcmp(key, "nfs.client.iosize")) {
			conf->iosize = val;
		} else if (!strcmp(key, "nfs.client.mount.options")) {
			handle_mntopts(value);
		} else if (!strcmp(key, "nfs.client.nextdowndelay")) {
			conf->nextdowndelay = val;
		} else if (!strcmp(key, "nfs.client.nfsiod_thread_max")) {
			conf->nfsiod_thread_max = val;
		} else if (!strcmp(key, "nfs.client.statfs_rate_limit")) {
			conf->statfs_rate_limit = val;
		} else {
			if (verbose)
				printf("ignoring unknown config value: %4ld %s=%s\n", linenum, key, value ? value : "");
		}

	}

	fclose(f);
	return (0);
}

/* set a sysctl config value */
int
sysctl_set(const char *name, int val)
{
	return (sysctlbyname(name, NULL, 0, &val, sizeof(val)));
}

void
config_sysctl(void)
{
	seteuid(eff_uid); /* must be root to do sysctls */
	if (config.access_cache_timeout != -1)
		sysctl_set("vfs.generic.nfs.client.access_cache_timeout", config.access_cache_timeout);
	if (config.allow_async != -1)
		sysctl_set("vfs.generic.nfs.client.allow_async", config.allow_async);
	if (config.initialdowndelay != -1)
		sysctl_set("vfs.generic.nfs.client.initialdowndelay", config.initialdowndelay);
	if (config.iosize != -1)
		sysctl_set("vfs.generic.nfs.client.iosize", config.iosize);
	if (config.nextdowndelay != -1)
		sysctl_set("vfs.generic.nfs.client.nextdowndelay", config.nextdowndelay);
	if (config.nfsiod_thread_max != -1)
		sysctl_set("vfs.generic.nfs.client.nfsiod_thread_max", config.nfsiod_thread_max);
	if (config.statfs_rate_limit != -1)
		sysctl_set("vfs.generic.nfs.client.statfs_rate_limit", config.statfs_rate_limit);
	seteuid(real_uid);
}

/*
 * A substitute for the pmap_getport function in the RPC library.
 * It makes an RPC call to the portmapper at addr to retrieve the
 * port number for the service corresponding to prog, vers and prot.
 * This function has an additional timeout arg that allows the caller
 * to specify a timeout if the portmapper doesn't respond.
 */
int
pmap_getport_timely(struct sockaddr_in *addr, int prog, int vers, int prot, int timeout)
{
	CLIENT *clnt;
	struct pmap args = { prog, vers, prot, 0 };
	u_short port = 0;
	struct timeval tv;
	int sock = RPC_ANYSOCK;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	addr->sin_port = htons(PMAPPORT);
	clnt = clntudp_bufcreate(addr, PMAPPROG, PMAPVERS, tv,
		&sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (clnt != NULL) {
		if (clnt_call(clnt, PMAPPROC_GETPORT, (xdrproc_t) xdr_pmap, &args,
		    (xdrproc_t) xdr_u_short, &port, tv) != RPC_SUCCESS) {
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(clnt, &rpc_createerr.cf_error);
		} else if (port == 0) {
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
		clnt_destroy(clnt);
	}

	(void) close(sock);

	return (port);
}

int
getnfsargs(char *spec, struct nfs_args *nfsargsp)
{
	CLIENT *clp;
	struct hostent *hp;
	static struct sockaddr_in saddr;
	struct timeval pertry, try;
	enum clnt_stat clnt_stat;
	int so = RPC_ANYSOCK, nfsvers, mntvers, retry;
	char *hostp, *delimp;
	u_short tport = 0;
	static struct nfhret nfhret;
	static char nam[MAXPATHLEN];

	strlcpy(nam, spec, MAXPATHLEN);
	if ((delimp = strchr(spec, '@')) != NULL) {
		hostp = delimp + 1;
	} else if ((delimp = strchr(spec, ':')) != NULL) {
		hostp = spec;
		spec = delimp + 1;
	} else {
		warnx("no <host>:<dirpath> or <dirpath>@<host> spec");
		return (EINVAL);
	}
	*delimp = '\0';

	/*
	 * Handle an internet host address
	 */
	if ((hp = gethostbyname(hostp)) != NULL) {
		memmove(&saddr.sin_addr, hp->h_addr, hp->h_length);
	} else if (isdigit(*hostp) && ((saddr.sin_addr.s_addr = inet_addr(hostp)) != INADDR_NONE)) {
		; /* it was an address */
	} else {
		warnx("can't get net id for host");
		return (ENOENT);
        }

	if (force4) {
		/* we don't need most of this version/port searching for force NFSv4 */
		nfsvers = NFS_VER4;
		saddr.sin_family = AF_INET;
		tport = 2049;
		nfhret.fhsize = 0;
		nfhret.auth = nfsargsp->auth ? nfsargsp->auth : RPCAUTH_SYS;
		goto got_everything;
	} else if (force2) {
		nfsvers = NFS_VER2;
		mntvers = RPCMNT_VER1;
	} else {
		nfsvers = NFS_VER3;
		mntvers = RPCMNT_VER3;
	}

	if (retrycnt < 0)
		retrycnt = (opflags & BGRND) ? DEF_RETRY_BG : DEF_RETRY_FG;
	retry = retrycnt;

tryagain:
	nfhret.stat = EACCES;	/* Mark not yet successful */

	for (;;) {
		saddr.sin_family = AF_INET;

		if (forceport) {
			/*
			 * We're given which port to use, but need to
			 * establish the protocol: TCP or UDP.
			 * If TCP is chosen by default or forced
			 * then attempt a TCP connection to the NFS
			 * server to check whether TCP is available.
			 * Fall back to UDP if necessary.
			 */
			saddr.sin_port = htons(forceport);

			if (nfsproto == IPPROTO_TCP) {
				seteuid(eff_uid);	// for reserved port
				clp = clnttcp_create(&saddr, RPCPROG_NFS,
					nfsvers, &so, 0, 0);
				seteuid(real_uid);
				if (clp) {
					/* TCP is OK */
					clnt_destroy(clp);
					(void) close(so);
					tport = forceport;
				} else if (!forceproto) {
					/* Fall back to UDP */
					nfsproto = IPPROTO_UDP;
					nfsargsp->sotype = SOCK_DGRAM;
					tport = forceport;
				}
			} else
				tport = forceport;
		} else {
			/*
			 * Query the portmapper to get the NFS server's
			 * port and proto - TCP or UDP.
			 */
			saddr.sin_port = htons(PMAPPORT);
			tport = pmap_getport_timely(&saddr, RPCPROG_NFS,
				nfsvers, nfsproto, pmap_timeout);
			if (tport == 0 && rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED) {
				/*
				 * If the portmapper reply indicates that
				 * the default transport (TCP) isn't supported
				 * and the user hasn't explicitly selected a
				 * protocol then try UDP.
				 */
				if (!forceproto) {
					nfsproto = IPPROTO_UDP;
					nfsargsp->sotype = SOCK_DGRAM;
					tport = pmap_getport_timely(&saddr, RPCPROG_NFS,
						nfsvers, nfsproto, pmap_timeout);
				}
				if (tport == 0 && (opflags & ISBGRND) == 0) {
					char s[] = "NFS Portmap";
					clnt_pcreateerror(s);
				}
			}
		}

		if (tport > 0) {
			saddr.sin_port = htons(mountport);
			pertry.tv_sec = 10;
			pertry.tv_usec = 0;
			so = RPC_ANYSOCK;
			/*
			 * temporarily revert to root, to avoid reserved port
			 * number restriction (port# less than 1024)
			 */
			seteuid(eff_uid);
			if (mnttcp_ok && nfsargsp->sotype == SOCK_STREAM)
			    clp = clnttcp_create(&saddr, RPCPROG_MNT, mntvers,
				&so, 0, 0);
			else
			    clp = clntudp_create(&saddr, RPCPROG_MNT, mntvers,
				pertry, &so);
			seteuid(real_uid);
			if (clp == NULL) {
				if ((opflags & ISBGRND) == 0) {
					char s[] = "Cannot MNT RPC";
					clnt_pcreateerror(s);
				}
			} else {
				clp->cl_auth = authunix_create_default();
				try.tv_sec = 10;
				try.tv_usec = 0;
				nfhret.auth = nfsargsp->auth;
				nfhret.vers = mntvers;
				clnt_stat = clnt_call(clp, RPCMNT_MOUNT,
				    (xdrproc_t) xdr_dir, spec, (xdrproc_t) xdr_fh,
					&nfhret, try);
				switch (clnt_stat) {
				case RPC_PROGVERSMISMATCH:
					if (nfsvers == NFS_VER3 && !force3) {
						retry = retrycnt;
						nfsvers = NFS_VER2;
						mntvers = RPCMNT_VER1;
						nfsargsp->flags &=
							~NFSMNT_NFSV3;
						goto tryagain;
					} else {
						char s[] = "MNT RPC";
						errx(EPROGMISMATCH, "%s", clnt_sperror(clp, s));
					}
				case RPC_SUCCESS:
					nfsargsp->flags |= NFSMNT_CALLUMNT;
					auth_destroy(clp->cl_auth);
					clnt_destroy(clp);
					retry = 0;
					break;
				default:
					/* XXX should give up on some errors */
					if ((opflags & ISBGRND) == 0) {
						char s[] = "bad MNT RPC";
						warnx("%s", clnt_sperror(clp, s));
					}
					break;
				}
			}
		}
		if (retry <= 0)
			break;
		--retry;
		if (opflags & BGRND) {
			opflags &= ~BGRND;
			if (daemon(0, 0))
				err(errno, "nfs bgrnd: fork failed");
			opflags |= ISBGRND;
		}
		sleep(60);
	}
	if (nfhret.stat) {
		if (opflags & ISBGRND)
			exit(nfhret.stat);
		warnx("can't access %s: %s", spec, strerror(nfhret.stat));
		return (nfhret.stat);
	}
got_everything:
	saddr.sin_port = forceport ? htons(forceport) : htons(tport);
	nfsargsp->addr = (struct sockaddr *) &saddr;
	nfsargsp->addrlen = sizeof (saddr);
	nfsargsp->fh = nfhret.nfh;
	nfsargsp->fhsize = nfhret.fhsize;
	nfsargsp->hostname = nam;
	nfsargsp->flags |= NFSMNT_SECFLAVOR;
	nfsargsp->auth = nfhret.auth;
	if (nfhret.sysok || nfsargsp->auth == RPCAUTH_SYS)
		nfsargsp->flags |= NFSMNT_SECSYSOK;
	return (0);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

int
xdr_fh(XDR *xdrsp, struct nfhret *np)
{
	int i, sec_opt;
	xdr_long_t auth, authcnt, authfnd = 0;

	sec_opt = (np->auth) ? 1 : 0;
	np->sysok = FALSE;

	if (!xdr_u_long(xdrsp, &np->stat))
		return (0);
	if (np->stat)
		return (1);
	switch (np->vers) {
	case 1:
		if (!sec_opt) {
			/* No "sec=" option was given, so default to sys */
			np->auth = RPCAUTH_SYS;
		}
		np->fhsize = NFSX_V2FH;
		return (xdr_opaque(xdrsp, (caddr_t)np->nfh, NFSX_V2FH));
	case 3:
		if (!xdr_long(xdrsp, &np->fhsize))
			return (0);
		if (np->fhsize <= 0 || np->fhsize > NFSX_V3FHMAX)
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)np->nfh, np->fhsize))
			return (0);
		if (!xdr_long(xdrsp, &authcnt))
			return (0);
		for (i = 0; i < authcnt; i++) {
			if (!xdr_long(xdrsp, &auth))
				return (0);
			if (sec_opt) {
				/* A "sec=" option was given, check if server supports it */
				if (auth == np->auth) {
					authfnd = 1;
					break;
				}
				continue;
			}

			switch (auth) {
			case RPCAUTH_SYS:
				if (!authfnd)
					np->auth = RPCAUTH_SYS;
				np->sysok = TRUE;
				authfnd = 1;
				break;
			case RPCAUTH_KRB5:
				if (!authfnd)
					np->auth = RPCAUTH_KRB5;
				authfnd = 1;
				break;
			case RPCAUTH_KRB5I:
				if (!authfnd)
					np->auth = RPCAUTH_KRB5I;
				authfnd = 1;
				break;
			case RPCAUTH_KRB5P:
				if (!authfnd)
					np->auth = RPCAUTH_KRB5P;
				authfnd = 1;
				break;
			}
		}

		if (!authfnd) {
			if (!(authcnt) && (!(sec_opt) || (np->auth == RPCAUTH_SYS))) {
				/*
				 * Some servers, such as DEC's OSF/1 return a nil authenticator
				 * list to indicate RPCAUTH_SYS.
				 */
				np->auth = RPCAUTH_SYS;
			}
			else 
				np->stat = EAUTH;
		}

		return (1);
	};
	return (0);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: mount_nfs [-o options] server:/path directory\n");
	exit(EINVAL);
}

/* Map from mount otions to printable formats. */
static struct opt {
	int o_opt;
	const char *o_name;
} optnames[] = {
	{ MNT_ASYNC,		"asynchronous" },
	{ MNT_EXPORTED,		"NFS exported" },
	{ MNT_LOCAL,		"local" },
	{ MNT_NODEV,		"nodev" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_QUOTA,		"with quotas" },
	{ MNT_RDONLY,		"read-only" },
	{ MNT_SYNCHRONOUS,	"synchronous" },
	{ MNT_UNION,		"union" },
	{ MNT_AUTOMOUNTED,	"automounted" },
	{ MNT_JOURNALED,	"journaled" },
	{ MNT_DEFWRITE, 	"defwrite" },
	{ MNT_IGNORE_OWNERSHIP,	"noowners" },
	{ MNT_NOATIME,		"noatime" },
	{ MNT_QUARANTINE,	"quarantine" },
	{ 0, 				NULL }
};

void
dump_mount_options(char *name)
{
	struct opt *o;
	int flags, i;

	printf("mount %s on %s\n", nfsargsp->hostname, name);

	flags = mntflags;
	printf("mount flags: 0x%x", flags);
	for (o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			printf(", %s", o->o_name);
			flags &= ~o->o_opt;
		}
	printf("\n");

	printf("%s %s", inet_ntoa(((struct sockaddr_in *)nfsargsp->addr)->sin_addr),
		((nfsproto == IPPROTO_TCP) ? "tcp" : 
		 (nfsproto == IPPROTO_UDP) ? "udp" : "???"));
	if (forceport)
		printf(",port=%d", forceport);
	if (mountport)
		printf(",mountport=%d", mountport);
	printf(", fh %d ", nfsargsp->fhsize);
	for (i=0; i < nfsargsp->fhsize; i++)
		printf("%02x", nfsargsp->fh[i] & 0xff);
	printf("\n");

	flags = nfsargsp->flags;
	printf("NFS options: 0x%x", flags);
	printf(" %s,retrycnt=%d,vers=%d", ((opflags & BGRND) ? "bg" : "fg"), retrycnt,
		(flags & NFSMNT_NFSV4) ? 4 : (flags & NFSMNT_NFSV3) ? 3 : 2);
	if ((flags & NFSMNT_SOFT) || (verbose > 1))
		printf(",%s", (flags & NFSMNT_SOFT) ? "soft" : "hard");
	if ((flags & NFSMNT_INT) || (verbose > 1))
		printf(",%sintr", (flags & NFSMNT_INT) ? "" : "no");
	if ((flags & NFSMNT_RESVPORT) || (verbose > 1))
		printf(",%sresvport", (flags & NFSMNT_RESVPORT) ? "" : "no");
	if ((flags & NFSMNT_NOCONN) || (verbose > 1))
		printf(",%sconn", (flags & NFSMNT_NOCONN) ? "no" : "");
	if ((flags & NFSMNT_NOLOCKS) || (verbose > 1))
		printf(",%slocks", (flags & NFSMNT_NOLOCKS) ? "no" : "");
	if ((flags & NFSMNT_LOCALLOCKS) || (verbose > 1))
		printf(",%slocallocks", (flags & NFSMNT_LOCALLOCKS) ? "" : "no");
	if ((flags & NFSMNT_NONEGNAMECACHE) || (verbose > 1))
		printf(",%snegnamecache", (flags & NFSMNT_NONEGNAMECACHE) ? "no" : "");
	if ((flags & NFSMNT_RSIZE) || (verbose > 1))
		printf(",rsize=%d", nfsargsp->rsize);
	if ((flags & NFSMNT_WSIZE) || (verbose > 1))
		printf(",wsize=%d", nfsargsp->wsize);
	if ((flags & NFSMNT_READDIRSIZE) || (verbose > 1))
		printf(",dsize=%d", nfsargsp->readdirsize);
	if ((flags & NFSMNT_READAHEAD) || (verbose > 1))
		printf(",readahead=%d", nfsargsp->readahead);
	if ((flags & NFSMNT_RDIRPLUS) || (verbose > 1))
		printf(",%srdirplus", (flags & NFSMNT_RDIRPLUS) ? "" : "no");
	if ((flags & NFSMNT_DUMBTIMR) || (verbose > 1))
		printf(",%sdumbtimer", (flags & NFSMNT_DUMBTIMR) ? "" : "no");
	if ((flags & NFSMNT_TIMEO) || (verbose > 1))
		printf(",timeout=%d", nfsargsp->timeo);
	if ((flags & NFSMNT_RETRANS) || (verbose > 1))
		printf(",retrans=%d", nfsargsp->retrans);
	if ((flags & NFSMNT_MAXGRPS) || (verbose > 1))
		printf(",maxgroups=%d", nfsargsp->maxgrouplist);
	if ((flags & NFSMNT_ACREGMIN) || (verbose > 1))
		printf(",acregmin=%d", nfsargsp->acregmin);
	if ((flags & NFSMNT_ACREGMAX) || (verbose > 1))
		printf(",acregmax=%d", nfsargsp->acregmax);
	if ((flags & NFSMNT_ACDIRMIN) || (verbose > 1))
		printf(",acdirmin=%d", nfsargsp->acdirmin);
	if ((flags & NFSMNT_ACDIRMAX) || (verbose > 1))
		printf(",acdirmax=%d", nfsargsp->acdirmax);
	printf(",sec=%s (%d)\n",
		(nfsargsp->auth == RPCAUTH_KRB5P) ? "krb5p" :
		(nfsargsp->auth == RPCAUTH_KRB5I) ? "krb5i" :
		(nfsargsp->auth == RPCAUTH_KRB5)  ? "krb5" :
		(nfsargsp->auth == RPCAUTH_SYS)   ? "sys" :
		(nfsargsp->auth == RPCAUTH_NULL)  ? "null" : "???",
		nfsargsp->auth);
	if ((flags & NFSMNT_DEADTIMEOUT) || (verbose > 1))
		printf(",deadtimeout=%d", nfsargsp->deadtimeout);
}

