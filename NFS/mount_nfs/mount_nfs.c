/*
 * Copyright (c) 1999-2022 Apple Inc. All rights reserved.
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
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <xpc/xpc.h>

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
#include <util.h>
#include <sysexits.h>

#include <oncrpc/rpc.h>

#define _NFS_XDR_SUBS_FUNCS_ /* define this to get xdrbuf function definitions */
#include <nfs/xdr_subs.h>

#include <TargetConditionals.h>

#include "mount_nfs.h"
#include "utils.h"

#define LOCALHOST_ADDR  "localhost"
#define _PATH_NFS_CONF  "/etc/nfs.conf"
#define READAHEAD_WARNING_RATIO 0.25

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
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	NULL,
};

/* mount options */

/* values */
#define ALTF_ATTRCACHE_VAL      0x00000001
#define ALTF_DSIZE              0x00000002
#define ALTF_MAXGROUPS          0x00000004
#define ALTF_MOUNTPORT          0x00000008
#define ALTF_PORT               0x00000010
#define ALTF_PROTO              0x00000020
#define ALTF_READAHEAD          0x00000040
#define ALTF_RETRANS            0x00000080
#define ALTF_RETRYCNT           0x00000100
#define ALTF_RSIZE              0x00000200
#define ALTF_SEC                0x00000400
#define ALTF_TIMEO              0x00000800
#define ALTF_VERS               0x00001000
#define ALTF_VERS2              0x00002000 /* deprecated */
#define ALTF_VERS3              0x00004000 /* deprecated */
#define ALTF_VERS4              0x00008000 /* deprecated */
#define ALTF_WSIZE              0x00010000
#define ALTF_DEADTIMEOUT        0x00020000
#define ALTF_REALM              0x00040000
#define ALTF_PRINCIPAL          0x00080000
#define ALTF_SVCPRINCIPAL       0x00100000
#define ALTF_ETYPE              0x00200000
#define ALTF_FH                 0x00400000
#define ALTF_NETID              0x00800000
#define ALTF_READLINK_NOCACHE   0x01000000
#define ALTF_ACCESS_CACHE       0x02000000

/* switches */
#define ALTF_ATTRCACHE          0x00000001
#define ALTF_BG                 0x00000002
#define ALTF_CONN               0x00000004
#define ALTF_DUMBTIMR           0x00000008
#define ALTF_HARD               0x00000010
#define ALTF_INTR               0x00000020
#define ALTF_LOCALLOCKS         0x00000040
#define ALTF_LOCKS              0x00000080
#define ALTF_MNTUDP             0x00000100
#define ALTF_NEGNAMECACHE       0x00000200
#define ALTF_RDIRPLUS           0x00000400
#define ALTF_RESVPORT           0x00000800
#define ALTF_SOFT               0x00001000
#define ALTF_TCP                0x00002000
#define ALTF_UDP                0x00004000
#define ALTF_MUTEJUKEBOX        0x00008000
#define ALTF_CALLBACK           0x00010000
#define ALTF_NAMEDATTR          0x00020000
#define ALTF_ACL                0x00040000
#define ALTF_ACLONLY            0x00080000
#define ALTF_NFC                0x00100000
#define ALTF_INET               0x00200000
#define ALTF_INET6              0x00400000
#define ALTF_QUOTA              0x00800000
#define ALTF_PFH                0x01000000
#define ALTF_NOOPAQUE_AUTH      0x02000000
#define ALTF_CALLUMNT           0x08000000
#define ALTF_LOCALHOST          0x10000000
#define ALTF_SKIP_RENEW         0x20000000

/* standard and value-setting options */
struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	MOPT_ASYNC,
	MOPT_SYNC,
	{ "multilabel", 0, MNT_MULTILABEL, 0 },
	{ "acdirmax", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acdirmin", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acrootdirmax", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acrootdirmin", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acregmax", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "acregmin", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "actimeo", 0, ALTF_ATTRCACHE_VAL, 1 },
	{ "deadtimeout", 0, ALTF_DEADTIMEOUT, 1 },
	{ "dsize", 0, ALTF_DSIZE, 1 },
	{ "etype", 0, ALTF_ETYPE, 1 },
	{ "fh", 0, ALTF_FH, 1 },
	{ "maxgroups", 0, ALTF_MAXGROUPS, 1 },
	{ "mountport", 0, ALTF_MOUNTPORT, 1 },
	{ "netid", 0, ALTF_NETID, 1 },
	{ "port", 0, ALTF_PORT, 1 },
	{ "principal", 0, ALTF_PRINCIPAL, 1 },
	{ "proto", 0, ALTF_PROTO, 1 },
	{ "readahead", 0, ALTF_READAHEAD, 1 },
	{ "realm", 0, ALTF_REALM, 1 },
	{ "retrans", 0, ALTF_RETRANS, 1 },
	{ "retrycnt", 0, ALTF_RETRYCNT, 1 },
	{ "rsize", 0, ALTF_RSIZE, 1 },
	{ "rwsize", 0, ALTF_RSIZE | ALTF_WSIZE, 1 },
	{ "sec", 0, ALTF_SEC, 1 },
	{ "sprincipal", 0, ALTF_SVCPRINCIPAL, 1 },
	{ "timeo", 0, ALTF_TIMEO, 1 },
	{ "vers", 0, ALTF_VERS, 1 },
	{ "wsize", 0, ALTF_WSIZE, 1 },
	{ "readlink_nocache", 0, ALTF_READLINK_NOCACHE, 1 },
	{ "accesscache", 0, ALTF_ACCESS_CACHE, 1 },
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
	{ "acl", 0, ALTF_ACL, 1 },
	{ "aclonly", 0, ALTF_ACLONLY, 1 },
	{ "bg", 0, ALTF_BG, 1 },
	{ "callback", 0, ALTF_CALLBACK, 1 },
	{ "conn", 0, ALTF_CONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "hard", 0, ALTF_HARD, 1 },
	{ "inet", 0, ALTF_INET, 1 },
	{ "inet6", 0, ALTF_INET6, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
	{ "locallocks", 0, ALTF_LOCALLOCKS, 1 },
	{ "lock", 0, ALTF_LOCKS, 1 },
	{ "lockd", 0, ALTF_LOCKS, 1 },
	{ "locks", 0, ALTF_LOCKS, 1 },
	{ "mntudp", 0, ALTF_MNTUDP, 1 },
	{ "mutejukebox", 0, ALTF_MUTEJUKEBOX, 1 },
	{ "namedattr", 0, ALTF_NAMEDATTR, 1 },
	{ "negnamecache", 0, ALTF_NEGNAMECACHE, 1 },
	{ "nfc", 0, ALTF_NFC, 1 },
	{ "nlm", 0, ALTF_LOCKS, 1 },
	{ "opaque_auth", 1, ALTF_NOOPAQUE_AUTH, 1 },
	{ "pfh", 0, ALTF_PFH, 1 },
	{ "quota", 0, ALTF_QUOTA, 1 },
	{ "rdirplus", 0, ALTF_RDIRPLUS, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ "udp", 0, ALTF_UDP, 1 },
	{ "callumnt", 0, ALTF_CALLUMNT, 1 },
	{ "localhost", 0, ALTF_LOCALHOST, 1 },
	{ "skip_renew", 0, ALTF_SKIP_RENEW, 1 },
	{ NULL }
};
/* inverse of mopts_switches (set up at runtime) */
struct mntopt *mopts_switches_no = NULL;

char *mntfromarg = NULL;
int nfsproto = 0;

#define SENDSZ  65536
#define RECVSZ  65536

/* flags controlling mount_nfs behavior */
#define BGRND           0x1
#define ISBGRND         0x2
int opflags = 0;
int verbose = 0;

#define DEF_RETRY_BG    10000
#define DEF_RETRY_FG    1
int retrycnt = -1;
int zdebug = 0;         /* Turn on testing flag to make kernel prepend '@' to realm */

/* mount options */
struct nfs_options_client options;

/* convenience macro for setting a mount flag to a particular value (on/off) */
#define SETFLAG(F, V) \
	do { \
	        NFS_BITMAP_SET(options.mflags_mask, F); \
	        if (V) \
	                NFS_BITMAP_SET(options.mflags, F); \
	        else \
	                NFS_BITMAP_CLR(options.mflags, F); \
	} while (0)

#define AOK     (void *)        // assert alignment is OK

/* function prototypes */
void            setNFSVersion(uint32_t, uint32_t);
void            dump_mount_options(struct nfs_fs_location *, char *);
void            set_krb5_sec_flavor_for_principal(void);
void            handle_mntopts(char *);
void            warn_badoptions(char *);
void            config_sysctl(void);
int             getaddresslist(struct nfs_fs_server *);
void            freeaddresslists(struct nfs_fs_location *);
int             assemble_mount_args(struct nfs_fs_location *, char **);
void            usage(void);
int             nfsparsevers(const char *, uint32_t *, uint32_t *);
char *          fh2hexstr(fhandle_t *);
enum clnt_stat  ping_rpc_statd(struct sockaddr *);
void            nfs_err(int, const char *, ...) __attribute__((format(printf, 2, 3)));

uint32_t
strtouint32(const char *str, char **eptr, int base)
{
	int64_t num;
	num = strtoll(str, eptr, base);
	if (num > UINT32_MAX) {
		num = UINT32_MAX;
	}
	if (num < 0) {
		num = 0;
	}

	return (uint32_t)num;
}

static int
is_ftp(const char *p)
{
	if (p) {
		static const char *ftp = "ftp://";
		return strncmp(p, ftp, strlen(ftp)) == 0;
	}
	return 0;
}

void
nfs_err(int error, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (options.mntflags & MNT_AUTOMOUNTED) {
		va_list ap2;
		va_copy(ap2, ap);
		vsyslog(LOG_ERR, fmt, ap2);
		va_end(ap2);
	}

	verrx(error, fmt, ap);
	va_end(ap);
}

void
setup_switches(void)
{
	uint i;

	mopts_switches_no = malloc(sizeof(mopts_switches));
	if (!mopts_switches_no) {
		nfs_err(ENOMEM, "memory allocation failed");
	}
	bcopy(mopts_switches, mopts_switches_no, sizeof(mopts_switches));
	for (i = 0; i < sizeof(mopts_switches) / sizeof(struct mntopt); i++) {
		mopts_switches_no[i].m_inverse = (mopts_switches[i].m_inverse == 0);
	}
}

