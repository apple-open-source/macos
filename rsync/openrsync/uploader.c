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

#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "extern.h"

/*
 * We're using O_RESOLVE_BENEATH in a couple of places just for some additional
 * safety on platforms that support it, so it's not a hard requirement.
 */
#ifndef O_RESOLVE_BENEATH
#define	O_RESOLVE_BENEATH	0
#endif

enum	uploadst {
	UPLOAD_FIND_NEXT = 0, /* find next to upload to sender */
	UPLOAD_WRITE, /* wait to write to sender */
	UPLOAD_FINISHED /* nothing more to do in phase */
};

/*
 * Used to keep track of data flowing from the receiver to the sender.
 * This is managed by the receiver process.
 */
struct	upload {
	enum uploadst	    state;
	char		   *buf; /* if not NULL, pending upload */
	size_t		    bufsz; /* size of buf */
	size_t		    bufmax; /* maximum size of buf */
	size_t		    bufpos; /* position in buf */
	size_t		    idx; /* current transfer index */
	mode_t		    oumask; /* umask for creating files */
	char		   *root; /* destination directory path */
	int		    rootfd; /* destination directory */
	int		    tempfd; /* temp directory */
	size_t		    csumlen; /* checksum length */
	int		    fdout; /* write descriptor to sender */
	struct flist	   *fl; /* file list */
	size_t		    flsz; /* size of file list */
	size_t		    nextack; /* next idx to acknowledge */
	struct flist	   *dfl; /* delayed delete file list */
	size_t		    dflsz; /* size of delayed delete list */
	size_t		    dflmax; /* allocated size of delayed delete list */
	int		   *newdir; /* non-zero if mkdir'd */
	int		    phase; /* current uploader phase (transfer, redo) */
};

static int pre_dir_delete(struct upload *p, struct sess *sess, enum delmode delmode);

static inline bool
force_delete_applicable(struct upload *p, struct sess *sess, mode_t mode)
{
	if (S_ISDIR(mode) && sess->opts->force_delete) {
		return sess->opts->del == DMODE_NONE ||
		    sess->opts->del == DMODE_BEFORE ||
		    sess->opts->del == DMODE_AFTER ||
		    sess->opts->del == DMODE_DELAY;
	}

	return sess->opts->del == DMODE_BEFORE;
}

/*
 * Log a directory by emitting the file and a trailing slash, just to
 * show the operator that we're a directory.
 */
static void
log_dir(struct sess *sess, const struct flist *f)
{
	size_t	 sz;

	sz = strlen(f->path);
	assert(sz > 0);
	LOG1("%s%s", f->path, (f->path[sz - 1] == '/') ? "" : "/");
}

/*
 * Log a link by emitting the file and the target, just to show the
 * operator that we're a link.
 */
static void
log_symlink(struct sess *sess, const struct flist *f)
{

	LOG1("%s -> %s", f->path, f->link);
}

/*
 * Simply log the socket, fifo, or device name.
 */
static void
log_other(struct sess *sess, const struct flist *f)
{

	LOG1("%s", f->path);
}

/*
 * Simply log the filename.
 */
static void
log_file(struct sess *sess, const struct flist *f)
{

	if (!sess->opts->server)
		LOG1("%s", f->path);
}

/*
 * Prepare the overall block set's metadata.
 * We always have at least one block.
 * The block size is an important part of the algorithm.
 * I use the same heuristic as the reference rsync, but implemented in a
 * bit more of a straightforward way.
 * In general, the individual block length is the rounded square root of
 * the total file size.
 * The minimum block length is 700.
 */
static void
init_null_blkset(struct blkset *p, off_t sz)
{

	p->size = sz;
	p->blksz = 0;
	p->len = 0;
	p->csum = 0;
	p->rem = 0;

}

static void
init_blkset(struct blkset *p, off_t sz, long block_size)
{
	double	 v;

	if (block_size > 0)
		p->len = block_size;
	else if (sz >= (BLOCK_SIZE_MIN * BLOCK_SIZE_MIN)) {
		/* Simple rounded-up integer square root. */

		v = sqrt(sz);
		p->len = ceil(v);

		/*
		 * Always be a multiple of eight.
		 * There's no reason to do this, but rsync does.
		 */

		if ((p->len % 8) > 0)
			p->len += 8 - (p->len % 8);
	} else
		p->len = BLOCK_SIZE_MIN;

	p->size = sz;
	if ((p->blksz = sz / p->len) == 0)
		p->rem = sz;
	else
		p->rem = sz % p->len;

	/* If we have a remainder, then we need an extra block. */

	if (p->rem)
		p->blksz++;
}

/*
 * For each block, prepare the block's metadata.
 * We use the mapped "map" file to set our checksums.
 */
static void
init_blk(struct blk *p, const struct blkset *set, off_t offs,
	size_t idx, const void *map, const struct sess *sess)
{

	p->idx = idx;
	/* Block length inherits for all but the last. */
	p->len = (idx == set->blksz - 1 && set->rem) ? set->rem : set->len;
	p->offs = offs;

	p->chksum_short = hash_fast(map, p->len);
	hash_slow(map, p->len, p->chksum_long, sess);
}

/*
 * Handle a symbolic link.
 * If we encounter directories existing in the symbolic link's place,
 * then try to unlink the directory.
 * Otherwise, simply overwrite with the symbolic link by renaming.
 * Return <0 on failure 0 on success.
 */
