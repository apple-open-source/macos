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
static char sccsid[] = "@(#)spec.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/spec.c,v 1.22 2005/03/29 11:44:17 tobez Exp $");

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <vis.h>
#include "metrics.h"
#include "mtree.h"
#include "extern.h"

int lineno;				/* Current spec line number. */

static void	 set(char *, NODE *);
static void	 unset(char *, NODE *);

NODE *
mtree_readspec(FILE *fi)
{
	NODE *centry, *last;
	char *p;
	NODE ginfo, *root;
	int c_cur, c_next;
	char buf[2048];

	centry = last = root = NULL;
	bzero(&ginfo, sizeof(ginfo));
	c_cur = c_next = 0;
	for (lineno = 1; fgets(buf, sizeof(buf), fi);
	    ++lineno, c_cur = c_next, c_next = 0) {
		/* Skip empty lines. */
		if (buf[0] == '\n')
			continue;

		/* Find end of line. */
		if ((p = index(buf, '\n')) == NULL) {
			RECORD_FAILURE(21, ERANGE);
			errx(1, "line %d too long", lineno);
		}

		/* See if next line is continuation line. */
		if (p[-1] == '\\') {
			--p;
			c_next = 1;
		}

		/* Null-terminate the line. */
		*p = '\0';

		/* Skip leading whitespace. */
		for (p = buf; *p && isspace(*p); ++p);

		/* If nothing but whitespace or comment char, continue. */
		if (!*p || *p == '#')
			continue;

#ifdef DEBUG
		(void)fprintf(stderr, "line %d: {%s}\n", lineno, p);
#endif
		if (c_cur) {
			set(p, centry);
			continue;
		}

		/* Grab file name, "$", "set", or "unset". */
		if ((p = strtok(p, "\n\t ")) == NULL) {
			RECORD_FAILURE(22, EINVAL);
			errx(1, "line %d: missing field", lineno);
		}

		if (p[0] == '/')
			switch(p[1]) {
			case 's':
				if (strcmp(p + 1, "set"))
					break;
				set(NULL, &ginfo);
				continue;
			case 'u':
				if (strcmp(p + 1, "unset"))
					break;
				unset(NULL, &ginfo);
				continue;
			}

		if (index(p, '/')) {
			RECORD_FAILURE(23, EINVAL);
			errx(1, "line %d: slash character in file name",
			lineno);
		}

		if (!strcmp(p, "..")) {
			/* Don't go up, if haven't gone down. */
			if (!root)
				goto noparent;
			if (last->type != F_DIR || last->flags & F_DONE) {
				if (last == root)
					goto noparent;
				last = last->parent;
			}
			last->flags |= F_DONE;
			continue;

noparent:		RECORD_FAILURE(24, EINVAL);
			errx(1, "line %d: no parent node", lineno);
		}

		if ((centry = calloc(1, sizeof(NODE) + strlen(p))) == NULL) {
			RECORD_FAILURE(25, ENOMEM);
			errx(1, "calloc");
		}
		*centry = ginfo;
#define	MAGIC	"?*["
		if (strpbrk(p, MAGIC))
			centry->flags |= F_MAGIC;
		if (strunvis(centry->name, p) == -1) {
			RECORD_FAILURE(26, EILSEQ);
			errx(1, "filename %s is ill-encoded", p);
		}
		set(NULL, centry);

		if (!root) {
			last = root = centry;
			root->parent = root;
		} else if (last->type == F_DIR && !(last->flags & F_DONE)) {
			centry->parent = last;
			last = last->child = centry;
		} else {
			centry->parent = last->parent;
			centry->prev = last;
			last = last->next = centry;
		}
	}
	return (root);
}

