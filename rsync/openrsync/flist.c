/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2019 Florian Obser <florian@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <sys/types.h>
#include COMPAT_MAJOR_MINOR_H
#include <sys/param.h>
#include <sys/stat.h>
#ifdef __sun
# include <sys/mkdev.h>
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_FTS
# include <fts.h>
#endif
#include <limits.h>
#include <inttypes.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * We allocate our file list in chunk sizes so as not to do it one by
 * one.
 * Preferably we get one or two allocation.
 */
#define	FLIST_CHUNK_SIZE (1024)

/*
 * These flags are part of the rsync protocol.
 * They are sent as the first byte for a file transmission and encode
 * information that affects subsequent transmissions.
 */
#define	FLIST_TOP_LEVEL	 	0x0001 /* needed for remote --delete */
#define	FLIST_MODE_SAME  	0x0002 /* mode is repeat */
#define	FLIST_XFLAGS	 	0x0004 /* Extended flags (protocol 28+) */
#define	FLIST_RDEV_SAME  	FLIST_XFLAGS /* protocol 27, rdev is repeat */
#define	FLIST_UID_SAME	 	0x0008 /* uid is repeat */
#define	FLIST_GID_SAME	 	0x0010 /* gid is repeat */
#define	FLIST_NAME_SAME  	0x0020 /* name is repeat */
#define	FLIST_NAME_LONG	 	0x0040 /* name >255 bytes */
#define	FLIST_TIME_SAME  	0x0080 /* time is repeat */

#define	FLIST_RDEV_MAJOR_SAME	0x0100 /* protocol 28+ (devices only) */
#define	FLIST_NO_DIR_CONTENT	0x0100 /* protocol 30+ (dirs only) */
#define	FLIST_HARDLINKED	0x0200 /* protocol 28+ (non-dirs only) */
#define	FLIST_INC_USER_NAME	0x0400 /* protocol 30+ */
#define	FLIST_DEV_SAME		FLIST_INC_USER_NAME /* protocol 28-29 only */
#define	FLIST_INC_GROUP_NAME	0x0800 /* protocol 30+ */
#define	FLIST_RDEV_MINOR_8	FLIST_INC_GROUP_NAME /* protocol 28-29 */
#define	FLIST_FIRST_HLINK	0x1000 /* protocol 30+ (hardlinks only) */
#define	FLIST_SEND_IO_ERRORS	0x1000 /* protocol 30 with flag, 31+ */
#define	FLIST_MODTIME_NSEC	0x2000 /* protocol 31+ */
#define	FLIST_ATIME_SAME	0x4000 /* command-line option, any protocol */
#define	FLIST_UNUSED_15		0x8000 /* unused */

/*
 * Required way to sort a filename list before protocol 29.
 */
static int
flist_cmp(const void *p1, const void *p2)
{
	const struct flist *f1 = p1, *f2 = p2;

	return strcmp(f1->wpath, f2->wpath);
}

/*
 * Required way to sort a filename list after protocol 29.
 * Rule #1: directories compare with a trailing "/"
 * Rule #2: Directories sort after non-directories
 * Rule #3: a directory named "." sorts first
 */
static int
flist_cmp29(const void *p1, const void *p2)
{
	const struct flist *f1 = p1, *f2 = p2;
	const char *s1 = f1->wpath;
	const char *s2 = f2->wpath;
	const char *sep1 = NULL;
	const char *sep2 = NULL;
	int ret;

	/* Rule #3: a directory named "." sorts first */
	if (*s1 == '.' && s1[1] == '\0') {
		return -1;
	}
	if (*s2 == '.' && s2[1] == '\0') {
		return 1;
	}

	/* Advance cursor to the first difference */
	while (*s1 == *s2) {
		if (*s1 == '\0') {
			return 0;
		}
		s1++;
		s2++;
	}

	/* Rule #1: directories compare with a trailing "/" */
	if (S_ISDIR(f1->st.mode) && *s1 == '\0') {
		s1 = "/";
	} else if (S_ISDIR(f2->st.mode) && *s2 == '\0') {
		s2 = "/";
	}

	/* Rule #2: Directories sort after non-directories */
	/* Find the dirname vs basename */
	sep1 = strrchr(s1, '/');
	sep2 = strrchr(s2, '/');

	/* If its a directory, compare dirname instead of basename */
	if (!sep1 && S_ISDIR(f1->st.mode)) {
		sep1 = s1 + strlen(s1);
	}
	if (!sep2 && S_ISDIR(f2->st.mode)) {
		sep2 = s2 + strlen(s2);
	}

	if (sep1 != NULL && sep2 != NULL) {
		/* Compare basedirs, including the trailing / */
		ret = strncmp(s1, s2, MIN(sep1 - s1, sep2 - s2) + 1);
		if (ret == 0) {
			/* If both are directories, sort the shorter one first */
			if (S_ISDIR(f1->st.mode) && S_ISDIR(f2->st.mode)) {
				return strlen(s1) > strlen(s2) ? 1 : -1;
			}
			/* Compare the remainder after the common basedir */
			ret = (int)MIN(sep1 - s1, sep2 - s2) + 1;
			s1 += ret;
			s2 += ret;
			sep1 = strrchr(s1, '/');
			sep2 = strrchr(s2, '/');
			if (!sep1 && sep2) {
				return -1;
			} else if (!sep2 && sep1) {
				return 1;
			}
			return strcmp(s1, s2);
		}
	} else if (sep1) {
		return 1;
	} else if (sep2) {
		return -1;
	}

	/* Advance cursor to the next difference */
	while (*s1 == *s2) {
		if (*s1 == '\0') {
			return 0;
		}
		s1++;
		s2++;
	}

	/* Rule #1: directories compare with a trailing "/" */
	if (S_ISDIR(f1->st.mode) && *s1 == '\0') {
		s1 = "/";
	} else if (S_ISDIR(f2->st.mode) && *s2 == '\0') {
		s2 = "/";
	}

	return ((u_char)*s1 - (u_char)*s2);
}

/*
 * Like the above, but we need to guarantee the relative order of directory
 * contents to their directory.
 */
int
flist_dir_cmp(const void *p1, const void *p2)
{
	const struct flist *f1 = p1, *f2 = p2;
	size_t s1, s2;

	s1 = strlen(f1->wpath);
	s2 = strlen(f2->wpath);
	if (strncmp(f1->wpath, f2->wpath, MINIMUM(s1, s2)) == 0) {
		/*
		 * One is the prefix of the other, sort the longer one later.
		 */
		return s2 > s1 ? 1 : -1;
	}

	return strcmp(f1->wpath, f2->wpath);
}

/*
 * Deduplicate our file list (which may be zero-length).
 * Returns zero on failure, non-zero on success.
 */
static int
flist_dedupe(const struct opts *opts, struct flist **fl, size_t *sz)
{
	size_t		 i, j;
	struct flist	*new;
	struct flist	*f, *fnext;

	if (*sz == 0)
		return 1;

	/* Create a new buffer, "new", and copy. */

	new = calloc(*sz, sizeof(struct flist));
	if (new == NULL) {
		ERR("calloc");
		return 0;
	}

	for (i = j = 0; i < *sz - 1; i++) {
		f = &(*fl)[i];
		fnext = &(*fl)[i + 1];

		if (strcmp(f->wpath, fnext->wpath) ||
		    strcmp(f->wpath, ".") == 0) {
			new[j++] = *f;
			continue;
		}

		/*
		 * Our working (destination) paths are the same.
		 * If the actual file is the same (as given on the
		 * command-line), then we can just discard the first.
		 * Otherwise, we need to bail out: it means we have two
		 * different files with the relative path on the
		 * destination side.
		 */

		if (strcmp(f->path, fnext->path) == 0) {
			new[j++] = *f;
			i++;
			/* 
			 * Do not warn when we came up with the duplicates
			 * ourselves from --relative.
			 */
			if (!opts->relative && !S_ISDIR(f->st.mode))
				WARNX("%s: duplicate path: %s",
					f->wpath, f->path);
			free(fnext->path);
			free(fnext->link);
			fnext->path = fnext->link = NULL;
			continue;
		}
		ERRX("%s: duplicate working path for "
		    "possibly different file: '%s' '%s'",
		    f->wpath, f->path, fnext->path);
		free(new);
		return 0;
	}

	/* Don't forget the last entry. */

	if (i == *sz - 1)
		new[j++] = (*fl)[i];

	/*
	 * Reassign to the deduplicated array.
	 * If we started out with *sz > 0, which we check for at the
	 * beginning, then we'll always continue having *sz > 0.
	 */

	free(*fl);
	*fl = new;
	*sz = j;
	assert(*sz);
	return 1;
}

static int
flist_is_subdir(const struct flist *child, const struct flist *cparent)
{
	size_t parlen;

	parlen = strlen(cparent->path);
	if (strncmp(cparent->path, child->path, parlen) != 0)
		return (0);

	return (child->path[parlen] == '/');
}

/*
 * We're now going to find our top-level directories.
 * This only applies to recursive and dirs modes.
 * If we have the first element as the ".", then that's the "top
 * directory" of our transfer.
 * Otherwise, mark up all top-level directories in the set.
 */
static void
flist_topdirs(struct sess *sess, struct flist *fl, size_t flsz)
{
	size_t		 i;
	const char	*cp, *wpath;
	struct flist	*ltop;

	if (!sess->opts->recursive && !sess->opts->dirs)
		return;

	ltop = NULL;
	for (i = 0; i < flsz; i++) {
		if (!S_ISDIR(fl[i].st.mode))
			continue;
		if (ltop != NULL && flist_is_subdir(&fl[i], ltop))
			continue;

		wpath = fl[i].wpath;

		/*
		 * In --recursive mode, we don't need to worry about any of
		 * this, as all directories specified are top-directories.  In
		 * --dirs mode, we have to be more careful to only mark those
		 * that end in '/' or '.'.
		 */
		if (!sess->opts->recursive && strcmp(wpath, ".") != 0) {
			/* Otherwise, only those ending in '/' or '/.'. */
			cp = strrchr(fl[i].wpath, '/');
			if (cp == NULL)
				continue;

			cp++;
			if (*cp != '\0' && strcmp(cp, ".") != 0)
				continue;
		}

		ltop = &fl[i];
		fl[i].st.flags |= FLSTAT_TOP_DIR;
		LOG4("%s: top-level", fl[i].wpath);
	}
}