static int
pre_symlink(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newlink = 0, updatelink = 0;
	char			*b, *temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISLNK(f->st.mode));

	if (!sess->opts->preserve_links) {
		WARNX("%s: ignoring symlink", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_symlink(sess, f);
		return 0;
	}

	/*
	 * See if the symlink already exists.
	 * If it's a directory, then try to unlink the directory prior
	 * to overwriting with a symbolic link.
	 * If it's a non-directory, we just overwrite it.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}

	if (rc != -1 && (sess->opts->inplace || !S_ISLNK(st.st_mode))) {
		if (force_delete_applicable(p, sess, st.st_mode))
			if (pre_dir_delete(p, sess, DMODE_DURING) == 0)
				return -1;
		if ((sess->opts->inplace || S_ISDIR(st.st_mode)) &&
		    unlinkat(p->rootfd, f->path,
		    S_ISDIR(st.st_mode) ? AT_REMOVEDIR : 0) == -1) {
			ERR("%s: unlinkat", f->path);
			sess->total_errors++;
			return 0;
		}
		rc = -1;
	}

	/*
	 * If the symbolic link already exists, then make sure that it
	 * points to the correct place.
	 */

	if (rc != -1) {
		b = symlinkat_read(p->rootfd, f->path);
		if (b == NULL) {
			ERRX1("symlinkat_read");
			return -1;
		}
		if (strcmp(f->link, b)) {
			free(b);
			b = NULL;
			LOG3("%s: updating symlink: %s", f->path, f->link);
			updatelink = 1;
		}
		free(b);
		b = NULL;
	}

	/*
	 * Create the temporary file as a symbolic link, then rename the
	 * temporary file as the real one, overwriting anything there.
	 */

	if (rc == -1 || updatelink) {
		/* XXX does this need an is_unsafe_link() check? */
		if (sess->opts->inplace) {
			LOG3("%s: creating symlink in-place: %s", f->path, f->link);
			if (symlinkat(f->link, p->rootfd, f->path) == -1) {
				ERR("symlinkat");
				return -1;
			}
		} else {
			LOG3("%s: creating symlink: %s", f->path, f->link);
			if (mktemplate(&temp, f->path, sess->opts->recursive,
			    IS_TMPDIR) == -1) {
				ERRX1("mktemplate");
				return -1;
			}
			if (mkstemplinkat(f->link, TMPDIR_FD, temp) == NULL) {
				ERR("mkstemplinkat");
				free(temp);
				return -1;
			}
		}

		newlink = 1;
	}

	rsync_set_metadata_at(sess, newlink,
	    newlink && sess->opts->temp_dir ? p->tempfd : p->rootfd, f,
	    newlink && temp != NULL ? temp : f->path);

	if (newlink && temp != NULL) {
		if (move_file(TMPDIR_FD, temp, p->rootfd, f->path, 1) == -1) {
			ERR("%s: move_file %s", temp, f->path);
			(void)unlinkat(TMPDIR_FD, temp, 0);
			sess->total_errors++;
			free(temp);
			return 0;
		}
		free(temp);
	}

	if (newlink)
		log_symlink(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for devices.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_dev(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newdev = 0, updatedev = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISBLK(f->st.mode) || S_ISCHR(f->st.mode));

	if (!sess->opts->devices || sess->opts->supermode == SMODE_OFF ||
	    (sess->opts->supermode != SMODE_ON && geteuid() != 0)) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_other(sess, f);
		return 0;
	}

	/*
	 * See if the dev already exists.
	 * If a non-device exists in its place, we'll replace that.
	 * If it replaces a directory, remove the directory first.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}


	if (rc != -1 && !(S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))) {
		if (force_delete_applicable(p, sess, st.st_mode))
			if (pre_dir_delete(p, sess, DMODE_DURING) == 0)
				return -1;
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			sess->total_errors++;
			return 0;
		}
		rc = -1;
	}

	/* Make sure existing device is of the correct type. */

	if (rc != -1) {
		if ((f->st.mode & (S_IFCHR|S_IFBLK)) !=
		    (st.st_mode & (S_IFCHR|S_IFBLK)) ||
		    f->st.rdev != st.st_rdev) {
			LOG3("%s: updating device", f->path);
			updatedev = 1;
		}
	}

	if (rc == -1 || updatedev) {
		if (sess->opts->inplace) {
			if (mknodat(p->rootfd, f->path, f->st.mode & (S_IFCHR|S_IFBLK),
			    f->st.rdev) == -1) {
				ERR("mknodat");
				return -1;
			}
		} else {
			if (mktemplate(&temp, f->path, sess->opts->recursive,
			    IS_TMPDIR) == -1) {
				ERRX1("mktemplate");
				return -1;
			}
			if (mkstempnodat(TMPDIR_FD, temp,
			    f->st.mode & (S_IFCHR|S_IFBLK), f->st.rdev)
			    == NULL) {
				ERR("mkstempnodat");
				free(temp);
				return -1;
			}
		}

		newdev = 1;
	}

	rsync_set_metadata_at(sess, newdev, TMPDIR_FD, f,
	    newdev && temp != NULL ? temp : f->path);

	if (newdev && temp != NULL) {
		if (move_file(TMPDIR_FD, temp, p->rootfd, f->path, 1) == -1) {
			ERR("%s: move_file %s", temp, f->path);
			(void)unlinkat(TMPDIR_FD, temp, 0);
			sess->total_errors++;
			free(temp);
			return 0;
		}
		free(temp);
	}

	log_other(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for FIFOs.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_fifo(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newfifo = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISFIFO(f->st.mode));

	if (!sess->opts->specials) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_other(sess, f);
		return 0;
	}

	/*
	 * See if the fifo already exists.
	 * If it exists as a non-FIFO, unlink it (if a directory) then
	 * mark it from replacement.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}

	if (rc != -1 && !S_ISFIFO(st.st_mode)) {
		if (force_delete_applicable(p, sess, st.st_mode))
			if (pre_dir_delete(p, sess, DMODE_DURING) == 0)
				return -1;
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			sess->total_errors++;
			return 0;
		}
		rc = -1;
	}

	if (rc == -1) {
		if (sess->opts->inplace) {
			if (mkfifoat(p->rootfd, f->path, S_IRUSR|S_IWUSR) == -1) {
				ERR("mkfifoat");
				return -1;
			}
		} else {
			if (mktemplate(&temp, f->path, sess->opts->recursive,
			    IS_TMPDIR) == -1) {
				ERRX1("mktemplate");
				return -1;
			}
			if (mkstempfifoat(TMPDIR_FD, temp) == NULL) {
				ERR("mkstempfifoat");
				free(temp);
				return -1;
			}
		}

		newfifo = 1;
	}

	rsync_set_metadata_at(sess, newfifo, TMPDIR_FD, f,
	    newfifo && temp != NULL ? temp : f->path);

	if (newfifo && temp != NULL) {
		if (move_file(TMPDIR_FD, temp, p->rootfd, f->path, 1) == -1) {
			ERR("%s: move_file %s", temp, f->path);
			(void)unlinkat(TMPDIR_FD, temp, 0);
			sess->total_errors++;
			free(temp);
			return 0;
		}
		free(temp);
	}

	log_other(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for socket files.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_sock(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newsock = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISSOCK(f->st.mode));

	if (!sess->opts->specials) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_other(sess, f);
		return 0;
	}

	/*
	 * See if the fifo already exists.
	 * If it exists as a non-FIFO, unlink it (if a directory) then
	 * mark it from replacement.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISSOCK(st.st_mode)) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			sess->total_errors++;
			return 0;
		}
		rc = -1;
	}

	if (rc == -1) {
		if (sess->opts->inplace) {
			if (mksock(temp, p->root) == -1) {
				ERR("mksock");
				return -1;
			}
		} else {
			if (mktemplate(&temp, f->path, sess->opts->recursive,
			    IS_TMPDIR) == -1) {
				ERRX1("mktemplate");
				return -1;
			}
			if (mkstempsock(sess->opts->temp_dir ?
			    sess->opts->temp_dir : p->root, temp) == NULL) {
				ERR("mkstempsock");
				free(temp);
				return -1;
			}
		}

		newsock = 1;
	}

	rsync_set_metadata_at(sess, newsock, TMPDIR_FD, f,
		newsock && temp != NULL ? temp : f->path);

	if (newsock && temp != NULL) {
		if (move_file(TMPDIR_FD, temp, p->rootfd, f->path, 1) == -1) {
			ERR("%s: move_file %s", temp, f->path);
			(void)unlinkat(TMPDIR_FD, temp, 0);
			sess->total_errors++;
			free(temp);
			return 0;
		}
		free(temp);
	}

	log_other(sess, f);
	return 0;
}

/*
 * Called before we update a directory to see if we need to delete any files
 * inside in the process.
 * Return 0 on failure, 1 on success.
 */
static int
pre_dir_delete(struct upload *p, struct sess *sess, enum delmode delmode)
{
	const struct flist *cf, *f;
	char *dirpath, *parg[2];
	FTS *fts;
	FTSENT *ent, *perish_ent = NULL;
	size_t stripdir;
	ENTRY hent, *hentp;
	int isroot, ret;

	fts = NULL;
	ret = 0;
	f = &p->fl[p->idx];
	isroot = strcmp(f->path, ".") == 0;

	if (asprintf(&dirpath, "%s/%s", p->root,
	    isroot ? "" : f->path) == -1) {
		ERRX1("%s: asprintf", f->path);
		return (ret);
	}

	if (!hcreate(p->flsz)) {
		ERR("hcreate");
		goto out;
	}

	/*
	 * Generate a list of just the paths in this directory that should exist.
	 */
	stripdir = strlen(f->path) + 1;
	for (size_t i = p->idx; i < p->flsz; i++) {
		char *slp;
		size_t slpos;

		/*
		 * Stop scanning once we're backed out of the directory we're
		 * looking at.
		 */
		cf = &p->fl[i];

		if (strcmp(cf->wpath, ".") == 0)
			continue;

		if (!isroot && strncmp(f->path, cf->wpath, stripdir - 1) != 0)
			break;

		/* Omit subdirectories' contents */
		slp = strrchr(cf->wpath, '/');
		slpos = (slp != NULL ? slp - cf->wpath : 0);

		if (isroot) {
			if (slpos != 0)
				continue;
		} else if (slpos >= stripdir)
			continue;

		memset(&hent, 0, sizeof(hent));
		if ((hent.key = strdup(cf->wpath)) == NULL) {
			ERR("strdup");
			goto out;
		}

		if ((hentp = hsearch(hent, ENTER)) == NULL) {
			ERR("hsearch");
			goto out;
		} else if (hentp->key != hent.key) {
			ERRX("%s: duplicate", cf->wpath);
			free(hent.key);
			goto out;
		}
	}

	parg[0] = dirpath;
	parg[1] = NULL;

	rules_base(dirpath);

	if ((fts = fts_open(parg, FTS_PHYSICAL, NULL)) == NULL) {
		ERR("fts_open");
		goto out;
	}

	stripdir = strlen(p->root) + 1;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_NS) {
			continue;
		} else if (stripdir >= ent->fts_pathlen) {
			continue;
		}

		if (ent->fts_info != FTS_DP &&
		    !flist_fts_check(sess, ent, FARGS_RECEIVER)) {
			if (ent->fts_errno != 0) {
				if (ent->fts_info == FTS_DNR)
					LOG1("%.*s", (int)ent->fts_namelen, ent->fts_name);
				sess->total_errors++;
			}
			errno = 0;
			continue;
		}

		assert(ent->fts_statp != NULL);

		/* This is for macOS fts, which returns "foo//bar" */
		if (ent->fts_path[stripdir] == '/') {
			stripdir++;
		}
		if (!sess->opts->del_excl && ent->fts_info != FTS_DP &&
		    rules_match(ent->fts_path + stripdir,
		    (ent->fts_info == FTS_D), FARGS_RECEIVER,
		    perish_ent != NULL) == -1) {
			WARNX("skip excluded file %s",
			    ent->fts_path + stripdir);
			fts_set(fts, ent, FTS_SKIP);
			ent->fts_parent->fts_number++;
			continue;
		}

		if (ent->fts_info == FTS_D && perish_ent != NULL)
			continue;

		/*
		 * If we visit a directory in post-order and perish_ent isn't
		 * set, then we must have skipped it in pre-order (e.g., due to
		 * a rule match) and we must not schedule it for deletion now.
		 */
		if (ent->fts_info == FTS_DP && perish_ent == NULL)
			continue;

		/* Look up in the hashtable. */
		memset(&hent, 0, sizeof(hent));
		hent.key = ent->fts_path + stripdir;
		if (hsearch(hent, FIND) != NULL) {
			if (ent->fts_info == FTS_D &&
			    strcmp(ent->fts_path, parg[0]) != 0) {
				fts_set(fts, ent, FTS_SKIP);
			}
			continue;
		}

		if (ent->fts_info == FTS_D) {
			perish_ent = ent;
			continue;
		} else if (ent == perish_ent) {
			assert(ent->fts_info == FTS_DP);
			perish_ent = NULL;
		}

		if (ent->fts_info != FTS_D) {
			if (ent->fts_info == FTS_DP && ent->fts_number != 0) {
				WARNX("%s: not empty, cannot delete",
				    ent->fts_path);
				ent->fts_parent->fts_number++;
				continue;
			}

			if (delmode == DMODE_DURING) {
				LOG1("%s: deleting", ent->fts_path + stripdir);

				if (sess->opts->dry_run)
					continue;

				flist_add_del(sess, ent->fts_path, stripdir, &p->dfl, &p->dflsz,
				    &p->dflmax, ent->fts_statp);
			} else {
				assert(delmode == DMODE_DELAY);
				flist_add_del(sess, ent->fts_path, stripdir, &p->dfl, &p->dflsz,
				    &p->dflmax, ent->fts_statp);
			}
		}
	}

	ret = 1;
