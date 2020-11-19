/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)create.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/create.c,v 1.37 2005/03/29 11:44:17 tobez Exp $");

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <grp.h>
#ifndef __APPLE__
#ifdef ENABLE_MD5
#include <md5.h>
#endif
#ifdef ENABLE_SHA1
#include <sha.h>
#endif
#ifdef ENABLE_RMD160
#include <ripemd.h>
#endif
#ifdef ENABLE_SHA256
#include <sha256.h>
#endif
#endif /* !__APPLE__ */
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>
#include "metrics.h"
#include "mtree.h"
#include "extern.h"

#ifdef __APPLE__
#include "commoncrypto.h"
#endif /* __APPLE__ */

#define	INDENTNAMELEN	15
#define	MAXLINELEN	80

static gid_t gid;
static uid_t uid;
static mode_t mode;
static u_long flags = 0xffffffff;
static char *xattrs = kNone;
static char *acl = kNone;
static u_quad_t xdstream_id;

static int	dsort(const FTSENT **, const FTSENT **);
static void	output(int, int *, const char *, ...) __printflike(3, 4);
static int	statd(FTS *, FTSENT *, uid_t *, gid_t *, mode_t *, u_long *, char **, char **, u_quad_t *);
static void	statf(int, FTSENT *);

void
cwalk(void)
{
	int error = 0;
	FTS *t;
	FTSENT *p;
	time_t cl;
	char *argv[2], host[MAXHOSTNAMELEN];
	char dot[] = ".";
	int indent = 0;
	char *path;

	if (!nflag) {
		(void)time(&cl);
		(void)gethostname(host, sizeof(host));
		(void)printf(
		    "#\t   user: %s\n#\tmachine: %s\n",
		    getlogin(), host);
		(void)printf(
		    "#\t   tree: %s\n#\t   date: %s",
		    fullpath, ctime(&cl));
	}

	argv[0] = dot;
	argv[1] = NULL;
	if ((t = fts_open(argv, ftsoptions, dsort)) == NULL) {
		error = errno;
		RECORD_FAILURE(76, error);
		errc(1, error, "fts_open()");
	}
	while ((p = fts_read(t))) {
		if (iflag)
			indent = p->fts_level * 4;
		if (check_excludes(p->fts_name, p->fts_path)) {
			fts_set(t, p, FTS_SKIP);
			continue;
		}
		switch(p->fts_info) {
		case FTS_D:
			if (!dflag)
				(void)printf("\n");
			if (!nflag) {
				path = escape_path(p->fts_path);
				(void)printf("# %s\n", path);
				free(path);
			}
			statd(t, p, &uid, &gid, &mode, &flags, &xattrs, &acl, &xdstream_id);
			statf(indent, p);
			break;
		case FTS_DP:
			if (!nflag && (p->fts_level > 0)) {
				path = escape_path(p->fts_path);
				(void)printf("%*s# %s\n", indent, "", path);
				free(path);
			}
			(void)printf("%*s..\n", indent, "");
			if (!dflag)
				(void)printf("\n");
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			break;
		default:
			if (!dflag)
				statf(indent, p);
			break;

		}
	}
	(void)fts_close(t);
	if (sflag && keys & F_CKSUM) {
		RECORD_FAILURE(77, WARN_CHECKSUM);
		warnx("%s checksum: %lu", fullpath, (unsigned long)crc_total);
	}
}