int
mount_nfs_imp(int argc, char *argv[])
{
	int c, error, try, delay, servcnt;
	uint32_t num;
	char mntonname[MAXPATHLEN];
	char *p, *xdrbuf = NULL;
	struct nfs_fs_location *nfsl = NULL;

	bzero(&options, sizeof(options));

	/* set up mopts_switches_no from mopts_switches */
	setup_switches();

	/* initialize and process options */
	config_read(_PATH_NFS_CONF, &config);

	while ((c = getopt(argc, argv, "234a:bcdF:g:I:iLlmo:Pp:R:r:sTt:Uvw:x:z")) != EOF) {
		switch (c) {
		case '4':
			setNFSVersion(VER2PVER(4, 0), 0);
			break;
		case '3':
			setNFSVersion(VER2PVER(3, 0), 0);
			break;
		case '2':
			setNFSVersion(VER2PVER(2, 0), 0);
			break;
		case 'a':
			num = strtouint32(optarg, &p, 10);
			if (*p) {
				warnx("illegal -a value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READAHEAD);
				options.readahead = (int)num;
			}
			break;
		case 'b':
			opflags |= BGRND;
			break;
		case 'c':
			SETFLAG(NFS_MFLAG_NOCONNECT, 1);
			break;
		case 'd':
			SETFLAG(NFS_MFLAG_DUMBTIMER, 1);
			break;
		case 'F': { // XXX silly temporary hack - please remove this!
			int fd;
			ssize_t len;
			if ((fd = open(optarg, O_RDONLY)) < 0) {
				nfs_err(errno, "could not open file containing file system specification: %s: %s", optarg, strerror(errno));
			}
			if ((mntfromarg = malloc(MAXPATHLEN)) == NULL) {
				nfs_err(ENOMEM, "memory allocation failed");
			}
			if ((len = read(fd, mntfromarg, MAXPATHLEN - 1)) < 0) {
				nfs_err(errno, "could not read file containing file system specification: %s: %s", optarg, strerror(errno));
			}
			mntfromarg[len--] = '\0';
			while (isspace(mntfromarg[len])) {
				mntfromarg[len--] = '\0';
			}
			close(fd);
		}
		break;
		case 'g':
			num = strtouint32(optarg, &p, 10);
			if (*p) {
				warnx("illegal maxgroups value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_MAX_GROUP_LIST);
				options.maxgrouplist = (uint32_t)num;
			}
			break;
		case 'I':
			num = strtouint32(optarg, &p, 10);
			if (*p || num == 0 || num > NFS_MAXDATA) {
				warnx("illegal -I value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READDIR_SIZE);
				options.readdirsize = (uint32_t)num;
			}
			break;
		case 'i':
			SETFLAG(NFS_MFLAG_INTR, 1);
			break;
		case 'L':
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
			options.lockmode = NFS_LOCK_MODE_DISABLED;
			break;
		case 'l':
			SETFLAG(NFS_MFLAG_RDIRPLUS, 1);
			break;
		case 'm':
			SETFLAG(NFS_MFLAG_RDIRPLUS, 0);
			break;
		case 'o':
			handle_mntopts(optarg);
			break;
		case 'P':
			SETFLAG(NFS_MFLAG_RESVPORT, 1);
			break;
		case 'p':
			options.principal = strdup(optarg);
			if (!options.principal) {
				nfs_err(1, "could not set principal: %s", strerror(1));
			}
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_PRINCIPAL);
			break;
		case 'R':
			num = strtouint32(optarg, &p, 10);
			if (*p) {
				warnx("illegal -R value -- %s", optarg);
			} else {
				retrycnt = (int)num;
			}
			break;
		case 'r':
			num = strtouint32(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0) {
				warnx("illegal -r value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READ_SIZE);
				options.rsize = (uint32_t)num;
			}
			break;
		case 's':
			SETFLAG(NFS_MFLAG_SOFT, 1);
			break;
		case 'T':
			options.socket_type = SOCK_STREAM;
			break;
		case 't':
			num = strtouint32(optarg, &p, 10);
			if (*p) {
				warnx("illegal -t value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_REQUEST_TIMEOUT);
				/* This value is in .1s units */
				options.request_timeout.tv_sec = num / 10;
				options.request_timeout.tv_nsec = (num % 10) * 100000000;
			}
			break;
		case 'w':
			num = strtouint32(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0) {
				warnx("illegal -w value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_WRITE_SIZE);
				options.wsize = (uint32_t)num;
			}
			break;
		case 'x':
			num = strtouint32(optarg, &p, 10);
			if (*p || num <= 0) {
				warnx("illegal -x value -- %s", optarg);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SOFT_RETRY_COUNT);
				options.soft_retry_count = num;
			}
			break;
		case 'U':
			SETFLAG(NFS_MFLAG_MNTUDP, 1);
			break;
		case 'v':
			verbose++;
			break;
		case 'z':
			/* special secret debug flag */
			zdebug = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	set_krb5_sec_flavor_for_principal();

	/* soft, read-only mount implies ... */
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_SOFT) &&
	    NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_SOFT) &&
	    (options.mntflags & MNT_RDONLY)) {
		/* ...use of local locks */
		if (!NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_LOCK_MODE)) {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
			options.lockmode = NFS_LOCK_MODE_LOCAL;
		}
		/* ... dead timeout */
		if (!NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_DEAD_TIMEOUT)) {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_DEAD_TIMEOUT);
			options.dead_timeout.tv_sec = 60;
			options.dead_timeout.tv_nsec = 0;
		}
	}
	if (options.socket_type || options.socket_family) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SOCKET_TYPE);
	}

	/*
	 * Run sysctls to update /etc/nfs.conf changes in the kernel
	 * Normally run by launchd job com.apple.nfsconf after any
	 * change to the /etc/nfs.conf.
	 */
	if ((argc == 1) && !strcmp(argv[0], "configupdate")) {
		if (geteuid() != 0) {
			nfs_err(1, "Must be superuser to configupdate");
		}
		config_sysctl();
		unlink_kext_tmpfile();
		exit(0);
	}

	if (!mntfromarg && (argc > 0)) {
		mntfromarg = *argv++;
		argc--;
	}
	if (argc != 1) {
		usage();
	}

	if (options.mntflags & MNT_NOFOLLOW) {
		size_t sc = strlcpy(mntonname, *argv, MAXPATHLEN);
		if (sc >= MAXPATHLEN) {
			nfs_err(1, "%s: %s", mntonname, strerror(EINVAL));
		}
	} else if (realpath(*argv, mntonname) == NULL) {
		if (errno) {
			nfs_err(errno, "realpath %s: %s", mntonname, strerror(errno));
		} else {
			nfs_err(1, "realpath %s", mntonname);
		}
	}

	error = parse_fs_locations(mntfromarg, &nfsl);
	if (error || !nfsl) {
		if (!error) {
			error = EINVAL;
		}
		nfs_err(error, "could not parse file system specification");
		exit(1);
	}

	if (retrycnt < 0) {
		retrycnt = (opflags & BGRND) ? DEF_RETRY_BG : DEF_RETRY_FG;
	}
	if (retrycnt <= 0) { /* if no retries, also use short timeouts */
		SETFLAG(NFS_MFLAG_MNTQUICK, 1);
	}

	/* Load nfs kext before mount is executed */
	load_nfs_kext();

	for (try = 1; try <= retrycnt + 1; try++) {
		if (try > 1) {
			if (opflags & BGRND) {
				printf("Retrying NFS mount in background...\n");
				opflags &= ~BGRND;
				#pragma clang diagnostic push
				#pragma clang diagnostic ignored "-Wdeprecated-declarations"
				if (daemon(0, 0)) {
					nfs_err(errno, "mount nfs background: fork failed: %s", strerror(errno));
				}
				#pragma clang diagnostic pop
				opflags |= ISBGRND;
			}
			delay = MIN(5 * (try - 1), 30);
			if (verbose) {
				printf("Retrying NFS mount in %d seconds...\n", delay);
			}
			sleep(delay);
		}

		/* determine addresses */
		getaddresslists(nfsl, &servcnt);
		if (!servcnt) {
			error = ENOENT;
			warnx("no usable addresses for host: %s", nfsl->nl_servers->ns_name);
			continue;
		}


		if (verbose) {
			dump_mount_options(nfsl, mntonname);
		}

		/* Check for expected readahead
		 * Using wsize which is much greater than rsize could end up with a different amount
		 * of readhead RPCs compared to the amount requested by the user.
		 * For more info, please see Radars #84746580 and #85403223
		 */
		if ((!NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READAHEAD) || options.readahead)) {
			if (options.wsize > options.rsize) {
				double biosize_ratio = (double)options.wsize / options.rsize;
				double readahead_ratio = options.readahead / biosize_ratio;
				if (readahead_ratio < READAHEAD_WARNING_RATIO) {
					warnx("wsize/rsize ratio is high: this could end up with unexpected readahead RPCs");
				}
			}
		}

		/* assemble mount arguments */
		if ((error = assemble_mount_args(nfsl, &xdrbuf))) {
			nfs_err(error, "error assembling mount args: %s", strerror(error));
		}

		/* mount the file system */
		if (verbose > 2) {
			printf("Calling mount(\"nfs\", %8.8x, %p)\n", options.mntflags, xdrbuf);
		}

		if (mount("nfs", mntonname, options.mntflags, xdrbuf)) {
			error = errno;
			/* Give up or keep trying... depending on the error. */
			switch (error) {
			case EPROGUNAVAIL:
				/* NFSv3: mount_nfs should fail when server is not running rpc.statd */
				if (!NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_NFS_VERSION) || options.nfs_version < NFS_VER4) {
					if (ping_rpc_statd(nfsl->nl_servers->ns_ailist->ai_addr) == RPC_PROGNOTREGISTERED) {
						nfs_err(error, "can't mount with remote locks when server (%s) is not running rpc.statd: %s", nfsl->nl_servers->ns_name, strerror(error));
						/*NOTREACHED*/
						break;
					}
				}
			case ETIMEDOUT:
			case EAGAIN:
			case EPIPE:
			case EADDRNOTAVAIL:
			case ENETDOWN:
			case ENETUNREACH:
			case ENETRESET:
			case ECONNABORTED:
			case ECONNRESET:
			case ENOTCONN:
			case ESHUTDOWN:
			case ECONNREFUSED:
			case EHOSTDOWN:
			case EHOSTUNREACH:
				/* keep trying */
				break;
			default:
				/* give up */
				nfs_err(error, "can't mount %s from %s onto %s: %s", nfsl->nl_path, nfsl->nl_servers->ns_name, mntonname, strerror(error));
				/*NOTREACHED*/
				break;
			}
			/* clean up before continuing */
			freeaddresslists(nfsl);
			if (xdrbuf) {
				xb_free(xdrbuf);
				xdrbuf = NULL;
			}
			continue;
		}
		error = 0;

		break;
	}

	if (error) {
		nfs_err(error, "can't mount %s from %s onto %s: %s", nfsl->nl_path, nfsl->nl_servers->ns_name, mntonname, strerror(error));
	}

	exit(error);
}

int
main(int argc, char *argv[])
{
	return mount_nfs_imp(argc, argv);
}

/*
 * Given a hexstring, return a filehandle
 */

const char *hexstring = "0123456789abcdef";

int
hexstr2fh(const char *hstr, fhandle_t *fh)
{
	const char *str = hstr;
	unsigned char *data;
	int len, cnt;

	if (str == NULL) {
		errno = EINVAL;
		return EINVAL;
	}
	;

	printf("hstr = [%s]\n", hstr);
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		str += 2;
	}

	len = (int)strlen(str);
	if (len % 2) {
		errno = EINVAL;
		return EINVAL;
	}
	fh->fh_len = len / 2;

	for (cnt = 0, data = fh->fh_data; *str; str++, cnt++) {
		const char *d = strchr(hexstring, tolower(*str));
		unsigned char dig;
		if (d == NULL) {
			errno = EINVAL;
			return EINVAL;
		}
		dig = (d - hexstring);
		if (cnt % 2 == 0) {
			*data = 0;
			*data |= ((dig << 4) & 0xf0);
		} else {
			*data |= (dig & 0xf);
			data++;
		}
	}
	return 0;
}

#if 1
char *
fh2hexstr(fhandle_t *fh)
{
	char *hstr;
	int len = fh->fh_len;
	int i, j;

	hstr = calloc(1, len * 2 + 3);
	if (hstr == NULL) {
		return NULL;
	}
	strlcpy(hstr, "0x", len * 2 + 3);
	for (i = 0, j = 2; i < len; i++, j += 2) {
		hstr[j] = hexstring[(fh->fh_data[i] >> 4) & 0xf];
		hstr[j + 1] = hexstring[fh->fh_data[i] & 0xf];
	}
	return hstr;
}
#endif

/*
 * given any protocol family and socket type preferences, determine what
 * netid-like value should be used for the "socket type" NFS mount argument.
 */
static char *
get_socket_type_mount_arg(void)
{
	const char *proto = "inet", *family = "";
	static char type[16];

	switch (options.socket_family) {
	case AF_LOCAL:
		switch (options.socket_type) {
		case SOCK_DGRAM:
			strlcpy(type, "ticlts", sizeof(type));
			break;
		case SOCK_STREAM:
		default:
			strlcpy(type, "ticotsord", sizeof(type));
			break;
		}
		break;
	case AF_UNSPEC:
	case AF_INET:
	case AF_INET6:
		if (options.socket_family == AF_INET) {
			family = "4";
		}
		if (options.socket_family == AF_INET6) {
			family = "6";
		}
		if (options.socket_type) {
			if (options.socket_type == SOCK_DGRAM) {
				proto = "udp";
			}
			if (options.socket_type == SOCK_STREAM) {
				proto = "tcp";
			}
		} else {
			if (nfsproto == IPPROTO_UDP) {
				proto = "udp";
			}
			if (nfsproto == IPPROTO_TCP) {
				proto = "tcp";
			}
		}
		snprintf(type, sizeof(type), "%s%s", proto, family);
		break;
	}
	return type;
}

/*
 * Put the XDR mount args together.
 */
