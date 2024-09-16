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

#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

enum	pfdt {
	PFD_SENDER_IN = 0, /* input from the sender */
	PFD_UPLOADER_IN, /* uploader input from a local file */
	PFD_DOWNLOADER_IN, /* downloader input from a local file */
	PFD_SENDER_OUT, /* output to the sender */
	PFD__MAX
};

/*
 * Returns the new destination file mode if the application of the
 * executability option rules would result in it differing from
 * dstmode.  Returns 0 otherwise.
 */
static mode_t
preserve_executability_check(const mode_t srcmode, const mode_t dstmode)
{
	const mode_t xmask = S_IXUSR | S_IXGRP | S_IXOTH;
	bool xsrc = srcmode & xmask;
	bool xdst = dstmode & xmask;
	mode_t mode = 0;

	if (xsrc != xdst) {
		if (xsrc) {
			mode_t rmask = dstmode & (S_IRUSR | S_IRGRP | S_IROTH);

			mode = dstmode | (rmask >> 2);
		} else {
			mode = dstmode & ~xmask;
		}
	}

	return mode;
}

int
rsync_set_metadata(struct sess *sess, int newfile,
	int fd, const struct flist *f, const char *path)
{
	uid_t		 uid = (uid_t)-1;
	gid_t		 gid = (gid_t)-1;
	mode_t		 mode;
	struct timespec	 ts[2];
	struct stat      st;
	bool		 pres_exec;

	pres_exec = !newfile && S_ISREG(f->st.mode) &&
	    (sess->opts->preserve_executability && !sess->opts->preserve_perms);

	if (pres_exec) {
		if (fstat(fd, &st) == -1)
			if (errno == ENOENT)
				return 1;
	}

	/* Conditionally adjust file modification time. */

	if (sess->opts->preserve_times &&
	    (!S_ISDIR(f->st.mode) || !sess->opts->omit_dir_times)) {
		ts[0].tv_nsec = UTIME_NOW;
		ts[1].tv_sec = f->st.mtime;
		ts[1].tv_nsec = 0;
		if (futimens(fd, ts) == -1) {
			ERR("%s: futimens", path);
			return 0;
		}
		LOG4("%s: updated date", f->path);
	}

	/*
	 * Conditionally adjust identifiers.
	 * If we have an EPERM, report it but continue on: this just
	 * means that we're mapping into an unknown (or disallowed)
	 * group identifier.
	 */
	if ((sess->opts->supermode == SMODE_ON || geteuid() == 0) &&
	    sess->opts->preserve_uids && sess->opts->supermode != SMODE_OFF)
		uid = f->st.uid;
	if (sess->opts->preserve_gids)
		gid = f->st.gid;

	mode = f->st.mode;
	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		if (fchown(fd, uid, gid) == -1) {
			if (errno != EPERM) {
				ERR("%s: fchown", path);
				return 0;
			}
			if (geteuid() == 0)
				WARNX("%s: identity unknown or not available "
				    "to user.group: %u.%u", f->path, uid, gid);
		} else
			LOG4("%s: updated uid and/or gid", f->path);
	}

	/* Conditionally adjust file permissions. */

	if (newfile || sess->opts->preserve_perms) {
		if (fchmod(fd, mode) == -1) {
			ERR("%s: fchmod", path);
			return 0;
		}
		LOG4("%s: updated permissions", f->path);
	} else if (pres_exec) {
		mode = preserve_executability_check(mode, st.st_mode);
		if (mode != 0) {
			if (fchmod(fd, mode) == -1) {
				ERR("%s: fchmod", path);
				return 0;
			}
			LOG4("%s: updated permissions", f->path);
		}
	}

	return 1;
}