static void
statf(int indent, FTSENT *p)
{
	int error = 0;
	struct group *gr;
	struct passwd *pw;
	uint32_t val;
	off_t len;
	int fd, offset;
	char *fflags;
	char *escaped_name;

	escaped_name = calloc(1, p->fts_namelen * 4  +  1);
	if (escaped_name == NULL) {
		RECORD_FAILURE(78, ENOMEM);
		errx(1, "statf(): calloc() failed");
	}
	strvis(escaped_name, p->fts_name, VIS_WHITE | VIS_OCTAL | VIS_GLOB);

	if (iflag || S_ISDIR(p->fts_statp->st_mode))
		offset = printf("%*s%s", indent, "", escaped_name);
	else
		offset = printf("%*s    %s", indent, "", escaped_name);

	free(escaped_name);

	if (offset > (INDENTNAMELEN + indent))
		offset = MAXLINELEN;
	else
		offset += printf("%*s", (INDENTNAMELEN + indent) - offset, "");

	if (!S_ISREG(p->fts_statp->st_mode) && !dflag)
		output(indent, &offset, "type=%s", inotype(p->fts_statp->st_mode));
	if (p->fts_statp->st_uid != uid) {
		if (keys & F_UNAME) {
			pw = getpwuid(p->fts_statp->st_uid);
			if (pw != NULL) {
				output(indent, &offset, "uname=%s", pw->pw_name);
			} else if (wflag) {
				RECORD_FAILURE(27448, WARN_UNAME);
				warnx("Could not get uname for uid=%u",
				    p->fts_statp->st_uid);
			} else {
				RECORD_FAILURE(79, EINVAL);
				errx(1,
				    "Could not get uname for uid=%u",
				    p->fts_statp->st_uid);
			}
		}
		if (keys & F_UID)
			output(indent, &offset, "uid=%u", p->fts_statp->st_uid);
	}
	if (p->fts_statp->st_gid != gid) {
		if (keys & F_GNAME) {
			gr = getgrgid(p->fts_statp->st_gid);
			if (gr != NULL) {
				output(indent, &offset, "gname=%s", gr->gr_name);
			} else if (wflag) {
				RECORD_FAILURE(27449, WARN_UNAME);
				warnx("Could not get gname for gid=%u",
				    p->fts_statp->st_gid);
			} else {
				RECORD_FAILURE(80, EINVAL);
				errx(1,
				    "Could not get gname for gid=%u",
				    p->fts_statp->st_gid);
			}
		}
		if (keys & F_GID)
			output(indent, &offset, "gid=%u", p->fts_statp->st_gid);
	}
	if (keys & F_MODE && (p->fts_statp->st_mode & MBITS) != mode)
		output(indent, &offset, "mode=%#o", p->fts_statp->st_mode & MBITS);
	if (keys & F_NLINK && p->fts_statp->st_nlink != 1)
		output(indent, &offset, "nlink=%u", p->fts_statp->st_nlink);
	if (keys & F_SIZE)
		output(indent, &offset, "size=%jd",
		    (intmax_t)p->fts_statp->st_size);
	if (keys & F_TIME) {
		if (tflag && !insert_mod) {
			output(indent, &offset, "time=%ld.%09ld",
			       (long)ts.tv_sec, ts.tv_nsec);
			insert_mod = 1;
		}
		if (!tflag) {
			output(indent, &offset, "time=%ld.%09ld",
			       (long)p->fts_statp->st_mtimespec.tv_sec,
			       p->fts_statp->st_mtimespec.tv_nsec);
		}
	}
	if (keys & F_CKSUM && S_ISREG(p->fts_statp->st_mode)) {
		if ((fd = open(p->fts_accpath, O_RDONLY, 0)) < 0 ||
		    crc(fd, &val, &len)) {
			error = errno;
			RECORD_FAILURE(27450, error);
			errc(1, error, "%s", p->fts_accpath);
		}
		(void)close(fd);
		output(indent, &offset, "cksum=%lu", (unsigned long)val);
	}
#ifdef ENABLE_MD5
	if (keys & F_MD5 && S_ISREG(p->fts_statp->st_mode)) {
		char *digest, buf[33];
#ifdef __clang__
/* clang doesn't like MD5 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		digest = MD5File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!digest) {
			error = errno;
			RECORD_FAILURE(81, error);
			errc(1, error, "%s", p->fts_accpath);
		}
		output(indent, &offset, "md5digest=%s", digest);
	}
#endif /* ENABLE_MD5 */
#ifdef ENABLE_SHA1
	if (keys & F_SHA1 && S_ISREG(p->fts_statp->st_mode)) {
		char *digest, buf[41];
#ifdef __clang__
/* clang doesn't like SHA1 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		digest = SHA1_File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!digest) {
			error = errno;
			RECORD_FAILURE(82, error);
			errc(1, error, "%s", p->fts_accpath);
		}
		output(indent, &offset, "sha1digest=%s", digest);
	}
#endif /* ENABLE_SHA1 */
#ifdef ENABLE_RMD160
	if (keys & F_RMD160 && S_ISREG(p->fts_statp->st_mode)) {
		char *digest, buf[41];
#ifdef __clang__
/* clang doesn't like RIPEMD160 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		digest = RIPEMD160_File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!digest) {
			error = errno;
			RECORD_FAILURE(83, error);
			errc(1, error, "%s", p->fts_accpath);
		}
		output(indent, &offset, "ripemd160digest=%s", digest);
	}
#endif /* ENABLE_RMD160 */
#ifdef ENABLE_SHA256
	if (keys & F_SHA256 && S_ISREG(p->fts_statp->st_mode)) {
		char *digest, buf[kSHA256NullTerminatedBuffLen];

		digest = SHA256_File(p->fts_accpath, buf);
		if (!digest) {
			error = errno;
			RECORD_FAILURE(84, error);
			errc(1, error, "%s", p->fts_accpath);
		}
		output(indent, &offset, "sha256digest=%s", digest);
	}
#endif /* ENABLE_SHA256 */
	if (keys & F_SLINK &&
	    (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE)) {
		char visbuf[MAXPATHLEN * 4];
		char *s = rlink(p->fts_accpath);
		strvis(visbuf, s, VIS_WHITE | VIS_OCTAL);
		output(indent, &offset, "link=%s", visbuf);
	}
	if (keys & F_FLAGS && p->fts_statp->st_flags != flags) {
		fflags = flags_to_string(p->fts_statp->st_flags);
		output(indent, &offset, "flags=%s", fflags);
		free(fflags);
	}
	if (keys & F_BTIME) {
		if (tflag && !insert_birth) {
			output(indent, &offset, "btime=%ld.%09ld",
			       ts.tv_sec, ts.tv_nsec);
			insert_birth = 1;
		}
		if (!tflag) {
			output(indent, &offset, "btime=%ld.%09ld",
			       p->fts_statp->st_birthtimespec.tv_sec,
			       p->fts_statp->st_birthtimespec.tv_nsec);
		}
	}
	// only check access time on regular files, as traversing a folder will update its access time
	if (keys & F_ATIME && S_ISREG(p->fts_statp->st_mode)) {
		if (tflag && !insert_access) {
			output(indent, &offset, "atime=%ld.%09ld",
			       ts.tv_sec, ts.tv_nsec);
			insert_access = 1;
		}
		if (!tflag) {
			output(indent, &offset, "atime=%ld.%09ld",
			       p->fts_statp->st_atimespec.tv_sec,
			       p->fts_statp->st_atimespec.tv_nsec);
		}
	}
	if (keys & F_CTIME) {
		if (tflag && !insert_change) {
			output(indent, &offset, "ctime=%ld.%09ld",
			       ts.tv_sec, ts.tv_nsec);
			insert_change = 1;
		}
		if (!tflag) {
			output(indent, &offset, "ctime=%ld.%09ld",
			       p->fts_statp->st_ctimespec.tv_sec,
			       p->fts_statp->st_ctimespec.tv_nsec);
		}
	}
	// date added to parent folder is only supported for files and directories
	if (keys & F_PTIME && (S_ISREG(p->fts_statp->st_mode) ||
			       S_ISDIR(p->fts_statp->st_mode))) {
		int supported;
		struct timespec ptimespec = ptime(p->fts_accpath, &supported);
		if (tflag && !insert_parent) {
			output(indent, &offset, "ptime=%ld.%09ld",
			       ts.tv_sec, ts.tv_nsec);
			insert_parent = 1;
		}
		if (!tflag && supported) {
			output(indent, &offset, "ptime=%ld.%09ld",
			       ptimespec.tv_sec,
			       ptimespec.tv_nsec);
		}
	}
	if (keys & F_XATTRS) {
		char buf[kSHA256NullTerminatedBuffLen];
		xattr_info *ai;
		
		ai = SHA256_Path_XATTRs(p->fts_accpath, buf);
		if (ai && ai->digest) {
			if ((strcmp(ai->digest, xattrs) != 0) || (ai->xdstream_priv_id != xdstream_id)) {
				output(indent, &offset, "xattrsdigest=%s.%llu", ai->digest, ai->xdstream_priv_id);
			}
			free(ai);
			ai = NULL;
		}
	}
	if (keys & F_INODE) {
		output(indent, &offset, "inode=%llu", p->fts_statp->st_ino);
	}
	if (keys & F_ACL) {
		char *digest, buf[kSHA256NullTerminatedBuffLen];
		
		digest = SHA256_Path_ACL(p->fts_accpath, buf);
		if (digest && (strcmp(digest, acl) != 0)) {
			output(indent, &offset, "acldigest=%s", digest);
		}
	}
	if (keys & F_SIBLINGID) {
		uint64_t sibling_id = get_sibling_id(p->fts_accpath);
		sibling_id = (sibling_id != p->fts_statp->st_ino) ? sibling_id : 0;
		output(indent, &offset, "siblingid=%llu", sibling_id);
	}
	
	(void)putchar('\n');
}

#define	MAXGID	5000
#define	MAXUID	5000
#define	MAXMODE	MBITS + 1
#define	MAXFLAGS 256
#define	MAXS 16

static int
statd(FTS *t, FTSENT *parent, uid_t *puid, gid_t *pgid, mode_t *pmode, u_long *pflags, char **pxattrs, char **pacl, u_quad_t *xdstream_id)
{
	int error = 0;
	FTSENT *p;
	gid_t sgid;
	uid_t suid;
	mode_t smode;
	u_long sflags;
	struct group *gr;
	struct passwd *pw;
	gid_t savegid = *pgid;
	uid_t saveuid = *puid;
	mode_t savemode = *pmode;
	u_long saveflags = *pflags;
	char *savexattrs = *pxattrs;
	char *saveacl = *pacl;
	u_quad_t savexdstream_id = *xdstream_id;
	u_short maxgid, maxuid, maxmode, maxflags;
	u_short g[MAXGID], u[MAXUID], m[MAXMODE], f[MAXFLAGS];
	char *fflags;
	static int first = 1;

	if ((p = fts_children(t, 0)) == NULL) {
		error = errno;
		if (error) {
			RECORD_FAILURE(85, error);
			errc(1, error, "%s", RP(parent));
		}
		return (1);
	}

	bzero(g, sizeof(g));
	bzero(u, sizeof(u));
	bzero(m, sizeof(m));
	bzero(f, sizeof(f));

	maxuid = maxgid = maxmode = maxflags = 0;
	for (; p; p = p->fts_link) {
		if (!dflag || (dflag && S_ISDIR(p->fts_statp->st_mode))) {
			smode = p->fts_statp->st_mode & MBITS;
			if (smode < MAXMODE && ++m[smode] > maxmode) {
				savemode = smode;
				maxmode = m[smode];
			}
			sgid = p->fts_statp->st_gid;
			if (sgid < MAXGID && ++g[sgid] > maxgid) {
				savegid = sgid;
				maxgid = g[sgid];
			}
			suid = p->fts_statp->st_uid;
			if (suid < MAXUID && ++u[suid] > maxuid) {
				saveuid = suid;
				maxuid = u[suid];
			}
			
			/*
			 * XXX
			 * note that we don't count the most common xattr/acl digest
			 * so set will always the default value (none)
			 */

			/*
			 * XXX
			 * note that the below will break when file flags
			 * are extended beyond the first 4 bytes of each
			 * half word of the flags
			 */
#define FLAGS2IDX(f) ((f & 0xf) | ((f >> 12) & 0xf0))
			sflags = p->fts_statp->st_flags;
			if (FLAGS2IDX(sflags) < MAXFLAGS &&
			    ++f[FLAGS2IDX(sflags)] > maxflags) {
				saveflags = sflags;
				maxflags = f[FLAGS2IDX(sflags)];
			}
		}
	}
	/*
	 * If the /set record is the same as the last one we do not need to output
	 * a new one.  So first we check to see if anything changed.  Note that we
	 * always output a /set record for the first directory.
	 */
	if ((((keys & F_UNAME) | (keys & F_UID)) && (*puid != saveuid)) ||
	    (((keys & F_GNAME) | (keys & F_GID)) && (*pgid != savegid)) ||
	    ((keys & F_MODE) && (*pmode != savemode)) ||
	    ((keys & F_FLAGS) && (*pflags != saveflags)) ||
	    (first)) {
		first = 0;
		if (dflag)
			(void)printf("/set type=dir");
		else
			(void)printf("/set type=file");
		if (keys & F_UNAME) {
			pw = getpwuid(saveuid);
			if (pw != NULL) {
				(void)printf(" uname=%s", pw->pw_name);
			} else if (wflag) {
				RECORD_FAILURE(27451, WARN_UNAME);
				warnx( "Could not get uname for uid=%u", saveuid);
			} else {
				RECORD_FAILURE(86, EINVAL);
				errx(1, "Could not get uname for uid=%u", saveuid);
			}
		}
		if (keys & F_UID)
			(void)printf(" uid=%lu", (u_long)saveuid);
		if (keys & F_GNAME) {
			gr = getgrgid(savegid);
			if (gr != NULL) {
				(void)printf(" gname=%s", gr->gr_name);
			} else if (wflag) {
				RECORD_FAILURE(27452, WARN_UNAME);
				warnx("Could not get gname for gid=%u", savegid);
			} else {
				RECORD_FAILURE(87, EINVAL);
				errx(1, "Could not get gname for gid=%u", savegid);
			}
		}
		if (keys & F_GID)
			(void)printf(" gid=%lu", (u_long)savegid);
		if (keys & F_MODE)
			(void)printf(" mode=%#o", savemode);
		if (keys & F_NLINK)
			(void)printf(" nlink=1");
		if (keys & F_FLAGS) {
			fflags = flags_to_string(saveflags);
			(void)printf(" flags=%s", fflags);
			free(fflags);
		}
		if (keys & F_XATTRS)
			(void)printf(" xattrsdigest=%s.%llu", savexattrs, savexdstream_id);
		if (keys & F_ACL)
			(void)printf(" acldigest=%s", saveacl);
		(void)printf("\n");
		*puid = saveuid;
		*pgid = savegid;
		*pmode = savemode;
		*pflags = saveflags;
		*pxattrs = savexattrs;
		*pacl = saveacl;
		*xdstream_id = savexdstream_id;
	}
	return (0);
}

static int
dsort(const FTSENT **a, const FTSENT **b)
{
	if (S_ISDIR((*a)->fts_statp->st_mode)) {
		if (!S_ISDIR((*b)->fts_statp->st_mode))
			return (1);
	} else if (S_ISDIR((*b)->fts_statp->st_mode))
		return (-1);
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

#include <stdarg.h>

void
output(int indent, int *offset, const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (*offset + strlen(buf) > MAXLINELEN - 3) {
		(void)printf(" \\\n%*s", INDENTNAMELEN + indent, "");
		*offset = INDENTNAMELEN + indent;
	}
	*offset += printf(" %s", buf) + 1;
}