out:
	if (delmode == DMODE_DURING) {
		qsort(p->dfl, p->dflsz, sizeof(struct flist), flist_dir_cmp);

		/* flist_del will report the error, just propagate status. */
		if (!flist_del(sess, p->rootfd, p->dfl, p->dflsz))
			ret = 0;

		flist_free(p->dfl, p->dflsz);
		p->dfl = NULL;
		p->dflsz = p->dflmax = 0;
	}

	fts_close(fts);
	free(dirpath);
	hdestroy();
	return (ret);
}

int
upload_del(struct upload *p, struct sess *sess)
{

	qsort(p->dfl, p->dflsz, sizeof(struct flist), flist_dir_cmp);
	return (flist_del(sess, p->rootfd, p->dfl, p->dflsz));
}

/*
 * Check whether the conditions for keep_dirlink are present.
 * Returns 1 if so, 0 otherwise.
 */
static int
keep_dirlinks_applies(const struct stat *st, const struct flist *f, 
	int rootfd)
{
	struct stat st2;
	/*
	 * The thing that is a dir on the sender side must be a
	 * symlink to a dir.
	 */
	if (!S_ISLNK(st->st_mode))
		return 0;
	if (fstatat(rootfd, f->path, &st2, 0) == -1) {
		ERR("%s: fstatat", f->path);
		return 0;
	}
	if (!S_ISDIR(st2.st_mode)) {
		return 0;
	}
	return 1;
}

