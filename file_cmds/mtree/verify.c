/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)verify.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/verify.c,v 1.24 2005/08/11 15:43:55 brian Exp $");

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <removefile.h>
#include "metrics.h"
#include "mtree.h"
#include "extern.h"

static NODE *root;
static char path[MAXPATHLEN];

static int	miss(NODE *, char *, size_t path_length);
static int	vwalk(void);

int
mtree_verifyspec(FILE *fi)
{
	int rval, mval;
	size_t path_length = 0;

	root = mtree_readspec(fi);
	rval = vwalk();
	mval = miss(root, path, path_length);
	
	if (rval != 0) {
		RECORD_FAILURE(60, WARN_MISMATCH);
		return rval;
	} else {
		if (mval != 0) {
			RECORD_FAILURE(61, WARN_MISMATCH);
		}
		return mval;
	}
}

static int
vwalk(void)
{
	int error = 0;
	FTS *t;
	FTSENT *p;
	NODE *ep, *level;
	int specdepth, rval;
	char *argv[2];
	char dot[] = ".";

	argv[0] = dot;
	argv[1] = NULL;
	if ((t = fts_open(argv, ftsoptions, NULL)) == NULL) {
		error = errno;
		RECORD_FAILURE(62, error);
		errc(1, error, "line %d: fts_open", lineno);
	}
	level = root;
	specdepth = rval = 0;
	while ((p = fts_read(t))) {
		if (check_excludes(p->fts_name, p->fts_path)) {
			fts_set(t, p, FTS_SKIP);
			continue;
		}
		switch(p->fts_info) {
		case FTS_D:
		case FTS_SL:
			break;
		case FTS_DP:
			if (level == NULL) {
				RECORD_FAILURE(63, EINVAL);
				errx(1 , "invalid root in vwalk");
			}
			if (specdepth > p->fts_level) {
				for (level = level->parent; level->prev;
				      level = level->prev);
				--specdepth;
			}
			continue;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", RP(p), strerror(p->fts_errno));
			continue;
		default:
			if (dflag)
				continue;
		}

		if (specdepth != p->fts_level)
			goto extra;
		for (ep = level; ep; ep = ep->next)
			if ((ep->flags & F_MAGIC &&
			    !fnmatch(ep->name, p->fts_name, FNM_PATHNAME)) ||
			    !strcmp(ep->name, p->fts_name)) {
				ep->flags |= F_VISIT;
				if ((ep->flags & F_NOCHANGE) == 0 &&
				    compare(ep->name, ep, p)) {
					RECORD_FAILURE(64, WARN_MISMATCH);
					rval = MISMATCHEXIT;
				}
				if (ep->flags & F_IGN)
					(void)fts_set(t, p, FTS_SKIP);
				else if (ep->child && ep->type == F_DIR &&
				    p->fts_info == FTS_D) {
					level = ep->child;
					++specdepth;
				}
				break;
			}

		if (ep)
			continue;
extra:
		if (!eflag) {
			(void)printf("%s extra", RP(p));

			if (rflag) {
				/* rflag implies: delete stuff if "extra" is observed" */
				if (mflag) {
					/* -mflag is used for sealing & verification -- use removefile for recursive behavior */
					removefile_state_t rmstate;
					rmstate = removefile_state_alloc();
					if (removefile(p->fts_accpath, rmstate, (REMOVEFILE_RECURSIVE))) {
						error = errno;
						RECORD_FAILURE(65, error);
						errx (1, "\n error deleting item (or descendant) at path %s (%s)", RP(p), strerror(error));
					}
					else {
						/* removefile success */
						(void) printf(", removed");
					}
					removefile_state_free(rmstate);

				}
				else {
					/* legacy: use rmdir/unlink if "-m" not specified */
					int syserr = 0;

					if (S_ISDIR(p->fts_statp->st_mode)){
						syserr = rmdir(p->fts_accpath);
					}
					else {
						syserr = unlink(p->fts_accpath);
					}

					/* log failures */
					if (syserr) {
						error = errno;
						RECORD_FAILURE(66, error);
						(void) printf(", not removed :%s", strerror(error));
					}
				}
			} else if (mflag) {
				RECORD_FAILURE(68956, WARN_MISMATCH);
				errx(1, "cannot generate the XML dictionary");
			}
			(void)putchar('\n');
		}
		(void)fts_set(t, p, FTS_SKIP);
	}
	(void)fts_close(t);
	if (sflag) {
		RECORD_FAILURE(67, WARN_CHECKSUM);
		warnx("%s checksum: %lu", fullpath, (unsigned long)crc_total);
	}
	return (rval);
}