int
rsync_set_metadata_at(struct sess *sess, int newfile, int rootfd,
	const struct flist *f, const char *path)
{
	uid_t		 uid = (uid_t)-1;
	gid_t		 gid = (gid_t)-1;
	mode_t		 mode;
	struct timespec	 ts[2];
	struct stat      st;
	bool		 pres_exec;

	pres_exec = !newfile && S_ISREG(f->st.mode) &&
	    (sess->opts->preserve_executability && !sess->opts->preserve_perms);

	if (pres_exec || sess->opts->ign_non_exist) {
		if (fstatat(rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) == -1)
			if (errno == ENOENT)
				return 1;
	}

	/* Conditionally adjust file modification time. */

	if (sess->opts->preserve_times &&
	    (!S_ISDIR(f->st.mode) || !sess->opts->omit_dir_times)) {
		ts[0].tv_nsec = UTIME_NOW;
		ts[1].tv_sec = f->st.mtime;
		ts[1].tv_nsec = 0;
		if (utimensat(rootfd, path, ts, AT_SYMLINK_NOFOLLOW) == -1) {
			ERR("%s: utimensat (2)", path);
			return 0;
		}
		LOG4("%s: updated date", f->path);
	}

	/*
	 * Conditionally adjust identifiers.
	 * If we have an EPERM, report it but continue on: this just
	 * means that we're mapping into an unknown (or disallowed)
	 * group identifier.
	 */
	if ((sess->opts->supermode == SMODE_ON || geteuid() == 0) &&
	    sess->opts->preserve_uids && sess->opts->supermode != SMODE_OFF)
		uid = f->st.uid;
	if (sess->opts->preserve_gids)
		gid = f->st.gid;

	mode = f->st.mode;
	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		if (fchownat(rootfd, path, uid, gid, AT_SYMLINK_NOFOLLOW) == -1) {
			if (errno != EPERM) {
				ERR("%s: fchownat", path);
				return 0;
			}
			if (geteuid() == 0)
				WARNX("%s: identity unknown or not available "
				    "to user.group: %u.%u", f->path, uid, gid);
		} else
			LOG4("%s: updated uid and/or gid", f->path);
	}

	/* Conditionally adjust file permissions. */

	if (newfile || sess->opts->preserve_perms) {
		if (fchmodat(rootfd, path, mode, AT_SYMLINK_NOFOLLOW) == -1) {
			if (!(S_ISLNK(f->st.mode) && errno == EOPNOTSUPP)) {
				ERR("%s: fchmodat (1) %d", path, errno);
				return 0;
			}
		}
		LOG4("%s: updated permissions", f->path);
	} else if (pres_exec) {
		mode = preserve_executability_check(mode, st.st_mode);
		if (mode != 0) {
			if (fchmodat(rootfd, path, mode, AT_SYMLINK_NOFOLLOW) == -1) {
				ERR("%s: fchmodat", path);
				return 0;
			}
			LOG4("%s: updated permissions", f->path);
		}
	}

	return 1;
}

struct info_for_hardlink {
	int64_t device;
	int64_t inode;
	const struct flist *ref; /* Points to full entry */
};
struct hardlinks {
	struct info_for_hardlink *infos;
	int n;
};

static int
info_for_hardlink_compare(const void *onep, const void *twop)
{
	const struct info_for_hardlink *one = onep;
	const struct info_for_hardlink *two = twop;

	if (one->inode == two->inode) {
		if (one->device < two->device)
			return -1;
		if (one->device > two->device)
			return 1;
	}
	if (one->inode < two->inode)
		return -1;
	if (one->inode > two->inode)
		return 1;
	assert(one->inode == two->inode);
	return 0;
}

/* Important: this needs to happen after fl is sorted. */
static int
build_for_hardlinks(struct info_for_hardlink *hl,
	const struct flist *const fl, const size_t flsz)
{
	size_t i;
	int hlsz = 0;

	for (i = 0; i < flsz; i++) {
		if (fl[i].st.inode == 0 && fl[i].st.device == 0)
			continue;
		hl[hlsz].device = fl[i].st.device;
		hl[hlsz].inode = fl[i].st.inode;
		hl[hlsz++].ref = &fl[i];
	}
	qsort(hl, hlsz, sizeof(struct info_for_hardlink),
		info_for_hardlink_compare);
	return hlsz;
}

const struct flist *
find_hl(const struct flist *const this, const struct hardlinks *const hl)
{
	/*
	 * How does the hardlink handling work?
	 * The first file with identical device/inode is written
	 * to disk.  Every subsequent one is not is not written
	 * and later hardlinked.
	 * For this function that means we return NULL when "our"
	 * flist entry is the first with same device/inode.  If it isn't
	 * that first one is returned.  We don't ever mess with subsequent
	 * ones.
	 */
	int i;
	int n_seen = 0;
	const struct flist *returnthis = NULL;

	/* TODO: use binary search here, it is already sorted */
	for (i = 0; i < hl->n; i++) {
		if (this->st.inode == hl->infos[i].inode &&
			this->st.device == hl->infos[i].device) {
			n_seen++;
			if (n_seen == 1) {
				if (this == hl->infos[i].ref)
					return NULL;
				else
					returnthis = hl->infos[i].ref;
			}
			if (n_seen == 2)
				return returnthis;
		}
	}
	return NULL;
}