/*
 * Filter through the fts() file information.
 * We want directories (pre-order) and regular files.
 * Everything else is skipped and possibly warned about.
 * Return zero to skip, non-zero to examine.
 */
int
flist_fts_check(struct sess *sess, FTSENT *ent, enum fmode fmode)
{

	if (ent->fts_info == FTS_F  ||
	    ent->fts_info == FTS_D)
		return 1;

	if (ent->fts_info == FTS_DC) {
		WARNX("%s: directory cycle", ent->fts_path);
	} else if (ent->fts_info == FTS_DNR) {
		errno = ent->fts_errno;
		WARN("%s: unreadable directory", ent->fts_path);
	} else if (ent->fts_info == FTS_DOT) {
		WARNX("%s: skipping dot-file", ent->fts_path);
	} else if (ent->fts_info == FTS_ERR) {
		errno = ent->fts_errno;
		WARN("%s", ent->fts_path);
	} else if (ent->fts_info == FTS_SLNONE) {
		if (sess->opts->copy_links || sess->opts->safe_links ||
		    sess->opts->copy_unsafe_links) {
			sess->total_errors++;
			return 0;
		} else {
			return sess->opts->preserve_links != 0;
		}
	} else if (ent->fts_info == FTS_SL) {
		/*
		 * If we're the receiver, we need to skip symlinks unless we're
		 * doing --preserve-links or --copy-dirlinks.  If we're the
		 * sender, we need to send the link along.
		 */
		if (sess->opts->preserve_links || sess->opts->copy_dirlinks ||
		    fmode == FARGS_SENDER) {
			return 1;
		}
		WARNX("%s: skipping symlink (5)", ent->fts_path);
	} else if (ent->fts_info == FTS_DEFAULT) {
		if ((sess->opts->devices && (S_ISBLK(ent->fts_statp->st_mode) ||
		    S_ISCHR(ent->fts_statp->st_mode))) ||
		    (sess->opts->specials &&
		    (S_ISFIFO(ent->fts_statp->st_mode) ||
		    S_ISSOCK(ent->fts_statp->st_mode))) ||
		    fmode == FARGS_SENDER) {
			return 1;
		}
		WARNX("%s: skipping special", ent->fts_path);
	} else if (ent->fts_info == FTS_NS) {
		errno = ent->fts_errno;
		sess->total_errors++;
		WARN("%s: could not stat", ent->fts_path);
	}

	return 0;
}

/*
 * Copy necessary elements in "st" into the fields of "f".
 */
static void
flist_copy_stat(struct flist *f, const struct stat *st)
{
	f->st.mode = st->st_mode;
	f->st.uid = st->st_uid;
	f->st.gid = st->st_gid;
	f->st.size = st->st_size;
	f->st.mtime = st->st_mtime;
	f->st.rdev = st->st_rdev;
	f->st.device = st->st_dev;
	f->st.inode = st->st_ino;
	f->st.nlink = st->st_nlink;
}

void
flist_free(struct flist *f, size_t sz)
{
	size_t	 i;

	if (f == NULL)
		return;

	for (i = 0; i < sz; i++) {
		if (f[i].pdfd >= 0)
			close(f[i].pdfd);
		free(f[i].path);
		free(f[i].link);
	}
	free(f);
}

/*
 * Serialise our file list (which may be zero-length) to the wire.
 * Makes sure that the receiver isn't going to block on sending us
 * return messages on the log channel.
 * Return zero on failure, non-zero on success.
 */
int
flist_send(struct sess *sess, int fdin, int fdout, const struct flist *fl,
    size_t flsz)
{
	size_t		 i, sz, gidsz = 0, uidsz = 0, sendidsz;
	uint16_t	 flag;
	const struct flist *f;
	const char	*fn;
	struct ident	*gids = NULL, *uids = NULL;
	int		 rc = 0;

	/* Double-check that we've no pending multiplexed data. */

	LOG2("sending file metadata list: %zu", flsz);

	for (i = 0; i < flsz; i++) {
		f = &fl[i];
		fn = f->wpath;
		sz = strlen(f->wpath);
		assert(sz > 0);
		assert(sz < INT32_MAX);

		/*
		 * If applicable, unclog the read buffer.
		 * This happens when the receiver has a lot of log
		 * messages and all we're doing is sending our file list
		 * without checking for messages.
		 */

		if (sess->mplex_reads &&
		    io_read_check(sess, fdin) &&
		    !io_read_flush(sess, fdin)) {
			ERRX1("io_read_flush");
			goto out;
		}

		/*
		 * For ease, make all of our filenames be "long"
		 * regardless their actual length.
		 * This also makes sure that we don't transmit a zero
		 * byte unintentionally.
		 */

		flag = FLIST_NAME_LONG;
		if ((FLSTAT_TOP_DIR & f->st.flags))
			flag |= FLIST_TOP_LEVEL;

		/*
		 * When we need to send the extra hardlinks data:
		 * For protocol 28+: Only non-directories that have nlink > 1
		 * For protocols less than 28: All regular files
		 */
		if (sess->opts->hard_links && protocol_newflist &&
		    (!S_ISDIR(f->st.mode) && f->st.nlink > 1)) {
			flag |= FLIST_HARDLINKED;
			if (minor(f->st.rdev) <= 0xff) {
                                flag |= FLIST_RDEV_MINOR_8;
			}
		} else if (sess->opts->hard_links && S_ISREG(f->st.mode)) {
			flag |= FLIST_HARDLINKED;
		}

		if (protocol_newflist) {
			if (!flag && !S_ISDIR(f->st.mode)) {
				flag |= FLIST_TOP_LEVEL;
			}
			if ((flag & 0xFF00) || !flag) {
				flag |= FLIST_XFLAGS;
			}
		}

		LOG3("%s: sending file metadata: "
			"size %jd, mtime %jd, mode %o, flag %o",
			fn, (intmax_t)f->st.size,
			(intmax_t)f->st.mtime, f->st.mode, flag);

		/* Now write to the wire. */
		/* FIXME: buffer this. */

		if (protocol_newflist && (FLIST_XFLAGS & flag)) {
			if (!io_write_byte(sess, fdout, flag)) {
				ERRX1("io_write_byte");
				goto out;
			} else if (!io_write_byte(sess, fdout, flag >> 8)) {
				ERRX1("io_write_byte");
				goto out;
			}
		} else {
			if (!io_write_byte(sess, fdout, flag)) {
				ERRX1("io_write_byte");
				goto out;
			}
		}

		if (!io_write_int(sess, fdout, (int)sz)) {
			ERRX1("io_write_int");
			goto out;
		} else if (!io_write_buf(sess, fdout, fn, sz)) {
			ERRX1("io_write_buf");
			goto out;
		} else if (!io_write_long(sess, fdout, f->st.size)) {
			ERRX1("io_write_long");
			goto out;
		} else if (!io_write_uint(sess, fdout, (uint32_t)f->st.mtime)) {
			ERRX1("io_write_uint");
			goto out;
		} else if (!io_write_uint(sess, fdout, f->st.mode)) {
			ERRX1("io_write_uint");
			goto out;
		}

		/* Conditional part: uid. */

		if (sess->opts->preserve_uids) {
			if (!io_write_uint(sess, fdout, f->st.uid)) {
				ERRX1("io_write_uint");
				goto out;
			}
			if (!idents_add(0, &uids, &uidsz, f->st.uid)) {
				ERRX1("idents_add");
				goto out;
			}
		}

		/* Conditional part: gid. */

		if (sess->opts->preserve_gids) {
			if (!io_write_uint(sess, fdout, f->st.gid)) {
				ERRX1("io_write_uint");
				goto out;
			}
			if (!idents_add(1, &gids, &gidsz, f->st.gid)) {
				ERRX1("idents_add");
				goto out;
			}
		}

		/* Conditional part: devices & special files. */

		if ((sess->opts->devices && (S_ISBLK(f->st.mode) ||
		    S_ISCHR(f->st.mode))) ||
		    (sess->opts->specials && (S_ISFIFO(f->st.mode) ||
		    S_ISSOCK(f->st.mode)))) {
			/*
			 * Protocols less than 28, the device number is
			 * transmitted as a single int.  In newer protocols, it
			 * is sent as separate ints for the major and minor.
			 * However, if the minor is small, we can optimize it
			 * down to a byte instead.
			 */
			if (!protocol_newflist) {
				if (!io_write_int(sess, fdout, f->st.rdev)) {
					ERRX1("io_write_int");
					goto out;
				}
			} else if (!io_write_int(sess, fdout,
			    major(f->st.rdev))) {
				ERRX1("io_write_int");
				goto out;
			} else if ((FLIST_RDEV_MINOR_8 & flag) &&
			    !io_write_byte(sess, fdout, minor(f->st.rdev))) {
				ERRX1("io_write_byte");
				goto out;
			} else if (!io_write_int(sess, fdout,
			    minor(f->st.rdev))) {
				ERRX1("io_write_int");
				goto out;
			}
		}

		/* Conditional part: symbolic link. */

		if (S_ISLNK(f->st.mode) &&
		    sess->opts->preserve_links) {
			fn = f->link;
			sz = strlen(f->link);
			assert(sz < INT32_MAX);
			if (!io_write_int(sess, fdout, (int)sz)) {
				ERRX1("io_write_int");
				goto out;
			}
			if (!io_write_buf(sess, fdout, fn, sz)) {
				ERRX1("io_write_buf");
				goto out;
			}
		}

		/*
		 * Conditional part: hard link. 
		 */

		if ((FLIST_HARDLINKED & flag)) {
			/*
			 * We do not talk to older versions of the protocol,
			 * so we can always send 64 bits here.
			 */
			if (!io_write_long(sess, fdout, f->st.device)) {
				ERRX1("io_write_long");
				goto out;
			}
			if (!io_write_long(sess, fdout, f->st.inode)) {
				ERRX1("io_write_long");
				goto out;
			}
		}

		if (S_ISREG(f->st.mode) || S_ISLNK(f->st.mode))
			sess->total_size += f->st.size;

		/*
		 * In protocols 28 and newer, we don't send the checksum if
		 * the item is not a regular file.
		 */
		if (sess->opts->checksum &&
		    (!protocol_newflist || S_ISREG(f->st.mode))) {
			if (!io_write_buf(sess, fdout, f->md, sizeof(f->md))) {
				ERRX1("io_write_buf checksum");
				goto out;
			}
		}

		/*
		 * Keep this at the very end; platform should emit a suitable
		 * looking error.
		 */
		if (f->sent != NULL && !f->sent(sess, fdout, f)) {
			ERRX1("platform sent");
			goto out;
		}
	}

	/* Signal end of file list. */

	if (!io_write_byte(sess, fdout, 0)) {
		ERRX1("io_write_byte");
		goto out;
	}

	/* Conditionally write identifier lists. */

	if (sess->opts->preserve_uids && sess->opts->numeric_ids != NIDS_FULL) {
		/* Account for "stealth" --numeric-ids, don't always send it. */
		if (!sess->opts->numeric_ids)
			sendidsz = uidsz;
		else
			sendidsz = 0;
		LOG2("sending uid list: %zu", sendidsz);
		if (!idents_send(sess, fdout, uids, sendidsz)) {
			ERRX1("idents_send");
			goto out;
		}
	}

	if (sess->opts->preserve_gids && sess->opts->numeric_ids != NIDS_FULL) {
		/* Account for "stealth" --numeric-ids, don't always send it. */
		if (!sess->opts->numeric_ids)
			sendidsz = gidsz;
		else
			sendidsz = 0;
		LOG2("sending gid list: %zu", sendidsz);
		if (!idents_send(sess, fdout, gids, sendidsz)) {
			ERRX1("idents_send");
			goto out;
		}
	}

	rc = 1;
out:
	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	return rc;
}