static int
miss(NODE *p, char *tail, size_t path_length)
{
	int create;
	char *tp;
	const char *type, *what;
	int serr;
	int rval = 0;
	int rrval = 0;
	size_t file_name_length = 0;

	for (; p; p = p->next) {
		if (p->type != F_DIR && (dflag || p->flags & F_VISIT))
			continue;
		file_name_length = strnlen(p->name, MAXPATHLEN);
		path_length += file_name_length;
		if (path_length >= MAXPATHLEN) {
			RECORD_FAILURE(61971, ENAMETOOLONG);
			continue;
		}
		(void)strcpy(tail, p->name);
		if (!(p->flags & F_VISIT)) {
			/* Don't print missing message if file exists as a
			   symbolic link and the -q flag is set. */
			struct stat statbuf;

			if (qflag && stat(path, &statbuf) == 0) {
				p->flags |= F_VISIT;
			} else {
				(void)printf("%s missing", path);
				RECORD_FAILURE(68, WARN_MISMATCH);
				rval = MISMATCHEXIT;
			}
		}
		if (p->type != F_DIR && p->type != F_LINK) {
			putchar('\n');
			continue;
		}

		create = 0;
		if (p->type == F_LINK)
			type = "symlink";
		else
			type = "directory";
		if (!(p->flags & F_VISIT) && uflag) {
			if (!(p->flags & (F_UID | F_UNAME))) {
				(void)printf(" (%s not created: user not specified)", type);
			} else if (!(p->flags & (F_GID | F_GNAME))) {
				(void)printf(" (%s not created: group not specified)", type);
			} else if (p->type == F_LINK) {
				if (symlink(p->slink, path)) {
					serr = errno;
					RECORD_FAILURE(69, serr);
					(void)printf(" (symlink not created: %s)\n",
					    strerror(serr));
				} else {
					(void)printf(" (created)\n");
				}
				if (lchown(path, p->st_uid, p->st_gid) == -1) {
					serr = errno;
					if (p->st_uid == (uid_t)-1)
						what = "group";
					else if (lchown(path, (uid_t)-1,
					    p->st_gid) == -1)
						what = "user & group";
					else {
						what = "user";
						errno = serr;
					}
					serr = errno;
					RECORD_FAILURE(70, serr);
					(void)printf("%s: %s not modified: %s"
					    "\n", path, what, strerror(serr));
				}
				continue;
			} else if (!(p->flags & F_MODE)) {
			    (void)printf(" (directory not created: mode not specified)");
			} else if (mkdir(path, S_IRWXU)) {
				serr = errno;
				RECORD_FAILURE(71, serr);
				(void)printf(" (directory not created: %s)",
				    strerror(serr));
			} else {
				create = 1;
				(void)printf(" (created)");
			}
		}
		if (!(p->flags & F_VISIT))
			(void)putchar('\n');

		for (tp = tail; *tp; ++tp);
		*tp = '/';
		++path_length;
		rrval = miss(p->child, tp + 1, path_length);
		if (rrval != 0) {
			RECORD_FAILURE(72, WARN_MISMATCH);
			rval = rrval;
		}
		path_length -= (file_name_length + 1);
		*tp = '\0';

		if (!create)
			continue;
		if (chown(path, p->st_uid, p->st_gid) == -1) {
			serr = errno;
			if (p->st_uid == (uid_t)-1)
				what = "group";
			else if (chown(path, (uid_t)-1, p->st_gid) == -1)
				what = "user & group";
			else {
				what = "user";
				errno = serr;
			}
			serr = errno;
			RECORD_FAILURE(73, serr);
			(void)printf("%s: %s not modified: %s\n",
			    path, what, strerror(serr));
		}
		if (chmod(path, p->st_mode)) {
			serr = errno;
			RECORD_FAILURE(74, serr);
			(void)printf("%s: permissions not set: %s\n",
			    path, strerror(serr));
		}
		if ((p->flags & F_FLAGS) && p->st_flags &&
		    chflags(path, (u_int)p->st_flags)) {
			serr = errno;
			RECORD_FAILURE(75, serr);
			(void)printf("%s: file flags not set: %s\n",
			    path, strerror(serr));
		}
	}
	return rval;
}
