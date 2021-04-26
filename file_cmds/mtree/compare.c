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
static char sccsid[] = "@(#)compare.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/compare.c,v 1.34 2005/03/29 11:44:17 tobez Exp $");

#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#ifndef __APPLE__
#ifdef ENABLE_MD5
#include <md5.h>
#endif
#ifdef ENABLE_RMD160
#include <ripemd.h>
#endif
#ifdef ENABLE_SHA1
#include <sha.h>
#endif
#ifdef ENABLE_SHA256
#include <sha256.h>
#endif
#endif /* !__APPLE__ */
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

#define	INDENTNAMELEN	8
#define	LABEL \
	if (!label++) { \
		len = printf("%s changed\n", RP(p)); \
		tab = "\t"; \
	}

extern CFMutableDictionaryRef dict;

// max/min times apfs can store on disk
#define APFS_MAX_TIME 0x7fffffffffffffffLL
#define APFS_MIN_TIME (-0x7fffffffffffffffLL-1)

static uint64_t
timespec_to_apfs_timestamp(struct timespec *ts)
{
	int64_t total;
	int64_t seconds;

	// `tv_nsec' can be > one billion, so we split it into two components:
	// seconds and actual nanoseconds
	// this allows us to detect overflow on the *total* number of nanoseconds
	// e.g. if (MAX_SECONDS+2, -2billion) is passed in, we return MAX_SECONDS
	seconds = ((int64_t)ts->tv_nsec / (int64_t)NSEC_PER_SEC);

	// compute total nanoseconds, checking for overflow:
	// seconds = sec + (ns/10e9)
	// total = seconds*10e9 + ns%10e9
	if (__builtin_saddll_overflow(ts->tv_sec, seconds, &seconds) ||
			__builtin_smulll_overflow(seconds, NSEC_PER_SEC, &total) ||
			__builtin_saddll_overflow(((int64_t)ts->tv_nsec % (int64_t)NSEC_PER_SEC), total, &total)) {
		// checking the sign of "seconds" tells us whether to cap the value at
		// the max or min time
		total = (ts->tv_sec > 0) ? APFS_MAX_TIME : APFS_MIN_TIME;
	}

	return (uint64_t)total;
}

static void
set_key_value_pair(void *in_key, uint64_t *in_val, bool is_string)
{
	CFStringRef key;
	CFNumberRef val;

	if (is_string) {
		key = CFStringCreateWithCString(NULL, (const char*)in_key, kCFStringEncodingUTF8);

	} else {
		key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%llu"), *(uint64_t*)in_key);
	}

	val = CFNumberCreate(NULL, kCFNumberSInt64Type, in_val);

	// we always expect the key to be not present
	if (key && val) {
		CFDictionaryAddValue(dict, key, val);
	} else {
		if (key) {
			CFRelease(key);
		}
		if (val) {
			CFRelease(val);
		}
		RECORD_FAILURE(1, EINVAL);
		errx(1, "set_key_value_pair: key/value is null");
	}

	if (key) {
		CFRelease(key);
	}
	if (val) {
		CFRelease(val);
	}
}