/*
 * Read the filename of a file list.
 * This is the most expensive part of the file list transfer, so a lot
 * of attention has gone into transmitting as little as possible.
 * Micro-optimisation, but whatever.
 * Fills in "f" with the full path on success.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_recv_name(struct sess *sess, int fd, struct flist *f, uint8_t flags,
    char last[PATH_MAX])
{
	uint8_t		 bval;
	size_t		 partial = 0;
	size_t		 pathlen = 0, len;

	/*
	 * Read our filename.
	 * If we have FLIST_NAME_SAME, we inherit some of the last
	 * transmitted name.
	 * If we have FLIST_NAME_LONG, then the string length is greater
	 * than byte-size.
	 */

	if (FLIST_NAME_SAME & flags) {
		if (!io_read_byte(sess, fd, &bval)) {
			ERRX1("io_read_byte");
			return 0;
		}
		partial = bval;
	}

	/* Get the (possibly-remaining) filename length. */

	if (FLIST_NAME_LONG & flags) {
		if (!io_read_size(sess, fd, &pathlen)) {
			ERRX1("io_read_size");
			return 0;
		}
	} else {
		if (!io_read_byte(sess, fd, &bval)) {
			ERRX1("io_read_byte");
			return 0;
		}
		pathlen = bval;
	}

	/* Allocate our full filename length. */
	/* FIXME: maximum pathname length. */

	if ((len = pathlen + partial) == 0) {
		ERRX("security violation: zero-length pathname");
		return 0;
	}

	if ((f->path = malloc(len + 1)) == NULL) {
		ERR("malloc");
		return 0;
	}
	f->path[len] = '\0';

	if (FLIST_NAME_SAME & flags)
		memcpy(f->path, last, partial);

	if (!io_read_buf(sess, fd, f->path + partial, pathlen)) {
		ERRX1("io_read_buf");
		return 0;
	}

	if (f->path[0] == '/' && !sess->opts->relative) {
		ERRX("security violation: absolute pathname: %s",
		    f->path);
		return 0;
	}

	if (strstr(f->path, "/../") != NULL ||
	    (len > 2 && strcmp(f->path + len - 3, "/..") == 0) ||
	    (len > 2 && strncmp(f->path, "../", 3) == 0) ||
	    strcmp(f->path, "..") == 0) {
		ERRX("%s: security violation: backtracking pathname",
		    f->path);
		return 0;
	}

	/* Record our last path and construct our filename. */

	strlcpy(last, f->path, PATH_MAX);
	if (sess->opts->relative && f->path[0] == '/') {
		f->wpath = f->path;
		while (f->wpath[0] == '/') {
			f->wpath++;
			len--;
		}

		/*
		 * f->path is allocated on the heap, so we just preserve that as
		 * the beginning of the path instead of having to add another
		 * pointer to retain the start of the buffer.
		 */
		memmove(f->path, f->wpath, len + 1);
	}
	f->wpath = f->path;
	return 1;
}

/*
 * Reallocate a file list in chunks of FLIST_CHUNK_SIZE;
 * Returns zero on failure, non-zero on success.
 */
static int
flist_realloc(struct flist **fl, size_t *sz, size_t *max)
{
	void	*pp;

	if (*sz + 1 <= *max)  {
		(*sz)++;
		return 1;
	}

	pp = recallocarray(*fl, *max,
		*max + FLIST_CHUNK_SIZE, sizeof(struct flist));
	if (pp == NULL) {
		ERR("recallocarray flist");
		return 0;
	}
	*fl = pp;
	*max += FLIST_CHUNK_SIZE;
	for (size_t i = *sz; i < *max; i++)
		(*fl)[i].pdfd = (*fl)[i].sendidx = -1;

	(*sz)++;
	return 1;
}

/*
 * Reallocate a file list in chunks of FLIST_CHUNK_SIZE;
 * Returns -1 on failure, index of new element on success.
 */
long
fl_new_index(struct fl *fl)
{
	if (flist_realloc(&fl->flp, &fl->sz, &fl->max) == 0)
		return -1;

	return fl->sz - 1;
}

/*
 * Returns a pointer to the new element, or NULL on error.
 */
struct flist *
fl_new(struct fl *fl)
{
	long index;

	index = fl_new_index(fl);
	if (index == -1)
		return NULL;
	return &(fl->flp[index]);
}

void
fl_pop(struct fl *fl)
{
	assert(fl->sz > 0);
	fl->sz--;
}

void
fl_init(struct fl *fl)
{
	memset(fl, 0, sizeof(*fl));
}

long
fl_curridx(struct fl *fl)
{
	assert(fl->sz);
	return fl->sz - 1;
}

struct flist *
fl_atindex(struct fl *fl, size_t idx)
{
	if (idx >= fl->sz) {
		ERRX("flist index error");
		return NULL;
	}
	return &(fl->flp[idx]);
}

void
fl_print(const char *id, struct fl *fl)
{
	size_t i;

	fprintf(stderr, "%s: fl_print sz %ld\n", id, fl->sz);
	if (fl->sz  < 1)
		return;
	for (i = 0; i < fl->sz -1; i++) {
		fprintf(stderr, "%s: fl idx %ld\n", id, i);
		fprintf(stderr, "%s: fl path %p '%s'\n", id, fl->flp[i].path, fl->flp[i].path);
		fprintf(stderr, "%s: fl wpath %p '%s'\n", id, fl->flp[i].wpath, fl->flp[i].wpath);
	}
}

static void
flist_chmod(const struct sess *sess, struct flist *ff)
{
	mode_t mode = ff->st.mode;

	if (S_ISDIR(mode)) {
		mode &= ~sess->chmod_dir_AND;
		mode |= sess->chmod_dir_OR;
		mode |= sess->chmod_dir_X;
	} else {
		mode &= ~sess->chmod_file_AND;
		mode |= sess->chmod_file_OR;
		if (ff->st.mode & (S_IXUSR | S_IXGRP | S_IXOTH))
			mode |= sess->chmod_file_X;
	}

	ff->st.mode = mode;
}

/*
 * Copy all the elements of path that are directories.
 * We need those for --relative, because we need to
 * restore their stat(2) values.
 */
static int
flist_append_dirs(const char *path, struct fl *fl)
{
	const char *wbegin;
	char *pos;
	struct stat st;
	struct flist *f;

	wbegin = path;
	while (wbegin[0] == '/')
		wbegin++;
	if ((pos = strrchr(wbegin, '/')) != NULL) {
		char *begin;

		if ((begin = strdup(path)) == NULL) {
			ERR("strdup");
			goto out;
		}

		wbegin = begin + (wbegin - path);
		pos = begin + (pos - path);
		*pos = '\0';

		if ((stat(begin, &st)) == -1) {
			ERR("%s: stat", begin);
			free(begin);
			goto out;
		}

		if ((f = fl_new(fl)) == NULL) {
			ERRX1("flist_realloc");
			free(begin);
			goto out;
		}

		memset(f, 0, sizeof(struct flist));
		f->path = begin;
		f->wpath = wbegin;
		flist_copy_stat(f, &st);
		if (strchr(wbegin, '/') != NULL) {
			if (!flist_append_dirs(begin, fl)) {
				ERRX1("flist_append_dirs");
				goto out;
			}
		}
	}

	return 1;
out:
	/* Error */
	return 0;
}

/*
 * Copy a regular or symbolic link file "path" into "f".
 * This handles the correct path creation and symbolic linking.
 * Returns zero on failure, non-zero on success.
*/
static int
flist_append(struct sess *sess, const struct stat *st,
	const char *path, struct fl *fl)
{
	struct flist *f;
	long oldidx;

	if ((oldidx = fl_new_index(fl)) == -1) {
		ERRX("fl_new failed");
		return 0;
	}
	/*
	 * Copy the full path for local addressing and transmit
	 * only the filename part for the receiver, unless
	 * --relative is given.
	 */
	f = fl_atindex(fl, oldidx);
	if ((f->path = strdup(path)) == NULL) {
		ERR("strdup");
		return 0;
	}

	if (!sess->opts->relative) {
		if ((f->wpath = strrchr(f->path, '/')) == NULL)
			f->wpath = f->path;
		else
			f->wpath++;
	} else {
		f->wpath = f->path;
		while (f->wpath[0] == '/')
			f->wpath++;
		if (!flist_append_dirs(f->path, fl)) {
			ERR("flist_append_dirs");
			return 0;
		}

		/*
		 * flist_append_dirs() may re-allocate our flist out from
		 * underneath us, reload the flist entry we're working on as
		 * needed.
		 */
		f = fl_atindex(fl, oldidx);
	}

	/*
	 * On the receiving end, we'll strip out all bits on the
	 * mode except for the file permissions.
	 * No need to warn about it here.
	 */

	flist_copy_stat(f, st);

	if (sess->opts->chmod != NULL) {
		/* Client-sender --chmod */
		flist_chmod(sess, f);
	}

	/* Optionally copy link information. */

	if (S_ISLNK(st->st_mode)) {
		char *link;

		link = symlink_read(f->path);
		if (link == NULL) {
			sess->total_errors++;
			ERRX1("symlink_read");
			return 0;
		}

		/*
		 * Give the installed filter a chance at it; it may need to
		 * append to the link we have, so we may end up with an entirely
		 * new string.
		 */
		if (sess->symlink_filter != NULL) {
			int error;

			error = sess->symlink_filter(link, &f->link,
			    FARGS_SENDER);
			if (error != 0) {
				sess->total_errors++;
				ERRX1("symlink_filter");
				return 0;
			}

			if (f->link == NULL)
				f->link = link;
			else
				free(link);
		} else {
			f->link = link;
		}
	}

	if (sess->opts->checksum && S_ISREG(f->st.mode) && f->st.size > 0) {
		int rc;

		rc = hash_file_by_path(AT_FDCWD, f->path, f->st.size, f->md);
		if (rc) {
			sess->total_errors++;
			ERRX1("hash_file_by_path");
			return 0;
		}
	}

	return 1;
}

