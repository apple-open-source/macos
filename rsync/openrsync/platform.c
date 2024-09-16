/*
 * Platform hooks to more cleanly provide some functionality not otherwise
 * applicable to openrsync at-large.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include "extern.h"

#ifdef __APPLE__
#include <assert.h>
#include <copyfile.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define	FLSTAT_PLATFORM_XATTR			FLSTAT_PLATFORM_BIT1
#define	FLSTAT_PLATFORM_CLEAR_XATTR		FLSTAT_PLATFORM_BIT2
#define	FLSTAT_PLATFORM_UNLINKED		FLSTAT_PLATFORM_BIT3

static int
apple_open_xattrs(const struct sess *sess, const struct flist *f, int oflags)
{
	char *path, tmpfile[PATH_MAX];
	size_t idx;
	int copyflags, fd, error, serrno;

	assert(strstr(f->path, "._") != NULL);

	copyflags = COPYFILE_PACK | COPYFILE_ACL | COPYFILE_XATTR;
	if (sess->opts->preserve_links)
		copyflags |= COPYFILE_NOFOLLOW;

	/* Chop off the ._ */
	if (asprintf(&path, "%s/%s", dirname(f->path),
	    basename(f->path) + 2) == -1) {
		ERR("asprintf");
		return -1;
	}

	idx = snprintf(tmpfile, sizeof(tmpfile), "%s.%d.", _PATH_TMP, getpid());

	/* We need to synthesize the xattrs. */
	for (const char *c = f->wpath; *c != '\0' && idx < sizeof(tmpfile) - 1;
	    c++) {
		if (*c == '/')
			tmpfile[idx++] = '_';
		else
			tmpfile[idx++] = *c;
	}

	tmpfile[idx++] = '\0';

	error = copyfile(path, tmpfile, NULL, copyflags);
	serrno = errno;
	free(path);

	if (error != 0) {
		errno = serrno;
		ERR("copyfile");
		return -1;
	}

	fd = open(tmpfile, oflags);
	serrno = errno;
	unlink(tmpfile);

	if (fd == -1)
		errno = serrno;

	return fd;
}

static int
apple_flist_sent(struct sess *sess, int fdout, const struct flist *f)
{
	char send;

	assert(sess->opts->extended_attributes);

	if ((f->st.flags & FLSTAT_PLATFORM_XATTR) != 0)
		send = 1;
	else
		send = 0;

	if (!io_write_byte(sess, fdout, send)) {
		ERRX("io_write_byte");
		return 0;
	}

	return 1;
}

#define	HAVE_PLATFORM_FLIST_MODIFY	1
int
platform_flist_modify(const struct sess *sess, struct fl *fl)
{
	struct flist *f;
	size_t insz;
	int copyflags;

	if (!sess->opts->extended_attributes)
		return 1;

	copyflags = COPYFILE_CHECK | COPYFILE_ACL | COPYFILE_XATTR;
	if (sess->opts->preserve_links)
		copyflags |= COPYFILE_NOFOLLOW;
	insz = fl->sz;
	for (size_t i = 0; i < insz; i++) {
		struct flist *packed;
		const char *base;
		char *ppath;
		ptrdiff_t stripdir;

		f = &fl->flp[i];
		base = strrchr(f->path, '/');
		if (base == NULL)
			base = f->path;
		else
			base++;

		if (strncmp(base, "._", 2) == 0)
			goto hooksent;

		if (copyfile(f->path, NULL, 0, copyflags) == 0)
			goto hooksent;

		stripdir = f->wpath - f->path;

		/* Setup the different bits */
		if (asprintf(&ppath, "%.*s._%s",
		    (int)(base - f->path), f->path, basename(f->path)) == -1) {
			ERR("asprintf --extended-attributes path");
			return 0;
		}

		packed = fl_new(fl);
		memcpy(packed, f, sizeof(*f));

		packed->path = ppath;
		packed->wpath = ppath + stripdir;
		packed->link = NULL;
		packed->open = apple_open_xattrs;
		packed->sent = apple_flist_sent;
		f->st.flags |= FLSTAT_PLATFORM_XATTR;

hooksent:
		f->sent = apple_flist_sent;
	}

	return 1;
}