int
assemble_mount_args(struct nfs_fs_location *nfslhead, char **xdrbufp)
{
	struct xdrbuf xb;
	int error, i, numlocs, numcomps, numaddrs;
	char uaddr[128], ifnamebuf[IFNAMSIZ], *p, *cp;
	size_t argslength_offset, attrslength_offset, end_offset;
	uint32_t mattrs[NFS_MATTR_BITMAP_LEN], scopeid;
	struct addrinfo *ai;
	void *sinaddr;
	struct nfs_fs_location *nfsl;
	struct nfs_fs_server *nfss;

	*xdrbufp = NULL;

	for (i = 0; i < NFS_MATTR_BITMAP_LEN; i++) {
		mattrs[i] = options.mattrs[i];
	}

	/* we know these are set/being passed in */
	NFS_BITMAP_SET(mattrs, NFS_MATTR_FS_LOCATIONS);
	NFS_BITMAP_SET(mattrs, NFS_MATTR_MNTFLAGS);

	if (options.force_localhost && is_ftp(mntfromarg)) {
		NFS_BITMAP_SET(mattrs, NFS_MATTR_MNTFROM);
	}

	/* any flags assigned? */
	for (i = 0; i < NFS_MFLAG_BITMAP_LEN; i++) {
		if (options.mflags_mask[i]) {
			NFS_BITMAP_SET(mattrs, NFS_MATTR_FLAGS);
			break;
		}
	}

	/* build xdr buffer */
	error = 0;
	xb_init_buffer(&xb, NULL, 0);
	xb_add_32(error, &xb, NFS_ARGSVERSION_XDR);
	argslength_offset = xb_offset(&xb);
	xb_add_32(error, &xb, 0); // args length
	xb_add_32(error, &xb, NFS_XDRARGS_VERSION_0);
	xb_add_bitmap(error, &xb, mattrs, NFS_MATTR_BITMAP_LEN);
	attrslength_offset = xb_offset(&xb);
	xb_add_32(error, &xb, 0); // attrs length
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_FLAGS)) {
		xb_add_bitmap(error, &xb, options.mflags_mask, NFS_MFLAG_BITMAP_LEN); /* mask */
		xb_add_bitmap(error, &xb, options.mflags, NFS_MFLAG_BITMAP_LEN); /* value */
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_NFS_VERSION)) {
		xb_add_32(error, &xb, options.nfs_version);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_NFS_MINOR_VERSION)) {
		xb_add_32(error, &xb, options.nfs_minor_version);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_NFS_VERSION_RANGE)) {
		xb_add_32(error, &xb, options.nfs_min_vers);
		xb_add_32(error, &xb, options.nfs_max_vers);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_READ_SIZE)) {
		xb_add_32(error, &xb, options.rsize);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_WRITE_SIZE)) {
		xb_add_32(error, &xb, options.wsize);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_READDIR_SIZE)) {
		xb_add_32(error, &xb, options.readdirsize);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_READAHEAD)) {
		xb_add_32(error, &xb, options.readahead);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_REG_MIN)) {
		xb_add_32(error, &xb, options.acregmin.tv_sec);
		xb_add_32(error, &xb, options.acregmin.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_REG_MAX)) {
		xb_add_32(error, &xb, options.acregmax.tv_sec);
		xb_add_32(error, &xb, options.acregmax.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN)) {
		xb_add_32(error, &xb, options.acdirmin.tv_sec);
		xb_add_32(error, &xb, options.acdirmin.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX)) {
		xb_add_32(error, &xb, options.acdirmax.tv_sec);
		xb_add_32(error, &xb, options.acdirmax.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_LOCK_MODE)) {
		xb_add_32(error, &xb, options.lockmode);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_SECURITY)) {
		xb_add_word_array(error, &xb, options.sec.flavors, options.sec.count);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_KERB_ETYPE)) {
		xb_add_32(error, &xb, options.etype.count);
		xb_add_32(error, &xb, options.etype.selected);
		for (uint32_t j = 0; j < options.etype.count; j++) {
			xb_add_32(error, &xb, options.etype.etypes[j]);
		}
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_MAX_GROUP_LIST)) {
		xb_add_32(error, &xb, options.maxgrouplist);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_SOCKET_TYPE)) {
		char *type = get_socket_type_mount_arg();
		xb_add_string(error, &xb, type, strlen(type));
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_NFS_PORT)) {
		xb_add_32(error, &xb, options.nfs_port);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_MOUNT_PORT)) {
		xb_add_32(error, &xb, options.mount_port);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_REQUEST_TIMEOUT)) {
		xb_add_32(error, &xb, options.request_timeout.tv_sec);
		xb_add_32(error, &xb, options.request_timeout.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_SOFT_RETRY_COUNT)) {
		xb_add_32(error, &xb, options.soft_retry_count);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_DEAD_TIMEOUT)) {
		xb_add_32(error, &xb, options.dead_timeout.tv_sec);
		xb_add_32(error, &xb, options.dead_timeout.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_FH)) {
		xb_add_fh(error, &xb, &options.fh.fh_data[0], options.fh.fh_len);
	}
	/* fs location */
	for (numlocs = 0, nfsl = nfslhead; nfsl; nfsl = nfsl->nl_next) {
		numlocs++;
	}
	xb_add_32(error, &xb, numlocs); /* fs location count */
	for (nfsl = nfslhead; nfsl; nfsl = nfsl->nl_next) {
		xb_add_32(error, &xb, nfsl->nl_servcnt); /* server count */
		for (nfss = nfsl->nl_servers; nfss; nfss = nfss->ns_next) {
			xb_add_string(error, &xb, nfss->ns_name, strlen(nfss->ns_name)); /* server name */
			/* list of addresses */
			for (numaddrs = 0, ai = nfss->ns_ailist; ai; ai = ai->ai_next) {
				numaddrs++;
			}
			xb_add_32(error, &xb, numaddrs); /* address count */
			for (ai = nfss->ns_ailist; ai; ai = ai->ai_next) {
				switch (ai->ai_addr->sa_family) {
				case AF_LOCAL:
					strlcpy(uaddr, ((struct sockaddr_un *)ai->ai_addr)->sun_path, sizeof(uaddr));
					break;
				case AF_INET:
				case AF_INET6:
					/* convert address to universal address string */
					if (ai->ai_family == AF_INET) {
						sinaddr = &((struct sockaddr_in*) AOK ai->ai_addr)->sin_addr;
					} else {
						sinaddr = &((struct sockaddr_in6*) AOK ai->ai_addr)->sin6_addr;
					}
					if (inet_ntop(ai->ai_family, sinaddr, uaddr, sizeof(uaddr)) != uaddr) {
						warn("unable to convert server address to string");
						error = errno;
					}
					if (ai->ai_family == AF_INET6 && (scopeid = ((struct sockaddr_in6*) AOK ai->ai_addr)->sin6_scope_id)) {
						if (if_indextoname(scopeid, ifnamebuf)) {
							snprintf(uaddr + strlen(uaddr), MIN(sizeof(ifnamebuf), sizeof(uaddr) - strlen(uaddr)), "%%%s", ifnamebuf);
						} else {
							warn("unable to convert scope_id %u to interface name", scopeid);
							error = errno;
						}
					}
					break;
				default:
					warn("Unsupported addres family %d", ai->ai_addr->sa_family);
					error = ENOTSUP;
					break;
				}
				xb_add_string(error, &xb, uaddr, strlen(uaddr)); /* address */
			}
			xb_add_32(error, &xb, 0); /* empty server info */
		}
		/* pathname */
		p = nfsl->nl_path;
		while (*p && (*p == '/')) {
			p++;
		}
		numcomps = 0;
		while (*p) {
			while (*p && (*p != '/')) {
				p++;
			}
			numcomps++;
			while (*p && (*p == '/')) {
				p++;
			}
		}
		xb_add_32(error, &xb, numcomps); /* pathname component count */
		p = nfsl->nl_path;
		while (*p && (*p == '/')) {
			p++;
		}
		while (*p) {
			cp = p;
			while (*p && (*p != '/')) {
				p++;
			}
			xb_add_string(error, &xb, cp, (p - cp)); /* component */
			if (error) {
				break;
			}
			while (*p && (*p == '/')) {
				p++;
			}
		}
		xb_add_32(error, &xb, 0); /* empty fsl info */
	}
	xb_add_32(error, &xb, options.mntflags);

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_MNTFROM)) {
		xb_add_string(error, &xb, mntfromarg, strlen(mntfromarg));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_REALM)) {
		xb_add_string(error, &xb, options.realm, strlen(options.realm));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_PRINCIPAL)) {
		xb_add_string(error, &xb, options.principal, strlen(options.principal));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_SVCPRINCIPAL)) {
		xb_add_string(error, &xb, options.sprinc, strlen(options.sprinc));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_LOCAL_NFS_PORT)) {
		xb_add_string(error, &xb, options.local_nfs_port, strlen(options.local_nfs_port));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_LOCAL_MOUNT_PORT)) {
		xb_add_string(error, &xb, options.local_mount_port, strlen(options.local_mount_port));
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_READLINK_NOCACHE)) {
		xb_add_32(error, &xb, options.readlink_nocache);
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN)) {
		xb_add_32(error, &xb, options.acrootdirmin.tv_sec);
		xb_add_32(error, &xb, options.acrootdirmin.tv_nsec);
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX)) {
		xb_add_32(error, &xb, options.acrootdirmax.tv_sec);
		xb_add_32(error, &xb, options.acrootdirmax.tv_nsec);
	}

	if (NFS_BITMAP_ISSET(mattrs, NFS_MATTR_ACCESS_CACHE)) {
		xb_add_32(error, &xb, options.access_cache);
	}

	xb_build_done(error, &xb);

	/* update opaque counts */
	end_offset = xb_offset(&xb);
	if (!error) {
		error = xb_seek(&xb, argslength_offset);
		xb_add_32(error, &xb, end_offset - argslength_offset + XDRWORD /*version*/);
	}
	if (!error) {
		error = xb_seek(&xb, attrslength_offset);
		xb_add_32(error, &xb, end_offset - attrslength_offset - XDRWORD /*don't include length field*/);
	}

	if (!error) {
		/* grab the assembled buffer */
		*xdrbufp = xb_buffer_base(&xb);
		xb.xb_flags &= ~XB_CLEANUP;
	}

	xb_cleanup(&xb);
	return error;
}

/*
 * nfsparsvers returns packed minimum and maximum versions to try.
 * The high order 16 bits hold the major version and the low
 * order 16 bits hold the minor version. Major and minor versions
 * are further constrain by the defines NFS_MAX_SUPPORTED_VERSION
 * and NFS_MAX_SUPPORTED_MINOR_VERSION below. On success return
 * 1, else return 0. If the mnt string being parsed has the maximum
 * version before the minimum version, will swap them.
 */
uint32_t verstab[] = {
	0,  /* Version 0 (does not exist) */
	0,  /* Version 1 (does not exist) */
	0,  /* Version 2 */
	0,  /* Version 3 */
	0   /* Version 4 */
};

#define NFS_MAX_SUPPORTED_VERSION  ((long)(sizeof (verstab) / sizeof (uint32_t) - 1))
#define NFS_MAX_SUPPORTED_MINOR_VERSION(v) ((long)(verstab[(v)]))

int
nfsparsevers(const char *vstr, uint32_t *pminvers, uint32_t *pmaxvers)
{
	const char *nptr = vstr;
	char *eptr;
	uint32_t tmp, *packvalp = pminvers;
	long val;

	*pmaxvers = 0;
	/* Versions are [2-9][0-9]*(.[0-9]+)?(-[2-9][0-9]*(.[0-9]*)?)? */
	do {
		val = strtol(nptr, &eptr, 10);
		if ((*eptr && *eptr != '.' && *eptr != '-') ||
		    val < 2 || val > NFS_MAX_SUPPORTED_VERSION) {
			return 0;
		}
		/* Assign the major version to the pack value */
		*packvalp = (uint16_t)val << 16;
		switch (*eptr) {
		case '\0':
			packvalp = NULL;
			break;
		case '.':
			/* Handle the minor version */
			nptr = eptr + 1;
			val = strtol(nptr, &eptr, 10);
			if ((*eptr && *eptr != '-') ||
			    val < 0 || val > NFS_MAX_SUPPORTED_MINOR_VERSION(PVER2MAJOR(*packvalp))) {
				return 0;
			}
			*packvalp |= (uint16_t)val;
			if (*eptr == '\0') {
				packvalp = NULL;
				break;
			}
		case '-':
			/* Handle processing the max version */
			nptr = eptr + 1;
			if (packvalp == pmaxvers) {
				return 0;
			}
			packvalp = pmaxvers;
			break;
		default:
			return 0;
		}
	} while (packvalp);
	/* Lets be nice */
	if (*pmaxvers && (*pminvers > *pmaxvers)) {
		tmp = *pminvers;
		*pminvers = *pmaxvers;
		*pmaxvers = tmp;
	}

	return 1;
}

/*
 * Set (and sanity check) the NFS version that is being reuqested to use.
 */
void
setNFSVersion(uint32_t pminvers, uint32_t pmaxvers)
{
	uint32_t vers = PVER2MAJOR(pminvers), mvers = PVER2MINOR(pminvers);

	if (pmaxvers) {
		/* Clear the any previous version specification */
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_NFS_VERSION);
		options.nfs_version = 0;
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_NFS_MINOR_VERSION);
		options.nfs_minor_version = 0;
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_NFS_VERSION_RANGE);
		options.nfs_min_vers = pminvers;
		options.nfs_max_vers = pmaxvers;
	} else {
		/* Clear any previous version range and old minor version */
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_NFS_VERSION_RANGE);
		options.nfs_min_vers = 0;
		options.nfs_max_vers = 0;
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_NFS_MINOR_VERSION);
		options.nfs_minor_version = 0;
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_NFS_VERSION);
		options.nfs_version = vers;
		if (mvers) {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_NFS_MINOR_VERSION);
			options.nfs_minor_version = mvers;
		}
	}
}

/*
 * Parse security flavors
 */
