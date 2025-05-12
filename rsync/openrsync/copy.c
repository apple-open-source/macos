/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2024, Klara, Inc.
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

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#ifndef MAX
#define	MAX(a,b)	(((a)>(b))?(a):(b))
#endif

#define _MAXBSIZE (64 * 1024)

/*
 * We're using AT_RESOLVE_BENEATH in a couple of places just for some additional
 * safety on platforms that support it, so it's not a hard requirement.
 */
#ifndef AT_RESOLVE_BENEATH
#define	AT_RESOLVE_BENEATH	0
#endif

/*
 * Return true if all bytes in buffer are zero.
 * A buffer of zero length is also considered a zero buffer.
 */
int
iszerobuf(const void *b, size_t len)
{
	const unsigned char *c = b;

	for (; len > 0; len--) {
		if (*c++ != '\0')
			return 0;
	}
	return 1;
}

/*
 * Calculate the depth of a path (in directories).
 * Account for ../, and return -1 if the depth drops below zero.
 *
 * Note that in strict mode, non-leading ../ components are assumed to be unsafe
 * and will return -1 as well because we do not know if the previous components
 * would be replaced with a symlink that makes it unsafe.
 */
static int
count_dir_depth(const char *path, int dirdepth, int strict)
{
	const char *dp, *lastp;
	bool leading = true;

	/* Blank or absolute symlinks are always unsafe */
	if (path == NULL || *path == '\0')
	{
		return 0;
	}

	dp = lastp = path;
	while (dp != NULL) {
		/* Skip any excess slashes */
		while (*dp == '/') {
			dp++;
		}
		if (strncmp(dp, "../", 3) == 0) {
			/* Traversing up directory depth */
			if (strict && !leading)
				return -1;
			dirdepth--;
		} else if (strncmp(dp, "./", 2) == 0) {
			/* No Change in directory depth */

			/*
			 * This is more strict than we need to be, but it
			 * matches what rsync 3.x did.  Presumably openrsync
			 * won't be running on a machine where ./ could be
			 * replaced.
			 */
			leading = false;
		} else if (strchr(dp, '/') != NULL) {
			/* Traversing down directory depth */
			dirdepth++;

			/*
			 * If we didn't hit one of the above cases, then we're
			 * no longer a leading component.  We're defining
			 * leading here as everything up to the first non-..
			 * and non-. component.  Subsequent ../ should fail.
			 */
			leading = false;
		}
		/* If we ever go above the starting point, fail */
		if (strict && dirdepth < 0) {
			return -1;
		}
		lastp = dp;
		dp = strchr(dp, '/');
		if (dp != NULL) {
			dp++;

			/*
			 * If we had a trailing '/', we'll zap lastp and break
			 * so that we don't examine the last component.
			 */
			if (*dp == '\0') {
				lastp = NULL;
				break;
			}
		}
	}

	/*
	 * lastp will be NULL if we had a trailing slash, as we don't need to
	 * inspect anything else -- it was properly accounted for in the loop.
	 */
	if (lastp != NULL && strcmp(lastp, "..") == 0) {
		dirdepth--;
		if (strict && !leading)
			return -1;
	}

	return dirdepth;
}