/*
 * If not found, create the destination directory in prefix order.
 * Create directories using the existing umask.
 * Return <0 on failure 0 on success.
 */
static int
pre_dir(struct upload *p, struct sess *sess)
{
	struct stat	 st;
	int		 rc;
	const struct flist *f;

	f = &p->fl[p->idx];
	assert(S_ISDIR(f->st.mode));

	if (!sess->opts->recursive && !sess->opts->relative &&
		!sess->opts->dirs) {
		WARNX("%s: ignoring directory 1 %d", f->path, sess->opts->relative);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_dir(sess, f);
		return 0;
	}

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISDIR(st.st_mode)) {
		if (sess->opts->keep_dirlinks &&
			keep_dirlinks_applies(&st, f, p->rootfd)) {
			return 0;
		}
		ERRX("%s: not a directory", f->path);
		return -1;
	} else if (rc != -1) {
		/*
		 * FIXME: we should fchmod the permissions here as well,
		 * as we may locally have shut down writing into the
		 * directory and that doesn't work.
		 */
		LOG3("%s: updating directory", f->path);

		if ((sess->opts->preserve_perms && st.st_mode != f->st.mode) ||
		    (sess->opts->preserve_times && !sess->opts->omit_dir_times &&
		     st.st_mtime != f->st.mtime)) {
			log_dir(sess, f);
		}

		if (sess->opts->del == DMODE_DURING || sess->opts->del == DMODE_DELAY) {
			pre_dir_delete(p, sess, sess->opts->del);
		}

		return 0;
	}

	/*
	 * We want to make the directory with default permissions (using
	 * our old umask, which we've since unset), then adjust
	 * permissions (assuming preserve_perms or new) afterward in
	 * case it's u-w or something.
	 */

	if (mkdirat(p->rootfd, f->path, 0777 & ~p->oumask) == -1) {
		ERR("%s: mkdirat", f->path);
		return -1;
	}

	p->newdir[p->idx] = 1;
	log_dir(sess, f);
	return 0;
}

/*
 * Process the directory time and mode for "idx" in the file list.
 * Returns zero on failure, non-zero on success.
 */
static int
post_dir(struct sess *sess, const struct upload *u, size_t idx)
{
	struct stat	 st;
	const struct flist *f;

	f = &u->fl[idx];
	assert(S_ISDIR(f->st.mode));

	/* We already warned about the directory in pre_process_dir(). */

	if (!sess->opts->recursive)
		return 1;
	if (sess->opts->dry_run)
		return 1;

	if (fstatat(u->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) == -1) {
		ERR("%s: fstatat", f->path);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		if (sess->opts->keep_dirlinks &&
			keep_dirlinks_applies(&st, f, u->rootfd)) {
			return 1;
		}
		WARNX("%s: not a directory", f->path);
		return 0;
	}

	return rsync_set_metadata_at(sess, u->newdir[idx], u->rootfd, f,
	    f->path);
}

/*
 * Check if file exists in the specified root directory.
 * Returns:
 *    -1 on error
 *     0 if file is considered the same
 *     1 if file exists and is possible match
 *     2 if file exists but quick check failed
 *     3 if file does not exist
 *     4 if file exists but should be ignored
 * The stat pointer st is only valid for 0, 1, 2, and 4 returns.
 */
static int
check_file(int rootfd, const struct flist *f, struct stat *st,
    struct sess *sess, const struct hardlinks *const hl,
    bool is_partialdir)
{
	const char *path = f->path;

	if (is_partialdir)
		path = download_partial_filepath(f);
	if (fstatat(rootfd, path, st, AT_SYMLINK_NOFOLLOW) == -1) {
		if (errno == ENOENT) {
			if (sess->opts->ign_non_exist) {
				LOG1("Skip non existing '%s'", f->path);
				return 0;
			} else
				return 3;
		}

		ERR("%s: fstatat", f->path);
		return -1;
	}

	if (!sess->opts->ign_exist && sess->opts->hard_links) {
		/*
		 * This covers the situation where a hardlink is sent,
		 * but non-hardlinked files with identical contents
		 * already exist.  They need to be replaced by a hardlink.
		 */
		if (find_hl(f, hl)) {
			if (st->st_nlink == 1)
				return 3;
		} else if (st->st_nlink > 1)
			return 3;
		/*
		 * This covers the situation where two separate identical,
		 * files are sent but they already exist as hardlink in
		 * the destination.  They need to be un-hardlinked.
		 *
		 * TODO: write tests that try to send a 3-way hardlink,
		 * overriding a 2-way hardlink and a plain file, all with
		 * identical contents.
		 */
		if (!find_hl(f, hl) && st->st_nlink > 1) {
			return 3;
		}
	}

	if (sess->opts->update && st->st_mtime > f->st.mtime) {
		LOG1("Skip newer '%s'", f->path);
		return 4;
	}

	if (sess->opts->ign_exist) {
		LOG1("Skip existing '%s'", f->path);
		return 4;
	}

	/* non-regular file needs attention */
	if (!S_ISREG(st->st_mode)) {
		return 2;
	}

	if (sess->role->append) {
		if (st->st_size >= f->st.size) {
			LOG1("Skip append '%s'", f->path);
			return 4;
		}
		return 2;
	}

	/* quick check if file is the same */
	if (st->st_size == f->st.size) {
		if (sess->opts->size_only)
			return 0;

		if (sess->opts->checksum) {
			unsigned char md[sizeof(f->md)];
			int rc;

			rc = hash_file_by_path(rootfd, f->path, f->st.size, md);

			if (rc == 0 && memcmp(md, f->md, sizeof(md)) == 0)
				return 0;

			return 2;
		}

		if (!sess->opts->ignore_times) {
			if (labs(f->st.mtime - st->st_mtime) <=
				sess->opts->modwin) {
				if (f->st.mtime != st->st_mtime)
					LOG3("%s: fits time modify window",
						f->path);
				return 0;
			}
			return 1;
		}
	}

	/* file needs attention */
	return 2;
}

/*
 * Check an alternate dir (e.g., copy dest mode dir) for a suitable version of
 * the file.
 *
 * Returns -1 on error, 0 on success, or 1 to keep trying further dirs.  The
 * *savedfd will be updated if we determine a match is a viable candidate for
 * *matchdir.
 */