static int
get_sec_flavors(const char *flavorlist_orig, struct nfs_sec *sec_flavs)
{
	char *flavorlist, *tofree;
	char *flavor;
	u_int32_t flav_bits;

#define SYS_BIT   0x00000001
#define KRB5_BIT  0x00000002
#define KRB5I_BIT 0x00000004
#define KRB5P_BIT 0x00000008
#define NONE_BIT  0x00000010

	/* try to make a copy of the string so we don't butcher the original */
	tofree = flavorlist = strdup(flavorlist_orig);
	if (!flavorlist) {
		return ENOMEM;
	}

	bzero(sec_flavs, sizeof(struct nfs_sec));
	flav_bits = 0;
	while (((flavor = strsep(&flavorlist, ":")) != NULL) && (sec_flavs->count < NX_MAX_SEC_FLAVORS)) {
		if (flavor[0] == '\0') {
			continue;
		}
		if (!strcmp("krb5p", flavor)) {
			if (flav_bits & KRB5P_BIT) {
				warnx("sec krb5p appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5P_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5P;
		} else if (!strcmp("krb5i", flavor)) {
			if (flav_bits & KRB5I_BIT) {
				warnx("sec krb5i appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5I_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5I;
		} else if (!strcmp("krb5", flavor)) {
			if (flav_bits & KRB5_BIT) {
				warnx("sec krb5 appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5;
		} else if (!strcmp("sys", flavor)) {
			if (flav_bits & SYS_BIT) {
				warnx("sec sys appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= SYS_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_SYS;
		} else if (!strcmp("none", flavor)) {
			if (flav_bits & NONE_BIT) {
				warnx("sec none appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= NONE_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_NONE;
		} else {
			warnx("unknown sec flavor '%s' ignored", flavor);
		}
	}

	free(tofree);

	if (sec_flavs->count) {
		return 0;
	} else {
		return 1;
	}
}

/*
 * Parse security flavors
 */
static int
get_etypes(const char *etypes_orig, struct nfs_etype *etypes)
{
	char *etype_list, *tofree;
	char *etype;
	uint32_t etype_bits;

#define DES3_BIT  0x00000001
#define AES128_BIT 0x00000002
#define AES256_BIT 0x00000004

	/* try to make a copy of the string so we don't butcher the original */
	tofree = etype_list = strdup(etypes_orig);
	if (!etype_list) {
		return ENOMEM;
	}

	bzero(etypes, sizeof(struct nfs_etype));
	etype_bits = 0;
	while (((etype = strsep(&etype_list, ":")) != NULL) && (etypes->count < NFS_MAX_ETYPES)) {
		if (etype[0] == '\0') {
			continue;
		}
		if (!strcasecmp("des3", etype) ||
		    !strcasecmp("des3-cbc-sha1", etype) || !strcasecmp("des3-cbc-sha1-kd", etype)) {
			if (etype_bits & DES3_BIT) {
				warnx("etype des3-cbc-sha1-kd  appears more than once: %s", etype_list);
				continue;
			}
			etype_bits |= DES3_BIT;
			etypes->etypes[etypes->count++] = NFS_DES3_CBC_SHA1_KD;
		} else if (!strcasecmp("aes128", etype) ||
		    !strcasecmp("aes128-cts-hmac-sha1", etype) || !strcasecmp("aes128-cts-hmac-sha1-96", etype)) {
			if (etype_bits & AES128_BIT) {
				warnx("etype aes128-cts-hmac-sha1-96  appears more than once: %s", etype_list);
				continue;
			}
			etype_bits |= AES128_BIT;
			etypes->etypes[etypes->count++] = NFS_AES128_CTS_HMAC_SHA1_96;
		} else if (!strcasecmp("aes256", etype) ||
		    !strcasecmp("aes256-cts-hmac-sha1", etype) || !strcasecmp("aes256-cts-hmac-sha1-96", etype)) {
			if (etype_bits & AES256_BIT) {
				warnx("etype aes256-cts-hmac-sha1-96  appears more than once: %s", etype_list);
				continue;
			}
			etype_bits |= AES256_BIT;
			etypes->etypes[etypes->count++] = NFS_AES256_CTS_HMAC_SHA1_96;
		} else {
			warnx("etype %s is unknown etype. Ignored", etype);
		}
	}
	free(tofree);

	etypes->selected = etypes->count; /* Nothing has been selected, so set selected to count */

	return (etypes->count) ? 0 : 1;
}

void
set_krb5_sec_flavor_for_principal(void)
{
	if (options.principal == NULL && options.realm == NULL && options.sprinc == NULL) {
		return;
	}

	if (options.sec.count == 0) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SECURITY);
		options.sec.count = 1;
		options.sec.flavors[0] = RPCAUTH_KRB5;
		warnx("no sec flavors specified for principal or realm, assuming kerberos");
	} else {
		int i;

		for (i = 0; i < options.sec.count; i++) {
			if (options.sec.flavors[i] == RPCAUTH_KRB5P ||
			    options.sec.flavors[i] == RPCAUTH_KRB5I ||
			    options.sec.flavors[i] == RPCAUTH_KRB5) {
				break;
			}
		}
		if (i == options.sec.count) {
			warnx("principal or realm specified but no kerberos is enabled");
		}
	}
}


void
handle_mntopts(char *opts)
{
	mntoptparse_t mop;
	char *p;
	const char *p2;
	int altflags = 0, dummyflags;
	long num;

	getmnt_silent = 1;

	/* do standard and value-setting options first */
	altflags = 0;
	mop = getmntopts(opts, mopts, &options.mntflags, &altflags);
	if (mop == NULL) {
		nfs_err(EINVAL, "getmntops failed: %s", opts);
	}

	if (options.mntflags) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_MNTFLAGS);
	}

	if (altflags & ALTF_ATTRCACHE_VAL) {
		if ((p2 = getmntoptstr(mop, "actimeo"))) {
			num = getmntoptnum(mop, "actimeo");
			if (num < 0) {
				warnx("illegal actimeo value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN);
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX);
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN);
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX);
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN);
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX);
				options.acregmin.tv_sec = num;
				options.acregmax.tv_sec = num;
				options.acdirmin.tv_sec = num;
				options.acdirmax.tv_sec = num;
				options.acrootdirmin.tv_sec = num;
				options.acrootdirmax.tv_sec = num;
				options.acregmin.tv_nsec = 0;
				options.acregmax.tv_nsec = 0;
				options.acdirmin.tv_nsec = 0;
				options.acdirmax.tv_nsec = 0;
				options.acrootdirmin.tv_nsec = 0;
				options.acrootdirmax.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acregmin"))) {
			num = getmntoptnum(mop, "acregmin");
			if (num < 0) {
				warnx("illegal acregmin value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN);
				options.acregmin.tv_sec = num;
				options.acregmin.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acregmax"))) {
			num = getmntoptnum(mop, "acregmax");
			if (num < 0) {
				warnx("illegal acregmax value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX);
				options.acregmax.tv_sec = num;
				options.acregmax.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acdirmin"))) {
			num = getmntoptnum(mop, "acdirmin");
			if (num < 0) {
				warnx("illegal acdirmin value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN);
				options.acdirmin.tv_sec = num;
				options.acdirmin.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acdirmax"))) {
			num = getmntoptnum(mop, "acdirmax");
			if (num < 0) {
				warnx("illegal acdirmax value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX);
				options.acdirmax.tv_sec = num;
				options.acdirmax.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acrootdirmin"))) {
			num = getmntoptnum(mop, "acrootdirmin");
			if (num < 0) {
				warnx("illegal acrootdirmin value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN);
				options.acrootdirmin.tv_sec = num;
				options.acrootdirmin.tv_nsec = 0;
			}
		}
		if ((p2 = getmntoptstr(mop, "acrootdirmax"))) {
			num = getmntoptnum(mop, "acrootdirmax");
			if (num < 0) {
				warnx("illegal acrootdirmax value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX);
				options.acrootdirmax.tv_sec = num;
				options.acrootdirmax.tv_nsec = 0;
			}
		}
	}
	if (altflags & ALTF_DEADTIMEOUT) {
		if ((p2 = getmntoptstr(mop, "deadtimeout"))) {
			num = strtol(p2, &p, 10);
			if (num < 0) {
				warnx("illegal deadtimeout value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_DEAD_TIMEOUT);
				options.dead_timeout.tv_sec = num;
				options.dead_timeout.tv_nsec = 0;
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
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READDIR_SIZE);
				options.readdirsize = (uint32_t)num;
			}
		}
	}
	if (altflags & ALTF_MAXGROUPS) {
		num = getmntoptnum(mop, "maxgroups");
		if (num <= 0 || num > 16) {
			warnx("illegal maxgroups value -- %s", getmntoptstr(mop, "maxgroups"));
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_MAX_GROUP_LIST);
			options.maxgrouplist = (uint32_t)num;
		}
	}
	if (altflags & ALTF_MOUNTPORT) {
		p2 = getmntoptstr(mop, "mountport");
		if (p2) {
			num = strtol(p2, &p, 10);
			if (*p == '\0') {
				if (num < 0 || num > 65535) {
					warnx("illegal mountport number -- %s", p2);
				} else {
					NFS_BITMAP_SET(options.mattrs, NFS_MATTR_MOUNT_PORT);
					options.mount_port = (uint32_t)num;
				}
			} else if (*p2 == '/') {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCAL_MOUNT_PORT);
				options.local_mount_port = strdup(p2);
				options.socket_family = AF_LOCAL;
			} else {
				warnx("unsupported mount port -- %s", p2);
			}
		} else {
			warnx("option mountport needs a value");
		}
	}
	if (altflags & ALTF_PORT) {
		p2 = getmntoptstr(mop, "port");
		if (p2) {
			num = strtol(p2, &p, 10);
			if (*p == '\0') {
				if (num < 0 || num > 65535) {
					warnx("illegal port number -- %s", p2);
				} else {
					NFS_BITMAP_SET(options.mattrs, NFS_MATTR_NFS_PORT);
					options.nfs_port = (uint32_t)num;
				}
			} else if (*p2 == '/') {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCAL_NFS_PORT);
				options.local_nfs_port = strdup(p2);
				options.socket_family = AF_LOCAL;
			} else {
				warnx("unsupported port -- %s", p2);
			}
		} else {
			warnx("option port needs a value");
		}
	}
	if (altflags & ALTF_PROTO || altflags & ALTF_NETID) {
		p2 = getmntoptstr(mop, "netid");
		if (p2 == NULL) {
			p2 = getmntoptstr(mop, "proto");
		}
		if (p2) {
			if (!strncasecmp(p2, "tcp", 4) || !strncasecmp(p2, "tcp4", 5)) {
				if (options.local_nfs_port || options.local_mount_port) {
					warnx("netid must be ticotsord or ticlts when specifing local socket ports");
				} else {
					options.socket_type = SOCK_STREAM;
					options.socket_family = AF_INET;
				}
			} else if (!strncasecmp(p2, "udp", 4) || !strncasecmp(p2, "udp4", 5)) {
				if (options.local_nfs_port || options.local_mount_port) {
					warnx("netid must be ticotsord or ticlts when specifing local socket ports");
				} else {
					options.socket_type = SOCK_DGRAM;
					options.socket_family = AF_INET;
				}
			} else if (!strncasecmp(p2, "tcp6", 5)) {
				if (options.local_nfs_port || options.local_mount_port) {
					warnx("netid must be ticotsord or ticlts when specifing local socket ports");
				} else {
					options.socket_type = SOCK_STREAM;
					options.socket_family = AF_INET6;
				}
			} else if (!strncasecmp(p2, "udp6", 5)) {
				if (options.local_nfs_port || options.local_mount_port) {
					warnx("netid must be ticotsord or ticlts when specifing local socket ports");
				} else {
					options.socket_type = SOCK_DGRAM;
					options.socket_family = AF_INET6;
				}
			} else if (!strncasecmp(p2, "ticotsord", 10)) {
				options.socket_type = SOCK_STREAM;
				options.socket_family = AF_LOCAL;
			} else if (!strncasecmp(p2, "ticlts", 7)) {
				options.socket_type = SOCK_DGRAM;
				options.socket_family = AF_LOCAL;
			} else {
				warnx("unknown protocol -- %s", p2);
			}
		}
	}
	if (altflags & ALTF_READAHEAD) {
		num = getmntoptnum(mop, "readahead");
		if (num < 0 || num > 128) {
			warnx("illegal readahead value -- %s", getmntoptstr(mop, "readahead"));
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READAHEAD);
			options.readahead = (uint32_t)num;
		}
	}
	if (altflags & ALTF_RETRANS) {
		num = getmntoptnum(mop, "retrans");
		if (num <= 0 || num > INT32_MAX) {
			warnx("illegal retrans value -- %s", getmntoptstr(mop, "retrans"));
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SOFT_RETRY_COUNT);
			options.soft_retry_count = (uint32_t)num;
		}
	}
	if (altflags & ALTF_RETRYCNT) {
		num = getmntoptnum(mop, "retrycnt");
		if (num < 0 || num > INT32_MAX) {
			warn("illegal retry count -- %s", getmntoptstr(mop, "retrycnt"));
		} else {
			retrycnt = (uint32_t)num;
		}
	}
	if (altflags & ALTF_RSIZE) {
		p2 = getmntoptstr(mop, "rwsize");
		if (!p2) {
			p2 = getmntoptstr(mop, "rsize");
		}
		if (p2) {
			num = strtol(p2, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if ((num <= 0 || num > NFS_MAXDATA) || *p) {
				warnx("illegal rsize value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READ_SIZE);
				options.rsize = (uint32_t)num;
			}
		}
	}
	if (altflags & ALTF_SEC) {
		p2 = getmntoptstr(mop, "sec");
		if (!p2) {
			warnx("missing security value for sec= option");
		} else if (get_sec_flavors(p2, &options.sec)) {
			warnx("couldn't parse security value -- %s", p2);
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SECURITY);
		}
	}
	if (altflags & ALTF_ETYPE) {
		p2 = getmntoptstr(mop, "etype");
		if (!p2) {
			warnx("missing encryption type value for etype= option");
		} else if (get_etypes(p2, &options.etype)) {
			warnx("couldn't parse type value -- %s", p2);
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_KERB_ETYPE);
		}
	}
	if (altflags & ALTF_TIMEO) {
		num = getmntoptnum(mop, "timeo");
		if (num <= 0) {
			warnx("illegal timeout value -- %s", getmntoptstr(mop, "timeo"));
		} else {
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_REQUEST_TIMEOUT);
			/* This value is in .1s units */
			options.request_timeout.tv_sec = num / 10;
			options.request_timeout.tv_nsec = (num % 10) * 100000000;
		}
	}
	if (altflags & ALTF_VERS) {
		uint32_t pminvers, pmaxvers;

		if (!(p2 = getmntoptstr(mop, "vers"))) {
			p2 = getmntoptstr(mop, "nfsvers");
		}

		if (p2) {
			if (nfsparsevers(p2, &pminvers, &pmaxvers)) {
				setNFSVersion(pminvers, pmaxvers);
			} else {
				warnx("illegal NFS version value -- %s", p2);
			}
		}
	}
	if (altflags & ALTF_VERS2) {
		warnx("option nfsv2 deprecated, use vers=#");
		setNFSVersion(VER2PVER(2, 0), 0);
	}
	if (altflags & ALTF_VERS3) {
		warnx("option nfsv3 deprecated, use vers=#");
		setNFSVersion(VER2PVER(3, 0), 0);
	}
	if (altflags & ALTF_VERS4) {
		warnx("option nfsv4 deprecated, use vers=#");
		setNFSVersion(VER2PVER(4, 0), 0);
	}
	if (altflags & ALTF_WSIZE) {
		p2 = getmntoptstr(mop, "rwsize");
		if (!p2) {
			p2 = getmntoptstr(mop, "wsize");
		}
		if (p2) {
			num = strtol(p2, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if ((num <= 0 || num > NFS_MAXDATA) || *p) {
				warnx("illegal wsize value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_WRITE_SIZE);
				options.wsize = (uint32_t)num;
			}
		}
	}
	if (altflags & ALTF_PRINCIPAL) {
		p2 = getmntoptstr(mop, "principal");
		if (!p2) {
			warnx("missing principal name");
		} else {
			if (options.principal) {
				warnx("principal is already set to %s. ignoring %s", options.principal, p2);
			} else {
				options.principal = strdup(p2);
			}
			if (options.principal) {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_PRINCIPAL);
			} else {
				nfs_err(1, "could not set principal: %s", strerror(1));
			}
		}
	}
	if (altflags & ALTF_REALM) {
		p2 = getmntoptstr(mop, "realm");
		if (!p2) {
			warnx("missing realm name");
		} else {
			if (*p2 == '@' || zdebug) {
				options.realm = strdup(p2);
			} else {
				(void) asprintf(&options.realm, "@%s", p2);
			}
			if (options.realm) {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_REALM);
			} else {
				nfs_err(1, "could not set realm: %s", strerror(1));
			}
		}
	}
	if (altflags & ALTF_SVCPRINCIPAL) {
		p2 = getmntoptstr(mop, "sprincipal");
		if (!p2) {
			warnx("missing server's principal");
		} else {
			options.sprinc = strdup(p2);
			if (options.sprinc) {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SVCPRINCIPAL);
			} else {
				nfs_err(1, "could not set server's principal: %s", strerror(1));
			}
		}
	}
	if (altflags & ALTF_FH) {
		p2 = getmntoptstr(mop, "fh");
		if (!p2) {
			warnx("missing hex string for file handle");
		} else {
			if (hexstr2fh(p2, &options.fh) == 0) {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_FH);
				if (verbose > 1) {
					char *hstr = fh2hexstr(&options.fh);
					if (hstr) {
						printf("Got fh %s\n", hstr);
						free(hstr);
					}
				}
			} else {
				nfs_err(1, "cound not set root file handle: %s", strerror(1));
			}
		}
	}
	if (altflags & ALTF_READLINK_NOCACHE) {
		if ((p2 = getmntoptstr(mop, "readlink_nocache"))) {
			num = getmntoptnum(mop, "readlink_nocache");
			if (num < 0) {
				warnx("illegal readlink_nocache value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_READLINK_NOCACHE);
				options.readlink_nocache = (int)num;
			}
		}
	}
	if (altflags & ALTF_ACCESS_CACHE) {
		if ((p2 = getmntoptstr(mop, "accesscache"))) {
			num = getmntoptnum(mop, "accesscache");
			if (num < 0) {
				warnx("illegal accesscache value -- %s", p2);
			} else {
				NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ACCESS_CACHE);
				options.access_cache = (int)num;
			}
		}
	}
	freemntopts(mop);

	/* next do positive form of switch options */
	altflags = 0;
	mop = getmntopts(opts, mopts_switches, &dummyflags, &altflags);
	if (mop == NULL) {
		nfs_err(EINVAL, "getmntops failed: %s", opts);
	}
	if (verbose > 2) {
		printf("altflags=0x%x\n", altflags);
	}

	if (altflags & ALTF_ACL) {
		SETFLAG(NFS_MFLAG_NOACL, 0);
	}
	if (altflags & ALTF_ACLONLY) {
		SETFLAG(NFS_MFLAG_ACLONLY, 1);
	}
	if (altflags & ALTF_ATTRCACHE) {
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN);
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX);
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN);
		NFS_BITMAP_CLR(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX);
	}
	if (altflags & ALTF_BG) {
		opflags |= BGRND;
	}
	if (altflags & ALTF_CALLBACK) {
		SETFLAG(NFS_MFLAG_NOCALLBACK, 0);
	}
	if (altflags & ALTF_CONN) {
		SETFLAG(NFS_MFLAG_NOCONNECT, 0);
	}
	if (altflags & ALTF_DUMBTIMR) {
		SETFLAG(NFS_MFLAG_DUMBTIMER, 1);
	}
	if (altflags & ALTF_HARD) {
		SETFLAG(NFS_MFLAG_SOFT, 0);
	}
	if (altflags & ALTF_INET) {
		options.socket_family = AF_INET;
	}
	if (altflags & ALTF_INET6) {
		options.socket_family = AF_INET6;
	}
	if (altflags & ALTF_INTR) {
		SETFLAG(NFS_MFLAG_INTR, 1);
	}
	if (altflags & ALTF_LOCALLOCKS) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
		options.lockmode = NFS_LOCK_MODE_LOCAL;
	}
	if (altflags & ALTF_LOCKS) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
		options.lockmode = NFS_LOCK_MODE_ENABLED;
	}
	if (altflags & ALTF_MNTUDP) {
		SETFLAG(NFS_MFLAG_MNTUDP, 1);
	}
	if (altflags & ALTF_MUTEJUKEBOX) {
		SETFLAG(NFS_MFLAG_MUTEJUKEBOX, 1);
	}
	if (altflags & ALTF_NAMEDATTR) {
		SETFLAG(NFS_MFLAG_NAMEDATTR, 1);
	}
	if (altflags & ALTF_NEGNAMECACHE) {
		SETFLAG(NFS_MFLAG_NONEGNAMECACHE, 0);
	}
	if (altflags & ALTF_NFC) {
		SETFLAG(NFS_MFLAG_NFC, 1);
	}
	if (altflags & ALTF_NOOPAQUE_AUTH) {
		SETFLAG(NFS_MFLAG_NOOPAQUE_AUTH, 1);
	}
	if (altflags & ALTF_QUOTA) {
		SETFLAG(NFS_MFLAG_NOQUOTA, 0);
	}
	if (altflags & ALTF_RDIRPLUS) {
		SETFLAG(NFS_MFLAG_RDIRPLUS, 1);
	}
	if (altflags & ALTF_RESVPORT) {
		SETFLAG(NFS_MFLAG_RESVPORT, 1);
	}
	if (altflags & ALTF_SOFT) {
		SETFLAG(NFS_MFLAG_SOFT, 1);
	}
	if (altflags & ALTF_TCP) {
		options.socket_type = SOCK_STREAM;
	}
	if (altflags & ALTF_UDP) {
		options.socket_type = SOCK_DGRAM;
	}
	if (altflags & ALTF_PFH) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_FH);
		options.fh.fh_len = 0;
	}
	if (altflags & ALTF_CALLUMNT) {
		SETFLAG(NFS_MFLAG_CALLUMNT, 1);
	}
	if (altflags & ALTF_LOCALHOST) {
		options.force_localhost = 1;
	}
	if (altflags & ALTF_SKIP_RENEW) {
		SETFLAG(NFS_MFLAG_SKIP_RENEW, 1);
	}
	freemntopts(mop);

	/* finally do negative form of switch options */
	altflags = 0;
	mop = getmntopts(opts, mopts_switches_no, &dummyflags, &altflags);
	if (mop == NULL) {
		nfs_err(EINVAL, "getmntops failed: %s", opts);
	}
	if (verbose > 2) {
		printf("negative altflags=0x%x\n", altflags);
	}

	if (altflags & ALTF_ACL) {
		SETFLAG(NFS_MFLAG_NOACL, 1);
	}
	if (altflags & ALTF_ACLONLY) {
		SETFLAG(NFS_MFLAG_ACLONLY, 0);
	}
	if (altflags & ALTF_ATTRCACHE) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN);
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX);
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN);
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX);
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN);
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX);
		options.acregmin.tv_sec = 0;
		options.acregmax.tv_sec = 0;
		options.acdirmin.tv_sec = 0;
		options.acdirmax.tv_sec = 0;
		options.acrootdirmin.tv_sec = 0;
		options.acrootdirmax.tv_sec = 0;
		options.acregmin.tv_nsec = 0;
		options.acregmax.tv_nsec = 0;
		options.acdirmin.tv_nsec = 0;
		options.acdirmax.tv_nsec = 0;
		options.acrootdirmin.tv_nsec = 0;
		options.acrootdirmax.tv_nsec = 0;
	}
	if (altflags & ALTF_BG) {
		opflags &= ~BGRND;
	}
	if (altflags & ALTF_CALLBACK) {
		SETFLAG(NFS_MFLAG_NOCALLBACK, 1);
	}
	if (altflags & ALTF_CONN) {
		SETFLAG(NFS_MFLAG_NOCONNECT, 1);
	}
	if (altflags & ALTF_NOOPAQUE_AUTH) {
		SETFLAG(NFS_MFLAG_NOOPAQUE_AUTH, 0);
	}
	if (altflags & ALTF_DUMBTIMR) {
		SETFLAG(NFS_MFLAG_DUMBTIMER, 0);
	}
	if (altflags & ALTF_HARD) {
		SETFLAG(NFS_MFLAG_SOFT, 1);
	}
	if (altflags & ALTF_INET) { /* noinet = inet6 */
		options.socket_family = AF_INET6;
	}
	if (altflags & ALTF_INET6) { /* noinet6 = inet */
		options.socket_family = AF_INET;
	}
	if (altflags & ALTF_INTR) {
		SETFLAG(NFS_MFLAG_INTR, 0);
	}
	if (altflags & ALTF_LOCALLOCKS) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
		options.lockmode = NFS_LOCK_MODE_ENABLED;
	}
	if (altflags & ALTF_LOCKS) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCK_MODE);
		options.lockmode = NFS_LOCK_MODE_DISABLED;
	}
	if (altflags & ALTF_MNTUDP) {
		SETFLAG(NFS_MFLAG_MNTUDP, 0);
	}
	if (altflags & ALTF_MUTEJUKEBOX) {
		SETFLAG(NFS_MFLAG_MUTEJUKEBOX, 0);
	}
	if (altflags & ALTF_NAMEDATTR) {
		SETFLAG(NFS_MFLAG_NAMEDATTR, 0);
	}
	if (altflags & ALTF_NEGNAMECACHE) {
		SETFLAG(NFS_MFLAG_NONEGNAMECACHE, 1);
	}
	if (altflags & ALTF_NFC) {
		SETFLAG(NFS_MFLAG_NFC, 0);
	}
	if (altflags & ALTF_QUOTA) {
		SETFLAG(NFS_MFLAG_NOQUOTA, 1);
	}
	if (altflags & ALTF_RDIRPLUS) {
		SETFLAG(NFS_MFLAG_RDIRPLUS, 0);
	}
	if (altflags & ALTF_RESVPORT) {
		SETFLAG(NFS_MFLAG_RESVPORT, 0);
	}
	if (altflags & ALTF_SOFT) {
		SETFLAG(NFS_MFLAG_SOFT, 0);
	}
	if (altflags & ALTF_TCP) { /* notcp = udp */
		options.socket_type = SOCK_DGRAM;
	}
	if (altflags & ALTF_UDP) { /* noudp = tcp */
		options.socket_type = SOCK_STREAM;
	}
	if (altflags & ALTF_CALLUMNT) {
		SETFLAG(NFS_MFLAG_CALLUMNT, 0);
	}
	if (altflags & ALTF_SKIP_RENEW) {
		SETFLAG(NFS_MFLAG_SKIP_RENEW, 0);
	}
	freemntopts(mop);

	if (options.socket_type || options.socket_family) {
		NFS_BITMAP_SET(options.mattrs, NFS_MATTR_SOCKET_TYPE);
	}

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
	if (myopts == NULL) {
		return;
	}

	for (opt = myopts; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		saveopt = opt;
		if (strncmp(opt, "no", 2) == 0) { // negative option
			opt += 2;
		}
		p = strchr(opt, '=');           // value option
		if (p) {
			*p = '\0';
		}

		known = &mopts[0];
		for (k = known; k->m_option != NULL; ++k) {
			if (strcasecmp(opt, k->m_option) == 0) {
				break;
			}
		}
		if (k->m_option != NULL) {      // known option
			continue;
		}

		known = &mopts_switches[0];
		for (k = known; k->m_option != NULL; ++k) {
			if (strcasecmp(opt, k->m_option) == 0) {
				break;
			}
		}
		if (k->m_option != NULL) {      // known option
			continue;
		}

		warnx("warning: option \"%s\" not known", saveopt);
	}

	free(myopts);
}