#define	HAVE_PLATFORM_FLIST_RECEIVED	1
void
platform_flist_received(struct sess *sess, struct flist *fl, size_t flsz)
{
	struct flist temp;

	if (!sess->opts->extended_attributes)
		return;

	/*
	 * With extended attributes, we synthesize an AppleDouble file (._foo)
	 * and transmit that alongside the base file that it applies to.  In
	 * order for this to work out cleanly, we need to be sure that we're
	 * requesting the base file before the AppleDouble file so that we can
	 * just pack the AppleDouble file back in when it's done.  If we don't
	 * do it here, it requires searching both when a file we know has
	 * some extended attributes finishes, and also when the AppleDouble file
	 * finishes in case some other reordering has occurred.
	 */
	for (size_t i = 0; i < flsz - 1; i++) {
		struct flist *search;
		const char *search_base;
		size_t search_prefix;

		search = &fl[i];

		if (!S_ISREG(search->st.mode))
			continue;

		search_base = strrchr(search->path, '/');
		if (search_base == NULL)
			search_base = search->path;
		else
			search_base++;

		if (strncmp(search_base, "._", 2) != 0)
			continue;

		/*
		 * Track the parts we need to match; the leading directory bits,
		 * and the trailing filename.
		 */
		search_prefix = search_base - search->path;
		search_base += 2;

		for (size_t j = i + 1; j < flsz; j++) {
			struct flist *next;

			next = &fl[j];

			/*
			 * It's already sorted in such a way that we shouldn't
			 * have to worry about missing anything if we stop
			 * searching as soon as we move out of the search
			 * prefix.
			 */
			if (strncmp(next->path, search->path, search_prefix) != 0)
				break;

			if (strcmp(next->path + search_prefix, search_base) != 0)
				continue;

			/* Swap */
			temp = *next;
			*next = *search;
			*search = temp;

			break;
		}
	}
}

#define	HAVE_PLATFORM_FLIST_ENTRY_RECEIVED	1
int
platform_flist_entry_received(struct sess *sess, int fdin, struct flist *f)
{
	uint8_t xattr;

	if (!sess->opts->extended_attributes)
		return 1;

	if (!io_read_byte(sess, fdin, &xattr)) {
		ERRX1("io_read_byte");
		return 0;
	}

	if (xattr)
		f->st.flags |= FLSTAT_PLATFORM_XATTR;
	else
		f->st.flags |= FLSTAT_PLATFORM_CLEAR_XATTR;
	return 1;
}

static int
apple_merge_appledouble(const struct sess *sess, struct flist *f,
    int fromfd, const char *fname, int tofd, const char *toname)
{
	char newname[PATH_MAX];
	const char *base;
	int error, ffd, tfd, serrno;

	/*
	 * We'll redo our base name calculation just to preserve the parts of
	 * toname so that we can more easily reconstruct it into `newname`.
	 */
	base = strrchr(toname, '/');
	if (base == NULL)
		base = toname;
	else
		base++;

	/* Chop off the leading ._ to get the unpacked name */
	if (snprintf(newname, sizeof(newname), "%.*s%s", (int)(base - toname),
	    toname, base + 2) == -1) {
		ERR("snprintf");
		return 0;
	}

	ffd = tfd = -1;
	ffd = openat(fromfd, fname, O_RDONLY);
	if (ffd == -1) {
		ERR("%s: openat", fname);
		return 0;
	}

	tfd = openat(tofd, newname,
	    O_WRONLY | (sess->opts->preserve_links ? O_NOFOLLOW : 0));
	if (tfd == -1) {
		ERR("%s: openat", newname);
		close(ffd);
		return 0;
	}

	error = fcopyfile(ffd, tfd, NULL,
	    COPYFILE_UNPACK | COPYFILE_ACL | COPYFILE_XATTR);
	serrno = errno;

	close(tfd);
	close(ffd);

	/*
	 * We won't make failing to unlink the AppleDouble file fatal to the
	 * operation if the unpack succeeded.
	 */
	if (error != 0) {
		ERRX1("%s: copyfile extended attributes from %s: %s",
		    newname, fname, strerror(serrno));
		return 0;
	}

	if (unlinkat(fromfd, fname, 0) != 0)
		ERRX1("%s: unlink: %s", fname, strerror(errno));

	f->st.flags |= FLSTAT_PLATFORM_UNLINKED;

	return 1;
}