/*
 * Receive a file list from the wire, filling in length "sz" (which may
 * possibly be zero) and list "flp" on success.
 * Return zero on failure, non-zero on success.
 */
int
flist_recv(struct sess *sess, int fdin, int fdout, struct flist **flp, size_t *sz)
{
	struct flist	*fl = NULL;
	struct flist	*ff;
	const struct flist *fflast = NULL;
	size_t		 flsz = 0, flmax = 0, lsz, gidsz = 0, uidsz = 0;
	uint16_t	 flag;
	char		 last[PATH_MAX];
	int64_t		 lval; /* temporary values... */
	int32_t		 ival;
	uint32_t	 uival;
	uint8_t		 bval;
	struct ident	*gids = NULL, *uids = NULL;

	last[0] = '\0';

	for (;;) {
		if (!io_read_byte(sess, fdin, &bval)) {
			ERRX1("io_read_byte");
			goto out;
		}
		flag = bval;
		if (flag == 0) {
			break;
		}
		/* Read the second byte of flags if there is one */
		if (protocol_newflist && (FLIST_XFLAGS & flag)) {
			if (!io_read_byte(sess, fdin, &bval)) {
				ERRX1("io_read_byte");
				goto out;
			}
			flag |= bval << 8;
		}

		/*
		 * The protocol uses ints for indexing, so we can't go too crazy here.
		 */
		if (flsz == INT_MAX) {
			ERR("remote sent too many files");
			goto out;
		}

		if (!flist_realloc(&fl, &flsz, &flmax)) {
			ERRX1("flist_realloc");
			goto out;
		}

		ff = &fl[flsz - 1];
		fflast = flsz > 1 ? &fl[flsz - 2] : NULL;

		/* Filename first. */

		if (!flist_recv_name(sess, fdin, ff, flag, last)) {
			ERRX1("flist_recv_name");
			goto out;
		}

		/* Read the file size. */

		if (!io_read_long(sess, fdin, &lval)) {
			ERRX1("io_read_long");
			goto out;
		}
		ff->st.size = lval;

		/* Read the modification time. */

		if (!(FLIST_TIME_SAME & flag)) {
			if (!io_read_uint(sess, fdin, &uival)) {
				ERRX1("io_read_uint");
				goto out;
			}
			ff->st.mtime = uival;	/* beyond 2038 */
		} else if (fflast == NULL) {
			WARNX1("same time without last entry");
			ff->st.mtime = 0;
		}  else
			ff->st.mtime = fflast->st.mtime;

		/* Read the file mode. */

		if (!(FLIST_MODE_SAME & flag)) {
			if (!io_read_uint(sess, fdin, &uival)) {
				ERRX1("io_read_uint");
				goto out;
			}
			ff->st.mode = uival;
		} else if (fflast == NULL) {
			WARNX1("same mode without last entry");
			ff->st.mode = 0;
		} else
			ff->st.mode = fflast->st.mode;

		if (S_ISDIR(ff->st.mode) && (flag & FLIST_TOP_LEVEL) != 0)
			ff->st.flags |= FLSTAT_TOP_DIR;

		if (sess->opts->chmod != NULL) {
			/* Client-receiver --chmod */
			flist_chmod(sess, ff);
		}

		/* Conditional part: uid. */

		if (sess->opts->preserve_uids) {
			if (!(FLIST_UID_SAME & flag)) {
				if (!io_read_uint(sess, fdin, &uival)) {
					ERRX1("io_read_int");
					goto out;
				}
				ff->st.uid = uival;
			} else if (fflast == NULL) {
				/*
				 * rsync 2.6.9 would sometimes send some of
				 * these because it used a comparison against a
				 * static 0 for uid/gid in determining this
				 * without checking if it had actually send a
				 * file before.
				 */
				WARNX1("same uid without last entry");
				ff->st.uid = 0;
			} else
				ff->st.uid = fflast->st.uid;
		}

		/* Conditional part: gid. */

		if (sess->opts->preserve_gids) {
			if (!(FLIST_GID_SAME & flag)) {
				if (!io_read_uint(sess, fdin, &uival)) {
					ERRX1("io_read_uint");
					goto out;
				}
				ff->st.gid = uival;
			} else if (fflast == NULL) {
				/*
				 * rsync 2.6.9 would sometimes send some of
				 * these because it used a comparison against a
				 * static 0 for uid/gid in determining this
				 * without checking if it had actually send a
				 * file before.
				 */
				WARNX1("same gid without last entry");
				ff->st.gid = 0;
			} else
				ff->st.gid = fflast->st.gid;
		}

		/* Conditional part: devices & special files. */

		if (((sess->opts->devices && (S_ISBLK(ff->st.mode) ||
		    S_ISCHR(ff->st.mode))) ||
		    (sess->opts->specials && (S_ISFIFO(ff->st.mode) ||
		    S_ISSOCK(ff->st.mode)))) && !protocol_newflist) {
			/*
			 * Protocols less than 28, the device number is
			 * transmitted as a single int.
			 */
			if (!(FLIST_RDEV_SAME & flag)) {
				if (!io_read_int(sess, fdin, &ival)) {
					ERRX1("io_read_int");
					goto out;
				}
				ff->st.rdev = ival;
			} else if (fflast == NULL) {
				WARNX1("same device without last entry");
				ff->st.rdev = 0;
			} else {
				ff->st.rdev = fflast->st.rdev;
			}
		} else if ((sess->opts->devices && (S_ISBLK(ff->st.mode) ||
		    S_ISCHR(ff->st.mode))) ||
		    (sess->opts->specials && (S_ISFIFO(ff->st.mode) ||
		    S_ISSOCK(ff->st.mode)))) {
			uint32_t dev_major, dev_minor;
			/*
			 * In protocol 28 and newer, the device number is sent
			 * as separate ints for the major and minor.
			 * However, if the minor is small, we can optimize it
			 * down to a byte instead.
			 */
			if (!(FLIST_RDEV_MAJOR_SAME & flag)) {
				if (!io_read_int(sess, fdin, &ival)) {
					ERRX1("io_read_int");
					goto out;
				}
				dev_major = ival;
			} else if (fflast == NULL) {
				WARNX1("same device major without last entry");
				dev_major = 0;
			} else {
				dev_major = fflast->st.rdev;
			}

			if ((FLIST_RDEV_MINOR_8 & flag)) {
				if (!io_read_byte(sess, fdin, &bval)) {
					ERRX1("io_read_int");
					goto out;
				}
				dev_minor = bval;
			} else {
				if (!io_read_int(sess, fdin, &ival)) {
					ERRX1("io_read_int");
					goto out;
				}
				dev_minor = ival;
			}

			ff->st.rdev = makedev(dev_major, dev_minor);
		}

		/* Conditional part: symbolic link. */

		if (S_ISLNK(ff->st.mode) &&
		    sess->opts->preserve_links) {
			char *link;

			if (!io_read_size(sess, fdin, &lsz)) {
				ERRX1("io_read_size");
				goto out;
			} else if (lsz == 0) {
				ERRX("empty link name");
				goto out;
			}
			link = calloc(lsz + 1, 1);
			if (link == NULL) {
				ERR("calloc");
				goto out;
			}
			if (!io_read_buf(sess, fdin, link, lsz)) {
				free(link);
				ERRX1("io_read_buf");
				goto out;
			}

			/* Give an installed filter a shot. */
			if (sess->symlink_filter != NULL) {
				int error;

				error = sess->symlink_filter(link, &ff->link,
				    FARGS_RECEIVER);

				if (error != 0) {
					free(link);
					ERRX1("symlink_filter");
					goto out;
				}

				if (ff->link == NULL)
					ff->link = link;
				else
					free(link);
			} else {
				ff->link = link;
			}
		}

		/*
		 * Conditional part: hard link. 
		 * All plain files send this info.
		 */

		if (sess->opts->hard_links && !protocol_newflist &&
		    S_ISREG(ff->st.mode)) {
			flag |= FLIST_HARDLINKED;
		}

		if ((FLIST_HARDLINKED & flag)) {
			/*
			 * We do not talk to older versions of the protocol,
			 * so we can always read 64 bits here.
			 */
			if (!(FLIST_DEV_SAME & flag)) {
				if (!io_read_ulong(sess, fdin,
				    (uint64_t *)&lval)) {
					ERRX1("io_read_long");
					goto out;
				}
				ff->st.device = lval;
			} else if (fflast == NULL) {
				WARNX1("same device without last entry");
				ff->st.device = 0;
			} else {
				ff->st.device = fflast->st.device;
			}

			if (!io_read_long(sess, fdin, &ff->st.inode)) {
				ERRX1("io_read_long");
				goto out;
			}
		}

		/*
		 * Keep this at the very end; platform should emit a suitable
		 * looking error.
		 */
		if (!platform_flist_entry_received(sess, fdin, ff))
			goto out;

		LOG3("%s: received file metadata: "
			"size %jd, mtime %jd, mode %o, rdev (%d, %d), flag %x",
			ff->path, (intmax_t)ff->st.size,
			(intmax_t)ff->st.mtime, ff->st.mode,
			major(ff->st.rdev), minor(ff->st.rdev),
			flag);

		if (S_ISREG(ff->st.mode) || S_ISLNK(ff->st.mode))
			sess->total_size += ff->st.size;

		/*
		 * In protocols 28 and newer, we don't get the checksum if
		 * the item is not a regular file.
		 */
		if (sess->opts->checksum &&
		    (!protocol_newflist || S_ISREG(ff->st.mode))) {
			if (!io_read_buf(sess, fdin, ff->md, sizeof(ff->md))) {
				ERRX1("io_read_buf");
				goto out;
			}
		}
	}

	/* Conditionally read the user/group list. */

	if (sess->opts->preserve_uids && sess->opts->numeric_ids != NIDS_FULL) {
		if (!idents_recv(sess, fdin, &uids, &uidsz)) {
			ERRX1("idents_recv");
			goto out;
		}
		LOG2("received uid list: %zu", uidsz);
	}

	if (sess->opts->preserve_gids && sess->opts->numeric_ids != NIDS_FULL) {
		if (!idents_recv(sess, fdin, &gids, &gidsz)) {
			ERRX1("idents_recv");
			goto out;
		}
		LOG2("received gid list: %zu", gidsz);
	}

	/* Remember to order the received list. */

	LOG2("received file metadata list: %zu", flsz);
	if (protocol_newsort) {
		qsort(fl, flsz, sizeof(struct flist), flist_cmp29);
	} else {
		qsort(fl, flsz, sizeof(struct flist), flist_cmp);
	}
	/*
	 * It's important that we keep track of the send index now, because we
	 * may want to trim or dedupe the flist before we proceed.  openrsync
	 * may dedupe on the sender side, but the reference rsync will not in
	 * order to give receivers flexibility in how they handle it.
	 */
	for (size_t i = 0; i < flsz; i++)
		fl[i].sendidx = (int)i;

	platform_flist_received(sess, fl, flsz);

	*sz = flsz;
	*flp = fl;

	/* Conditionally remap and reassign identifiers. */

	if (sess->opts->preserve_uids && !sess->opts->numeric_ids) {
		idents_remap(sess, 0, uids, uidsz);
		idents_assign_uid(sess, fl, flsz, uids, uidsz);
	}

	if (sess->opts->preserve_gids && !sess->opts->numeric_ids) {
		idents_remap(sess, 1, gids, gidsz);
		idents_assign_gid(sess, fl, flsz, gids, gidsz);
	}

	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	return 1;
out:
	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	*sz = 0;
	*flp = NULL;
	return 0;
}