char *
make_safe_link(const char *link)
{
	char *dest, *dsep, *end, *start, *walker;
	size_t depth = 0, destsz, linksz;
	char trailer = 0;

	/*
	 * We want at least enough space for "./" in the output buffer, in case
	 * the input is empty.
	 */
	linksz = strlen(link);
	destsz = MAX(2, linksz);
	dest = malloc(destsz + 1);
	if (dest == NULL) {
		ERR("malloc");
		return (NULL);
	}

	if (linksz == 0) {
		*dest = '\0';
		goto out;
	}

	(void)strlcpy(dest, link, destsz + 1);

	trailer = link[linksz - 1];

	start = dest;
	end = &dest[linksz + 1];

	walker = start;
	while (*walker == '/')
		walker++;

	/* Empty string or just a lot of '/' */
	if (*walker == '\0') {
		*dest = '\0';
		goto out;
	}

	/* At a minimum we need to move the remaining bits back to the front. */
	memmove(start, walker, end - walker);

	dsep = walker = start;
	while ((walker = strsep(&dsep, "/")) != NULL) {
		char *next;

		if (walker > start)
			*(walker - 1) = '/';
		if (*walker == '\0') {
			if (dsep == NULL)
				break;

			/* Normalize double slashes while we're here. */
			memmove(walker, walker + 1, end - (walker + 1));
			dsep--;
			end--;

			continue;
		}

		next = dsep;
		if (next == NULL)
			next = end - 1;
		if (strncmp(walker, ".", next - walker) == 0) {
			/* Just move /./ out of the way. */
			if (*next == '\0') {
				/* Truncate, preserve trailing slash. */
				*walker = *(walker + 1);
				break;
			}

			memmove(walker, next, end - next);
			end -= next - walker;
			dsep -= next - walker;
			continue;
		}

		if (strncmp(walker, "..", next - walker) == 0) {
			/*
			 * We strip as soon as we move from 1 -> 0, we don't
			 * just allow "foo/.." to remain as-is.
			 */
			if (depth <= 1) {
				/*
				 * Strip everything prior to this, because we
				 * just moved out of our root.
				 */
				if (*next == '\0') {
					/* Just zap the whole string. */
					*start = '\0';
					break;
				}

				/* Otherwise, preserve the trailing part. */
				memmove(start, next, end - next);
				end -= next - start;
				dsep -= next - start;

				depth = 0;
			} else {
				depth--;
			}

			continue;
		} else {
			depth++;
		}
	}

out:
	if (*dest == '\0')
		(void)snprintf(dest, destsz + 1, ".%s",
		    trailer == '/' ? "/" : "");

	return (dest);
}

/*
 * Determine if the target of a symlink is "safe", relative to
 * the src (the path of the symlink).  Absolute symlinks are unsafe.
 * Any symlink target that reaches above the root, is unsafe.
 */
int
is_unsafe_link(const char *link, const char *src, const char *root)
{
	size_t rootlen;
	int srcdepth;
	int linkdepth;

	/* Blank or absolute symlinks are always unsafe */
	if (link == NULL || *link == '\0' || *link == '/')
	{
		return 1;
	}

	if (root != NULL) {
		rootlen = strlen(root);
		if (strncmp(src, root, rootlen) == 0) {
			/* Make src relative to root */
			src += rootlen;
			if (*src == '/') {
				src++;
			}
		} else {
			/* src is outside of the root, this is unsafe */
			WARNX("%s: is_unsafe_link: src file is outside of the root: %s\n", src, root);
			return 1;
		}
	}

	srcdepth = count_dir_depth(src, 0, 0);
	if (srcdepth < 0) {
		/* src escapes the root, this in unsafe */
		WARNX("%s: is_unsafe_link: src escaped the root: %s\n", src, root);
		return 1;
	}
	linkdepth = count_dir_depth(link, srcdepth, 1);

	return linkdepth < 0;
}

static int
copy_internal(int fromfd, int tofd)
{
	char buf[_MAXBSIZE];
	ssize_t r, w;

	while ((r = read(fromfd, buf, sizeof(buf))) > 0) {
		if (iszerobuf(buf, sizeof(buf))) {
			if (lseek(tofd, r, SEEK_CUR) == -1)
				return -1;
		} else {
			w = write(tofd, buf, r);
			if (r != w || w == -1)
				return -1;
		}
	}
	if (r == -1)
		return -1;
	if (ftruncate(tofd, lseek(tofd, 0, SEEK_CUR)) == -1)
		return -1;
	return 0;
}

/*
 * Create the directory struction required for storing backups.
 * The fname will be the relative filename prefixed with the backup_dir.
 * We then check the deepest directory and see if we can mkdir it, if we can
 * (or it exists), we advance to the second step.  If the mkdir fails with
 * ENOENT because the parent doesn't exist, we work backwards through the
 * provided path until we find a directory that exists or that we can create.
 *
 * In the second step, we work forwards through the path again and create the
 * child directories required, and chown/chmod them match the directories that
 * we are backing up.
 */