#define	HAVE_PLATFORM_MOVE_FILE	1
int
platform_move_file(const struct sess *sess, struct flist *fl,
    int fromfd, const char *fname, int tofd, const char *toname, int final)
{

	if (final && sess->opts->extended_attributes) {
		const char *base;

		base = basename((void *)toname);
		if (strncmp(base, "._", 2) == 0) {
			/* We won't move this, we'll just unpack it. */
			return apple_merge_appledouble(sess, fl, fromfd, fname,
			    tofd, toname);
		}
	}

	if (move_file(fromfd, fname, tofd, toname, final) != 0) {
		ERR("%s: move_file: %s", fname, toname);
		return 0;
	}

	return 1;
}

#define	HAVE_PLATFORM_FINISH_TRANSFER 1
int
platform_finish_transfer(const struct sess *sess, struct flist *fl,
    int rootfd, const char *name)
{
	const char *base;

	/*
	 * Here we'll handle the case of an --inplace transfer being finished.
	 */
	if (!sess->opts->extended_attributes || sess->opts->dlupdates)
		return 1;
	if ((fl->st.flags & FLSTAT_PLATFORM_UNLINKED) != 0)
		return 1;

	base = basename((void *)name);
	if (strncmp(base, "._", 2) != 0) {
#ifdef NOTYET
		/*
		 * Not yet clear: optimal approach to clearing extended
		 * attributes on a file.  copyfile() from /dev/null -> path
		 * works to clear them, but trying to plumb the root path
		 * through adds some complexity.  There is no such thing as
		 * copyfileat() that takes dirfd / path pairs, and we can't take
		 * the approach we do to merge the AppleDouble file in because
		 * fcopyfile() is implemented in userspace and won't accept
		 * /dev/null at the moment.
		 */
		if ((fl->st.flags & FLSTAT_PLATFORM_CLEAR_XATTR) != 0) {
			/* XXX Clear xattrs / acls */
		}
#endif

		return 1;
	}

	/* Merge xattrs in. */
	return apple_merge_appledouble(sess, fl, rootfd, name, rootfd, name);
}
#endif /* __APPLE__ */

#if !HAVE_PLATFORM_FLIST_MODIFY
int
platform_flist_modify(const struct sess *sess, struct fl *fl)
{

	return 1;
}
#endif

#if !HAVE_PLATFORM_FLIST_RECEIVED
void
platform_flist_received(struct sess *sess, struct flist *fl, size_t flsz)
{

}
#endif

#if !HAVE_PLATFORM_FLIST_ENTRY_RECEIVED
int
platform_flist_entry_received(struct sess *sess, int fdin, struct flist *f)
{

	return 1;
}
#endif

#if !HAVE_PLATFORM_MOVE_FILE
int
platform_move_file(const struct sess *sess, struct flist *fl,
    int fromfd, const char *fname, int tofd, const char *toname, int final)
{

	if (move_file(fromfd, fname, tofd, toname, final) != 0) {
		ERR("%s: move_file: %s", fname, toname);
		return 0;
	}

	return 1;
}
#endif

#if !HAVE_PLATFORM_FINISH_TRANSFER
int
platform_finish_transfer(const struct sess *sess, struct flist *fl,
    int rootfd, const char *name)
{

	return 1;
}
#endif