static int
flist_gen_dirent_file(struct sess *sess, const char *type, const char *root,
    struct fl *fl, const struct stat *st)
{
	/* filter files */
	if (rules_match(root, S_ISDIR(st->st_mode), FARGS_SENDER, 0) == -1) {
		WARNX("%s: skipping excluded %s", root, type);
		return 1;
	}
	/* don't even try to add it if we can't read it, just succeed. */
	if (sess->opts->ignore_nonreadable && access(root, R_OK) != 0)
		return 1;
	/* add it to our world view */
	if (!flist_append(sess, st, root, fl)) {
		ERRX1("flist_append");
		return 0;
	}

	return 1;
}

static ssize_t
flist_dirent_strip(struct sess *sess, const char *root)
{
	char	 *cp;
	ssize_t	 stripdir;

	/*
	 * If we end with a slash, it means that we're not supposed to
	 * copy the directory part itself---only the contents.
	 * So set "stripdir" to be what we take out.
	 */
	stripdir = strlen(root);
	assert(stripdir > 0);
	if (root[stripdir - 1] != '/')
		stripdir = 0;

	/*
	 * If we're not stripping anything, then see if we need to strip
	 * out the leading material in the path up to and including the
	 * last directory component.
	 */

	if (stripdir == 0 && !sess->opts->relative)
		if ((cp = strrchr(root, '/')) != NULL)
			stripdir = cp - root + 1;

	return (stripdir);
}

static int
flist_normalize_path(const char *root, char *rootbuf, size_t rootbufsz)
{
	size_t rootlen;

	rootlen = strlen(root);
	/* Convert trailing '/./' -> '/.' */
	if (rootlen >= 3 && strcmp(&root[rootlen - 3], "/./") == 0)
		rootlen--;
	if (rootlen >= rootbufsz)
		return (0);

	memcpy(rootbuf, root, rootlen);
	/* Convert trailing '/' -> '/.' */
	if (rootbuf[rootlen - 1] == '/')
		rootbuf[rootlen++] = '.';
	rootbuf[rootlen] = '\0';
	return (1);
}

/*
 * Generate a flist possibly-recursively given a file root, which may
 * also be a regular file or symlink.
 * On success, augments the generated list in "flp" of length "sz".
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_dirent(struct sess *sess, const char *root, struct fl *fl, ssize_t stripdir)
{
	const char	*cargv[2];
	int		 rc = 0, flag;
	FTS		*fts;
	FTSENT		*ent;
	struct flist	*f;
	size_t		 i, nxdev = 0;
	ssize_t		 pstripdir = stripdir;
	dev_t		*newxdev, *xdev = NULL;
	struct stat	 st, st2;
	int              ret;
	char             buf[PATH_MAX], buf2[PATH_MAX], rootbuf[BIGPATH_MAX];

	/*
	 * If we're a file, then revert to the same actions we use for
	 * the non-recursive scan.
	 */

	if (sess->opts->copy_links)
		ret = stat(root, &st);
	else			
		ret = lstat(root, &st);
	if (ret == -1) {
		sess->total_errors++;
		ERR("%s: (l)stat", root);
		return 0;
	} else if (S_ISREG(st.st_mode)) {
		return flist_gen_dirent_file(sess, "file", root, fl, &st);
	} else if (S_ISLNK(st.st_mode)) {
		/*
		 * How does this work?
		 * - see whether the symlink target is a dir
		 * - if yes, recurse
		 *
		 * We did an lstat, now we need a stat.
		 */
		if (sess->opts->copy_dirlinks ||
		    sess->opts->copy_unsafe_links ||
		    sess->opts->safe_links) {
			if (stat(root, &st2) == -1) {
				sess->total_errors++;
				ERR("%s: stat", root);
				return 0;
			}
			if ((ret = (int)readlink(root, buf, sizeof(buf))) == -1) {
				sess->total_errors++;
				ERR("%s: readlink", root);
				return 0;
			}
			buf[ret] = '\0';
		}
		if (sess->opts->safe_links &&
		    is_unsafe_link(buf, root, root)) {
			LOG1("ignoring unsafe symlink: %s -> %s", root, buf);
			return 1;
		}
		if (sess->opts->copy_dirlinks) {
			if (S_ISDIR(st2.st_mode)) {
				if (stripdir == -1)
					stripdir = flist_dirent_strip(sess, root);
				snprintf(buf2, sizeof(buf2), "%s/", root);
				LOG4("symlinks: recursing '%s' -> '%s' '%s'\n",
				    root, buf, buf2);
				flist_gen_dirent(sess, buf2, fl, stripdir);
				return 1;
			}
		}

		if (sess->opts->copy_unsafe_links &&
		    is_unsafe_link(buf, root, root)) {
			if (S_ISDIR(st2.st_mode)) {
				if (stripdir == -1)
					stripdir = flist_dirent_strip(sess, root);
				snprintf(buf2, sizeof(buf2), "%s/", root);
				LOG4("symlinks: recursing '%s' -> '%s' '%s'",
				    root, buf, buf2);
				return flist_gen_dirent(sess, buf2, fl, stripdir);
			} else {
				return flist_gen_dirent_file(sess, "file",
				    root, fl, &st2);
			}
		}

		return flist_gen_dirent_file(sess, "symlink", root, fl, &st);
	} else if (!S_ISDIR(st.st_mode)) {
		return flist_gen_dirent_file(sess, "special", root, fl, &st);
	}

	if (!flist_normalize_path(root, rootbuf, sizeof(rootbuf))) {
		/*
		 * If we failed to normalize the path, that's catastrophic and
		 * we should bail out to be safe.  Notably, we could end up with
		 * sorting issues that lead to us being very confused about what
		 * we're transferring.
		 */
		ERR("%s: flist_normalize_path", root);
		return 0;
	}

	cargv[0] = rootbuf;
	cargv[1] = NULL;

	if (stripdir == -1)
		stripdir = flist_dirent_strip(sess, rootbuf);

	/*
	 * If we're recursive, then we need to take down all of the
	 * files and directory components, so use fts(3).
	 * Copying the information file-by-file into the flstat.
	 * We'll make sense of it in flist_send.
	 */

	if ((fts = fts_open((char * const *)cargv,
		    sess->opts->copy_links ? FTS_LOGICAL: FTS_PHYSICAL |
		    FTS_NOCHDIR, NULL)) == NULL) {
		sess->total_errors++;
		ERR("fts_open");
		return 0;
	}

	errno = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_DP)
			rules_dir_pop(ent->fts_path, stripdir);

		if (!flist_fts_check(sess, ent, FARGS_SENDER)) {
			errno = 0;
			continue;
		}

		if (ent->fts_info == FTS_D)
			rules_dir_push(ent->fts_path, stripdir,
			    sess->opts->from0 ? 0 : '\n');

		/* We don't allow symlinks without -l. */

		assert(ent->fts_statp != NULL);
		if (S_ISLNK(ent->fts_statp->st_mode)) {
			if (sess->opts->copy_dirlinks ||
			    sess->opts->copy_unsafe_links ||
			    sess->opts->safe_links) {
				/* We did lstat, now we need stat */
				if (stat(ent->fts_accpath, &st2) == -1) {
					ERR("%s: stat", ent->fts_accpath);
					sess->total_errors++;
					continue;
				}
				if ((ret = (int)readlink(ent->fts_accpath, buf, sizeof(buf))) == -1) {
					ERR("%s: readlink", ent->fts_accpath);
					sess->total_errors++;
					continue;
				}
				buf[ret] = '\0';
			}
			if (sess->opts->safe_links &&
			    is_unsafe_link(buf, ent->fts_accpath, rootbuf)) {
				LOG1("ignoring unsafe symlink %s -> %s", ent->fts_accpath, buf);
				continue;
			}
			if (sess->opts->copy_dirlinks) {
				if (S_ISDIR(st2.st_mode)) {
					ret = flist_gen_dirent(sess, ent->fts_path,
					    fl, stripdir);
					if (!ret)
						sess->total_errors++;

					continue;
				}
			}
		}

		/*
		 * If rsync is told to avoid crossing a filesystem
		 * boundary when recursing, then replace all mount point
		 * directories with empty directories.  The latter is
		 * prevented by telling rsync multiple times to avoid
		 * crossing a filesystem boundary when recursing.
		 * Replacing mount point directories is tricky. We need
		 * to sort out which directories to include.  As such,
		 * keep track of unique device inodes, and use these for
		 * comparison.
		 */

		if (sess->opts->one_file_system &&
		    ent->fts_statp->st_dev != st.st_dev) {
			if (sess->opts->one_file_system > 1 ||
			    !S_ISDIR(ent->fts_statp->st_mode))
				continue;

			flag = 0;
			for (i = 0; i < nxdev; i++)
				if (xdev[i] == ent->fts_statp->st_dev) {
					flag = 1;
					break;
				}
			if (flag)
				continue;

			if ((newxdev = reallocarray(xdev, nxdev + 1,
			    sizeof(dev_t))) == NULL) {
				ERRX1("reallocarray flist_gen_dirent()");
				goto out;
			}
			xdev = newxdev;
			xdev[nxdev] = ent->fts_statp->st_dev;
			nxdev++;
		}

		/* This is for macOS fts, which returns "foo//bar" */
		if (ent->fts_path[stripdir] == '/') {
			stripdir++;
		}
		/* filter files */
		if (rules_match(ent->fts_path + stripdir,
		    (ent->fts_info == FTS_D), FARGS_SENDER, 0) == -1) {
			WARNX("%s: skipping excluded file",
			    ent->fts_path + stripdir);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* don't even try to add it if we can't read it, just succeed. */
		if (sess->opts->ignore_nonreadable &&
		    access(ent->fts_path, R_OK) != 0)
			continue;

		/* Allocate a new file entry. */

		if ((f = fl_new(fl)) == NULL) {
			ERRX1("flist_realloc");
			goto out;
		}

		/* Our path defaults to "." for the root. */

		if (ent->fts_path[stripdir] == '\0') {
			if (asprintf(&f->path, "%s.", ent->fts_path) == -1) {
				ERR("asprintf");
				f->path = NULL;
				goto out;
			}
		} else {
			if ((f->path = strdup(ent->fts_path)) == NULL) {
				ERR("strdup");
				goto out;
			}
		}

		if (pstripdir != -1) {
			/*
			 * We recursed; must not have "foo/." dir specs.
			 * If we do mkdir fails.
			 */
			size_t sz2 = strlen(f->path);

			if (sz2 >= 2 && f->path[sz2 - 1] == '.' &&
			    f->path[sz2 - 2] == '/') {
				(f->path)[sz2 - 2] = '\0';
			}

			/*
			 * If we recursed a symlink, removing the trailing
			 * "/" to avoid issues sorting the flist.
			 */
			if (sz2 >= 1 && f->path[sz2 - 1] == '/') {
				(f->path)[sz2 - 1] = '\0';
			}
		}

		f->wpath = f->path + stripdir;
		flist_copy_stat(f, ent->fts_statp);

		/* Optionally copy link information. */

		if (S_ISLNK(ent->fts_statp->st_mode)) {
			if (sess->opts->copy_unsafe_links &&
			    is_unsafe_link(buf, ent->fts_path, rootbuf)) {
				flist_copy_stat(f, &st2);
				LOG3("copy_unsafe_links: converting unsafe "
				    "link %s -> %s to a regular file\n",
				    ent->fts_path, buf);
			} else {
				f->link = symlink_read(ent->fts_accpath);
				if (f->link == NULL) {
					ERRX1("symlink_read");
					sess->total_errors++;
					fl_pop(fl);
					continue;
				}
			}
		}

		if (sess->opts->checksum &&
		    S_ISREG(f->st.mode) && f->st.size > 0) {
			rc = hash_file_by_path(AT_FDCWD, f->path, f->st.size, f->md);
			if (rc) {
				ERR("%s: hash_file_by_path", f->path);
				sess->total_errors++;
				fl_pop(fl);
				continue;
			}
		}

		/* Reset errno for next fts_read() call. */
		errno = 0;
	}
	if (errno) {
		ERR("fts_read");
		goto out;
	}

	LOG3("generated %zu filenames: %s", fl->sz, root);
	rc = 1;