/*
 * Pledges: unveil, unix, rpath, cpath, wpath, stdio, fattr, chown.
 * Pledges (dry-run): -unix, -cpath, -wpath, -fattr, -chown.
 */
int
rsync_receiver(struct sess *sess, struct cleanup_ctx *cleanup_ctx,
    int fdin, int fdout, const char *root)
{
	struct role	 receiver;
	struct flist	*fl = NULL, *dfl = NULL;
	size_t		 i, flsz = 0, dflsz = 0, length, flist_bytes = 0;
	char		*derived_root = NULL, *tofree;
	int		 rc = 0, dfd = -1, tfd = -1, phase = 0, c;
	int32_t		 ioerror;
	struct stat	 st;
	struct pollfd	 pfd[PFD__MAX];
	struct download	*dl = NULL;
	struct upload	*ul = NULL;
	mode_t		 oumask;
	struct info_for_hardlink *hl = NULL;
	struct hardlinks hls;
	bool		 root_missing = false;
	int		 max_phase = sess->protocol >= 29 ? 2 : 1;

#ifndef __APPLE__
	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil", NULL) == -1)
		err(ERR_IPC, "pledge");
#endif

	/*
	 * The receiver's metadata phase is actually tracked in the uploader, so
	 * we'll just leave it NULL for now and let the uploader set it.  It
	 * won't be used in anything the receiver calls for now, but it's good
	 * to keep track of it properly anyways.
	 */
	memset(&receiver, 0, sizeof(receiver));
	receiver.append = sess->opts->append;
	receiver.phase = NULL;
	sess->role = &receiver;

	/*
	 * If the root doesn't exist, we may be substituting cwd instead as the
	 * root if we're only transferring a single file.  We won't know until
	 * after the file list is transferred, so we open it up to cwd
	 * proactively.
	 */
	if (stat(root, &st) == -1 && errno == ENOENT) {
		root_missing = true;
#ifndef __APPLE__
		if (unveil(".", "rwc") == -1)
			err(ERR_IPC, ".: unveil");
#endif
	}

#ifndef __APPLE__
	/*
	 * Make our entire view of the file-system be limited to what's
	 * in the root directory.
	 * This prevents us from accidentally (or "under the influence")
	 * writing into other parts of the file-system.
	 */
	if (sess->opts->basedir[0]) {
		/*
		 * XXX just unveil everything for read
		 * Could unveil each basedir or maybe a common path
		 * also the fact that relative path are relative to the
		 * root does not help.
		 */
		if (unveil("/", "r") == -1)
			err(ERR_IPC, "%s: unveil", root);
	}

	if (unveil(root, "rwc") == -1)
		err(ERR_IPC, "%s: unveil", root);

	if (unveil(NULL, NULL) == -1)
		err(ERR_IPC, "unveil");
#endif

	/* Client sends exclusions. */
	if (!sess->opts->server && sess->opts->read_batch == NULL)
		send_rules(sess, fdout);

	/*
	 * Server receives exclusions if delete is on, unless we're deleting
	 * excluded files, too.
	 */
	if (sess->opts->server && sess->opts->del &&
	    (!sess->opts->del_excl || protocol_delrules))
		recv_rules(sess, fdin);

	/*
	 * If we're doing --files-from, we need to do that before we can receive
	 * any files.
	 */
	if (sess->opts->server && sess->opts->filesfrom) {
		read_filesfrom(sess, ".");
		for (i = 0; i < sess->filesfrom_n; i++) {
			length = strlen(sess->filesfrom[i]);
			if (sess->filesfrom[i][length - 1] == '\n') {
				sess->filesfrom[i][length - 1] = '\0';
				length--;
			}
			/* Send the terminating zero, too */
			if (write(fdout, sess->filesfrom[i], length + 1) < 0) {
				ERR("write files-from remote file");
				return 0;
			}
		}
		i = 0;
		if (write(fdout, &i, 1) < 0) {
			ERR("write files-from remote file terminator");
			return 0;
		}
	}

	/*
	 * Start by receiving the file list and our mystery number.
	 * These we're going to be touching on our local system.
	 */

	flist_bytes = sess->total_read;
	if (!flist_recv(sess, fdin, fdout, &fl, &flsz)) {
		ERRX1("flist_recv");
		goto out;
	}

	hl = reallocarray(NULL, flsz, sizeof(*hl));
	if (hl == NULL) {
		ERRX1("reallocarray receiver");
		goto out;
	}
	sess->total_files = flsz;
	sess->flist_size = sess->total_read - flist_bytes;
	hls.n = build_for_hardlinks(hl, fl, flsz); /* Size is same */
	hls.infos = hl;

	/* The IO error is sent after the file list. */

	if (!io_read_int(sess, fdin, &ioerror)) {
		ERRX1("io_read_int");
		goto out;
	} else if (ioerror != 0) {
		LOG2("Got ioerror=%d", ioerror);
		sess->total_errors++;
	}

	if (flsz == 0 && !sess->opts->server) {
		WARNX("receiver has empty file list: exiting");
		rc = 1;
		goto out;
	} else if (!sess->opts->server)
		LOG1("Transfer starting: %zu files", flsz);

	LOG2("%s: receiver destination", root);

	/*
	 * Create the path for our destination directory, if we're not
	 * in dry-run mode (which would otherwise crash w/the pledge).
	 * This uses our current umask: we might set the permissions on
	 * this directory in post_dir().
	 */
	if (!sess->opts->dry_run && flsz > 0) {
		bool implied_dir = false;

		if (flsz > 1)
			implied_dir = true;
		else if (sess->opts->relative && strchr(fl[0].path, '/') != NULL)
			implied_dir = true;
		else if (root[strlen(root) - 1] == '/')
			implied_dir = true;
		else if (sess->opts->filesfrom != NULL)
			implied_dir = true;
		else if (S_ISDIR(fl[0].st.mode))
			implied_dir = true;

		/*
		 * If we're only transferring a single non-directory, then the
		 * root is actually cwd and the destination specified in args is
		 * the filename.
		 *
		 * The receiver doesn't do this if the destination has a
		 * trailing slash to indicate that it's actually a directory.
		 */
		if (!implied_dir && (root_missing || !S_ISDIR(st.st_mode))) {
			char *rpath;
			const char *wpath;

			/*
			 * If we're not in relative mode, we strip the leading
			 * directory part anyways.  If we are in relative mode,
			 * we're not hitting this path unless it's in the
			 * current directory.
			 */
			wpath = strrchr(root, '/');
			if (wpath != NULL) {
				wpath++;

				derived_root = strndup(root, wpath - root);
				if (derived_root == NULL) {
					ERR("strdup");
					rc = 1;
					goto out;
				}

				rpath = strdup(wpath);
				if (rpath == NULL) {
					ERR("strdup");
					free(derived_root);
					rc = 1;
					goto out;
				}

				wpath = rpath;
				root = derived_root;
			} else {
				/* Current directory, just copy. */
				wpath = rpath = strdup(root);
				if (rpath == NULL) {
					ERR("strdup");
					rc = 1;
					goto out;
				}

				root = ".";
			}

			free(fl[0].path);
			fl[0].path = rpath;
			fl[0].wpath = wpath;
		} else {
			if ((tofree = strdup(root)) == NULL)
				err(ERR_NOMEM, NULL);
			if (mkpath(tofree) < 0)
				err(ERR_FILE_IO, "%s: mkpath", tofree);
			free(tofree);
		}
	}


	/*
	 * Disable umask() so we can set permissions fully.
	 * Then open the directory if we're not in dry_run.
	 */

	oumask = umask(0);

	/*
	 * Try opening the root directory.  If we're in dry_run and
	 * fail, just report the error and continue on---don't try to
	 * create the directory.
	 */

#ifdef O_DIRECTORY
	dfd = open(root, O_RDONLY | O_DIRECTORY, 0);
	if (dfd == -1) {
		if (!sess->opts->dry_run && flsz != 0) {
			ERR("%s: open", root);
			goto out;
		} else
			if (!sess->opts->dry_run)
				WARN("%s: open", root);
	}
	if (sess->opts->temp_dir) {
		tfd = open(sess->opts->temp_dir, O_RDONLY | O_DIRECTORY, 0);
		if (dfd == -1) {
			if (!sess->opts->dry_run) {
				ERR("%s: open", sess->opts->temp_dir);
				goto out;
			} else
				if (!sess->opts->dry_run)
					WARN("%s: open", sess->opts->temp_dir);
		}
	}
#else
	if ((dfd = open(root, O_RDONLY, 0)) == -1) {
		if (!sess->opts->dry_run && flsz != 0) {
			ERR("%s: open", root);
			goto out;
		} else
			WARN("%s: open", root);
	} else if (dfd != -1) {
		if (fstat(dfd, &st) == -1) {
			if (!sess->opts->dry_run) {
				ERR("%s: fstat", root);
				goto out;
			} else {
				WARN("%s: fstat", root);
				close(dfd);
				dfd = -1;
			}
		} else if (!S_ISDIR(st.st_mode)) {
			if (!sess->opts->dry_run) {
				ERRX("%s: not a directory", root);
				goto out;
			} else {
				WARN("%s: fstat", root);
				close(dfd);
				dfd = -1;
			}
		}
	}
	if (sess->opts->temp_dir) {
		if ((tfd = open(sess->opts->temp_dir, O_RDONLY, 0)) == -1) {
			if (!sess->opts->dry_run) {
				ERR("%s: open", sess->opts->temp_dir);
				goto out;
			} else
				WARN("%s: open", sess->opts->temp_dir);
		} else if (fstat(tfd, &st) == -1) {
			if (!sess->opts->dry_run) {
				ERR("%s: fstat", sess->opts->temp_dir);
				goto out;
			} else {
				WARN("%s: fstat", sess->opts->temp_dir);
				close(tfd);
				tfd = -1;
			}
		} else if (!S_ISDIR(st.st_mode)) {
			if (!sess->opts->dry_run) {
				ERRX("%s: not a directory",
				    sess->opts->temp_dir);
				goto out;
			} else {
				WARN("%s: fstat", sess->opts->temp_dir);
				close(tfd);
				tfd = -1;
			}
		}
	}
#endif

	/*
	 * Begin by conditionally getting all files we have currently
	 * available in our destination.
	 */
	/* XXX --dirs should also do deletion of dirs whose contents are copied. */
	if (sess->opts->del == DMODE_BEFORE && sess->opts->recursive &&
	    dfd != -1) {
		if (!flist_gen_dels(sess, root, &dfl, &dflsz, fl, flsz)) {
			ERRX1("flist_gen_dels");
			goto out;
		}

		/* If we have a local set, go for the deletion. */
		if (!flist_del(sess, dfd, dfl, dflsz)) {
			ERRX1("flist_del");
			goto out;
		}
	}

	/* Initialise poll events to listen from the sender. */

	pfd[PFD_SENDER_IN].fd = fdin;
	pfd[PFD_UPLOADER_IN].fd = -1;
	pfd[PFD_DOWNLOADER_IN].fd = -1;
	pfd[PFD_SENDER_OUT].fd = fdout;

	pfd[PFD_SENDER_IN].events = POLLIN;
	pfd[PFD_UPLOADER_IN].events = POLLIN;
	pfd[PFD_DOWNLOADER_IN].events = POLLIN;
	pfd[PFD_SENDER_OUT].events = POLLOUT;

	ul = upload_alloc(root, dfd, tfd, fdout, CSUM_LENGTH_PHASE1, fl, flsz,
	    oumask);

	if (ul == NULL) {
		ERRX1("upload_alloc");
		goto out;
	}

	dl = download_alloc(sess, fdin, fl, flsz, dfd, tfd);
	if (dl == NULL) {
		ERRX1("download_alloc");
		goto out;
	}

	cleanup_set_download(cleanup_ctx, dl);

	LOG2("%s: ready for phase 1 data", root);

	for (;;) {
		if ((c = poll(pfd, PFD__MAX, poll_timeout)) == -1) {
			ERR("poll");
			goto out;
		} else if (c == 0) {
			ERRX("poll: timeout");
			goto out;
		}

		for (i = 0; i < PFD__MAX; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX("poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX("poll: hangup");
				goto out;
			}

		/*
		 * If we have a read event and we're multiplexing, we
		 * might just have error messages in the pipe.
		 * It's important to flush these out so that we don't
		 * clog the pipe.
		 * Unset our polling status if there's nothing that
		 * remains in the pipe.
		 */

		if (sess->mplex_reads &&
		    (pfd[PFD_SENDER_IN].revents & POLLIN)) {
			if (!io_read_flush(sess, fdin)) {
				ERRX1("io_read_flush");
				goto out;
			} else if (sess->mplex_read_remain == 0)
				pfd[PFD_SENDER_IN].revents &= ~POLLIN;
		}


		/*
		 * We run the uploader if we have files left to examine
		 * (i < flsz) or if we have a file that we've opened and
		 * is read to mmap.
		 */

		if ((pfd[PFD_UPLOADER_IN].revents & POLLIN) ||
		    (pfd[PFD_SENDER_OUT].revents & POLLOUT)) {
			c = rsync_uploader(ul,
				&pfd[PFD_UPLOADER_IN].fd,
				sess, &pfd[PFD_SENDER_OUT].fd, &hls);
			if (c < 0) {
				ERRX1("rsync_uploader");
				goto out;
			}
		}

		/*
		 * We need to run the downloader when we either have
		 * read events from the sender or an asynchronous local
		 * open is ready.
		 * XXX: we don't disable PFD_SENDER_IN like with the
		 * uploader because we might stop getting error
		 * messages, which will otherwise clog up the pipes.
		 */

		if ((pfd[PFD_SENDER_IN].revents & POLLIN) ||
		    (pfd[PFD_DOWNLOADER_IN].revents & POLLIN)) {
			c = rsync_downloader(dl, sess,
				&pfd[PFD_DOWNLOADER_IN].fd, flsz, &hls);
			if (c < 0) {
				ERRX1("rsync_downloader");
				goto out;
			} else if (c == 0) {
				assert(phase >= 0 && phase <= max_phase);

				/*
				 * Process any delayed updates.
				 * For protocol 29 we handle these in a later phase.
				 */
				if (!protocol_dlrename ||
				    phase == PHASE_DLUPDATES) {
					delayed_renames(sess);
					free(sess->dlrename);
					sess->dlrename = NULL;
				}

				/*
				 * Downloader finished the last of this phase,
				 * so finish up the tail end of acks.
				 */
				upload_ack_complete(ul, sess, fdout);
				phase++;
				if (phase == max_phase + 1)
					break;

				LOG2("%s: receiver ready for phase %d data (%zu to redo)",
				    root, phase + 1, download_needs_redo(dl));

				sess->role->append = 0;

				/*
				 * Signal the uploader to start over, and
				 * re-enable polling.
				 */
				upload_next_phase(ul, sess, fdout);
				pfd[PFD_SENDER_OUT].fd = fdout;
				continue;
			}
		}
	}

	assert(phase == max_phase + 1);

	/*
	 * Following transfers, we'll take care of --delete-after.
	 */
	/* XXX --dirs should also do deletion of dirs whose contents are copied. */
	if (sess->opts->del == DMODE_AFTER && sess->opts->recursive &&
	    dfd != -1) {
		if (!flist_gen_dels(sess, root, &dfl, &dflsz, fl, flsz)) {
				ERRX1("flist_gen_dels");
				goto out;
		}

		/* If we have a local set, go for the deletion. */
		if (!flist_del(sess, dfd, dfl, dflsz)) {
			ERRX1("flist_del");
			goto out;
		}
	} else if (sess->opts->del == DMODE_DELAY) {
		if (!upload_del(ul, sess)) {
			ERRX1("upload_del");
			goto out;
		}
	}

	/*
	 * Now all of our transfers are complete, so we can fix up our
	 * directory permissions.
	 */

	if (!rsync_uploader_tail(ul, sess)) {
		ERRX1("rsync_uploader_tail");
		goto out;
	}

	/* Process server statistics and say good-bye. */

	if (!sess_stats_recv(sess, fdin)) {
		ERRX1("sess_stats_recv");
		goto out;
	}
	if (sess->opts->read_batch == NULL && !io_write_int(sess, fdout, -1)) {
		ERRX1("io_write_int");
		goto out;
	}

	LOG2("receiver finished updating");
	rc = 1;
out:
	free(derived_root);
	delayed_renames(sess);
	free(sess->dlrename);
	sess->dlrename = NULL;
	free(hl);
	hl = NULL;
	upload_free(ul);

	/*
	 * If we get signalled, we'll need to also free the download from that
	 * context.  Side step potential issues by just declaring a cleanup
	 * hold, which will just block the signals we try to handle cleanly for
	 * this critical section.
	 */
	cleanup_hold(cleanup_ctx);
	download_free(sess, dl);
	cleanup_set_download(cleanup_ctx, NULL);
	cleanup_release(cleanup_ctx);

	if (dfd != -1)
		close(dfd);
	if (tfd != -1)
		close(tfd);

	flist_free(fl, flsz);
	flist_free(dfl, dflsz);

	return rc;
}