static int
mk_backup_dir(struct sess *sess, int rootfd, const char *fname)
{
	char *bpath, *bporig, *bpend;
	char *bpp;
	char *rpath = NULL;
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	struct stat st;
	int ret = 0;

	bporig = bpath = strdup(fname);
	if (bpath == NULL) {
		return ENOMEM;
	}
	bpend = bpath + strlen(bpath);
	while (strncmp(bpath, "./", 2) == 0) {
		bpath += 2;
	}
	rpath = bpath + strlen(sess->opts->backup_dir);
	assert(rpath < bpend);
	if (*rpath == '/') {
		rpath++;
	}
	/*
	 * Walk backwards through the backup path to find the deepest directory
	 * that already exists.
	 */
	while ((bpp = strrchr(bpath, '/')) != NULL) {
		*bpp = '\0';
		if ((ret = mkdirat(rootfd, bpath, mode)) == 0 || errno == EEXIST) {
			ret = 0;
			/* Found a directory that exists or that we could create */
			break;
		} else if (errno != ENOENT) {
			ERR("%s: mkdir", bpath);
			goto out;
		}
	}

	/*
	 * Walk forwards through the backup path creating the ancestor directories
	 * as we go.
	 */
	bpp = bpath + strlen(bpath);
	assert(bpp < bpend);
	while (1) {
		if ((rpath + strlen(rpath)) != bpend && *rpath != '\0') {
			if ((ret = fstatat(rootfd, rpath, &st, AT_RESOLVE_BENEATH)) < 0) {
				ERR("%s: stat", rpath);
				goto out;
			} else {
				fchownat(rootfd, bpath, st.st_uid, st.st_gid,
				    AT_SYMLINK_NOFOLLOW);
				fchmodat(rootfd, bpath, st.st_mode,
				    AT_SYMLINK_NOFOLLOW);
			}
		}
		*bpp = '/';
		bpp += strlen(bpp);
		if (bpp == bpend) {
			break;
		}
		assert(bpp < bpend);
		if ((ret = mkdirat(rootfd, bpath, mode)) < 0) {
			ERR("%s: mkdir", bpath);
			goto out;
		}
	}

out:
	free(bporig);
	return ret;
}

int
backup_file(int fromdfd, const char *fname, int todfd, const char *tname,
    int replace, const struct fldstat *dstat)
{
	struct stat st;
	int rc;

	rc = move_file(fromdfd, fname, todfd, tname, replace);
	if (rc != 0)
		return rc;

	if (fstatat(todfd, tname, &st, 0) == -1)
		return -1;

	/*
	 * Set metadata on the backup file to match the metadata
	 * from the original destination file.
	 */
	if (st.st_atim.tv_sec != dstat->atime.tv_sec ||
	    st.st_atim.tv_nsec != dstat->atime.tv_nsec ||
	    st.st_mtim.tv_sec != dstat->mtime.tv_sec ||
	    st.st_mtim.tv_nsec != dstat->mtime.tv_nsec) {
		const struct timespec ts[] = {
			dstat->atime, dstat->mtime,
		};

		rc = utimensat(todfd, tname, ts, AT_SYMLINK_NOFOLLOW);
		if (rc != 0)
			ERR("%s: utimensat", tname);
	}

	if (st.st_mode != dstat->mode) {
		rc = fchmodat(todfd, tname, dstat->mode, AT_SYMLINK_NOFOLLOW);
		if (rc != 0)
			ERR("%s: fchmodat", tname);
	}

	if (st.st_uid != dstat->uid || st.st_gid != dstat->gid) {
		const uid_t uid = dstat->uid;
		const uid_t gid = dstat->gid;

		if (uid != (uid_t)-1 || gid != (gid_t)-1) {
			rc = fchownat(todfd, tname, uid, gid, AT_SYMLINK_NOFOLLOW);
			if (rc != 0)
				ERR("%s: fchownat", tname);
		}
	}

	return 0;
}