out:
	fts_close(fts);
	free(xdev);
	return rc;
}

/*
 * Generate a flist recursively given the array of directories (or
 * files, symlinks, doesn't matter) specified in argv (argc >0).
 * On success, stores the generated list in "flp" with length "sz",
 * which may be zero.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_dirs(struct sess *sess, size_t argc, char **argv, struct fl *fl)
{
	const char	*dname;
	size_t		 i;
	int		 errors = 0;

	for (i = 0; i < argc; i++) {
		dname = argv[i];
		if (dname[0] == '\0')
			dname = ".";
		rules_base(dname);
		if (sess->opts->relative) {
			if (!flist_append_dirs(dname, fl)) {
				ERR("flist_append_dirs");
				return 0;
			}
		}
		if (!flist_gen_dirent(sess, dname, fl, -1))
			errors++;
	}

	LOG2("recursively generated %zu filenames", fl->sz);

	return errors ? 0 : 1;
}

/*
 * Generate list of files from the command-line argc (>0) and argv.
 * On success, stores the generated list in "flp" with length "sz",
 * which may be zero.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_files(struct sess *sess, size_t argc, char **argv, struct fl *fl)
{
	const char	*fname;
	size_t		 i;
	struct stat	 st;
	int              ret;

	assert(argc);

	rules_base(".");
	if ((fl->flp = calloc(argc, sizeof(struct flist))) == NULL) {
		ERR("calloc");
		return 0;
	}
	fl->max = argc;
	fl->sz = 0;

	for (i = 0; i < argc; i++) {
		fname = argv[i];
		if (fname[0] == '\0')
			fname = ".";
		if (sess->opts->copy_links)
			ret = stat(fname, &st);
		else
			ret = lstat(fname, &st);

		if (ret == -1) {
			sess->total_errors++;
			ERR("'%s': (l)stat", fname);
			goto out;
		}

		/*
		 * File type checks.
		 * In non-recursive mode, we don't accept directories.
		 * We also skip symbolic links without -l.
		 * Beyond that, we only accept regular files unless we're
		 * allowing specials or devices.
		 */

		if (S_ISDIR(st.st_mode)) {
			if (!sess->opts->dirs) {
				WARNX("%s: skipping directory", fname);
				continue;
			}
		}

		/* filter files */
		if (rules_match(fname, S_ISDIR(st.st_mode), FARGS_SENDER,
		    0) == -1) {
			WARNX("%s: skipping excluded file", fname);
			continue;
		}

		/* Add this file to our file-system worldview. */
		if (!flist_append(sess, &st, fname, fl)) {
			ERRX1("flist_append");
			goto out;
		}
	}

	LOG2("non-recursively generated %zu filenames", fl->sz);
	return 1;
out:
	flist_free(fl->flp, argc);
	fl->flp = NULL;
	return 0;
}

#if 0
/*
 * Generate a list of files from a syncfile that are contained within
 * the arguments given on the command line.
 * This overrides everything we're given on the command line.
 * TODO: mmap() the file to avoid the billion reads.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_syncfile(struct sess *sess, size_t argc, char **argv,
	struct flist **flp, size_t *sz)
{
	int		 fd, first = 1;
	ssize_t		 ssz;
	char		*path = NULL, *link = NULL;
	struct flist	*fl;
	struct stat	 st;
	size_t		 tmpsz, pathsz, linksz, i, stripdir = 0;
	const char	*cp;

	if ((fd = open(sess->opts->syncfile, O_RDONLY, 0)) == -1) {
		ERR("%s (1)", sess->opts->syncfile);
		return 0;
	}

	/* Read until end of file. */

	while ((ssz = read(fd, &pathsz, sizeof(size_t))) != 0) {
		free(path);
		free(link);
		path = link = NULL;
		if (ssz < 0) {
			ERR("%s", sess->opts->syncfile);
			goto out;
		} else if ((size_t)ssz != sizeof(size_t)) {
			ERRX("%s: short read", sess->opts->syncfile);
			goto out;
		} else if ((path = calloc(pathsz + 1, 1)) == NULL) {
			ERR("calloc");
			goto out;
		} else if ((ssz = read(fd, path, pathsz)) < 0) {
			ERR("%s", sess->opts->syncfile);
			goto out;
		} else if ((size_t)ssz != pathsz) {
			ERRX("%s: short read", sess->opts->syncfile);
			goto out;
		} else if ((ssz = read(fd, &st, sizeof(struct stat))) < 0) {
			ERR("%s", sess->opts->syncfile);
			goto out;
		} else if ((size_t)ssz != sizeof(struct stat)) {
			ERR("%s", sess->opts->syncfile);
			goto out;
		}

		if (S_ISLNK(st.st_mode)) {
			if ((ssz = read(fd, &linksz, sizeof(size_t))) < 0) {
				ERR("%s", sess->opts->syncfile);
				goto out;
			} else if ((size_t)ssz != sizeof(size_t)) {
				ERRX("%s: short read", sess->opts->syncfile);
				goto out;
			} else if ((link = calloc(linksz + 1, 1)) == NULL) {
				ERR("calloc");
				goto out;
			} else if ((ssz = read(fd, link, linksz)) < 0) {
				ERR("%s", sess->opts->syncfile);
				goto out;
			} else if ((size_t)ssz != linksz) {
				ERRX("%s: short read", sess->opts->syncfile);
				goto out;
			}
		}

		/*
		 * We want to make sure that the requested file is part
		 * of the set in our syncfile.
		 * If the request is recursive, we check that the
		 * syncfile has at least the requested root.
		 * If it's non-recursive, it must exist exactly.
		 */

		if (!sess->opts->recursive) {
			for (i = 0; i < argc; i++)
				if (strcmp(argv[i], path) == 0)
					break;
			if (i == argc)
				continue;

			if (S_ISDIR(st.st_mode)) {
				WARNX("%s: skipping directory", path);
				continue;
			} else if (S_ISLNK(st.st_mode)) {
				if (!sess->opts->preserve_links) {
					WARNX("%s: skipping symlink (4)", path);
					continue;
				}
			} else if (!S_ISREG(st.st_mode)) {
				WARNX("%s: skipping special", path);
				continue;
			}
		} else {
			for (i = 0; i < argc; i++) {
				tmpsz = strlen(argv[i]);
				if (pathsz < tmpsz)
					continue;
				if (strncmp(argv[i], path, tmpsz))
					continue;
				if (path[tmpsz] == '\0' || path[tmpsz] == '/')
					break;
			}
			if (i == argc)
				continue;
		}

		/* 
		 * We need to find the common root that we're going to
		 * build in the receiver, so use the first entry as a
		 * referent.
		 * If it ends with a slash, we're going to omit the
		 * directory altogether, so the stripdir will be the
		 * full length of the file.
		 * Otherwise, we take the final path component.
		 */

		if (first) {
			if ((stripdir = strlen(path)) == 0) {
				ERRX("%s: empty root", sess->opts->syncfile);
				goto out;
			} else if (path[stripdir - 1] != '/') {
				if ((cp = strrchr(path, '/')) != NULL)
					stripdir = cp - path + 1;
			}
			first = 0;
		}

		/* Create the entry. */

		*flp = reallocarray(*flp, *sz + 1, sizeof(struct flist));
		if (*flp == NULL) {
			ERR("reallocarray flist_gen_syncfile()");
			goto out;
		}
		fl = &(*flp)[*sz];
		(*sz)++;
		memset(fl, 0, sizeof(struct flist));
		fl->path = path;
		fl->link = link;
		fl->wpath = fl->path + stripdir;
		flist_copy_stat(fl, &st);
		path = link = NULL;
	}