static int
pre_file_check_altdir(struct sess *sess, const struct upload *p,
    const char **matchdir, struct stat *st, const char *root,
    const struct flist *f, const struct hardlinks *const hl, int rc,
    int basemode, int *savedfd, bool is_partialdir)
{
	int dfd, x;

	dfd = openat(p->rootfd, root, O_RDONLY | O_DIRECTORY);
	if (dfd == -1) {
		if (errno == ENOENT || is_partialdir || errno == EACCES)
			return 1;
		err(ERR_FILE_IO, "%s: pre_file_check_altdir: openat", root);
	}
	x = check_file(dfd, f, st, sess, hl, is_partialdir);
	/* found a match */
	if (x == 0) {
		if (rc >= 0) {
			/* found better match, delete file in rootfd */
			if (unlinkat(p->rootfd, f->path, 0) == -1 &&
			    errno != ENOENT) {
				ERR("%s: unlinkat", f->path);
				close(dfd);
				return -1;
			}
		}

		switch (basemode) {
		case BASE_MODE_COPY:
			LOG3("%s: copying: up to date in %s",
			    f->path, root);
			copy_file(p->rootfd, root, f);
			rsync_set_metadata_at(sess, 1, p->rootfd, f, f->path);
			break;
		case BASE_MODE_LINK:
			LOG3("%s: hardlinking: up to date in %s",
			    f->path, root);
			if (linkat(dfd, f->path, p->rootfd, f->path, 0) == -1) {
				/*
				 * GNU rsync falls back to copy here.
				 * I think it is more correct to
				 * fail since the user requested
				 * --link-dest and the manpage states
				 * that there will be hardlinking.
				 */
				ERR("hard link '%s/%s'", root, f->path);
			}
			break;
		case BASE_MODE_COMPARE:
		default:
			LOG3("%s: skipping: up to date in %s", f->path, root);
			break;
		}
		close(dfd);
		return 0;
	} else if ((x == 1 || x == 2) && *matchdir == NULL) {
		/* found a local file that is a close match */
		*matchdir = root;
		if (savedfd != NULL) {
			int prevfd;

			if ((prevfd = *savedfd) != -1)
				close(prevfd);
			*savedfd = dfd;
			/* Don't close() it. */
			dfd = -1;
		}
	}

	if (dfd != -1)
		close(dfd);
	return 1;
}

/*
 * Try to find a file in the same directory as the target that is
 * likely to be the same file (same size, mtime), and open that
 * as a basis file, to reduce the amount of data to be transferred.
 * Returns -1 on error, 0 on success, or 1 to keep trying further.
 */
static int
pre_file_fuzzy(struct sess *sess, struct upload *p, struct flist *f,
    struct stat *stp, int *filefd)
{
	int dirfd;
	DIR *dirp;
	int fd;
	struct dirent *di;
	struct stat st;
	char pathbuf[PATH_MAX];
	char root[PATH_MAX];
	char *rp;

	if (!S_ISREG(f->st.mode) || !f->st.size || !f->st.mtime) {
		return 1;
	}

	strlcpy((char*)&root, f->path, sizeof(root));
	if ((rp = strrchr(root, '/')) != NULL) {
		/* Keep the trailing / */
		*(++rp) = '\0';
	} else {
		root[0] = '\0';
	}

	if (*root == '\0') {
		dirfd = dup(p->rootfd);
	} else if ((dirfd = openat(p->rootfd, root, O_RDONLY | O_DIRECTORY |
	    O_RESOLVE_BENEATH)) < 0) {
		ERR("%s: pre_file_fuzzy: openat", root);
		return -1;
	}

	if (!(dirp = fdopendir(dirfd))) {
		ERR("%s: pre_file_fuzzy: opendirfd", root);
		close(dirfd);
		return -1;
	}

	while ((di = readdir(dirp)) != NULL) {
		if (di->d_name[0] == '.' && (di->d_name[1] == '\0' ||
		    (di->d_name[1] == '.' && di->d_name[2] == '\0'))) {
			continue;
		}
		if (!S_ISREG(DTTOIF(di->d_type))) {
			continue;
		}
		/* root has the trailing / already */
		if (snprintf(pathbuf, sizeof(pathbuf), "%s%s", root,
		    di->d_name) > sizeof(pathbuf)) {
			continue;
		}
		if (fstatat(p->rootfd, pathbuf, &st, AT_SYMLINK_NOFOLLOW) ==
		    -1) {
			ERR("%s: pre_file_fuzzy: fstatat", pathbuf);
			continue;
		}
		if (st.st_size == f->st.size && st.st_mtime == f->st.mtime) {
			if ((fd = openat(p->rootfd, pathbuf, O_RDONLY |
			    O_NOFOLLOW | O_RESOLVE_BENEATH)) == -1) {
				ERR("%s: pre_file_fuzzy: openat", pathbuf);
				continue;
			}
			*stp = st;
			*filefd = fd;
			f->iflags |= IFLAG_BASIS_FOLLOWS | IFLAG_HLINK_FOLLOWS;
			f->basis = BASIS_FUZZY;
			free(f->link);
			f->link = strdup(pathbuf);
			LOG4("fuzzy basis selected for %s: %s", f->path, f->link);

			(void)closedir(dirp);
			return 0;
		}
	}
	(void)closedir(dirp);

	return 1;
}

/*
 * Try to open the file at the current index.
 * If the file does not exist, returns with >0.
 * Return <0 on failure, 0 on success w/nothing to be done, >0 on
 * success and the file needs attention.
 */
static int
pre_file(struct upload *p, int *filefd, off_t *size,
    struct sess *sess, const struct hardlinks *hl)
{
	struct flist *f;
	const char *matchdir = NULL, *partialdir = NULL;
	struct stat st;
	size_t psize;
	int i, pdfd = -1, rc, ret;

	f = &p->fl[p->idx];
	assert(S_ISREG(f->st.mode));

	if (sess->opts->dry_run == DRY_FULL ||
	    sess->opts->read_batch != NULL) {
		log_file(sess, f);
		return 0;
	}

	if (sess->opts->max_size >= 0 && f->st.size > sess->opts->max_size) {
		WARNX("skipping over max-size file %s", f->path);
		return 0;
	}
	if (sess->opts->min_size >= 0 && f->st.size < sess->opts->min_size) {
		WARNX("skipping under min-size file %s", f->path);
		return 0;
	}

	/*
	 * For non dry-run cases, we'll write the acknowledgement later
	 * in the rsync_uploader() function.
	 */

	*size = 0;
	*filefd = -1;