int
backup_to_dir(struct sess *sess, int rootfd, const struct flist *f,
    const char *dest, mode_t mode)
{
	int ret = 0;
	struct stat st;

	if (fstatat(rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) < 0) {
		/* Can't backup files that do not exist */
		return 0;
	}

	if ((ret = mk_backup_dir(sess, rootfd, dest)) != 0) {
		ERR("%s: mk_backup_dir: %s", f->path, dest);
		return ret;
	}

	if (S_ISDIR(mode)) {
		/* Make an empty directory as the backup */
		if ((ret = mkdirat(rootfd, dest, mode)) > 0) {
			ERR("%s: mkdirat", dest);
			return ret;
		}
		unlinkat(rootfd, f->path, AT_REMOVEDIR);
	} else if (sess->opts->preserve_links && S_ISLNK(mode)) {
		/* apply safe_symlinks here */
		unlinkat(rootfd, dest, AT_RESOLVE_BENEATH);
		if ((ret = symlinkat(f->link, rootfd, dest)) < 0) {
			ERR("%s: symlinkat", dest);
			return ret;
		}
		unlinkat(rootfd, f->path, AT_RESOLVE_BENEATH);
	} else if (!S_ISREG(mode)) {
		WARNX("backup_to_dir: skipping non-regular file "
		    "%s\n", f->path);
		return 0;
	} else {
		if ((ret = backup_file(rootfd, f->path, rootfd, dest, 1, &f->dstat)) < 0) {
			ERR("%s: backup_file: %s", f->path, dest);
			return ret;
		}
	}

	return ret;
}

int
move_file(int fromdfd, const char *fname, int todfd, const char *tname,
    int replace)
{
	int fromfd, tofd;
	int ret, serrno;
	int toflags = O_WRONLY | O_NOFOLLOW | O_TRUNC | O_CREAT;

	if (!replace) {
		toflags |= O_EXCL;
	}

	/* We'll try a rename(2) first. */
	ret = renameat(fromdfd, fname, todfd, tname);
	if (ret == 0)
		return (0);
	if (ret == -1 && errno != EXDEV)
		return (ret);

	/* Fallback to a copy. */
	fromfd = openat(fromdfd, fname, O_RDONLY | O_NOFOLLOW);
	if (fromfd == -1)
		return (-1);

	/* Unlink tname if it exists and is not writeable */
	if (faccessat(todfd, tname, W_OK, AT_RESOLVE_BENEATH) == -1 &&
	    errno == EACCES)
		unlinkat(todfd, tname, AT_RESOLVE_BENEATH);

	tofd = openat(todfd, tname, toflags, 0600);
	if (tofd == -1) {
		serrno = errno;
		close(fromfd);
		errno = serrno;
		return (-1);
	}

	ret = copy_internal(fromfd, tofd);
	if (ret == 0) {
		struct stat fromst, tost;
		int rc;

		ret = fstat(tofd, &tost);
		if (ret)
			goto errout;

		ret = fstat(fromfd, &fromst);
		if (ret)
			goto errout;

		if (fromst.st_mode != tost.st_mode) {
			rc = fchmod(tofd, fromst.st_mode);
			if (rc == -1)
				ERR("%s: fchmod", tname);
		}

		if (fromst.st_uid != tost.st_uid ||
		    fromst.st_gid != tost.st_gid) {
			rc = fchown(tofd, fromst.st_uid, fromst.st_gid);
			if (rc == -1)
				ERR("%s: fchown", tname);
		}

		if (fromst.st_atime != tost.st_atime ||
		    fromst.st_mtime != tost.st_mtime) {
			struct timespec ts[] = {
				fromst.st_atim, fromst.st_mtim
			};

			rc = futimens(tofd, ts);
			if (rc == -1)
				ERR("%s: futimens", tname);
		}
	}

errout:
	serrno = errno;
	close(fromfd);
	close(tofd);
	errno = serrno;

	if (ret == 0)
		(void)unlinkat(fromdfd, fname, 0);

	return (ret);
}

void
copy_file(int rootfd, const char *basedir, const struct flist *f)
{
	int fromfd, tofd, dfd;

	dfd = openat(rootfd, basedir, O_RDONLY | O_DIRECTORY);
	if (dfd == -1)
		err(ERR_FILE_IO, "%s: copy_file dfd: openat", basedir);

	fromfd = openat(dfd, f->path, O_RDONLY | O_NOFOLLOW);
	if (fromfd == -1)
		err(ERR_FILE_IO, "%s/%s: copy_file fromfd: openat", basedir, f->path);
	close(dfd);

	tofd = openat(rootfd, f->path,
	    O_WRONLY | O_NOFOLLOW | O_TRUNC | O_CREAT, 0600);
	if (tofd == -1)
		err(ERR_FILE_IO, "%s: copy_file tofd: openat", f->path);

	if (copy_internal(fromfd, tofd) == -1)
		err(ERR_FILE_IO, "%s: copy file", f->path);

	close(fromfd);
	close(tofd);
}