out:
	LOG2("syncfile generated %zu filenames", *sz);
	free(path);
	free(link);
	close(fd);
	return 1;
}
#endif

/*
 * Generate a sorted, de-duplicated list of file metadata.
 * In non-recursive mode (the default), we use only the files we're
 * given.
 * Otherwise, directories are recursively examined.
 * Returns zero on failure, non-zero on success.
 * On success, "fl" will need to be freed with flist_free().
 */
int
flist_gen(struct sess *sess, size_t argc, char **argv, struct fl *fl)
{
	int	 rc;

#if 0
	if (sess->opts->syncfile == NULL) {
#endif
	assert(argc > 0);
	rc = sess->opts->recursive ?
		flist_gen_dirs(sess, argc, argv, fl) :
		flist_gen_files(sess, argc, argv, fl);
#if 0
	} else
		rc = flist_gen_syncfile(sess, argc, argv, fl);
#endif

	/* After scanning, lock our file-system view. */

	/*
	 * If our flist_gen_*() call failed and we didn't have any transfer
	 * errors, then consider the situation fatal and bail out.  Otherwise,
	 * we'll still proceed with what we have.
	 */
	if (!rc && sess->total_errors == 0)
		return 0;

	if (!platform_flist_modify(sess, fl))
		return 0;

	if (protocol_newsort) {
		qsort(fl->flp, fl->sz, sizeof(struct flist), flist_cmp29);
	} else {
		qsort(fl->flp, fl->sz, sizeof(struct flist), flist_cmp);
	}

	if (flist_dedupe(sess->opts, &(fl->flp), &fl->sz)) {
		flist_topdirs(sess, fl->flp, fl->sz);
		return 1;
	}

	ERRX1("flist_dedupe");
	flist_free(fl->flp, fl->sz);
	fl->flp = NULL;
	return 0;
}

/*
 * Generate a list of files in root to delete that are within the
 * top-level directories stipulated by "wfl".
 * Only handles symbolic links, directories, and regular files.
 * Returns zero on failure (fl and flsz will be NULL and zero), non-zero
 * on success.
 * On success, "fl" will need to be freed with flist_free().
 */