	rc = check_file(p->rootfd, f, &st, sess, hl, false);
	if (rc == -1)
		return -1;
	if (rc == 4)
		return 0;
	if (rc == 2 && !S_ISREG(st.st_mode)) {
		int uflags = 0;
		bool do_unlink = false;

		if (force_delete_applicable(p, sess, st.st_mode))
			if (pre_dir_delete(p, sess, DMODE_DURING) == 0)
				return -1;

		/*
		 * If we're operating --inplace, need to clear out any stale
		 * non-file entries since we'll want to just open or create it
		 * and get to it.
		 */
		do_unlink = S_ISDIR(st.st_mode) || sess->opts->inplace;
		if (S_ISDIR(st.st_mode))
			uflags |= AT_REMOVEDIR;
		if (do_unlink && unlinkat(p->rootfd, f->path, uflags) == -1) {
			ERR("%s: unlinkat", f->path);
			sess->total_errors++;
			return 0;
		}

		/*
		 * Fix the return value so that we don't try to set metadata of
		 * what we unlinked below.
		 */
		if (do_unlink)
			rc = 3;
	}

	/*
	 * If the file exists, we need to fix permissions *before* we try to
	 * update it or we risk not being able to open it in the first place if
	 * the permissions are thoroughly messed up.
	 */
	if (rc >= 0 && rc < 3) {
		bool fix_metadata = (rc != 0 || !sess->opts->ign_non_exist) &&
		    !sess->opts->dry_run;

		if (fix_metadata &&
		    !rsync_set_metadata_at(sess, 0, p->rootfd, f, f->path)) {
			ERRX1("rsync_set_metadata");
			return -1;
		}

		if (rc == 0) {
			LOG3("%s: skipping: up to date", f->path);
			return 0;
		}
	}

	if (sess->opts->partial_dir != NULL) {
		char partial_path[PATH_MAX];
		const char *file_partial_dir;

		file_partial_dir = download_partial_path(sess, f, partial_path,
		    sizeof(partial_path));
		ret = pre_file_check_altdir(sess, p, &partialdir,
		    &st, file_partial_dir, f, hl, rc, BASE_MODE_COPY, &pdfd,
		    true);

		if (ret <= 0)
			return ret;

		if (partialdir != NULL) {
			matchdir = partialdir;
			psize = st.st_size;
		}
	}

	/* check alternative locations for better match */
	for (i = 0; sess->opts->basedir[i] != NULL; i++) {
		ret = pre_file_check_altdir(sess, p, &matchdir, &st,
		    sess->opts->basedir[i], f, hl, rc,
		    sess->opts->alt_base_mode, NULL, false);

		if (ret <= 0) {
			if (pdfd != -1)
				close(pdfd);
			return ret;
		}
	}

	/*
	 * partialdir is a special case, we'll work on it from there.
	 */
	if (matchdir != NULL && partialdir == NULL) {
		assert(pdfd == -1);
		/* copy match from basedir into root as a start point */
		copy_file(p->rootfd, matchdir, f);
		if (fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) ==
		    -1) {
			ERR("%s: fstatat", f->path);
			return -1;
		}
	}

	if (partialdir != NULL) {
		*size = psize;
		*filefd = openat(pdfd, download_partial_filepath(f),
		    O_RDONLY | O_NOFOLLOW);

		if (*filefd != -1)
			f->pdfd = pdfd;
	} else if (sess->opts->fuzzy_basis && rc == 3 &&
	    (ret = pre_file_fuzzy(sess, p, f, &st, filefd)) == 0) {
		/* Only consider fuzzy matches if the destination does not exist */
		assert(pdfd == -1);
		*size = st.st_size;
	} else {
		assert(pdfd == -1);
		*size = st.st_size;
		*filefd = openat(p->rootfd, f->path, O_RDONLY | O_NOFOLLOW);
	}
	/* If there is a symlink in our way, we will get EMLINK */
	if (*filefd == -1 && errno != ENOENT && errno != EMLINK) {
		ERR("%s: pre_file: openat", f->path);
		if (pdfd != -1)
			close(pdfd);
		return -1;
	}

	/* file needs attention */
	return 1;
}

/*
 * Allocate an uploader object in the correct state to start.
 * Returns NULL on failure or the pointer otherwise.
 * On success, upload_free() must be called with the allocated pointer.
 */
struct upload *
upload_alloc(const char *root, int rootfd, int tempfd, int fdout,
	size_t clen, struct flist *fl, size_t flsz, mode_t msk)
{
	struct upload	*p;

	if ((p = calloc(1, sizeof(struct upload))) == NULL) {
		ERR("calloc");
		return NULL;
	}

	p->state = UPLOAD_FIND_NEXT;
	p->oumask = msk;
	p->root = strdup(root);
	if (p->root == NULL) {
		ERR("strdup");
		free(p);
		return NULL;
	}
	p->rootfd = rootfd;
	p->tempfd = tempfd;
	p->csumlen = clen;
	p->fdout = fdout;
	p->fl = fl;
	p->flsz = flsz;
	p->nextack = 0;
	p->newdir = calloc(flsz, sizeof(int));
	if (p->newdir == NULL) {
		ERR("calloc");
		free(p->root);
		free(p);
		return NULL;
	}
	return p;
}

void
upload_next_phase(struct upload *p, struct sess *sess, int fdout)
{

	assert(p->state == UPLOAD_FINISHED);

	/* Reset for the redo phase. */
	p->state = UPLOAD_FIND_NEXT;
	p->nextack = 0;
	p->idx = 0;
	p->phase++;
	p->csumlen = CSUM_LENGTH_PHASE2;
}

/*
 * Perform all cleanups and free.
 * Passing a NULL to this function is ok.
 */
void
upload_free(struct upload *p)
{

	if (p == NULL)
		return;
	free(p->root);
	free(p->newdir);
	free(p->buf);
	free(p);
}

void
upload_ack_complete(struct upload *p, struct sess *sess, int fdout)
{
	struct flist *fl;
	size_t idx;

	assert(p->state != UPLOAD_WRITE);
	if (p->nextack == p->flsz || !sess->opts->remove_source)
		return;

	/*
	 * We'll halt at the next file the uploader needs to process since the
	 * status of flist entries after that are irrelevant.
	 */
	for (idx = p->nextack; idx < p->idx; idx++) {
		fl = &p->fl[idx];

		/* Skip over non-files */
		if (!S_ISREG(fl->st.mode))
			continue;

		/* Entry not yet processed by the downloader. */
		if ((fl->flstate & FLIST_DONE_MASK) == 0)
			break;

		/*
		 * Failed is only set if there's no hope of recovering, so we
		 * can just skip this one entirely.
		 */
		if ((fl->flstate & FLIST_FAILED) != 0)
			continue;

		/*
		 * Redo entries in the redo phase have also not been processed,
		 */
		if (p->phase > 0 && (fl->flstate & FLIST_REDO) != 0)
			break;

		/*
		 * Any redo left, we can skip over -- they won't be completing,
		 * if we're in the redo phase the downloader would have either
		 * cleared the redo flag if it succeeded, or it would have
		 * additionally marked it as having FAILED.
		 */
		if ((fl->flstate & FLIST_REDO) != 0)
			continue;

		if ((fl->flstate & (FLIST_SUCCESS | FLIST_SUCCESS_ACKED)) ==
		    FLIST_SUCCESS) {
			if (!io_write_int_tagged(sess, fdout, (int)idx, IT_SUCCESS))
				break;
			fl->flstate |= FLIST_SUCCESS_ACKED;
		}
	}

	p->nextack = idx;
}