static void
set(char *t, NODE *ip)
{
	int error = 0;
	int type;
	char *kw, *val = NULL;
	struct group *gr;
	struct passwd *pw;
	mode_t *m;
	int value;
	char *ep;
	char *l;

	for (; (kw = strtok(t, "= \t\n")); t = NULL) {
		ip->flags |= type = parsekey(kw, &value);
		if ((value == 0) || (val = strtok(NULL, " \t\n")) == NULL) {
			RECORD_FAILURE(27, EINVAL);
			errx(1, "line %d: missing value", lineno);
		}
		switch(type) {
			case F_CKSUM:
				ip->cksum = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(28, EINVAL);
					errx(1, "line %d: invalid checksum %s",
					     lineno, val);
				}
				break;
			case F_MD5:
				ip->md5digest = strdup(val);
				if (!ip->md5digest) {
					RECORD_FAILURE(29, ENOMEM);
					errx(1, "strdup");
				}
				break;
			case F_SHA1:
				ip->sha1digest = strdup(val);
				if (!ip->sha1digest) {
					RECORD_FAILURE(30, ENOMEM);
					errx(1, "strdup");
				}
				break;
			case F_SHA256:
				ip->sha256digest = strdup(val);
				if (!ip->sha256digest) {
					RECORD_FAILURE(31, ENOMEM);
					errx(1, "strdup");
				}
				break;
			case F_RMD160:
				ip->rmd160digest = strdup(val);
				if (!ip->rmd160digest) {
					RECORD_FAILURE(32, ENOMEM);
					errx(1, "strdup");
				}
				break;
			case F_FLAGS:
				if (strcmp("none", val) == 0) {
					ip->st_flags = 0;
				} else if (strtofflags(&val, &ip->st_flags, NULL) != 0) {
					RECORD_FAILURE(33, EINVAL);
					errx(1, "line %d: invalid flag %s", lineno, val);
				}
				break;
			case F_GID:
				ip->st_gid = (gid_t)strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(34, EINVAL);
					errx(1, "line %d: invalid gid %s", lineno, val);
				}
				break;
			case F_GNAME:
				if ((gr = getgrnam(val)) == NULL) {
					RECORD_FAILURE(35, EINVAL);
					errx(1, "line %d: unknown group %s", lineno, val);
				}
				ip->st_gid = gr->gr_gid;
				break;
			case F_IGN:
				/* just set flag bit */
				break;
			case F_MODE:
				if ((m = setmode(val)) == NULL) {
					RECORD_FAILURE(36, EINVAL);
					errx(1, "line %d: invalid file mode %s",
					     lineno, val);
				}
				ip->st_mode = getmode(m, 0);
				free(m);
				break;
			case F_NLINK:
				ip->st_nlink = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(37, EINVAL);
					errx(1, "line %d: invalid link count %s",
					     lineno,  val);
				}
				break;
			case F_SIZE:
				ip->st_size = strtoq(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(38, EINVAL);
					errx(1, "line %d: invalid size %s",
					     lineno, val);
				}
				break;
			case F_SLINK:
				ip->slink = malloc(strlen(val) + 1);
				if (ip->slink == NULL) {
					RECORD_FAILURE(39, ENOMEM);
					errx(1, "malloc");
				}
				if (strunvis(ip->slink, val) == -1) {
					RECORD_FAILURE(40, EILSEQ);
					errx(1, "symlink %s is ill-encoded", val);
				}
				break;
			case F_TIME:
				ip->st_mtimespec.tv_sec = strtoul(val, &ep, 10);
				if (*ep != '.') {
					RECORD_FAILURE(41, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				val = ep + 1;
				ip->st_mtimespec.tv_nsec = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(42, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				break;
			case F_TYPE:
				switch(*val) {
					case 'b':
						if (!strcmp(val, "block"))
							ip->type = F_BLOCK;
						break;
					case 'c':
						if (!strcmp(val, "char"))
							ip->type = F_CHAR;
						break;
					case 'd':
						if (!strcmp(val, "dir"))
							ip->type = F_DIR;
						break;
					case 'f':
						if (!strcmp(val, "file"))
							ip->type = F_FILE;
						if (!strcmp(val, "fifo"))
							ip->type = F_FIFO;
						break;
					case 'l':
						if (!strcmp(val, "link"))
							ip->type = F_LINK;
						break;
					case 's':
						if (!strcmp(val, "socket"))
							ip->type = F_SOCK;
						break;
					default:
						RECORD_FAILURE(43, EINVAL);
						errx(1, "line %d: unknown file type %s",
						     lineno, val);
				}
				break;
			case F_UID:
				ip->st_uid = (uid_t)strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(44, EINVAL);
					errx(1, "line %d: invalid uid %s", lineno, val);
				}
				break;
			case F_UNAME:
				if ((pw = getpwnam(val)) == NULL) {
					RECORD_FAILURE(45, EINVAL);
					errx(1, "line %d: unknown user %s", lineno, val);
				}
				ip->st_uid = pw->pw_uid;
				break;
			case F_BTIME:
				ip->st_birthtimespec.tv_sec = strtoul(val, &ep, 10);
				if (*ep != '.') {
					RECORD_FAILURE(46, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				val = ep + 1;
				ip->st_birthtimespec.tv_nsec = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(47, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				break;
			case F_ATIME:
				ip->st_atimespec.tv_sec = strtoul(val, &ep, 10);
				if (*ep != '.') {
					RECORD_FAILURE(48, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				val = ep + 1;
				ip->st_atimespec.tv_nsec = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(49, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				break;
			case F_CTIME:
				ip->st_ctimespec.tv_sec = strtoul(val, &ep, 10);
				if (*ep != '.') {
					RECORD_FAILURE(50, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				val = ep + 1;
				ip->st_ctimespec.tv_nsec = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(51, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				break;
			case F_PTIME:
				ip->st_ptimespec.tv_sec = strtoul(val, &ep, 10);
				if (*ep != '.') {
					RECORD_FAILURE(52, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				val = ep + 1;
				ip->st_ptimespec.tv_nsec = strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(53, EINVAL);
					errx(1, "line %d: invalid time %s",
					     lineno, val);
				}
				break;
			case F_XATTRS:
				/*
				 * Note this is nested inside an strtok loop,
				 * strtok_r must be used to preserve the strtok context
				 * of the loop.
				 */
				ep = strtok_r(val,".", &l);
				ip->xattrsdigest = strdup(ep);
				if (!ip->xattrsdigest) {
					error = errno;
					RECORD_FAILURE(54, error);
					errc(1, error, "strdup");
				}
				val = strtok_r(NULL,".", &l);
				if (val) {
					ip->xdstream_priv_id = strtoull(val, &ep, 10);
					if (*ep) {
						RECORD_FAILURE(55, EINVAL);
						errx(1, "line %d: invalid private id %s",
						     lineno, val);
					}
				} else {
					ip->xdstream_priv_id = 0;
				}
				break;
			case F_INODE:
				ip->st_ino = (ino_t)strtoull(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(56, EINVAL);
					errx(1, "line %d: invalid inode %s",
					     lineno, val);
				}
				break;
			case F_ACL:
				ip->acldigest = strdup(val);
				if (!ip->acldigest) {
					error = errno;
					RECORD_FAILURE(57, error);
					errc(1, error, "strdup");
				}
				break;
			case F_SIBLINGID:
				ip->sibling_id = (quad_t)strtoull(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(58, EINVAL);
					errx(1, "line %d: invalid sibling id %s", lineno, val);
				}
				break;
			case F_NXATTR:
				ip->nxattr = (quad_t)strtoull(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(604804, EINVAL);
					errx(1, "line %d: invalid xattr count %s", lineno, val);
				}
				break;
			case F_DATALESS:
				if (strcmp("1", val) == 0) {
					ip->st_flags |= SF_DATALESS;
				} else if (strcmp("0", val) != 0) {
					RECORD_FAILURE(1293596, EINVAL);
					errx(1, "line %d: invalid value for dataless: %s", lineno, val);
				}
				break;
			case F_PROTECTION_CLASS:
				ip->protection_class = (uint)strtoul(val, &ep, 10);
				if (*ep) {
					RECORD_FAILURE(1875702, EINVAL);
					errx(1, "line %d: invalid value for protectionclass: %s", lineno, val);
				}
				break;
		}
	}
}

static void
unset(char *t, NODE *ip)
{
	char *p;

	while ((p = strtok(t, "\n\t ")))
		ip->flags &= ~parsekey(p, NULL);
}