int
compare(char *name __unused, NODE *s, FTSENT *p)
{
	int error = 0;
	struct timeval tv[2];
	uint32_t val;
	int fd, label;
	off_t len;
	char *cp;
	const char *tab = "";
	char *fflags, *badflags;
	u_long flags;

	label = 0;
	switch(s->type) {
	case F_BLOCK:
		if (!S_ISBLK(p->fts_statp->st_mode)) {
			RECORD_FAILURE(2, EINVAL);
			goto typeerr;
		}
		break;
	case F_CHAR:
		if (!S_ISCHR(p->fts_statp->st_mode)) {
			RECORD_FAILURE(3, EINVAL);
			goto typeerr;
		}
		break;
	case F_DIR:
		if (!S_ISDIR(p->fts_statp->st_mode)) {
			RECORD_FAILURE(4, EINVAL);
			goto typeerr;
		}
		break;
	case F_FIFO:
		if (!S_ISFIFO(p->fts_statp->st_mode)) {
			RECORD_FAILURE(5, EINVAL);
			goto typeerr;
		}
		break;
	case F_FILE:
		if (!S_ISREG(p->fts_statp->st_mode)) {
			RECORD_FAILURE(6, EINVAL);
			goto typeerr;
		}
		break;
	case F_LINK:
		if (!S_ISLNK(p->fts_statp->st_mode)) {
			RECORD_FAILURE(7, EINVAL);
			goto typeerr;
		}
		break;
	case F_SOCK:
		if (!S_ISSOCK(p->fts_statp->st_mode)) {
			RECORD_FAILURE(8, EINVAL);
typeerr:		LABEL;
			(void)printf("\ttype expected %s found %s\n",
			    ftype(s->type), inotype(p->fts_statp->st_mode));
			return (label);
		}
		break;
	}
	/* Set the uid/gid first, then set the mode. */
	if (s->flags & (F_UID | F_UNAME) && s->st_uid != p->fts_statp->st_uid) {
		LABEL;
		(void)printf("%suser expected %lu found %lu",
		    tab, (u_long)s->st_uid, (u_long)p->fts_statp->st_uid);
		if (uflag) {
			if (chown(p->fts_accpath, s->st_uid, -1)) {
				error = errno;
				RECORD_FAILURE(9, error);
				(void)printf(" not modified: %s\n",
				    strerror(error));
			} else {
				(void)printf(" modified\n");
			}
		} else {
			(void)printf("\n");
		}
		tab = "\t";
	}
	if (s->flags & (F_GID | F_GNAME) && s->st_gid != p->fts_statp->st_gid) {
		LABEL;
		(void)printf("%sgid expected %lu found %lu",
		    tab, (u_long)s->st_gid, (u_long)p->fts_statp->st_gid);
		if (uflag) {
			if (chown(p->fts_accpath, -1, s->st_gid)) {
				error = errno;
				RECORD_FAILURE(10, error);
				(void)printf(" not modified: %s\n",
				    strerror(error));
			} else {
				(void)printf(" modified\n");
			}
		} else {
			(void)printf("\n");
		}
		tab = "\t";
	}
	if (s->flags & F_MODE &&
	    !S_ISLNK(p->fts_statp->st_mode) &&
	    s->st_mode != (p->fts_statp->st_mode & MBITS)) {
		LABEL;
		(void)printf("%spermissions expected %#o found %#o",
		    tab, s->st_mode, p->fts_statp->st_mode & MBITS);
		if (uflag) {
			if (chmod(p->fts_accpath, s->st_mode)) {
				error = errno;
				RECORD_FAILURE(11, error);
				(void)printf(" not modified: %s\n",
				    strerror(error));
			} else {
				(void)printf(" modified\n");
			}
		} else {
			(void)printf("\n");
		}
		tab = "\t";
	}
	if (s->flags & F_NLINK && s->type != F_DIR &&
	    s->st_nlink != p->fts_statp->st_nlink) {
		LABEL;
		(void)printf("%slink_count expected %u found %u\n",
		    tab, s->st_nlink, p->fts_statp->st_nlink);
		tab = "\t";
	}
	if (s->flags & F_SIZE && s->st_size != p->fts_statp->st_size &&
		!S_ISDIR(p->fts_statp->st_mode)) {
		LABEL;
		(void)printf("%ssize expected %jd found %jd\n", tab,
		    (intmax_t)s->st_size, (intmax_t)p->fts_statp->st_size);
		tab = "\t";
	}
	if ((s->flags & F_TIME) &&
	     ((s->st_mtimespec.tv_sec != p->fts_statp->st_mtimespec.tv_sec) ||
	     (s->st_mtimespec.tv_nsec != p->fts_statp->st_mtimespec.tv_nsec))) {
		if (!mflag) {
			LABEL;
			(void)printf("%smodification time expected %.24s.%09ld ",
				     tab, ctime(&s->st_mtimespec.tv_sec), s->st_mtimespec.tv_nsec);
			(void)printf("found %.24s.%09ld",
				     ctime(&p->fts_statp->st_mtimespec.tv_sec), p->fts_statp->st_mtimespec.tv_nsec);
			if (uflag) {
				tv[0].tv_sec = s->st_mtimespec.tv_sec;
				tv[0].tv_usec = s->st_mtimespec.tv_nsec / 1000;
				tv[1] = tv[0];
				if (utimes(p->fts_accpath, tv)) {
					error = errno;
					RECORD_FAILURE(12, error);
					(void)printf(" not modified: %s\n",
						     strerror(error));
				} else {
					(void)printf(" modified\n");
				}
			} else {
				(void)printf("\n");
			}
			tab = "\t";
		}
		if (!insert_mod && mflag) {
			uint64_t s_mod_time = timespec_to_apfs_timestamp(&s->st_mtimespec);
			char *mod_string = "MODIFICATION";
			set_key_value_pair(mod_string, &s_mod_time, true);
			insert_mod = 1;
		}
	}
	if (s->flags & F_CKSUM) {
		if ((fd = open(p->fts_accpath, O_RDONLY, 0)) < 0) {
			LABEL;
			error = errno;
			RECORD_FAILURE(13, error);
			(void)printf("%scksum: %s: %s\n",
			    tab, p->fts_accpath, strerror(error));
			tab = "\t";
		} else if (crc(fd, &val, &len)) {
			(void)close(fd);
			LABEL;
			error = errno;
			RECORD_FAILURE(14, error);
			(void)printf("%scksum: %s: %s\n",
			    tab, p->fts_accpath, strerror(error));
			tab = "\t";
		} else {
			(void)close(fd);
			if (s->cksum != val) {
				LABEL;
				(void)printf("%scksum expected %lu found %lu\n",
				    tab, s->cksum, (unsigned long)val);
				tab = "\t";
			}
		}
	}
	if (s->flags & F_FLAGS) {
		// There are unpublished flags that should not fail comparison
		// we convert to string and back to filter them out
		fflags = badflags = flags_to_string(p->fts_statp->st_flags);
		if (strcmp("none", fflags) == 0) {
			flags = 0;
		} else if (strtofflags(&badflags, &flags, NULL) != 0)
			errx(1, "invalid flag %s", badflags);
		free(fflags);
		if (s->st_flags != flags) {
			LABEL;
			fflags = flags_to_string(s->st_flags);
			(void)printf("%sflags expected \"%s\"", tab, fflags);
			free(fflags);
			
			fflags = flags_to_string(flags);
			(void)printf(" found \"%s\"", fflags);
			free(fflags);
			
			if (uflag) {
				if (chflags(p->fts_accpath, (u_int)s->st_flags)) {
					error = errno;
					RECORD_FAILURE(15, error);
					(void)printf(" not modified: %s\n",
						     strerror(error));
				} else {
					(void)printf(" modified\n");
				}
			} else {
				(void)printf("\n");
			}
			tab = "\t";
		}
	}
#ifdef ENABLE_MD5
	if (s->flags & F_MD5) {
		char *new_digest, buf[33];
#ifdef __clang__
/* clang doesn't like MD5 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		new_digest = MD5File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!new_digest) {
			LABEL;
			error = errno;
			RECORD_FAILURE(16, error);
			printf("%sMD5: %s: %s\n", tab, p->fts_accpath,
			       strerror(error));
			tab = "\t";
		} else if (strcmp(new_digest, s->md5digest)) {
			LABEL;
			printf("%sMD5 expected %s found %s\n", tab, s->md5digest,
			       new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_MD5 */
#ifdef ENABLE_SHA1
	if (s->flags & F_SHA1) {
		char *new_digest, buf[41];
#ifdef __clang__
/* clang doesn't like SHA1 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		new_digest = SHA1_File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!new_digest) {
			LABEL;
			error = errno;
			RECORD_FAILURE(17, error);
			printf("%sSHA-1: %s: %s\n", tab, p->fts_accpath,
			       strerror(error));
			tab = "\t";
		} else if (strcmp(new_digest, s->sha1digest)) {
			LABEL;
			printf("%sSHA-1 expected %s found %s\n",
			       tab, s->sha1digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_SHA1 */
#ifdef ENABLE_RMD160
	if (s->flags & F_RMD160) {
		char *new_digest, buf[41];
#ifdef __clang__
/* clang doesn't like RIPEMD160 due to security concerns, but it's used for file data/metadata integrity.. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		new_digest = RIPEMD160_File(p->fts_accpath, buf);
#pragma clang diagnostic pop
#endif
		if (!new_digest) {
			LABEL;
			error = errno;
			RECORD_FAILURE(18, error);
			printf("%sRIPEMD160: %s: %s\n", tab,
			       p->fts_accpath, strerror(error));
			tab = "\t";
		} else if (strcmp(new_digest, s->rmd160digest)) {
			LABEL;
			printf("%sRIPEMD160 expected %s found %s\n",
			       tab, s->rmd160digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_RMD160 */
#ifdef ENABLE_SHA256
	if (s->flags & F_SHA256) {
		char *new_digest, buf[kSHA256NullTerminatedBuffLen];

		new_digest = SHA256_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			error = errno;
			RECORD_FAILURE(19, error);
			printf("%sSHA-256: %s: %s\n", tab, p->fts_accpath,
			       strerror(error));
			tab = "\t";
		} else if (strcmp(new_digest, s->sha256digest)) {
			LABEL;
			printf("%sSHA-256 expected %s found %s\n",
			       tab, s->sha256digest, new_digest);
			tab = "\t";
		}
	}
#endif /* ENABLE_SHA256 */

	if (s->flags & F_SLINK &&
	    strcmp(cp = rlink(p->fts_accpath), s->slink)) {
		LABEL;
		(void)printf("%slink_ref expected %s found %s\n",
		      tab, s->slink, cp);
	}
	if ((s->flags & F_BTIME) &&
	    ((s->st_birthtimespec.tv_sec != p->fts_statp->st_birthtimespec.tv_sec) ||
	     (s->st_birthtimespec.tv_nsec != p->fts_statp->st_birthtimespec.tv_nsec))) {
		    if (!mflag) {
			    LABEL;
			    (void)printf("%sbirth time expected %.24s.%09ld ",
					 tab, ctime(&s->st_birthtimespec.tv_sec), s->st_birthtimespec.tv_nsec);
			    (void)printf("found %.24s.%09ld\n",
					 ctime(&p->fts_statp->st_birthtimespec.tv_sec), p->fts_statp->st_birthtimespec.tv_nsec);
			    tab = "\t";
		    }
		    if (!insert_birth && mflag) {
			    uint64_t s_create_time = timespec_to_apfs_timestamp(&s->st_birthtimespec);
			    char *birth_string = "BIRTH";
			    set_key_value_pair(birth_string, &s_create_time, true);
			    insert_birth = 1;
		    }
	    }
	if ((s->flags & F_ATIME) &&
	    ((s->st_atimespec.tv_sec != p->fts_statp->st_atimespec.tv_sec) ||
	     (s->st_atimespec.tv_nsec != p->fts_statp->st_atimespec.tv_nsec))) {
		    if (!mflag) {
			    LABEL;
			    (void)printf("%saccess time expected %.24s.%09ld ",
					 tab, ctime(&s->st_atimespec.tv_sec), s->st_atimespec.tv_nsec);
			    (void)printf("found %.24s.%09ld\n",
					 ctime(&p->fts_statp->st_atimespec.tv_sec), p->fts_statp->st_atimespec.tv_nsec);
			    tab = "\t";
		    }
		    if (!insert_access && mflag) {
			    uint64_t s_access_time = timespec_to_apfs_timestamp(&s->st_atimespec);
			    char *access_string = "ACCESS";
			    set_key_value_pair(access_string, &s_access_time, true);
			    insert_access = 1;

		    }
	    }
	if ((s->flags & F_CTIME) &&
	    ((s->st_ctimespec.tv_sec != p->fts_statp->st_ctimespec.tv_sec) ||
	     (s->st_ctimespec.tv_nsec != p->fts_statp->st_ctimespec.tv_nsec))) {
		    if (!mflag) {
			    LABEL;
			    (void)printf("%smetadata modification time expected %.24s.%09ld ",
					 tab, ctime(&s->st_ctimespec.tv_sec), s->st_ctimespec.tv_nsec);
			    (void)printf("found %.24s.%09ld\n",
					 ctime(&p->fts_statp->st_ctimespec.tv_sec), p->fts_statp->st_ctimespec.tv_nsec);
			    tab = "\t";
		    }
		    if (!insert_change && mflag) {
			    uint64_t s_mod_time = timespec_to_apfs_timestamp(&s->st_ctimespec);
			    char *change_string = "CHANGE";
			    set_key_value_pair(change_string, &s_mod_time, true);
			    insert_change = 1;
		    }
	    }
	if (s->flags & F_PTIME) {
		int supported;
		struct timespec ptimespec = ptime(p->fts_accpath, &supported);
		if (!supported) {
			if (mflag) {
				ptimespec.tv_sec = 0;
				ptimespec.tv_nsec = 0;
				supported = 1;
			} else {
				LABEL;
				(void)printf("%stime added to parent folder expected %.24s.%09ld found that it is not supported\n",
					     tab, ctime(&s->st_ptimespec.tv_sec), s->st_ptimespec.tv_nsec);
				tab = "\t";
			}
		}
		if (supported && ((s->st_ptimespec.tv_sec != ptimespec.tv_sec) ||
			   (s->st_ptimespec.tv_nsec != ptimespec.tv_nsec))) {
			if (!mflag) {
				LABEL;
				(void)printf("%stime added to parent folder expected %.24s.%09ld ",
					      tab, ctime(&s->st_ptimespec.tv_sec), s->st_ptimespec.tv_nsec);
				(void)printf("found %.24s.%09ld\n",
					      ctime(&ptimespec.tv_sec), ptimespec.tv_nsec);
				tab = "\t";
			} else if (!insert_parent && mflag) {
				uint64_t s_added_time = timespec_to_apfs_timestamp(&s->st_ptimespec);
				char *added_string = "DATEADDED";
				set_key_value_pair(added_string, &s_added_time, true);
				insert_parent = 1;
			}
		}
	}
	if (s->flags & F_XATTRS) {
		char buf[kSHA256NullTerminatedBuffLen];
		xattr_info *ai;
		ai = SHA256_Path_XATTRs(p->fts_accpath, buf);
		if (!mflag) {
			if (ai && !ai->digest) {
				LABEL;
				printf("%sxattrsdigest missing, expected: %s\n", tab, s->xattrsdigest);
				tab = "\t";
			} else if (ai && strcmp(ai->digest, s->xattrsdigest)) {
				LABEL;
				printf("%sxattrsdigest expected %s found %s\n",
				       tab, s->xattrsdigest, ai->digest);
				tab = "\t";
			}
		}
		if (mflag) {
			if (ai && ai->xdstream_priv_id != s->xdstream_priv_id) {
				set_key_value_pair((void*)&ai->xdstream_priv_id, &s->xdstream_priv_id, false);
			}
		}
		free(ai);
	}
	if ((s->flags & F_INODE) &&
	    (p->fts_statp->st_ino != s->st_ino)) {
		if (!mflag) {
			LABEL;
			(void)printf("%sinode expected %llu found %llu\n",
				     tab, s->st_ino, p->fts_statp->st_ino);
			tab = "\t";
		}
		if (mflag) {
			set_key_value_pair((void*)&p->fts_statp->st_ino, &s->st_ino, false);
		}
	}
	if (s->flags & F_ACL) {
		char *new_digest, buf[kSHA256NullTerminatedBuffLen];
		new_digest = SHA256_Path_ACL(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sacldigest missing, expected: %s\n", tab, s->acldigest);
			tab = "\t";
		} else if (strcmp(new_digest, s->acldigest)) {
			LABEL;
			printf("%sacldigest expected %s found %s\n",
			       tab, s->acldigest, new_digest);
			tab = "\t";
		}
	}
	if (s->flags & F_SIBLINGID) {
		uint64_t new_sibling_id = get_sibling_id(p->fts_accpath);
		new_sibling_id = (new_sibling_id != p->fts_statp->st_ino) ? new_sibling_id : 0;
		if (new_sibling_id != s->sibling_id) {
			if (!mflag) {
				LABEL;
				(void)printf("%ssibling id expected %llu found %llu\n",
					     tab, s->sibling_id, new_sibling_id);
				tab = "\t";
			}
			if (mflag) {
				set_key_value_pair((void*)&new_sibling_id, &s->sibling_id, false);
			}
		}
	}
	
	return (label);
}

const char *
inotype(u_int type)
{
	switch(type & S_IFMT) {
	case S_IFBLK:
		return ("block");
	case S_IFCHR:
		return ("char");
	case S_IFDIR:
		return ("dir");
	case S_IFIFO:
		return ("fifo");
	case S_IFREG:
		return ("file");
	case S_IFLNK:
		return ("link");
	case S_IFSOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

const char *
ftype(u_int type)
{
	switch(type) {
	case F_BLOCK:
		return ("block");
	case F_CHAR:
		return ("char");
	case F_DIR:
		return ("dir");
	case F_FIFO:
		return ("fifo");
	case F_FILE:
		return ("file");
	case F_LINK:
		return ("link");
	case F_SOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

char *
rlink(char *name)
{
	int error = 0;
	static char lbuf[MAXPATHLEN];
	ssize_t len;

	if ((len = readlink(name, lbuf, sizeof(lbuf) - 1)) == -1) {
		error = errno;
		RECORD_FAILURE(20, error);
		errc(1, error, "line %d: %s", lineno, name);
	}
	lbuf[len] = '\0';
	return (lbuf);
}