/*
 * Iterates through all available files and conditionally gets the file
 * ready for processing to check whether it's up to date.
 * If not up to date or empty, sends file information to the sender.
 * If returns 0, we've processed all files there are to process.
 * If returns >0, we're waiting for POLLIN or POLLOUT data.
 * Otherwise returns <0, which is an error.
 */
int
rsync_uploader(struct upload *u, int *fileinfd,
	struct sess *sess, int *fileoutfd, const struct hardlinks *const hl)
{
	struct blkset	    blk;
	void		   *mbuf, *bufp;
	ssize_t		    msz;
	size_t		    i, pos, sz;
	size_t		    linklen;
	off_t		    offs, filesize;
	int		    c;

	if (sess->role->phase == NULL)
		sess->role->phase = &u->phase;

	/* Once finished this should never get called again. */
	assert(u->state != UPLOAD_FINISHED);

	/*
	 * If we have an upload in progress, then keep writing until the
	 * buffer has been fully written.
	 * We must only have the output file descriptor working and also
	 * have a valid buffer to write.
	 */

	if (u->state == UPLOAD_WRITE) {
		assert(u->buf != NULL);
		assert(*fileoutfd != -1);
		assert(*fileinfd == -1);

		/*
		 * Unfortunately, we need to chunk these: if we're
		 * the server side of things, then we're multiplexing
		 * output and need to wrap this in chunks.
		 * This is a major deficiency of rsync.
		 * FIXME: add a "fast-path" mode that simply dumps out
		 * the buffer non-blocking if we're not mplexing.
		 */

		if (u->bufpos < u->bufsz) {
			sz = MAX_CHUNK < (u->bufsz - u->bufpos) ?
				MAX_CHUNK : (u->bufsz - u->bufpos);
			c = io_write_buf(sess, u->fdout,
				u->buf + u->bufpos, sz);
			if (c == 0) {
				ERRX1("io_write_nonblocking");
				return -1;
			}
			u->bufpos += sz;
			if (u->bufpos < u->bufsz)
				return 1;
		}

		/*
		 * Let the UPLOAD_FIND_NEXT state handle things if we
		 * finish, as we'll need to write a POLLOUT message and
		 * not have a writable descriptor yet.
		 */

		u->state = UPLOAD_FIND_NEXT;
		u->idx++;

		/*
		 * For delay-updates, there's no use scanning the flist for
		 * every file since they won't flip to SUCCESS until after the
		 * delayed updates have been processed.
		 */
		if (!sess->opts->dlupdates)
			upload_ack_complete(u, sess, *fileoutfd);
		return 1;
	}

	/*
	 * If we invoke the uploader without a file currently open, then
	 * we iterate through til the next available regular file and
	 * start the opening process.
	 * This means we must have the output file descriptor working.
	 */

	if (u->state == UPLOAD_FIND_NEXT) {
		assert(*fileinfd == -1);
		assert(*fileoutfd != -1);

		for ( ; u->idx < u->flsz; u->idx++) {
			assert(u->fl[u->idx].sendidx != -1);
			if (u->phase == PHASE_REDO &&
			    (u->fl[u->idx].flstate & FLIST_REDO) == 0)
				continue;
			else if (u->phase == PHASE_DLUPDATES)
				continue;
			if (S_ISDIR(u->fl[u->idx].st.mode))
				c = pre_dir(u, sess);
			else if (S_ISLNK(u->fl[u->idx].st.mode))
				c = pre_symlink(u, sess);
			else if (S_ISREG(u->fl[u->idx].st.mode))
				c = pre_file(u, fileinfd, &filesize, sess, hl);
			else if (S_ISBLK(u->fl[u->idx].st.mode) ||
			    S_ISCHR(u->fl[u->idx].st.mode))
				c = pre_dev(u, sess);
			else if (S_ISFIFO(u->fl[u->idx].st.mode))
				c = pre_fifo(u, sess);
			else if (S_ISSOCK(u->fl[u->idx].st.mode))
				c = pre_sock(u, sess);
			else
				c = 0;
			if (!sess->lateprint)
				output(sess, &u->fl[u->idx], 1);

			if (c < 0)
				return -1;
			else if (c > 0)
				break;

			u->fl[u->idx].flstate |= FLIST_SUCCESS;

			if (!protocol_itemize) {
				continue;
			}
			u->bufsz = sizeof(int32_t); /* identifier */
			u->bufsz += sizeof(int16_t); /* iflags */
			if (IFLAG_BASIS_FOLLOWS & u->fl[u->idx].iflags) {
				/* basis flag */
				u->bufsz += sizeof(int8_t);
	                }
			if (IFLAG_HLINK_FOLLOWS & u->fl[u->idx].iflags) {
				/* vstring len byte */
				u->bufsz += sizeof(int8_t);
				if ((linklen = strlen(u->fl[u->idx].link)) >
				    0x7f) {
					/* 2nd len byte */
					u->bufsz += sizeof(int8_t);
				}
				u->bufsz += linklen; /* vstring */
			}
			if (u->bufsz > u->bufmax) {
				if ((bufp = realloc(u->buf, u->bufsz)) == NULL) {
					ERR("realloc");
					return -1;
				}
				u->buf = bufp;
				u->bufmax = u->bufsz;
			}
			u->bufpos = pos = 0;
			io_buffer_int(u->buf, &pos, u->bufsz, (int)u->idx);
			io_buffer_short(u->buf, &pos, u->bufsz,
			    u->fl[u->idx].iflags);
			if (IFLAG_BASIS_FOLLOWS & u->fl[u->idx].iflags) {
				io_buffer_byte(u->buf, &pos, u->bufsz,
				    u->fl[u->idx].basis);
	                }
			if (IFLAG_HLINK_FOLLOWS & u->fl[u->idx].iflags) {
				io_buffer_vstring(u->buf, &pos, u->bufsz,
				    u->fl[u->idx].link, linklen);
	                }
		}

		/*
		 * Whether we've finished writing files or not, we
		 * disable polling on the output channel.
		 */

		*fileoutfd = -1;
		if (u->idx == u->flsz) {
			assert(*fileinfd == -1);
			if (sess->opts->read_batch == NULL &&
			    !io_write_int(sess, u->fdout, -1)) {
				ERRX1("io_write_int");
				return -1;
			}
			u->state = UPLOAD_FINISHED;
			LOG4("uploader: finished");
			return 0;
		}

		/* Go back to the event loop, if necessary. */

		u->state = UPLOAD_WRITE;
	}