/*
 * read the NFS client values from nfs.conf
 */
int
config_read(const char *confpath, struct nfs_conf_client *conf)
{
	FILE *f;
	size_t len, linenum = 0;
	char *line, *p, *key, *value;
	long val;

	if (!(f = fopen(confpath, "r"))) {
		if (errno != ENOENT) {
			warn("%s", confpath);
		}
		return 1;
	}
	for (; (line = fparseln(f, &len, &linenum, NULL, 0)); free(line)) {
		if (len <= 0) {
			continue;
		}
		/* trim trailing whitespace */
		p = line + len - 1;
		while ((p > line) && isspace(*p)) {
			*p-- = '\0';
		}
		/* find key start */
		key = line;
		while (isspace(*key)) {
			key++;
		}
		/* find equals/value */
		value = p = strchr(line, '=');
		if (p) { /* trim trailing whitespace on key */
			do {
				*p-- = '\0';
			} while ((p > line) && isspace(*p));
		}
		/* find value start */
		if (value) {
			do {
				value++;
			} while (isspace(*value));
		}

		/* all client keys start with "nfs.client." */
		if (strncmp(key, "nfs.client.", 11)) {
			if (verbose > 1) {
				printf("%4ld %s=%s\n", linenum, key, value ? value : "");
			}
			continue;
		}
		val = !value ? 1 : strtol(value, NULL, 0);
		if (verbose) {
			printf("%4ld %s=%s (%ld)\n", linenum, key, value ? value : "", val);
		}

		if (!strcmp(key, "nfs.client.access_cache_timeout")) {
			if (value && val) {
				conf->access_cache_timeout = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.access_for_getattr")) {
			conf->access_for_getattr = (int)val;
		} else if (!strcmp(key, "nfs.client.allow_async")) {
			conf->allow_async = (int)val;
		} else if (!strcmp(key, "nfs.client.callback_port")) {
			if (value && val) {
				conf->callback_port = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.initialdowndelay")) {
			conf->initialdowndelay = (int)val;
		} else if (!strcmp(key, "nfs.client.iosize")) {
			if (value && val) {
				conf->iosize = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.mount.options")) {
			handle_mntopts(value);
		} else if (!strcmp(key, "nfs.client.nextdowndelay")) {
			conf->nextdowndelay = (int)val;
		} else if (!strcmp(key, "nfs.client.nfsiod_thread_max")) {
			if (value) {
				conf->nfsiod_thread_max = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.statfs_rate_limit")) {
			if (value && val) {
				conf->statfs_rate_limit = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.is_mobile")) {
			conf->is_mobile = (int)val;
		} else if (!strcmp(key, "nfs.client.squishy_flags")) {
			if (value && val) {
				conf->squishy_flags = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.root_steals_gss_context")) {
			if (value && val) {
				conf->root_steals_gss_context = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.default_nfs4domain")) {
			if (value) {
				conf->default_nfs4domain = strndup(value, MAXPATHLEN);
			}
		} else if (!strcmp(key, "nfs.client.mount_timeout")) {
			if (value && val) {
				conf->mount_timeout = (int)val;
			}
		} else if (!strcmp(key, "nfs.client.mount_quick_timeout")) {
			if (value && val) {
				conf->mount_quick_timeout = (int)val;
			}
		} else {
			if (verbose) {
				printf("ignoring unknown config value: %4ld %s=%s\n", linenum, key, value ? value : "");
			}
		}
	}

	fclose(f);
	return 0;
}

/* set a sysctl config value */
int
sysctl_set(const char *name, int val)
{
	return sysctlbyname(name, NULL, 0, &val, sizeof(val));
}


/*
 * Includes needed for IOKit
 */
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

/*
 * If a machine has an internal battery we consider it a mobile machine
 *
 * Shamelessly stolen from Buddy, OS Installer, and Migration with slight modification to turn it into C
 * and to check for an internal battery.
 * N.B. Needs to be compiled with -framework CoreFoundation -framework IOKit
 *
 * We have an internal battery if it is present and
 * kIOPSTypeKey == kIOPSInternalBatteryType
 */
static bool
machineHasInternalBattery(void)
{
	bool battery = false;
	CFTypeRef psInfo = IOPSCopyPowerSourcesInfo();

	if (!psInfo) {
		return false;
	}

	CFArrayRef psArray = IOPSCopyPowerSourcesList(psInfo);
	if (psArray == NULL) {
		CFRelease(psInfo);
		return false;
	}
	CFIndex count = CFArrayGetCount(psArray);
	int i;

	for (i = 0; i < count; ++i) {
		CFTypeRef value = CFArrayGetValueAtIndex(psArray, i);
		if (value) {
			CFDictionaryRef psDetailed = IOPSGetPowerSourceDescription(psInfo, value);
			CFBooleanRef presentState = CFDictionaryGetValue(psDetailed, CFSTR(kIOPSIsPresentKey));
			if (presentState && (CFGetTypeID(presentState) == CFBooleanGetTypeID()) && CFBooleanGetValue(presentState)) {
				CFStringRef psType = CFDictionaryGetValue(psDetailed, CFSTR(kIOPSTypeKey));
				if (psType && (CFGetTypeID(psType) == CFStringGetTypeID())) {
					if (CFStringCompare(psType, CFSTR(kIOPSInternalBatteryType), 0) == kCFCompareEqualTo) {
						battery = true;
						break;
					}
				}
			}
		}
	}

	CFRelease(psArray);
	CFRelease(psInfo);

	return battery;
}

/*
 * Identify a mobile device if it has a battery or
 * via the hardware model identifier,
 * e.g. "MacBookAir1,1"
 *
 * N.B. Using the hardware model is legacy.
 */
static int
mobile_client(void)
{
	char model[128];
	size_t len = sizeof(model);

	if (machineHasInternalBattery()) {
		return 1;
	}

	if (sysctlbyname("hw.model", &model, &len, NULL, 0) < 0 || len <= 0) {
		return 0;
	}

	return strnstr(model, "Book", len) != NULL;
}


void
config_sysctl(void)
{
	if (config.access_cache_timeout != -1) {
		sysctl_set("vfs.generic.nfs.client.access_cache_timeout", config.access_cache_timeout);
	}
	if (config.access_for_getattr != -1) {
		sysctl_set("vfs.generic.nfs.client.access_for_getattr", config.access_for_getattr);
	}
	if (config.allow_async != -1) {
		sysctl_set("vfs.generic.nfs.client.allow_async", config.allow_async);
	}
	if (config.callback_port != -1) {
		sysctl_set("vfs.generic.nfs.client.callback_port", config.callback_port);
	}
	if (config.initialdowndelay != -1) {
		sysctl_set("vfs.generic.nfs.client.initialdowndelay", config.initialdowndelay);
	}
	if (config.iosize != -1) {
		sysctl_set("vfs.generic.nfs.client.iosize", config.iosize);
	}
	if (config.nextdowndelay != -1) {
		sysctl_set("vfs.generic.nfs.client.nextdowndelay", config.nextdowndelay);
	}
	if (config.nfsiod_thread_max != -1) {
		sysctl_set("vfs.generic.nfs.client.nfsiod_thread_max", config.nfsiod_thread_max);
	}
	if (config.statfs_rate_limit != -1) {
		sysctl_set("vfs.generic.nfs.client.statfs_rate_limit", config.statfs_rate_limit);
	}
	if (config.is_mobile == -1) {
		config.is_mobile = mobile_client();
	}
	sysctl_set("vfs.generic.nfs.client.is_mobile", config.is_mobile);
	if (config.squishy_flags != -1) {
		sysctl_set("vfs.generic.nfs.client.squishy_flags", config.squishy_flags);
	}
	if (config.root_steals_gss_context != -1) {
		sysctl_set("vfs.generic.nfs.client.root_steals_gss_context", config.root_steals_gss_context);
	}
	if (config.mount_timeout != -1) {
		sysctl_set("vfs.generic.nfs.client.mount_timeout", config.mount_timeout);
	}
	if (config.mount_quick_timeout != -1) {
		sysctl_set("vfs.generic.nfs.client.mount_quick_timeout", config.mount_quick_timeout);
	}
	if (config.default_nfs4domain != NULL) {
		sysctlbyname("vfs.generic.nfs.client.default_nfs4domain", NULL, 0, config.default_nfs4domain, strnlen(config.default_nfs4domain, MAXPATHLEN));
	}
}

/*
 * Parse the mntfrom argument into a set of file system
 * locations (host/path parts).
 *
 * We support specifying multiple servers that can be used to back this
 * file system in the event that a server is (or becomes) unavailable.
 *
 * Examples of how the file system can be specified:
 *
 *     host:/path
 *     host1,host2,host3:/path
 *     host1,host2,host3:/path1,host4:/path2
 *     host1:/path1,host2,host3:/path2,host4,host5,host6:/path3,host7:/path2
 *
 * Comma followed by a colon(slash) indicates multiple locations are specified.
 * The list of hosts for a location is comma separated.  The path is terminated
 * by the next comma or the end of the string.
 *
 * If a host starts with '[' then skip IPv6 literal characters
 * until we find ']'.  If we find other characters (or the
 * closing ']' isn't followed by a ':', then don't consider
 * it to be an IPv6 literal address.
 *
 * Scan the string to find the next colon(slash) or comma.
 * The (next) host is the portion of the string preceding that token.
 * Once a colon(slash) is found, grab the path following the colon and
 * then move on to the next location.
 */
int
parse_fs_locations(const char *mntfrom, struct nfs_fs_location **nfslp)
{
	struct nfs_fs_location *nfslhead = NULL;
	struct nfs_fs_location *nfslprev = NULL;
	struct nfs_fs_location *nfsl = NULL;
	struct nfs_fs_server *nfss = NULL, *nfssprev;
	struct sockaddr_in6 sin6;
	char *argcopy, *p, *q, *host, *path, *colon, *colonslash, ch;
	int error = 0;

	*nfslp = NULL;

	argcopy = strdup(mntfrom);
	if (!argcopy) {
		return ENOMEM;
	}
	p = argcopy;

	while (*p) {
		/* allocate a new fs location */
		if (!((nfsl = malloc(sizeof(*nfsl))))) {
			error = ENOMEM;
			goto outfree;
		}
		bzero(nfsl, sizeof(*nfsl));
		colon = colonslash = NULL;

		/* parse hosts */
		nfssprev = NULL;
		while (*p) {
			if (!((nfss = malloc(sizeof(*nfss))))) {
				error = ENOMEM;
				goto outfree;
			}
			bzero(nfss, sizeof(*nfss));
			if (is_ftp(p)) {
				host = LOCALHOST_ADDR;
				goto addhost;
			}
			host = p;
			if (*p == '[') {  /* Looks like an IPv6 literal address */
				p++;
				while (isxdigit(*p) || (*p == ':')) {
					if (*p == ':') {
						if (!colon) {
							colon = p;
						}
						if (!colonslash && (*(p + 1) == '/')) {
							colonslash = p;
						}
					}
					p++;
				}
				if ((*p == ']') && ((*(p + 1) == ',') || (*(p + 1) == ':'))) {
					/* Found "[IPv6]:", double check that it's acceptable and use it. */
					ch = *p;
					*p = '\0';
					if (inet_pton(AF_INET6, host + 1, &sin6)) { /* It was a valid IPv6 literal */
						*p = ch;
						p++;
						goto addhost;
					}
					/* It wasn't a valid IPv6 literal, move on */
					*p = ch;
					p++;
				}
			}
			/* if end of host not found yet, search for "," or ":/" or ":" */
			while (*p && (!colon || !colonslash)) {
				if (*p == ':') {
					if (!colon) {
						colon = p;
					}
					if (!colonslash && (*(p + 1) == '/')) {
						break;
					}
				}
				if (*p == ',') { /* we have a host, add it to the list */
					break;
				}
				p++;
			}
			if (!*p) {
				/* If we hit the end of the string, the host must be the string preceding the colon. */
				if (!colon) { /* No colon?! */
					error = EINVAL;
					goto outfree;
				}
				p = colon;
			}
addhost:
			/* We have a host, add it to the list. */
			/* (p points to the next character (comma or colon)) */
			ch = *p;
			*p = '\0';
			nfss->ns_name = strdup(host);
			*p = ch;
			if (!nfss->ns_name) {
				error = ENOMEM;
				goto outfree;
			}
			/* Add the host to the list of servers. */
			if (nfssprev) {
				nfssprev->ns_next = nfss;
			} else {
				nfsl->nl_servers = nfss;
			}
			nfsl->nl_servcnt++;
			nfssprev = nfss;
			nfss = NULL;
			if (*p != ',') { /* move on to get the path */
				if (!is_ftp(p)) {
					p++;
				}
				break;
			}
			p++;
			/* forget about any previously-seen colon */
			colon = colonslash = NULL;
			/* get next host */
		}
		if (!*p) {
			/* If we hit the end of the string, the path must be the string following the colon. */
			if (!colon) { /* No colon?! */
				error = EINVAL;
				goto outfree;
			}
			p = colon;
			p++;
		}
		/*
		 * Parse the path.
		 * Find a comma (with a colon somewhere after it) or the end of the string.
		 */
		path = p;
		while (*p) {
			if (*p == ',') {
				/* It looks like we have hit the end of the path, start of the next location. */
				/* But first, verify that there's a colon after the comma. */
				q = p + 1;
				while (*q && (*q != ':')) {
					q++;
				}
				if (!*q) { /* No colon, rest of string is all path. */
					p = q;
				}
				break;
			}
			p++;
		}
		/* Add the path to fs location. */
		ch = *p;
		*p = '\0';
		nfsl->nl_path = strdup(path);
		*p = ch;
		if (!nfsl->nl_path) {
			error = ENOMEM;
			goto outfree;
		}
		/* Add the fs location to the list of locations. */
		if (nfslprev) {
			nfslprev->nl_next = nfsl;
		} else {
			nfslhead = nfsl;
		}
		nfslprev = nfsl;
		nfsl = NULL;
		/* get the next fs location */
		if (*p == ',') {
			p++;
		}
	}

	if (!nfslhead) {
		error = EINVAL;
		goto outfree;
	}

	free(argcopy);
	*nfslp = nfslhead;
	return 0;

outfree:
	if (nfsl) {
		free(nfsl);
	}
	if (nfss) {
		free(nfss);
	}
	while (nfslhead) {
		while (nfslhead->nl_servers) {
			nfss = nfslhead->nl_servers->ns_next;
			free(nfslhead->nl_servers);
			nfslhead->nl_servers = nfss;
		}
		nfsl = nfslhead->nl_next;
		free(nfslhead);
		nfslhead = nfsl;
	}

	free(argcopy);
	return error;
}

/*
 * Compare the addresses in two addrinfo structures.
 */
static int
addrinfo_cmp(struct addrinfo *a1, struct addrinfo *a2)
{
	if (a1->ai_family != a2->ai_family) {
		return a1->ai_family - a2->ai_family;
	}
	if (a1->ai_addrlen != a2->ai_addrlen) {
		return a1->ai_addrlen - a2->ai_addrlen;
	}
	return bcmp(a1->ai_addr, a2->ai_addr, a1->ai_addrlen);
}

/*
 * Resolve the given host name to a list of usable addresses
 */
int
getaddresslist(struct nfs_fs_server *nfss)
{
	struct addrinfo aihints, *ailist = NULL, *ai, *aiprev, *ainext, *aidiscard;
	char *hostname, namebuf[NI_MAXHOST];
	void *sinaddr;
	char uaddr[128];
	const char *uap;
	int local = 0;

	nfss->ns_ailist = NULL;
	hostname = nfss->ns_name;
	if ((hostname[0] == '[') && (hostname[strlen(hostname) - 1] == ']')) {
		/* Looks like an IPv6 literal */
		strlcpy(namebuf, hostname + 1, sizeof(namebuf));
		namebuf[strlen(namebuf) - 1] = '\0';
		hostname = namebuf;
	} else if (*hostname == '\0' || ((hostname[0] == '<') && (hostname[strlen(hostname) - 1] == '>'))) {
		/* Looks like an AF_LOCAL address */
		struct sockaddr_un *un;

		if (*hostname == '<') {
			strlcpy(namebuf, hostname, sizeof(namebuf));
			hostname = namebuf;
			hostname[strlen(hostname) - 1] = '\0';
			hostname += 1;
		}
		ailist = malloc(sizeof(struct addrinfo));
		if (ailist == NULL) {
			warnx("Can't allocate addrinfo sturcture for local address for %s", hostname);
			return ENOMEM;
		}
		un = (struct sockaddr_un *)malloc(sizeof(struct sockaddr_un));
		if (un == NULL) {
			free(ailist);
			warnx("Can't allocate local socket address for %s", hostname);
			return ENOMEM;
		}
		un->sun_len = sizeof(struct sockaddr_un);
		un->sun_family = AF_LOCAL;
		if (verbose > 2) {
			printf("hostname is <%s> : %s\n", hostname, options.local_nfs_port);
		}
		if (*hostname != '\0') {
			if (options.local_nfs_port) {
				if (strcmp(options.local_nfs_port, hostname)) {
					warnx("%s overrides port=%s", hostname, options.local_nfs_port);
				}
				free(options.local_nfs_port);
			}
			options.local_nfs_port = strdup(hostname);
			if (options.local_nfs_port == NULL) {
				nfs_err(1, "No memory to duplicate hostname %s: %s", options.local_nfs_port, strerror(1));
			}
			NFS_BITMAP_SET(options.mattrs, NFS_MATTR_LOCAL_NFS_PORT);
			if (verbose > 2) {
				printf("Setting local_nfs_port to %s\n", options.local_nfs_port);
			}
		}
		un->sun_path[0] = '\0';
		local = 1;
		ailist->ai_flags = 0;
		ailist->ai_family = AF_LOCAL;
		ailist->ai_addrlen = sizeof(struct sockaddr_un);
		ailist->ai_socktype = options.socket_type ? options.socket_type : SOCK_STREAM;
		ailist->ai_protocol = 0;
		ailist->ai_addr = (struct sockaddr *)un;
		ailist->ai_canonname = NULL;
		ailist->ai_next = NULL;
	}

	if (!local) {
		bzero(&aihints, sizeof(aihints));
		aihints.ai_flags = AI_ADDRCONFIG;
		aihints.ai_socktype = options.socket_type;
		char *socketaddr = options.force_localhost ? LOCALHOST_ADDR : hostname;
		if (getaddrinfo(socketaddr, NULL, &aihints, &ailist)) {
			warnx("can't resolve host: %s", socketaddr);
			return ENOENT;
		}
	}

	/* strip out addresses that don't match the mount options given */
	aidiscard = NULL;
	aiprev = NULL;

	for (ai = ailist; ai; ai = ainext) {
		ainext = ai->ai_next;

		/* If socket_family is set, eliminate addresses that aren't of that family. */
		if (options.socket_family && (ai->ai_family != options.socket_family)) {
			goto discard;
		}

		/* If socket_type is set, eliminate addresses that aren't of that type. */
		/* (This should have been taken care of by specifying the type above.) */
		if (options.socket_type && (ai->ai_socktype != options.socket_type)) {
			goto discard;
		}

		/* If nfs_version is set >= 4, eliminate DGRAM addresses. */
		if ((options.nfs_version >= 4) && (ai->ai_socktype == SOCK_DGRAM)) {
			goto discard;
		}

		/* eliminate unknown protocol families */
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6) && (ai->ai_family != AF_LOCAL)) {
			goto discard;
		}

		/* If socket_type is not set, eliminate duplicate addresses with different socktypes. */
		if (!options.socket_type && aiprev &&
		    (ai->ai_socktype != aiprev->ai_socktype) &&
		    !addrinfo_cmp(aiprev, ai)) {
			goto discard;
		}

		aiprev = ai;
		if (verbose > 2) {
			if (ai->ai_family == AF_LOCAL) {
				uap = ((struct sockaddr_un *)ai->ai_addr)->sun_path;
				printf("usable address: %s %s\n",
				    (ai->ai_socktype == SOCK_DGRAM) ? "ticlts" :
				    (ai->ai_socktype = SOCK_STREAM) ? "ticotsord" : "unknown",
				    uap);
			} else {
				if (ai->ai_family == AF_INET) {
					sinaddr = &((struct sockaddr_in*) AOK ai->ai_addr)->sin_addr;
				} else {
					sinaddr = &((struct sockaddr_in6*) AOK ai->ai_addr)->sin6_addr;
				}
				uap = inet_ntop(ai->ai_family, sinaddr, uaddr, sizeof(uaddr));
				printf("usable address: %s %s %s\n",
				    (ai->ai_socktype == SOCK_DGRAM) ? "udp" :
				    (ai->ai_socktype == SOCK_STREAM) ? "tcp" : "???",
				    (ai->ai_family == AF_INET) ? "inet" :
				    (ai->ai_family == AF_INET6) ? "inet6" : "???",
				    uap ? uap : "???");
			}
		}
		continue;
discard:
		/* Add ai to the discard list */
		if (aiprev) {
			aiprev->ai_next = ai->ai_next;
		} else {
			ailist = ai->ai_next;
		}
		ai->ai_next = aidiscard;
		aidiscard = ai;
		if (verbose > 2) {
			if (ai->ai_family == AF_LOCAL) {
				uap = ((struct sockaddr_un *)ai->ai_addr)->sun_path;
				printf("discard address: %s %s\n",
				    (ai->ai_socktype == SOCK_DGRAM) ? "ticlts" :
				    (ai->ai_socktype = SOCK_STREAM) ? "ticotsord" : "unknown",
				    uap);
			} else {
				if (ai->ai_family == AF_INET) {
					sinaddr = &((struct sockaddr_in*) AOK ai->ai_addr)->sin_addr;
				} else {
					sinaddr = &((struct sockaddr_in6*) AOK ai->ai_addr)->sin6_addr;
				}
				uap = inet_ntop(ai->ai_family, sinaddr, uaddr, sizeof(uaddr));
				printf("discard address: %s %s %s\n",
				    (ai->ai_socktype == SOCK_DGRAM) ? "udp" :
				    (ai->ai_socktype == SOCK_STREAM) ? "tcp" : "???",
				    (ai->ai_family == AF_INET) ? "inet" :
				    (ai->ai_family == AF_INET6) ? "inet6" : "???",
				    uap ? uap : "???");
			}
		}
	}

	/* free up any discarded addresses */
	if (aidiscard) {
		free(aidiscard);
	}

	nfss->ns_ailist = ailist;
	return 0;
}

/*
 * Resolve each server to a list of usable addresses.
 */
void
getaddresslists(struct nfs_fs_location *nfsl, int *servcnt)
{
	struct nfs_fs_server *nfss;
	int error;

	*servcnt = 0;

	while (nfsl) {
		nfss = nfsl->nl_servers;
		while (nfss) {
			error = getaddresslist(nfss);
			if (!error && nfss->ns_ailist) {
				*servcnt += 1;
			}
			nfss = nfss->ns_next;
		}
		nfsl = nfsl->nl_next;
	}
}

/*
 * Free up the addresses for all the servers.
 */
void
freeaddresslists(struct nfs_fs_location *nfsl)
{
	struct nfs_fs_server *nfss;

	while (nfsl) {
		nfss = nfsl->nl_servers;
		while (nfss) {
			if (nfss->ns_ailist) {
				freeaddrinfo(nfss->ns_ailist);
				nfss->ns_ailist = NULL;
			}
			nfss = nfss->ns_next;
		}
		nfsl = nfsl->nl_next;
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: mount_nfs [-o options] server:/path directory\n");
	exit(EINVAL);
}

/* Map from mount options to printable formats. */
static struct opt {
	int o_opt;
	const char *o_name;
} optnames[] = {
	{ MNT_ASYNC, "asynchronous" },
	{ MNT_EXPORTED, "NFS exported" },
	{ MNT_LOCAL, "local" },
	{ MNT_NODEV, "nodev" },
	{ MNT_NOEXEC, "noexec" },
	{ MNT_NOSUID, "nosuid" },
	{ MNT_QUOTA, "with quotas" },
	{ MNT_RDONLY, "read-only" },
	{ MNT_SYNCHRONOUS, "synchronous" },
	{ MNT_UNION, "union" },
	{ MNT_AUTOMOUNTED, "automounted" },
	{ MNT_JOURNALED, "journaled" },
	{ MNT_DEFWRITE, "defwrite" },
	{ MNT_IGNORE_OWNERSHIP, "noowners" },
	{ MNT_NOATIME, "noatime" },
	{ MNT_QUARANTINE, "quarantine" },
	{ MNT_DONTBROWSE, "nobrowse" },
	{ MNT_CPROTECT, "protect"},
	{ MNT_ROOTFS, "rootfs"},
	{ MNT_DOVOLFS, "dovolfs"},
	{ MNT_NOUSERXATTR, "nouserxattr"},
	{ MNT_MULTILABEL, "multilabel"},
	{ 0, NULL }
};

static const char *
sec_flavor_name(uint32_t flavor)
{
	switch (flavor) {
	case RPCAUTH_NONE:      return "none";
	case RPCAUTH_SYS:       return "sys";
	case RPCAUTH_KRB5:      return "krb5";
	case RPCAUTH_KRB5I:     return "krb5i";
	case RPCAUTH_KRB5P:     return "krb5p";
	default:                return "?";
	}
}

static const char *
etype_name(uint32_t etype)
{
	switch (etype) {
	case NFS_DES3_CBC_SHA1_KD:              return "des3-cbc-sha1-kd";
	case NFS_AES128_CTS_HMAC_SHA1_96:       return "aes128-cts-hmac-sha1-96";
	case NFS_AES256_CTS_HMAC_SHA1_96:       return "aes256-cts-hmac-sha1-96";
	default:                                return "?";
	}
}

void
dump_mount_options(struct nfs_fs_location *nfslhead, char *mntonname)
{
	struct nfs_fs_location *nfsl;
	struct nfs_fs_server *nfss;
	struct addrinfo *ai;
	void *sinaddr;
	char uaddr[128];
	const char *uap;
	struct opt *o;
	int flags, i;

	printf("mount %s on %s\n", mntfromarg, mntonname);

	flags = options.mntflags;
	printf("mount flags: 0x%x", flags);
	for (o = optnames; flags && o->o_opt; o++) {
		if (flags & o->o_opt) {
			printf(", %s", o->o_name);
			flags &= ~o->o_opt;
		}
	}
	printf("\n");

	switch (options.socket_family) {
	case AF_LOCAL:
		printf("socket: type:%s", ((options.socket_type == SOCK_STREAM) ? "ticotsord" :
		    (options.socket_type == SOCK_DGRAM) ? "ticlts" : "unknown"));
		break;
	case AF_INET:
	case AF_INET6:
	case AF_UNSPEC:
		printf("socket: type:%s", ((options.socket_type == SOCK_STREAM) ? "tcp" :
		    (options.socket_type == SOCK_DGRAM) ? "udp" : "any"));
		if (options.socket_family != AF_UNSPEC) {
			printf("%s%s", (options.socket_type ? "" : ",inet"),
			    ((options.socket_family == AF_INET) ? "4" :
			    (options.socket_family == AF_INET6) ? "6" : ""));
		}
		break;
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_NFS_PORT)) {
		printf(",port=%d", options.nfs_port);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_MOUNT_PORT)) {
		printf(",mountport=%d", options.mount_port);
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_MNTUDP) || (verbose > 1)) {
		printf(",%smntudp", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_MNTUDP) ? "" : "no");
	}
	printf("\n");
	printf("file system locations:\n");
	for (nfsl = nfslhead; nfsl; nfsl = nfsl->nl_next) {
		printf("%s\n", nfsl->nl_path);
		for (nfss = nfsl->nl_servers; nfss; nfss = nfss->ns_next) {
			printf("  %s\n", nfss->ns_name);
			for (ai = nfss->ns_ailist; ai; ai = ai->ai_next) {
				switch (ai->ai_family) {
				case AF_INET:
				case AF_INET6:
					if (ai->ai_family == AF_INET) {
						sinaddr = &((struct sockaddr_in*) AOK ai->ai_addr)->sin_addr;
					} else {
						sinaddr = &((struct sockaddr_in6*) AOK ai->ai_addr)->sin6_addr;
					}
					uap = inet_ntop(ai->ai_family, sinaddr, uaddr, sizeof(uaddr));
					printf("    %s %s\n",
					    (ai->ai_family == AF_INET) ? "inet" : "inet6",
					    uap ? uap : "???");
					break;
				case AF_LOCAL:
					printf("    local transport %s\n", ((struct sockaddr_un *)ai->ai_addr)->sun_path);
					break;
				default:
					printf("    ??? ???\n");
				}
			}
		}
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_FH)) {
		printf("fh %d ", options.fh.fh_len);
		for (i = 0; i < (int)options.fh.fh_len; i++) {
			printf("%02x", options.fh.fh_data[i] & 0xff);
		}
		printf("\n");
	}

	printf("NFS options:");
	printf(" %s,retrycnt=%d", ((opflags & BGRND) ? "bg" : "fg"), retrycnt);
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_NFS_VERSION)) {
		printf(",vers=%d", options.nfs_version);
		if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_NFS_MINOR_VERSION)) {
			printf(".%d", options.nfs_minor_version);
		}
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_NFS_VERSION_RANGE)) {
		printf(",vers=%d", PVER2MAJOR(options.nfs_min_vers));
		if (PVER2MINOR(options.nfs_min_vers)) {
			printf(".%d", PVER2MINOR(options.nfs_min_vers));
		}
		printf("-%d", PVER2MAJOR(options.nfs_max_vers));
		if (PVER2MINOR(options.nfs_max_vers)) {
			printf(".%d", PVER2MINOR(options.nfs_max_vers));
		}
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_SOFT) || (verbose > 1)) {
		printf(",%s", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_SOFT) ? "soft" : "hard");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_INTR) || (verbose > 1)) {
		printf(",%sintr", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_INTR) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_RESVPORT) || (verbose > 1)) {
		printf(",%sresvport", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_RESVPORT) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NOCONNECT) || (verbose > 1)) {
		printf(",%sconn", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NOCONNECT) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NOCALLBACK) || (verbose > 1)) {
		printf(",%scallback", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NOCALLBACK) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NONEGNAMECACHE) || (verbose > 1)) {
		printf(",%snegnamecache", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NONEGNAMECACHE) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NAMEDATTR) || (verbose > 1)) {
		printf(",%snamedattr", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NAMEDATTR) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NOACL) || (verbose > 1)) {
		printf(",%sacl", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NOACL) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_ACLONLY) || (verbose > 1)) {
		printf(",%saclonly", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_ACLONLY) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_CALLUMNT) || (verbose > 1)) {
		printf(",%scallumnt", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_CALLUMNT) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_LOCK_MODE) || (verbose > 1)) {
		switch (options.lockmode) {
		case NFS_LOCK_MODE_ENABLED:
			printf(",locks");
			break;
		case NFS_LOCK_MODE_DISABLED:
			printf(",nolocks");
			break;
		case NFS_LOCK_MODE_LOCAL:
			printf(",locallocks");
			break;
		}
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NOQUOTA) || (verbose > 1)) {
		printf(",%squota", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NOQUOTA) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READ_SIZE) || (verbose > 1)) {
		printf(",rsize=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READ_SIZE) ? options.rsize : NFS_RSIZE);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_WRITE_SIZE) || (verbose > 1)) {
		printf(",wsize=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_WRITE_SIZE) ? options.wsize : NFS_WSIZE);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READAHEAD) || (verbose > 1)) {
		printf(",readahead=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READAHEAD) ? options.readahead : NFS_DEFRAHEAD);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READDIR_SIZE) || (verbose > 1)) {
		printf(",dsize=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READDIR_SIZE) ? options.readdirsize : NFS_READDIRSIZE);
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_RDIRPLUS) || (verbose > 1)) {
		printf(",%srdirplus", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_RDIRPLUS) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_DUMBTIMER) || (verbose > 1)) {
		printf(",%sdumbtimer", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_DUMBTIMER) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_REQUEST_TIMEOUT) || (verbose > 1)) {
		printf(",timeo=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_REQUEST_TIMEOUT) ?
		    ((options.request_timeout.tv_sec * 10) + (options.request_timeout.tv_nsec % 100000000)) : 10);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_SOFT_RETRY_COUNT) || (verbose > 1)) {
		printf(",retrans=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_SOFT_RETRY_COUNT) ? options.soft_retry_count : NFS_RETRANS);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_MAX_GROUP_LIST) || (verbose > 1)) {
		printf(",maxgroups=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_MAX_GROUP_LIST) ? options.maxgrouplist : NFS_MAXGRPS);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN) || (verbose > 1)) {
		printf(",acregmin=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MIN) ? options.acregmin.tv_sec : NFS_MINATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX) || (verbose > 1)) {
		printf(",acregmax=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_REG_MAX) ? options.acregmax.tv_sec : NFS_MAXATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN) || (verbose > 1)) {
		printf(",acdirmin=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN) ? options.acdirmin.tv_sec : NFS_MINATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX) || (verbose > 1)) {
		printf(",acdirmax=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX) ? options.acdirmax.tv_sec : NFS_MAXATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN) || (verbose > 1)) {
		printf(",acrootdirmin=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MIN) ? options.acrootdirmin.tv_sec : NFS_MINATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX) || (verbose > 1)) {
		printf(",acrootdirmax=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ATTRCACHE_ROOTDIR_MAX) ? options.acrootdirmax.tv_sec : NFS_MAXATTRTIMO);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_DEAD_TIMEOUT) || (verbose > 1)) {
		printf(",deadtimeout=%ld", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_DEAD_TIMEOUT) ? options.dead_timeout.tv_sec : 0);
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_MUTEJUKEBOX) || (verbose > 1)) {
		printf(",%smutejukebox", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_MUTEJUKEBOX) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_EPHEMERAL) || (verbose > 1)) {
		printf(",%sephemeral", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_EPHEMERAL) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NFC) || (verbose > 1)) {
		printf(",%snfc", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NFC) ? "" : "no");
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_SECURITY)) {
		printf(",sec=%s", sec_flavor_name(options.sec.flavors[0]));
		for (i = 1; i < options.sec.count; i++) {
			printf(":%s", sec_flavor_name(options.sec.flavors[i]));
		}
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_KERB_ETYPE)) {
		printf(",etype=%s", etype_name(options.etype.etypes[0]));
		for (uint32_t j = 1; j < options.etype.count; j++) {
			printf(":%s", etype_name(options.etype.etypes[j]));
		}
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_REALM)) {
		printf(",realm=%s", options.realm);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_PRINCIPAL)) {
		printf(",principal=%s", options.principal);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_SVCPRINCIPAL)) {
		printf(",sprincipal=%s", options.sprinc);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READLINK_NOCACHE) || (verbose > 1)) {
		printf(",readlink_nocache=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_READLINK_NOCACHE) ? options.readlink_nocache : 0);
	}
	if (NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ACCESS_CACHE)) {
		printf(",accesscache=%d", NFS_BITMAP_ISSET(options.mattrs, NFS_MATTR_ACCESS_CACHE) ? options.access_cache : 0);
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_NOOPAQUE_AUTH) || (verbose > 1)) {
		printf(",%sopaque_auth", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_NOOPAQUE_AUTH) ? "no" : "");
	}
	if (NFS_BITMAP_ISSET(options.mflags_mask, NFS_MFLAG_SKIP_RENEW) || (verbose > 1)) {
		printf(",%sskip_renew", NFS_BITMAP_ISSET(options.mflags, NFS_MFLAG_SKIP_RENEW) ? "" : "no");
	}
	printf("\n");
}

enum clnt_stat
ping_rpc_statd(struct sockaddr *raddr)
{
	CLIENT *clnt;
	struct timeval to = {10, 0};
	int sock = RPC_ANYSOCK;
	rpc_createerr.cf_stat = RPC_SUCCESS;

	if (options.socket_type == SOCK_DGRAM) {
		clnt = clntudp_bufcreate_timeout(raddr, RPCPROG_STAT, RPCMNT_VER1, &sock, SENDSZ, RECVSZ, NULL, &to);
	} else {
		clnt = clnttcp_create_timeout(raddr, RPCPROG_STAT, RPCMNT_VER1, &sock, SENDSZ, RECVSZ, NULL, &to);
	}

	if (clnt != NULL) {
		clnt_destroy(clnt);
	}

	return rpc_createerr.cf_stat;
}