int
flist_gen_dels(struct sess *sess, const char *root, struct flist **fl,
    size_t *sz,	const struct flist *wfl, size_t wflsz)
{
	char		**cargv = NULL;
	int		  rc = 0, skip_post = 0, c, flag;
	FTS		 *fts = NULL;
	FTSENT		 *ent, *perish_ent = NULL;
	struct flist	 *f;
	struct stat	  st;
	size_t		  cargvs = 0, i, j, max = 0, stripdir;
	ENTRY		  hent;
	ENTRY		 *hentp;

	*fl = NULL;
	*sz = 0;

	/* Only run this code when we're recursive. */

	if (!sess->opts->recursive)
		return 1;

	/*
	 * Gather up all top-level directories for scanning.
	 * This is stipulated by rsync's --delete behaviour, where we
	 * only delete things in the top-level directories given on the
	 * command line.
	 */

	for (i = 0; i < wflsz; i++)
		if (FLSTAT_TOP_DIR & wfl[i].st.flags)
			cargvs++;
	if (cargvs == 0)
		return 1;

	if ((cargv = calloc(cargvs + 1, sizeof(char *))) == NULL) {
		ERR("calloc");
		return 0;
	}

	for (i = j = 0; i < wflsz && j < cargvs; i++) {
		if (!(FLSTAT_TOP_DIR & wfl[i].st.flags))
			continue;
		assert(S_ISDIR(wfl[i].st.mode));
		c = asprintf(&cargv[j], "%s/%s", root, wfl[i].wpath);
		if (c == -1) {
			ERR("asprintf");
			cargv[j] = NULL;
			goto out;
		}

		/*
		 * We generally shouldn't have that many top-dirs in a transfer,
		 * so this shouldn't be a major drag on performance and will
		 * save us from some extra redundant directory walks later on.
		 */
		for (size_t dj = 0; dj < j; dj++) {
			if (strcmp(cargv[dj], cargv[j]) == 0) {
				free(cargv[j]);
				cargv[j] = NULL;
				break;
			}
		}

		if (cargv[j] == NULL) {
			cargvs--;
			continue;
		}

		LOG4("%s: will scan for deletions", cargv[j]);
		j++;
	}

	cargv[j] = NULL;

	LOG2("delete from %zu directories", cargvs);

	/*
	 * Next, use the standard hcreate(3) hashtable interface to hash
	 * all of the files that we want to synchronise.
	 * This way, we'll be able to determine which files we want to
	 * delete in O(n) time instead of O(n * search) time.
	 * Plus, we can do the scan in-band and only allocate the files
	 * we want to delete.
	 */

	if (!hcreate(wflsz)) {
		ERR("hcreate");
		goto out;
	}

	for (i = 0; i < wflsz; i++) {
		const char *kpath;

		memset(&hent, 0, sizeof(ENTRY));
		kpath = wfl[i].wpath;
		while (strncmp(kpath, "./", 2) == 0)
			kpath += 2;
		if ((hent.key = strdup(kpath)) == NULL) {
			ERR("strdup");
			goto out;
		}
		if ((hentp = hsearch(hent, ENTER)) == NULL) {
			ERR("hsearch");
			goto out;
		} else if (hentp->key != hent.key) {
			/*
			 * Duplicate entry; this may happen if we had a single
			 * src spec listed multiple times, so just drop it.
			 */
			free(hent.key);
		}
	}

	/*
	 * Now we're going to try to descend into all of the top-level
	 * directories stipulated by the file list.
	 * If the directories don't exist, it's ok.
	 */

	if ((fts = fts_open(cargv, FTS_PHYSICAL, NULL)) == NULL) {
		ERR("fts_open");
		goto out;
	}

	stripdir = strlen(root) + 1;
	errno = 0;
	while ((ent = fts_read(fts)) != NULL) {
		const char *rpath;

		if (ent->fts_info == FTS_NS)
			continue;

		/*
		 * skip_post indicates that we just skipped recursing into this
		 * dir, so we should also not consider it for deletion (which is
		 * all we do in post-order).
		 */
		if (skip_post && ent->fts_info == FTS_DP) {
			skip_post = 0;
			continue;
		}

		/*
		 * Here we want directories in post-order because that's where
		 * we'll ultimately schedule a directory for deletion.
		 */
		if (ent->fts_info != FTS_DP &&
		    !flist_fts_check(sess, ent, FARGS_RECEIVER)) {
			if (ent->fts_errno != 0)
				sess->total_errors++;
			ent->fts_parent->fts_number++;
			errno = 0;
			continue;
		} else if (stripdir >= ent->fts_pathlen)
			continue;

		assert(ent->fts_statp != NULL);

		/*
		 * If rsync is told to avoid crossing a filesystem
		 * boundary when recursing, then exclude all entries
		 * from the list with a device inode, which does not
		 * match that of one of the top-level directories.
		 */

		if (sess->opts->one_file_system) {
			flag = 0;
			for (i = 0; i < wflsz; i++) {
				if (stat(wfl[i].path, &st) == -1) {
					ERR("%s: stat", wfl[i].path);
					goto out;
				}
				if (ent->fts_statp->st_dev == st.st_dev) {
					flag = 1;
					break;
				}
			}
			if (!flag)
				continue;
		}

		/* This is for macOS fts, which returns "foo//bar" */
		if (ent->fts_path[stripdir] == '/') {
			stripdir++;
		}

		/*
		 * Normalize the path by stripping any leading "./" components
		 * so that we don't have any false-negatives leading to a bogus
		 * deletion.
		 */
		rpath = ent->fts_path + stripdir;
		while (strncmp(rpath, "./", 2) == 0)
			rpath += 2;

		/* filter files on delete */
		if (!sess->opts->del_excl && ent->fts_info != FTS_DP &&
		    rules_match(rpath, (ent->fts_info == FTS_D),
		    FARGS_RECEIVER, perish_ent != NULL) == -1) {
			WARNX("skip excluded file %s", rpath);
			if (ent->fts_info == FTS_D)
				skip_post = 1;
			ent->fts_parent->fts_number++;
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/*
		 * We only check directories in pre-order when we have not
		 * descended down a tree that we already know is perishing.
		 */
		if (ent->fts_info == FTS_D && perish_ent != NULL)
			continue;

		/* Look up in hashtable. */
		memset(&hent, 0, sizeof(ENTRY));
		hent.key = (char *)rpath;
		if (hsearch(hent, FIND) != NULL)
			continue;

		/*
		 * Pre-order isn't used for deleting directories because we may
		 * have some files inside that are excluded from deletion, but
		 * we still want to do the above search in case we need to set
		 * the perish bit.
		 */
		if (ent->fts_info == FTS_D) {
			perish_ent = ent;
			continue;
		} else if (ent == perish_ent) {
			assert(ent->fts_info == FTS_DP);
			perish_ent = NULL;
		}

		if (ent->fts_info == FTS_DP && ent->fts_number > 0) {
			/*
			 * Just warn that we have some files inside that are not
			 * scheduled to be deleted, and propagate the exception
			 * in case we would have deleted the parent directory.
			 */
			WARNX("%s: not empty, cannot delete", ent->fts_path);
			ent->fts_parent->fts_number++;
			continue;
		}

		/* Not found: we'll delete it. */

		if (!flist_realloc(fl, sz, &max)) {
			ERRX1("flist_realloc");
			goto out;
		}
		f = &(*fl)[*sz - 1];

		if ((f->path = strdup(ent->fts_path)) == NULL) {
			ERR("strdup");
			goto out;
		}
		f->wpath = f->path + stripdir;
		flist_copy_stat(f, ent->fts_statp);
		errno = 0;
	}

	if (errno) {
		ERR("fts_read");
		goto out;
	}

	if (protocol_newsort) {
		qsort(*fl, *sz, sizeof(struct flist), flist_cmp29);
	} else {
		qsort(*fl, *sz, sizeof(struct flist), flist_dir_cmp);
	}
	rc = 1;
out:
	if (fts != NULL)
		fts_close(fts);
	for (i = 0; i < cargvs; i++)
		free(cargv[i]);
	free(cargv);
	hdestroy();
	return rc;
}

/*
 * Add a file to be deleted after transfers are complete.
 */
int
flist_add_del(struct sess *sess, const char *path, size_t stripdir,
    struct flist **fl, size_t *sz, size_t *flmax, const struct stat *st)
{
	struct flist *f;

	if (!flist_realloc(fl, sz, flmax)) {
		ERRX1("flist_realloc");
		return (0);
	}

	f = &(*fl)[*sz - 1];
	if ((f->path = strdup(path)) == NULL) {
		ERR("strdup");
		return (0);
	}

	f->wpath = f->path + stripdir;
	flist_copy_stat(f, st);

	return (1);
}

/*
 * Delete all files and directories in "fl".
 * If called with a zero-length "fl", does nothing.
 * If dry_run is specified, simply write what would be done.
 * Return zero on failure, non-zero on success.
 */
int
flist_del(struct sess *sess, int root, const struct flist *fl, size_t flsz)
{
	size_t	 del_limit = flsz;
	long	 begin, end, inc;
	long	 i;
	int	 flag;
	char buf[PATH_MAX];

	if (flsz == 0)
		return 1;

	assert(sess->opts->del || sess->opts->force_delete);
	assert(sess->opts->recursive || sess->opts->force_delete);

	if (sess->total_errors > 0 && !sess->opts->ignore_errors)
		return 1;

	/*
	 * (max_delete == 0)    Attempt to delete all files in flist.
	 * (max_delete > 0)     Attempt to delete at most max_delete files.
	 * (max_delete < 0)     Delete no files.
	 */
	if (sess->opts->max_delete < 0)
		return 1;
	if (sess->opts->max_delete > 0) {
		if (sess->total_deleted >= sess->opts->max_delete ||
		    sess->err_del_limit)
			return 1;

		/*
		 * If the number of files deleted so far plus the number to
		 * be deleted in this pass exceeds max_delete then limit the
		 * number of files to be deleted to the difference of the two.
		 */
		if (sess->total_deleted + flsz > (size_t)sess->opts->max_delete) {
			del_limit = sess->opts->max_delete - sess->total_deleted;
			sess->err_del_limit = true;
		}
	}

	/* Process flist in reverse order starting with protocol 29
	 * (due to change in sorting algorithm).
	 */
	if (sess->protocol < 29) {
		begin = 0;
		end = del_limit;
		inc = 1;
	} else {
		begin = del_limit - 1;
		end = -1;
		inc = -1;
	}

	for (i = begin; i != end; i += inc) {
		LOG1("%s: deleting", fl[i].wpath);
		if (sess->opts->dry_run)
			continue;
		assert(root != -1);
		sess->total_deleted++;
		flag = S_ISDIR(fl[i].st.mode) ? AT_REMOVEDIR : 0;
		if (sess->opts->backup) {
			if (sess->opts->backup_dir != NULL) {
				LOG3("%s: doing backup-dir to %s", fl[i].wpath,
				    sess->opts->backup_dir);
				if (snprintf(buf, sizeof(buf), "%s/%s%s",
				    sess->opts->backup_dir, fl[i].wpath,
				    sess->opts->backup_suffix) >=
				    (int)sizeof(buf)) {
					ERR("%s: backup-dir: compound backup "
					    "path too long: %s/%s%s > %d",
					    fl[i].wpath,
					    sess->opts->backup_dir,
					    fl[i].wpath,
					    sess->opts->backup_suffix,
					    (int)sizeof(buf));
					sess->total_errors++;
					continue;
				}
				if (backup_to_dir(sess, root, &fl[i], buf,
				    fl[i].st.mode) == -1) {
					ERR("%s: backup_to_dir: %s",
					    fl[i].wpath, buf);
					sess->total_errors++;
					continue;
				}
			} else if (!S_ISDIR(fl[i].st.mode)) {
				LOG3("%s: doing backup", fl[i].wpath);
				if (snprintf(buf, sizeof(buf), "%s%s",
				    fl[i].wpath, sess->opts->backup_suffix) >=
				    (int)sizeof(buf)) {
					ERR("%s: backup: compound backup path "
					    "too long: %s%s > %d", fl[i].wpath,
					    fl[i].wpath,
					    sess->opts->backup_suffix,
					    (int)sizeof(buf));
					sess->total_errors++;
					continue;
				}
				if (move_file(root, fl[i].wpath,
				    root, buf, 1) == -1) {
					ERR("%s: move_file: %s", fl[i].wpath,
					    buf);
					sess->total_errors++;
					continue;
				}
			}
		}
		if (unlinkat(root, fl[i].wpath, flag) == -1 &&
		    errno != ENOENT) {
			ERR("%s: unlinkat", fl[i].wpath);
			sess->total_errors++;
			continue;
		}
	}

	if (del_limit < flsz) {
		LOG0("Deletions stopped due to --max-delete limit (%zu skipped)",
		    flsz - del_limit);
	}

	return 1;
}

void
cleanup_filesfrom(struct sess *sess)
{
	unsigned int i;

	if (sess->opts->filesfrom != NULL) {
		for (i = 0; i < sess->filesfrom_n; i++) {
			free(sess->filesfrom[i]);
		}
	}
	free(sess->filesfrom);
	sess->filesfrom_n = 0;
	sess->filesfrom = NULL;
}

static int
append_filesfrom(struct sess *sess, const char *basedir, char *file)
{
	size_t file_length = strlen(file);

	if (file[file_length-1] == '\n') {
		file[file_length-1] = '\0';
		file_length--;
	}

	/* Skip comment lines */
	if (file[0] == '#' || file[0] == ';')
		return 1;
	/* Reject paths leading with .. */
	if (!strncmp(file, "..", 2)) {
		ERRX("Can't have files-from with '..'");
		return 0;
	}
	/* Reject paths with .. in the middle */
	if (strstr(file, "/..") != NULL) {
		ERRX("Can't have files-from with '..'");
		return 0;
	}

	if ((sess->filesfrom = realloc(sess->filesfrom,
	    sizeof(char *) * (sess->filesfrom_n + 1))) == NULL) {
		ERR("realloc");
		return 0;
	}

	asprintf(&(sess->filesfrom[sess->filesfrom_n]),"%s", file);
	if (sess->filesfrom[sess->filesfrom_n] == NULL) {
		ERR("asprintf");
		cleanup_filesfrom(sess);
		return 0;
	}
	LOG4("Added '%s' to flist", sess->filesfrom[sess->filesfrom_n]);
	sess->filesfrom_n++;

	return 1;
}

static int
fdgets(struct sess *sess, int fd, char *buf, int bufsz)
{
	int length = 0;
	size_t n = 1;

	while (n == 1 && length < bufsz) {
		n = read(fd, buf + length, 1);
		if (n == -1) {
			if (errno == EWOULDBLOCK || errno == EINTR ||
				errno == EAGAIN) {
				n = 1;
				continue;
			} else {
				ERR("read(2) of files-from file failed");
				return -1;
			}
		} else
			length++;
		if (*(buf + length - 1) == '\0')
			break;
		if (!sess->opts->from0 && *(buf + length - 1) == '\n') {
			*(buf + length - 1) = '\0';
			break;
		}
	}
	return length;
}

static int
strp_cmp(const void *p1, const void *p2)
{
	return strcmp(*(const char **)p1, *(const char **)p2);
}

void
print_filesfrom(char *const *filesfrom, int filesfrom_n, const char *marker)
{
	int i;

	for (i = 0; i < filesfrom_n; i++) {
		LOG0("filesfrom %s %d: '%s'", marker, i, filesfrom[i]);
	}
}

int
read_filesfrom(struct sess *sess, const char *basedir)
{
	FILE *f;
	char buf[PATH_MAX] = { 0 };
	int retval;

	if (strcmp(sess->opts->filesfrom, "-") == 0) {
		f = stdin;
	} else if (sess->opts->filesfrom_host) {
		f = NULL;
	} else {
		if ((f = fopen(sess->opts->filesfrom_path, "r")) == NULL) {
			ERR("fopen ro: '%s'", sess->opts->filesfrom);
			return 0;
		}
	}

	sess->filesfrom_n = 0;
	sess->filesfrom = NULL;

	retval = 0;
	while (1) {
		if (sess->opts->filesfrom_host) {
			/*
			 * This is doing single byte read system calls.
			 * Before you change that consider:
			 * If you over-read even by a single bytes you
			 * are out-of-protocol with no way to recover.
			 * There is no guarantee that there is a system
			 * call boundary after the terminating empty
			 * string.
			 * The other side might be malicious, e.g. when
			 * using a public rsync server.
			 */
			if (fdgets(sess, sess->filesfrom_fd, buf, sizeof(buf)) == -1)
				goto out;
			if (append_filesfrom(sess, basedir, buf) == 0)
				goto out;
		} else {
			if (fdgets(sess, fileno(f), buf, PATH_MAX) == -1) {
				ERR("fdgets: '%s'", sess->opts->filesfrom);
				goto out;
			}
			if (append_filesfrom(sess, basedir, buf) == 0)
				goto out;
		}
		if (buf[0] == '\0')
			break;
	}

	qsort(sess->filesfrom, sess->filesfrom_n, sizeof(char *), strp_cmp);

	retval = 1;
out:
	if (f != NULL && f != stdin && fclose(f) == EOF) {
		ERR("fclose: '%s'", sess->opts->filesfrom);
		retval = 0;
	}
	if (!retval)
		cleanup_filesfrom(sess);
	return retval;
}