	/* Initialies our blocks. */

	assert(u->state == UPLOAD_WRITE);
	memset(&blk, 0, sizeof(struct blkset));
	blk.csum = u->csumlen;

	if (sess->opts->read_batch != NULL)
		goto nowrite;

	if (*fileinfd != -1 && filesize > 0) {
		if (sess->opts->block_size > (512 << 20))
			errx(1, "--block-size=%ld: must be no greater than %d",
			     sess->opts->block_size, (512 << 20));
		if (sess->opts->whole_file) {
			init_null_blkset(&blk, filesize);
		} else {
			init_blkset(&blk, filesize, sess->opts->block_size);
		}

		if (sess->opts->no_cache) {
#if defined(F_NOCACHE)
			fcntl(*fileinfd, F_NOCACHE);
#elif defined(O_DIRECT)
			int getfl;

			if ((getfl = fcntl(*fileinfd, F_GETFL)) < 0) {
				warn("fcntl failed");
			} else {
				fcntl(*fileinfd, F_SETFL, getfl | O_DIRECT);
			}
#endif
		}

		if (u->phase == 0 && (sess->role->append ||
		    sess->opts->whole_file)) {
			goto skipmap;
		}

		assert(blk.blksz);

		blk.blks = calloc(blk.blksz, sizeof(struct blk));
		if (blk.blks == NULL) {
			ERR("calloc");
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		}

		if ((mbuf = malloc(blk.len)) == NULL) {
			ERR("malloc");
			close(*fileinfd);
			*fileinfd = -1;
			free(blk.blks);
			return -1;
		}

		offs = 0;
		i = 0;
		do {
			msz = pread(*fileinfd, mbuf, blk.len, offs);
			if ((size_t)msz != blk.len && (size_t)msz != blk.rem) {
				ERR("pread");
				close(*fileinfd);
				*fileinfd = -1;
				free(mbuf);
				free(blk.blks);
				return -1;
			}
			init_blk(&blk.blks[i], &blk, offs, i, mbuf, sess);
			offs += blk.len;
			LOG4(
			    "i=%ld, offs=%lld, msz=%ld, blk.len=%lu, blk.rem=%lu",
			    i, (long long)offs, msz, blk.len, blk.rem);
			i++;
		} while (i < blk.blksz);

		free(mbuf);

		LOG3("%s: mapped %jd B with %zu blocks",
		    u->fl[u->idx].path, (intmax_t)blk.size,
		    blk.blksz);

	  skipmap:
		close(*fileinfd);
		*fileinfd = -1;
	} else {
		if (*fileinfd != -1) {
			close(*fileinfd);
			*fileinfd = -1;
		}
		blk.len = MAX_CHUNK; /* Doesn't matter. */
		LOG3("%s: not mapped", u->fl[u->idx].path);
	}

	assert(*fileinfd == -1);

	/* Make sure the block metadata buffer is big enough. */

	u->bufsz =
	    sizeof(int32_t) + /* identifier */
	    sizeof(int32_t) + /* block count */
	    sizeof(int32_t) + /* block length */
	    sizeof(int32_t) + /* checksum length */
	    sizeof(int32_t);  /* block remainder */

	if (u->phase > 0 || !sess->role->append) {
		u->bufsz += blk.blksz *
			(sizeof(int32_t) + /* short checksum */
			 blk.csum); /* long checksum */
	}
	if (protocol_itemize) {
		u->fl[u->idx].iflags |= IFLAG_TRANSFER;
		u->bufsz += sizeof(int16_t); /* iflags */
		if (IFLAG_BASIS_FOLLOWS & u->fl[u->idx].iflags) {
			u->bufsz += sizeof(int8_t); /* basis flag */
                }
		if (IFLAG_HLINK_FOLLOWS & u->fl[u->idx].iflags) {
			u->bufsz += sizeof(int8_t); /* vstring len byte */
			if ((linklen = strlen(u->fl[u->idx].link)) > 0x7f) {
				u->bufsz += sizeof(int8_t); /* 2nd len byte */
			}
			u->bufsz += linklen; /* vstring */
		}
	}

	if (u->bufsz > u->bufmax) {
		if ((bufp = realloc(u->buf, u->bufsz)) == NULL) {
			ERR("realloc");
			free(blk.blks);
			return -1;
		}
		u->buf = bufp;
		u->bufmax = u->bufsz;
	}

	u->bufpos = pos = 0;
	io_buffer_int(u->buf, &pos, u->bufsz, u->fl[u->idx].sendidx);
	if (protocol_itemize) {
		io_buffer_short(u->buf, &pos, u->bufsz, u->fl[u->idx].iflags);
		if (IFLAG_BASIS_FOLLOWS & u->fl[u->idx].iflags) {
			io_buffer_byte(u->buf, &pos, u->bufsz, u->fl[u->idx].basis);
                }
		if (IFLAG_HLINK_FOLLOWS & u->fl[u->idx].iflags) {
			io_buffer_vstring(u->buf, &pos, u->bufsz,
			    u->fl[u->idx].link, linklen);
                }
	}
	io_buffer_int(u->buf, &pos, u->bufsz, (int)blk.blksz);
	io_buffer_int(u->buf, &pos, u->bufsz, (int)blk.len);
	io_buffer_int(u->buf, &pos, u->bufsz, (int)blk.csum);
	io_buffer_int(u->buf, &pos, u->bufsz, (int)blk.rem);

	if (!sess->role->append && !sess->opts->whole_file) {
		for (i = 0; i < blk.blksz; i++) {
			io_buffer_int(u->buf, &pos, u->bufsz,
				      blk.blks[i].chksum_short);
			io_buffer_buf(u->buf, &pos, u->bufsz,
				      blk.blks[i].chksum_long, blk.csum);
		}
	}
	assert(pos == u->bufsz);

	sess->total_files_xfer++;
	sess->total_xfer_size += u->fl[u->idx].st.size;

nowrite:
	/* Reenable the output poller and clean up. */

	*fileoutfd = u->fdout;
	free(blk.blks);
	return 1;
}

/*
 * Fix up the directory permissions and times post-order.
 * We can't fix up directory permissions in place because the server may
 * want us to have overly-tight permissions---say, those that don't
 * allow writing into the directory.
 * We also need to do our directory times post-order because making
 * files within the directory will change modification times.
 * Returns zero on failure, non-zero on success.
 */
int
rsync_uploader_tail(struct upload *u, struct sess *sess)
{
	size_t	 i;


	if (!sess->opts->preserve_times &&
	    !sess->opts->preserve_perms)
		return 1;

	LOG2("fixing up directory times and permissions");

	for (i = 0; i < u->flsz; i++)
		if (S_ISDIR(u->fl[i].st.mode))
			if (!post_dir(sess, u, i))
				return 0;

	return 1;
}
