/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <sys/syscall.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/acl.h>
#include <libkern/OSByteOrder.h>
#include <membership.h>
#include <fts.h>
#include <libgen.h>

#include <TargetConditionals.h>
#if !TARGET_OS_EMBEDDED
#include <quarantine.h>

#define	XATTR_QUARANTINE_NAME qtn_xattr_name
#else /* TARGET_OS_EMBEDDED */
#define qtn_file_t void *
#define QTN_SERIALIZED_DATA_MAX 0
static void * qtn_file_alloc(void) { return NULL; }
static int qtn_file_init_with_fd(void *x, int y) { return -1; }
static void qtn_file_free(void *x) { return; }
static int qtn_file_apply_to_fd(void *x, int y) { return -1; }
static char *qtn_error(int x) { return NULL; }
static int qtn_file_to_data(void *x, char *y, size_t z) { return -1; }
static void *qtn_file_clone(void *x) { return NULL; }
#define	XATTR_QUARANTINE_NAME "figgledidiggledy"
#endif /* TARGET_OS_EMBEDDED */

#include "copyfile.h"

enum cfInternalFlags {
	cfDelayAce = 1,
};

/*
 * The state structure keeps track of
 * the source filename, the destination filename, their
 * associated file-descriptors, the stat infomration for the
 * source file, the security information for the source file,
 * the flags passed in for the copy, a pointer to place statistics
 * (not currently implemented), debug flags, and a pointer to callbacks
 * (not currently implemented).
 */
struct _copyfile_state
{
    char *src;
    char *dst;
    int src_fd;
    int dst_fd;
    struct stat sb;
    filesec_t fsec;
    copyfile_flags_t flags;
    unsigned int	internal_flags;
    void *stats;
    uint32_t debug;
    copyfile_callback_t	statuscb;
    void	*ctx;
    qtn_file_t qinfo;	/* Quarantine information -- probably NULL */
    filesec_t original_fsec;
    filesec_t permissive_fsec;
    off_t totalCopied;
    int err;
};

struct acl_entry {
        u_int32_t       ae_magic;
#define _ACL_ENTRY_MAGIC        0xac1ac101
        u_int32_t       ae_tag;
        guid_t          ae_applicable;
        u_int32_t       ae_flags;
        u_int32_t       ae_perms;
};

#define PACE(ace)  do { \
	struct acl_entry *__t = (struct acl_entry*)(ace); \
	fprintf(stderr, "%s(%d): " #ace " = { flags = %#x, perms = %#x }\n", __FUNCTION__, __LINE__, __t->ae_flags, __t->ae_perms); \
	} while (0)

#define PACL(ace) \
	do { \
		ssize_t __l; char *__cp = acl_to_text(ace, &__l); \
		fprintf(stderr, "%s(%d):  " #ace " = %s\n", __FUNCTION__, __LINE__, __cp ? __cp : "(null)"); \
	} while (0)

static int
acl_compare_permset_np(acl_permset_t p1, acl_permset_t p2)
{
	struct pm { u_int32_t ap_perms; } *ps1, *ps2;
	ps1 = (struct pm*) p1;
	ps2 = (struct pm*) p2;

    return ((ps1->ap_perms == ps2->ap_perms) ? 1 : 0);
}

/*
 * Internally, the process is broken into a series of
 * private functions.
 */
static int copyfile_open	(copyfile_state_t);
static int copyfile_close	(copyfile_state_t);
static int copyfile_data	(copyfile_state_t);
static int copyfile_stat	(copyfile_state_t);
static int copyfile_security	(copyfile_state_t);
static int copyfile_xattr	(copyfile_state_t);
static int copyfile_pack	(copyfile_state_t);
static int copyfile_unpack	(copyfile_state_t);

static copyfile_flags_t copyfile_check	(copyfile_state_t);
static filesec_t copyfile_fix_perms(copyfile_state_t, filesec_t *);
static int copyfile_preamble(copyfile_state_t *s, copyfile_flags_t flags);
static int copyfile_internal(copyfile_state_t state, copyfile_flags_t flags);
static int copyfile_unset_posix_fsec(filesec_t);
static int copyfile_quarantine(copyfile_state_t);

#define COPYFILE_DEBUG (1<<31)
#define COPYFILE_DEBUG_VAR "COPYFILE_DEBUG"

#ifndef _COPYFILE_TEST
# define copyfile_warn(str, ...) syslog(LOG_WARNING, str ": %m", ## __VA_ARGS__)
# define copyfile_debug(d, str, ...) \
   do { \
    if (s && (d <= s->debug)) {\
	syslog(LOG_DEBUG, "%s:%d:%s() " str "\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__); \
    } \
   } while (0)
#else
#define copyfile_warn(str, ...) \
    fprintf(stderr, "%s:%d:%s() " str ": %s\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__, (errno) ? strerror(errno) : "")
# define copyfile_debug(d, str, ...) \
    do { \
    if (s && (d <= s->debug)) {\
	fprintf(stderr, "%s:%d:%s() " str "\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__); \
    } \
    } while(0)
#endif

static int copyfile_quarantine(copyfile_state_t s)
{
    int rv = 0;
    if (s->qinfo == NULL)
    {
	int error;
	s->qinfo = qtn_file_alloc();
	if (s->qinfo == NULL)
	{
	    rv = -1;
	    goto done;
	}
	if ((error = qtn_file_init_with_fd(s->qinfo, s->src_fd)) != 0)
	{
	    qtn_file_free(s->qinfo);
	    s->qinfo = NULL;
	    rv = -1;
	    goto done;
	}
    }
done:
    return rv;
}

static int
add_uberace(acl_t *acl)
{
	acl_entry_t entry;
	acl_permset_t permset;
	uuid_t qual;

	if (mbr_uid_to_uuid(getuid(), qual) != 0)
		goto error_exit;

	/*
	 * First, we create an entry, and give it the special name
	 * of ACL_FIRST_ENTRY, thus guaranteeing it will be first.
	 * After that, we clear out all the permissions in it, and
	 * add three permissions:  WRITE_DATA, WRITE_ATTRIBUTES, and
	 * WRITE_EXTATTRIBUTES.  We put these into an ACE that allows
	 * the functionality, and put this into the ACL.
	 */
	if (acl_create_entry_np(acl, &entry, ACL_FIRST_ENTRY) == -1)
		goto error_exit;
	if (acl_get_permset(entry, &permset) == -1)
		goto error_exit;
	if (acl_clear_perms(permset) == -1)
		goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_DATA) == -1)
		goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_ATTRIBUTES) == -1)
		goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_EXTATTRIBUTES) == -1)
		goto error_exit;
	if (acl_add_perm(permset, ACL_APPEND_DATA) == -1)
		goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_SECURITY) == -1)
		goto error_exit;
	if (acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == -1)
		goto error_exit;

	if(acl_set_permset(entry, permset) == -1)
		goto error_exit;
	if(acl_set_qualifier(entry, qual) == -1)
		goto error_exit;

	return 0;
error_exit:
	return -1;
}

static int
is_uberace(acl_entry_t ace)
{
	int retval = 0;
	acl_permset_t perms, tperms;
	acl_t tacl;
	acl_entry_t tentry;
	acl_tag_t tag;
	guid_t *qual;
	uuid_t myuuid;

	// Who am I, and who is the ACE for?
	mbr_uid_to_uuid(geteuid(), myuuid);
	qual = (guid_t*)acl_get_qualifier(ace);

	// Need to create a temporary acl, so I can get the uberace template.
	tacl = acl_init(1);
	if (tacl == NULL) {
		goto done;
	}
	add_uberace(&tacl);
	if (acl_get_entry(tacl, ACL_FIRST_ENTRY, &tentry) != 0) {
		goto done;
	}
	acl_get_permset(tentry, &tperms);

	// Now I need to get
	acl_get_tag_type(ace, &tag);
	acl_get_permset(ace, &perms);

	if (tag == ACL_EXTENDED_ALLOW &&
		(memcmp(qual, myuuid, sizeof(myuuid)) == 0) &&
		acl_compare_permset_np(tperms, perms))
		retval = 1;

done:

	if (tacl)
		acl_free(tacl);

	return retval;
}

static void
remove_uberace(int fd, struct stat *sbuf)
{
	filesec_t fsec = NULL;
	acl_t acl = NULL;
	acl_entry_t entry;
	struct stat sb;

	fsec = filesec_init();
	if (fsec == NULL) {
		goto noacl;
	}

	if (fstatx_np(fd, &sb, fsec) != 0) {
		if (errno == ENOTSUP)
			goto noacl;
		goto done;
	}

	if (filesec_get_property(fsec, FILESEC_ACL, &acl) != 0) {
		goto done;
	}

	if (acl_get_entry(acl, ACL_FIRST_ENTRY, &entry) == 0) {
		if (is_uberace(entry))
		{
			mode_t m = sbuf->st_mode & ~S_IFMT;

			if (acl_delete_entry(acl, entry) != 0 ||
				filesec_set_property(fsec, FILESEC_ACL, &acl) != 0 ||
				filesec_set_property(fsec, FILESEC_MODE, &m) != 0 ||
				fchmodx_np(fd, fsec) != 0)
				goto noacl;
		}
	}

done:
	if (acl)
		acl_free(acl);
	if (fsec)
		filesec_free(fsec);
	return;

noacl:
	fchmod(fd, sbuf->st_mode & ~S_IFMT);
	goto done;
}

static void
reset_security(copyfile_state_t s)
{
    /* If we haven't reset the file security information
     * (COPYFILE_SECURITY is not set in flags)
     * restore back the permissions the file had originally
     *
     * One of the reasons this seems so complicated is that
     * it is partially at odds with copyfile_security().
     *
     * Simplisticly, we are simply trying to make sure we
     * only copy what was requested, and that we don't stomp
     * on what wasn't requested.
     */

#ifdef COPYFILE_RECURSIVE
	if (s->dst_fd > -1) {
		struct stat sbuf;

		if (s->src_fd > -1 && (s->flags & COPYFILE_STAT))
			fstat(s->src_fd, &sbuf);
		else
			fstat(s->dst_fd, &sbuf);
		
		if (!(s->internal_flags & cfDelayAce))
			remove_uberace(s->dst_fd, &sbuf);
	}
#else
    if (s->permissive_fsec && (s->flags & COPYFILE_SECURITY) != COPYFILE_SECURITY) {
	if (s->flags & COPYFILE_ACL) {
		/* Just need to reset the BSD information -- mode, owner, group */
		(void)fchown(s->dst_fd, s->dst_sb.st_uid, s->dst_sb.st_gid);
		(void)fchmod(s->dst_fd, s->dst_sb.st_mode);
	} else {
		/*
		 * flags is either COPYFILE_STAT, or neither; if it's
		 * neither, then we restore both ACL and POSIX permissions;
		 * if it's STAT, however, then we only want to restore the
		 * ACL (which may be empty).  We do that by removing the
		 * POSIX information from the filesec object.
		 */
		if (s->flags & COPYFILE_STAT) {
			copyfile_unset_posix_fsec(s->original_fsec);
		}
		if (fchmodx_np(s->dst_fd, s->original_fsec) < 0 && errno != ENOTSUP)
		    copyfile_warn("restoring security information");
	}
    }

    if (s->permissive_fsec) {
	filesec_free(s->permissive_fsec);
	s->permissive_fsec = NULL;
    }

    if (s->original_fsec) {
	filesec_free(s->original_fsec);
	s->original_fsec = NULL;
    }
#endif

    return;
}

/*
 * copytree -- recursively copy a hierarchy.
 *
 * Unlike normal copyfile(), copytree() can copy an entire hierarchy.
 * Care is taken to keep the ACLs set up correctly, in addition to the
 * normal copying that is done.  (When copying a hierarchy, we can't
 * get rid of the "allow-all-writes" ACE on a directory until we're done
 * copying the *contents* of the directory.)
 *
 * The other big difference from copyfile (for the moment) is that copytree()
 * will use a call-back function to pass along information about what is
 * about to be copied, and whether or not it succeeded.
 *
 * copytree() is called from copyfile() -- but copytree() itself then calls
 * copyfile() to copy each individual object.
 *
 * XXX - no effort is made to handle overlapping hierarchies at the moment.
 *
 */

static int
copytree(copyfile_state_t s)
{
	char *slash;
	int retval = 0;
	int (*sfunc)(const char *, struct stat *);
	copyfile_callback_t status = NULL;
	char srcisdir = 0, dstisdir = 0, dstexists = 0;
	struct stat sbuf;
	char *src, *dst;
	const char *dstpathsep = "";
#ifdef NOTYET
	char srcpath[PATH_MAX * 2 + 1], dstpath[PATH_MAX * 2 + 1];
#endif
	char *srcroot;
	FTS *fts = NULL;
	FTSENT *ftsent;
	ssize_t offset = 0;
	const char *paths[2] =  { 0 };
	unsigned int flags = 0;
	int fts_flags = FTS_NOCHDIR;

	if (s == NULL) {
		errno = EINVAL;
		retval = -1;
		goto done;
	}
	if (s->flags & (COPYFILE_MOVE | COPYFILE_UNLINK | COPYFILE_CHECK | COPYFILE_PACK | COPYFILE_UNPACK)) {
		errno = EINVAL;
		retval = -1;
		goto done;
	}

	flags = s->flags & (COPYFILE_ALL | COPYFILE_NOFOLLOW | COPYFILE_VERBOSE);

	paths[0] = src = s->src;
	dst = s->dst;

	if (src == NULL || dst == NULL) {
		errno = EINVAL;
		retval = -1;
		goto done;
	}

	sfunc = (flags & COPYFILE_NOFOLLOW_SRC) ? lstat : stat;
	if ((sfunc)(src, &sbuf) == -1) {
		retval = -1;
		goto done;
	}
	if (sbuf.st_mode & S_IFDIR) {
		srcisdir = 1;
	}

	sfunc = (flags & COPYFILE_NOFOLLOW_DST) ? lstat : stat;
	if ((sfunc)(dst, &sbuf) == -1) {
		if (errno != ENOENT) {
			retval = -1;
			goto done;
		}
	} else {
		dstexists = 1;
		if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
			dstisdir = 1;
		}
	}

#ifdef NOTYET
	// This doesn't handle filesystem crossing and case sensitivity
	// So there's got to be a better way

	if (realpath(src, srcpath) == NULL) {
		retval = -1;
		goto done;
	}

	if (realpath(dst, dstpath) == NULL &&
		(errno == ENOENT && realpath(dirname(dst), dstpath) == NULL)) {
		retval = -1;
		goto done;
	}
	if (strstr(srcpath, dstpath) != NULL) {
		errno = EINVAL;
		retval = -1;
		goto done;
	}
#endif
	srcroot = basename((char*)src);
	if (srcroot == NULL) {
		retval = -1;
		goto done;
	}

	/*
	 * To work on as well:
	 * We have a few cases when copying a hierarchy:
	 * 1)  src is a non-directory, dst is a directory;
	 * 2)  src is a non-directory, dst is a non-directory;
	 * 3)  src is a non-directory, dst does not exist;
	 * 4)  src is a directory, dst is a directory;
	 * 5)  src is a directory, dst is a non-directory;
	 * 6)  src is a directory, dst does not exist
	 *
	 * (1) copies src to dst/basename(src).
	 * (2) fails if COPYFILE_EXCLUSIVE is set, otherwise copies src to dst.
	 * (3) and (6) copy src to the name dst.
	 * (4) copies the contents of src to the contents of dst.
	 * (5) is an error.
	 */

	if (dstisdir) {
		// copy /path/to/src to /path/to/dst/src
		// Append "/" and (fts_path - strlen(basename(src))) to dst?
		dstpathsep = "/";
		slash = strrchr(src, '/');
		if (slash == NULL)
			offset = 0;
		else
			offset = slash - src + 1;
	} else {
		// copy /path/to/src to /path/to/dst
		// append (fts_path + strlen(src)) to dst?
		dstpathsep = "";
		offset = strlen(src);
	}

	if (s->flags | COPYFILE_NOFOLLOW_SRC)
		fts_flags |= FTS_PHYSICAL;
	else
		fts_flags |= FTS_LOGICAL;

	fts = fts_open((char * const *)paths, fts_flags, NULL);

	status = s->statuscb;
	while ((ftsent = fts_read(fts)) != NULL) {
		int rv = 0;
		char *dstfile = NULL;
		int cmd = 0;
		copyfile_state_t tstate = copyfile_state_alloc();
		if (tstate == NULL) {
			errno = ENOMEM;
			retval = -1;
			break;
		}
		tstate->statuscb = s->statuscb;
		tstate->ctx = s->ctx;
		asprintf(&dstfile, "%s%s%s", dst, dstpathsep, ftsent->fts_path + offset);
		if (dstfile == NULL) {
			copyfile_state_free(tstate);
			errno = ENOMEM;
			retval = -1;
			break;
		}
		switch (ftsent->fts_info) {
		case FTS_D:
			tstate->internal_flags |= cfDelayAce;
			cmd = COPYFILE_RECURSE_DIR;
			break;
		case FTS_SL:
		case FTS_SLNONE:
		case FTS_DEFAULT:
		case FTS_F:
			cmd = COPYFILE_RECURSE_FILE;
			break;
		case FTS_DP:
			cmd = COPYFILE_RECURSE_DIR_CLEANUP;
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
		case FTS_NSOK:
		default:
			errno = ftsent->fts_errno;
			if (status) {
				rv = (*status)(COPYFILE_RECURSE_ERROR, COPYFILE_ERR, tstate, ftsent->fts_path, dstfile, s->ctx);
				if (rv == COPYFILE_SKIP || rv == COPYFILE_CONTINUE) {
					errno = 0;
					goto skipit;
				}
				if (rv == COPYFILE_QUIT) {
					retval = -1;
					goto stopit;
				}
			} else {
				retval = -1;
				goto stopit;
			}
		case FTS_DOT:
			goto skipit;

		}

		if (cmd == COPYFILE_RECURSE_DIR || cmd == COPYFILE_RECURSE_FILE) {
			if (status) {
				rv = (*status)(cmd, COPYFILE_START, tstate, ftsent->fts_path, dstfile, s->ctx);
				if (rv == COPYFILE_SKIP) {
					if (cmd == COPYFILE_RECURSE_DIR) {
						rv = fts_set(fts, ftsent, FTS_SKIP);
						if (rv == -1) {
							rv = (*status)(0, COPYFILE_ERR, tstate, ftsent->fts_path, dstfile, s->ctx);
							if (rv == COPYFILE_QUIT)
								retval = -1;
						}
					}
					goto skipit;
				}
				if (rv == COPYFILE_QUIT) {
					retval = -1; errno = 0;
					goto stopit;
				}
			}
			rv = copyfile(ftsent->fts_path, dstfile, tstate, flags);
			if (rv < 0) {
				if (status) {
					rv = (*status)(cmd, COPYFILE_ERR, tstate, ftsent->fts_path, dstfile, s->ctx);
					if (rv == COPYFILE_QUIT) {
						retval = -1;
						goto stopit;
					} else
						rv = 0;
					goto skipit;
				} else {
					retval = -1;
					goto stopit;
				}
			}
			if (status) {
				rv = (*status)(cmd, COPYFILE_FINISH, tstate, ftsent->fts_path, dstfile, s->ctx);
				if (rv == COPYFILE_QUIT) {
					retval = -1; errno = 0;
					goto stopit;
				}
			}
		} else if (cmd == COPYFILE_RECURSE_DIR_CLEANUP) {
			int tfd;

			if (status) {
				rv = (*status)(cmd, COPYFILE_START, tstate, ftsent->fts_path, dstfile, s->ctx);
				if (rv == COPYFILE_QUIT) {
					retval = -1; errno = 0;
					goto stopit;
				} else if (rv == COPYFILE_SKIP) {
					rv = 0;
					goto skipit;
				}
			}
			tfd = open(dstfile,  O_RDONLY);
			if (tfd != -1) {
				struct stat sb;
				if (s->flags & COPYFILE_STAT) {
					(s->flags & COPYFILE_NOFOLLOW_SRC ? lstat : stat)(ftsent->fts_path, &sb);
				} else {
					(s->flags & COPYFILE_NOFOLLOW_DST ? lstat : stat)(dstfile, &sb);
				}
				remove_uberace(tfd, &sb);
				close(tfd);
				if (status) {
					rv = (*status)(COPYFILE_RECURSE_DIR_CLEANUP, COPYFILE_FINISH, tstate, ftsent->fts_path, dstfile, s->ctx);
					if (rv == COPYFILE_QUIT) {
						rv = -1; errno = 0;
						goto stopit;
					}
				}
			} else {
				if (status) {
					rv = (*status)(COPYFILE_RECURSE_DIR_CLEANUP, COPYFILE_ERR, tstate, ftsent->fts_path, dstfile, s->ctx);
					if (rv == COPYFILE_QUIT) {
						retval = -1;
						goto stopit;
					} else if (rv == COPYFILE_SKIP || rv == COPYFILE_CONTINUE) {
						if (rv == COPYFILE_CONTINUE)
							errno = 0;
						retval = 0;
						goto skipit;
					}
				} else {
					retval = -1;
					goto stopit;
				}
			}
			rv = 0;
		}
skipit:
stopit:
		copyfile_state_free(tstate);
		free(dstfile);
		if (retval == -1)
			break;
	}

done:
	if (fts)
		fts_close(fts);

	return retval;
}

/*
 * fcopyfile() is used to copy a source file descriptor to a destination file
 * descriptor.  This allows an application to figure out how it wants to open
 * the files (doing various security checks, perhaps), and then just pass in
 * the file descriptors.
 */
int fcopyfile(int src_fd, int dst_fd, copyfile_state_t state, copyfile_flags_t flags)
{
    int ret = 0;
    copyfile_state_t s = state;
    struct stat dst_sb;

    if (src_fd < 0 || dst_fd < 0)
    {
	errno = EINVAL;
	return -1;
    }

    if (copyfile_preamble(&s, flags) < 0)
	return -1;

    copyfile_debug(2, "set src_fd <- %d", src_fd);
    if (s->src_fd == -2 && src_fd > -1)
    {
	s->src_fd = src_fd;
	if (fstatx_np(s->src_fd, &s->sb, s->fsec) != 0)
	{
	    if (errno == ENOTSUP || errno == EPERM)
		fstat(s->src_fd, &s->sb);
	    else
	    {
		copyfile_warn("fstatx_np on src fd %d", s->src_fd);
		return -1;
	    }
	}
    }

    /* prevent copying on unsupported types */
    switch (s->sb.st_mode & S_IFMT)
    {
	case S_IFLNK:
	case S_IFDIR:
	case S_IFREG:
	    break;
	default:
	    errno = ENOTSUP;
	    return -1;
    }

    copyfile_debug(2, "set dst_fd <- %d", dst_fd);
    if (s->dst_fd == -2 && dst_fd > -1)
	s->dst_fd = dst_fd;

    (void)fstat(s->dst_fd, &dst_sb);
    (void)fchmod(s->dst_fd, (dst_sb.st_mode & ~S_IFMT) | (S_IRUSR | S_IWUSR));

    (void)copyfile_quarantine(s);

    ret = copyfile_internal(s, flags);

    if (ret >= 0 && !(s->flags & COPYFILE_STAT))
    {
	(void)fchmod(s->dst_fd, dst_sb.st_mode & ~S_IFMT);
    }

    if (s->err) {
	errno = s->err;
	s->err = 0;
    }
    if (state == NULL) {
	int t = errno;
	copyfile_state_free(s);
	errno = t;
    }

    return ret;

}

/*
 * the original copyfile() routine; this copies a source file to a destination
 * file.  Note that because we need to set the names in the state variable, this
 * is not just the same as opening the two files, and then calling fcopyfile().
 * Oh, if only life were that simple!
 */
int copyfile(const char *src, const char *dst, copyfile_state_t state, copyfile_flags_t flags)
{
    int ret = 0;
    int createdst = 0;
    copyfile_state_t s = state;
    struct stat dst_sb;

    if (src == NULL && dst == NULL)
    {
	errno = EINVAL;
	return -1;
    }

    if (copyfile_preamble(&s, flags) < 0)
    {
	return -1;
    }

/*
 * This macro is... well, it's not the worst thing you can do with cpp, not
 *  by a long shot.  Essentially, we are setting the filename (src or dst)
 * in the state structure; since the structure may not have been cleared out
 * before being used again, we do some of the cleanup here:  if the given
 * filename (e.g., src) is set, and state->src is not equal to that, then
 * we need to check to see if the file descriptor had been opened, and if so,
 * close it.  After that, we set state->src to be a copy of the given filename,
 * releasing the old copy if necessary.
 */
#define COPYFILE_SET_FNAME(NAME, S) \
  do { \
    if (NAME != NULL) {									\
	if (S->NAME != NULL && strncmp(NAME, S->NAME, MAXPATHLEN)) {			\
	    copyfile_debug(2, "replacing string %s (%s) -> (%s)", #NAME, NAME, S->NAME);\
	    if (S->NAME##_fd != -2 && S->NAME##_fd > -1) {				\
		copyfile_debug(4, "closing %s fd: %d", #NAME, S->NAME##_fd);		\
		close(S->NAME##_fd);							\
		S->NAME##_fd = -2;							\
	    }										\
	}										\
	if (S->NAME) {									\
	    free(S->NAME);								\
	    S->NAME = NULL;								\
	}										\
	if ((S->NAME = strdup(NAME)) == NULL)						\
	    return -1;									\
    }											\
  } while (0)

    COPYFILE_SET_FNAME(src, s);
    COPYFILE_SET_FNAME(dst, s);

    if (s->flags & COPYFILE_RECURSIVE) {
	ret = copytree(s);
	goto exit;
    }

    /*
     * Get a copy of the source file's security settings
     */
    if ((s->original_fsec = filesec_init()) == NULL)
	goto error_exit;

    if ((s->flags & COPYFILE_NOFOLLOW_DST) && lstat(s->dst, &dst_sb) == 0 &&
	(dst_sb.st_mode & S_IFLNK)) {
	if (s->permissive_fsec)
	    free(s->permissive_fsec);
	s->permissive_fsec = NULL;
    } else if(statx_np(s->dst, &dst_sb, s->original_fsec) == 0)
    {
	   /*
	    * copyfile_fix_perms() will make a copy of the permission set,
	    * and insert at the beginning an ACE that ensures we can write
	    * to the file and set attributes.
	    */

	if((s->permissive_fsec = copyfile_fix_perms(s, &s->original_fsec)) != NULL)
	{
	    /*
	     * Set the permissions for the destination to our copy.
	     * We should get ENOTSUP from any filesystem that simply
	     * doesn't support it.
	     */
	    if (chmodx_np(s->dst, s->permissive_fsec) < 0 && errno != ENOTSUP)
	    {
		copyfile_warn("setting security information");
		filesec_free(s->permissive_fsec);
		s->permissive_fsec = NULL;
	    }
	}
    } else if (errno == ENOENT) {
	createdst = 1;
    }

    /*
     * If COPYFILE_CHECK is set in flags, then all we are going to do
     * is see what kinds of things WOULD have been copied (see
     * copyfile_check() below).  We return that value.
     */
    if (COPYFILE_CHECK & flags)
    {
	ret = copyfile_check(s);
	goto exit;
    } else if ((ret = copyfile_open(s)) < 0)
	goto error_exit;

    ret = copyfile_internal(s, flags);
    if (ret == -1)
	goto error_exit;

#ifdef COPYFILE_RECURSIVE
    if (!(flags & COPYFILE_STAT)) {
	if (!createdst)
	{
	/* Just need to reset the BSD information -- mode, owner, group */
	(void)fchown(s->dst_fd, dst_sb.st_uid, dst_sb.st_gid);
	(void)fchmod(s->dst_fd, dst_sb.st_mode);
	}
    }
#endif

    reset_security(s);

exit:
    if (state == NULL) {
	int t = errno;
	copyfile_state_free(s);
	errno = t;
    }

    return ret;

error_exit:
    ret = -1;
    if (s->err) {
	errno = s->err;
	s->err = 0;
    }
    goto exit;
}

/*
 * Shared prelude to the {f,}copyfile().  This initializes the
 * state variable, if necessary, and also checks for both debugging
 * and disabling environment variables.
 */
static int copyfile_preamble(copyfile_state_t *state, copyfile_flags_t flags)
{
    copyfile_state_t s;

    if (*state == NULL)
    {
	if ((*state = copyfile_state_alloc()) == NULL)
	    return -1;
    }

    s = *state;

    if (COPYFILE_DEBUG & flags)
    {
	char *e;
	if ((e = getenv(COPYFILE_DEBUG_VAR)))
	{
	    errno = 0;
	    s->debug = (uint32_t)strtol(e, NULL, 0);

	    /* clamp s->debug to 1 if the environment variable is not parsable */
	    if (s->debug == 0 && errno != 0)
		s->debug = 1;
	}
	copyfile_debug(2, "debug value set to: %d", s->debug);
    }

#if 0
    /* Temporarily disabled */
    if (getenv(COPYFILE_DISABLE_VAR) != NULL)
    {
	copyfile_debug(1, "copyfile disabled");
	return 2;
    }
#endif
    copyfile_debug(2, "setting flags: %d", s->flags);
    s->flags = flags;

    return 0;
}

/*
 * The guts of {f,}copyfile().
 * This looks through the flags in a particular order, and calls the
 * associated functions.
 */
static int copyfile_internal(copyfile_state_t s, copyfile_flags_t flags)
{
    int ret = 0;

    if (s->dst_fd < 0 || s->src_fd < 0)
    {
	copyfile_debug(1, "file descriptors not open (src: %d, dst: %d)", s->src_fd, s->dst_fd);
	s->err = EINVAL;
	return -1;
    }

    /*
     * COPYFILE_PACK causes us to create an Apple Double version of the
     * source file, and puts it into the destination file.  See
     * copyfile_pack() below for all the gory details.
     */
    if (COPYFILE_PACK & flags)
    {
	if ((ret = copyfile_pack(s)) < 0)
	{
	    if (s->dst) unlink(s->dst);
	    goto exit;
	}
	goto exit;
    }

    /*
     * COPYFILE_UNPACK is the undoing of COPYFILE_PACK, obviously.
     * The goal there is to take an Apple Double file, and turn it
     * into a normal file (with data fork, resource fork, modes,
     * extended attributes, ACLs, etc.).
     */
    if (COPYFILE_UNPACK & flags)
    {
	if ((ret = copyfile_unpack(s)) < 0)
	    goto error_exit;
	goto exit;
    }

    /*
     * If we have quarantine info set, we attempt
     * to apply it to dst_fd.  We don't care if
     * it fails, not yet anyway.
     */
    if (s->qinfo)
	(void)qtn_file_apply_to_fd(s->qinfo, s->dst_fd);

    /*
     * COPYFILE_XATTR tells us to copy the extended attributes;
     * this is seperate from the extended security (aka ACLs),
     * however.  If we succeed in this, we continue to the next
     * stage; if we fail, we return with an error value.  Note
     * that we fail if the errno is ENOTSUP, but we don't print
     * a warning in that case.
     */
    if (COPYFILE_XATTR & flags)
    {
	if ((ret = copyfile_xattr(s)) < 0)
	{
	    if (errno != ENOTSUP && errno != EPERM)
		copyfile_warn("error processing extended attributes");
	    goto exit;
	}
    }

    /*
     * Simialr to above, this tells us whether or not to copy
     * the non-meta data portion of the file.  We attempt to
     * remove (via unlink) the destination file if we fail.
     */
    if (COPYFILE_DATA & flags)
    {
	if ((ret = copyfile_data(s)) < 0)
	{
	    copyfile_warn("error processing data");
	    if (s->dst && unlink(s->dst))
		    copyfile_warn("%s: remove", s->src ? s->src : "(null src)");
	    goto exit;
	}
    }

    /*
     * COPYFILE_SECURITY requests that we copy the security, both
     * extended and mundane (that is, ACLs and POSIX).
     */
    if (COPYFILE_SECURITY & flags)
    {
	if ((ret = copyfile_security(s)) < 0)
	{
	    copyfile_warn("error processing security information");
	    goto exit;
	}
    }

    if (COPYFILE_STAT & flags)
    {
	if ((ret = copyfile_stat(s)) < 0)
	{
	    copyfile_warn("error processing POSIX information");
	    goto exit;
	}
    }

exit:
    return ret;

error_exit:
    ret = -1;
    goto exit;
}

/*
 * A publicly-visible routine, copyfile_state_alloc() sets up the state variable.
 */
copyfile_state_t copyfile_state_alloc(void)
{
    copyfile_state_t s = (copyfile_state_t) calloc(1, sizeof(struct _copyfile_state));

    if (s != NULL)
    {
	s->src_fd = -2;
	s->dst_fd = -2;
	s->fsec = filesec_init();
    } else
	errno = ENOMEM;

    return s;
}

/*
 * copyfile_state_free() returns the memory allocated to the state structure.
 * It also closes the file descriptors, if they've been opened.
 */
int copyfile_state_free(copyfile_state_t s)
{
    if (s != NULL)
    {
	if (s->fsec)
	    filesec_free(s->fsec);

	if (s->original_fsec)
	    filesec_free(s->original_fsec);

	if (s->permissive_fsec)
	    filesec_free(s->permissive_fsec);

	if (s->qinfo)
	    qtn_file_free(s->qinfo);

	if (copyfile_close(s) < 0)
	{
	    copyfile_warn("error closing files");
	    return -1;
	}
	if (s->dst)
	    free(s->dst);
	if (s->src)
	    free(s->src);
	free(s);
    }
    return 0;
}

/*
 * Should we worry if we can't close the source?  NFS says we
 * should, but it's pretty late for us at this point.
 */
static int copyfile_close(copyfile_state_t s)
{
    if (s->src && s->src_fd >= 0)
	close(s->src_fd);

    if (s->dst && s->dst_fd >= 0) {
	if (close(s->dst_fd))
	    return -1;
    }

    return 0;
}

/*
 * The purpose of this function is to set up a set of permissions
 * (ACL and traditional) that lets us write to the file.  In the
 * case of ACLs, we do this by putting in a first entry that lets
 * us write data, attributes, and extended attributes.  In the case
 * of traditional permissions, we set the S_IWUSR (user-write)
 * bit.
 */
static filesec_t copyfile_fix_perms(copyfile_state_t s __unused, filesec_t *fsec)
{
    filesec_t ret_fsec = NULL;
    mode_t mode;
    acl_t acl = NULL;

    if ((ret_fsec = filesec_dup(*fsec)) == NULL)
	goto error_exit;

    if (filesec_get_property(ret_fsec, FILESEC_ACL, &acl) == 0)
    {
#ifdef COPYFILE_RECURSIVE
	if (add_uberace(&acl))
		goto error_exit;
#else
	acl_entry_t entry;
	acl_permset_t permset;
	uuid_t qual;

	if (mbr_uid_to_uuid(getuid(), qual) != 0)
	    goto error_exit;

	/*
	 * First, we create an entry, and give it the special name
	 * of ACL_FIRST_ENTRY, thus guaranteeing it will be first.
	 * After that, we clear out all the permissions in it, and
	 * add three permissions:  WRITE_DATA, WRITE_ATTRIBUTES, and
	 * WRITE_EXTATTRIBUTES.  We put these into an ACE that allows
	 * the functionality, and put this into the ACL.
	 */
	if (acl_create_entry_np(&acl, &entry, ACL_FIRST_ENTRY) == -1)
	    goto error_exit;
	if (acl_get_permset(entry, &permset) == -1)
	    goto error_exit;
	if (acl_clear_perms(permset) == -1)
	    goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_DATA) == -1)
	    goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_ATTRIBUTES) == -1)
	    goto error_exit;
	if (acl_add_perm(permset, ACL_WRITE_EXTATTRIBUTES) == -1)
	    goto error_exit;
	if (acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == -1)
	    goto error_exit;

	if(acl_set_permset(entry, permset) == -1)
	    goto error_exit;
	if(acl_set_qualifier(entry, qual) == -1)
	    goto error_exit;
#endif

	if (filesec_set_property(ret_fsec, FILESEC_ACL, &acl) != 0)
	    goto error_exit;
    }

    /*
     * This is for the normal, mundane, POSIX permission model.
     * We make sure that we can write to the file.
     */
    if (filesec_get_property(ret_fsec, FILESEC_MODE, &mode) == 0)
    {
	if ((mode & (S_IWUSR | S_IRUSR)) != (S_IWUSR | S_IRUSR))
	{
	    mode |= S_IWUSR|S_IRUSR;
	    if (filesec_set_property(ret_fsec, FILESEC_MODE, &mode) != 0)
		goto error_exit;
	}
    }

exit:
    if (acl)
	acl_free(acl);

    return ret_fsec;

error_exit:
    if (ret_fsec)
    {
	filesec_free(ret_fsec);
	ret_fsec = NULL;
    }
    goto exit;
}

/*
 * Used to clear out the BSD/POSIX security information from
 * a filesec
 */
static int
copyfile_unset_posix_fsec(filesec_t fsec)
{
	(void)filesec_set_property(fsec, FILESEC_OWNER, _FILESEC_UNSET_PROPERTY);
	(void)filesec_set_property(fsec, FILESEC_GROUP, _FILESEC_UNSET_PROPERTY);
	(void)filesec_set_property(fsec, FILESEC_MODE, _FILESEC_UNSET_PROPERTY);
	return 0;
}

/*
 * Used to remove acl information from a filesec_t
 * Unsetting the acl alone in Tiger was insufficient
 */
static int copyfile_unset_acl(copyfile_state_t s)
{
    int ret = 0;
    if (filesec_set_property(s->fsec, FILESEC_ACL, NULL) == -1)
    {
	copyfile_debug(5, "unsetting acl attribute on %s", s->dst ? s->dst : "(null dst)");
	++ret;
    }
    if (filesec_set_property(s->fsec, FILESEC_UUID, NULL) == -1)
    {
	copyfile_debug(5, "unsetting uuid attribute on %s", s->dst ? s->dst : "(null dst)");
	++ret;
    }
    if (filesec_set_property(s->fsec, FILESEC_GRPUUID, NULL) == -1)
    {
	copyfile_debug(5, "unsetting group uuid attribute on %s", s->dst ? s->dst : "(null dst)");
	++ret;
    }
    return ret;
}

/*
 * copyfile_open() does what one expects:  it opens up the files
 * given in the state structure, if they're not already open.
 * It also does some type validation, to ensure that we only
 * handle file types we know about.
 */
static int copyfile_open(copyfile_state_t s)
{
    int oflags = O_EXCL | O_CREAT | O_WRONLY;
    int islnk = 0, isdir = 0;
    int osrc = 0, dsrc = 0;

    if (s->src && s->src_fd == -2)
    {
	if ((COPYFILE_NOFOLLOW_SRC & s->flags ? lstatx_np : statx_np)
		(s->src, &s->sb, s->fsec))
	{
	    copyfile_warn("stat on %s", s->src);
	    return -1;
	}

	/* prevent copying on unsupported types */
	switch (s->sb.st_mode & S_IFMT)
	{
	    case S_IFLNK:
		islnk = 1;
		if ((size_t)s->sb.st_size > SIZE_T_MAX) {
			s->err = ENOMEM;	/* too big for us to copy */
			return -1;
		}
		osrc = O_SYMLINK;
		break;
	    case S_IFDIR:
		isdir = 1;
		break;
	    case S_IFREG:
		break;
	    default:
		if (!(strcmp(s->src, "/dev/null") == 0 && (s->flags & COPYFILE_METADATA))) {
			s->err = ENOTSUP;
			return -1;
		}
	}
	/*
	 * If we're packing, then we are actually
	 * creating a file, no matter what the source
	 * was.
	 */
	if (s->flags & COPYFILE_PACK) {
		/*
		 * O_SYMLINK and O_NOFOLLOW are not compatible options:
		 * if the file is a symlink, and O_NOFOLLOW is specified,
		 * open will return ELOOP, whether or not O_SYMLINK is set.
		 * However, we know whether or not it was a symlink from
		 * the stat above (although there is a potentiaal for a race
		 * condition here, but it will err on the side of returning
		 * ELOOP from open).
		 */
		if (!islnk)
			osrc = (s->flags & COPYFILE_NOFOLLOW_SRC) ? O_NOFOLLOW : 0;
		isdir = islnk = 0;
	}

	if ((s->src_fd = open(s->src, O_RDONLY | osrc , 0)) < 0)
	{
		copyfile_warn("open on %s", s->src);
		return -1;
	} else
	    copyfile_debug(2, "open successful on source (%s)", s->src);

	(void)copyfile_quarantine(s);
    }

    if (s->dst && s->dst_fd == -2)
    {
	/*
	 * COPYFILE_UNLINK tells us to try removing the destination
	 * before we create it.  We don't care if the file doesn't
	 * exist, so we ignore ENOENT.
	 */
	if (COPYFILE_UNLINK & s->flags)
	{
	    if (remove(s->dst) < 0 && errno != ENOENT)
	    {
		copyfile_warn("%s: remove", s->dst);
		return -1;
	    }
	}

	if (s->flags & COPYFILE_NOFOLLOW_DST) {
		struct stat st;

		dsrc = O_NOFOLLOW;
		if (lstat(s->dst, &st) != -1) {
			if ((st.st_mode & S_IFMT) == S_IFLNK)
				dsrc = O_SYMLINK;
		}
	}

	if (islnk) {
		size_t sz = (size_t)s->sb.st_size + 1;
		char *bp;

		bp = calloc(1, sz);
		if (bp == NULL) {
			copyfile_warn("cannot allocate %d bytes", sz);
			return -1;
		}
		if (readlink(s->src, bp, sz-1) == -1) {
			copyfile_warn("cannot readlink %s", s->src);
			free(bp);
			return -1;
		}
		if (symlink(bp, s->dst) == -1) {
			if (errno != EEXIST || (s->flags & COPYFILE_EXCL)) {
				copyfile_warn("Cannot make symlink %s", s->dst);
				free(bp);
				return -1;
			}
		}
		free(bp);
		s->dst_fd = open(s->dst, O_RDONLY | O_SYMLINK);
		if (s->dst_fd == -1) {
			copyfile_warn("Cannot open symlink %s for reading", s->dst);
			return -1;
		}
	} else if (isdir) {
		mode_t mode;
		mode = s->sb.st_mode & ~S_IFMT;

		if (mkdir(s->dst, mode) == -1) {
			if (errno != EEXIST || (s->flags & COPYFILE_EXCL)) {
				copyfile_warn("Cannot make directory %s", s->dst);
				return -1;
			}
		}
		s->dst_fd = open(s->dst, O_RDONLY | dsrc);
		if (s->dst_fd == -1) {
			copyfile_warn("Cannot open directory %s for reading", s->dst);
			return -1;
		}
	} else while((s->dst_fd = open(s->dst, oflags | dsrc, s->sb.st_mode | S_IWUSR)) < 0)
	{
	    /*
	     * We set S_IWUSR because fsetxattr does not -- at the time this comment
	     * was written -- allow one to set an extended attribute on a file descriptor
	     * for a read-only file, even if the file descriptor is opened for writing.
	     * This will only matter if the file does not already exist.
	     */
	    switch(errno)
	    {
		case EEXIST:
		    copyfile_debug(3, "open failed, retrying (%s)", s->dst);
		    if (s->flags & COPYFILE_EXCL)
			break;
		    oflags = oflags & ~O_CREAT;
		    if (s->flags & (COPYFILE_PACK | COPYFILE_DATA))
		    {
			copyfile_debug(4, "truncating existing file (%s)", s->dst);
			oflags |= O_TRUNC;
		    }
		    continue;
		case EACCES:
		    if(chmod(s->dst, (s->sb.st_mode | S_IWUSR) & ~S_IFMT) == 0)
			continue;
		    else {
			/*
			 * If we're trying to write to a directory to which we don't
			 * have access, the create above would have failed, but chmod
			 * here would have given us ENOENT.  But the real error is
			 * still one of access, so we change the errno we're reporting.
			 * This could cause confusion with a race condition.
			 */

			if (errno == ENOENT)
				errno = EACCES;
			break;
		    }
		case EISDIR:
		    copyfile_debug(3, "open failed because it is a directory (%s)", s->dst);
		    if ((s->flags & COPYFILE_EXCL) ||
			(!isdir && (s->flags & COPYFILE_DATA)))
			break;
		    oflags = (oflags & ~O_WRONLY) | O_RDONLY;
		    continue;
	    }
	    copyfile_warn("open on %s", s->dst);
	    return -1;
	}
	copyfile_debug(2, "open successful on destination (%s)", s->dst);
    }

    if (s->dst_fd < 0 || s->src_fd < 0)
    {
	copyfile_debug(1, "file descriptors not open (src: %d, dst: %d)",
		s->src_fd, s->dst_fd);
	s->err = EINVAL;
	return -1;
    }
    return 0;
}


/*
 * copyfile_check(), as described above, essentially tells you
 * what you'd have to copy, if you wanted it to copy the things
 * you asked it to copy.
 * In other words, if you pass in COPYFILE_ALL, and the file in
 * question had no extended attributes but did have an ACL, you'd
 * get back COPYFILE_ACL.
 */
static copyfile_flags_t copyfile_check(copyfile_state_t s)
{
    acl_t acl = NULL;
    copyfile_flags_t ret = 0;
    int nofollow = (s->flags & COPYFILE_NOFOLLOW_SRC);
    qtn_file_t qinfo;

    if (!s->src)
    {
	s->err = EINVAL;
	return -1;
    }

    /* check EAs */
    if (COPYFILE_XATTR & s->flags)
	if (listxattr(s->src, 0, 0, nofollow ? XATTR_NOFOLLOW : 0) > 0)
	{
	    ret |= COPYFILE_XATTR;
	}

    if (COPYFILE_ACL & s->flags)
    {
	(COPYFILE_NOFOLLOW_SRC & s->flags ? lstatx_np : statx_np)
		(s->src, &s->sb, s->fsec);

	if (filesec_get_property(s->fsec, FILESEC_ACL, &acl) == 0)
	    ret |= COPYFILE_ACL;
    }

    copyfile_debug(2, "check result: %d (%s)", ret, s->src);

    if (acl)
	acl_free(acl);

    if (s->qinfo) {
	/* If the state has had quarantine info set already, we use that */
	ret |= ((s->flags & COPYFILE_XATTR) ? COPYFILE_XATTR : COPYFILE_ACL);
    } else {
	qinfo = qtn_file_alloc();
	/*
	 * For quarantine information, we need to see if the source file
	 * has any.  Since it may be a symlink, however, and we may, or
	 * not be following, *and* there's no qtn* routine which can optionally
	 * follow or not follow a symlink, we need to instead work around
	 * this limitation.
	*/
	if (qinfo) {
		int fd;
		int qret = 0;
		struct stat sbuf;

		/*
		 * If we care about not following symlinks, *and* the file exists
		 * (which is to say, lstat doesn't return an error), *and* the file
		 * is a symlink, then we open it up (with O_SYMLINK), and use
		 * qtn_file_init_with_fd(); if none of that is true, however, then
		 * we can simply use qtn_file_init_with_path().
		 */
		if (nofollow
			&& lstat(s->src, &sbuf) == 0
				&& ((sbuf.st_mode & S_IFMT) == S_IFLNK)) {
			fd = open(s->src, O_RDONLY | O_SYMLINK);
			if (fd != -1) {
				if (!qtn_file_init_with_fd(qinfo, fd)) {
					qret |= ((s->flags & COPYFILE_XATTR) ? COPYFILE_XATTR : COPYFILE_ACL);
				}
				close(fd);
			}
		} else {
			if (!qtn_file_init_with_path(qinfo, s->src)) {
				qret |= ((s->flags & COPYFILE_XATTR) ? COPYFILE_XATTR : COPYFILE_ACL);
			}
		}
		qtn_file_free(qinfo);
		ret |= qret;
	}
    }
    return ret;
}

/*
 * Attempt to copy the data section of a file.  Using blockisize
 * is not necessarily the fastest -- it might be desirable to
 * specify a blocksize, somehow.  But it's a size that should be
 * guaranteed to work.
 */
static int copyfile_data(copyfile_state_t s)
{
    size_t blen;
    char *bp = 0;
    ssize_t nread;
    int ret = 0;
    size_t iBlocksize = 0;
    size_t oBlocksize = 0;
    const size_t onegig = 1 << 30;
    struct statfs sfs;
    copyfile_callback_t status = s->statuscb;

    /* Unless it's a normal file, we don't copy.  For now, anyway */
    if ((s->sb.st_mode & S_IFMT) != S_IFREG)
	return 0;

    if (fstatfs(s->src_fd, &sfs) == -1) {
	iBlocksize = s->sb.st_blksize;
    } else {
	iBlocksize = sfs.f_iosize;
    }

    /* Work-around for 6453525, limit blocksize to 1G */
    if (iBlocksize > onegig) {
	iBlocksize = onegig;
    }

    if ((bp = malloc(iBlocksize)) == NULL)
	return -1;

    if (fstatfs(s->dst_fd, &sfs) == -1 || sfs.f_iosize == 0) {
	oBlocksize = iBlocksize;
    } else {
	oBlocksize = sfs.f_iosize;
	if (oBlocksize > onegig)
	    oBlocksize = onegig;
    }

    blen = iBlocksize;

    s->totalCopied = 0;
/* If supported, do preallocation for Xsan / HFS volumes */
#ifdef F_PREALLOCATE
    {
       fstore_t fst;

       fst.fst_flags = 0;
       fst.fst_posmode = F_PEOFPOSMODE;
       fst.fst_offset = 0;
       fst.fst_length = s->sb.st_size;
       /* Ignore errors; this is merely advisory. */
       (void)fcntl(s->dst_fd, F_PREALLOCATE, &fst);
    }
#endif

    while ((nread = read(s->src_fd, bp, blen)) > 0)
    {
	ssize_t nwritten;
	size_t left = nread;
	void *ptr = bp;
	int loop = 0;

	while (left > 0) {
		nwritten = write(s->dst_fd, ptr, MIN(left, oBlocksize));
		switch (nwritten) {
		case 0:
			if (++loop > 5) {
				copyfile_warn("writing to output %d times resulted in 0 bytes written", loop);
				ret = -1;
				s->err = EAGAIN;
				goto exit;
			}
			break;
		case -1:
			copyfile_warn("writing to output file got error");
			if (status) {
				int rv = (*status)(COPYFILE_COPY_DATA, COPYFILE_ERR, s, s->src, s->dst, s->ctx);
				if (rv == COPYFILE_SKIP) {	// Skip the data copy
					ret = 0;
					goto exit;
				}
				if (rv == COPYFILE_CONTINUE) {	// Retry the write
					errno = 0;
					continue;
				}
			}
			ret = -1;
			goto exit;
		default:
			left -= nwritten;
			ptr = ((char*)ptr) + nwritten;
			loop = 0;
			break;
		}
		s->totalCopied += nwritten;
		if (status) {
			int rv = (*status)(COPYFILE_COPY_DATA, COPYFILE_PROGRESS,  s, s->src, s->dst, s->ctx);
			if (rv == COPYFILE_QUIT) {
				ret = -1; s->err = ECANCELED;
				goto exit;
			}
		}
	}
    }
    if (nread < 0)
    {
	copyfile_warn("reading from %s", s->src ? s->src : "(null src)");
	ret = -1;
	goto exit;
    }

    if (ftruncate(s->dst_fd, s->sb.st_size) < 0)
    {
	ret = -1;
	goto exit;
    }

exit:
    if (ret == -1)
    {
	s->err = errno;
    }
    free(bp);
    return ret;
}

/*
 * copyfile_security() will copy the ACL set, and the
 * POSIX set.  Complexities come when dealing with
 * inheritied permissions, and when dealing with both
 * POSIX and ACL permissions.
 */
static int copyfile_security(copyfile_state_t s)
{
    int copied = 0;
    int has_uberace = 0;
    acl_flagset_t flags;
    struct stat sb;
    acl_entry_t entry_src = NULL, entry_dst = NULL;
    acl_t acl_src = NULL, acl_dst = NULL;
    int ret = 0;
    filesec_t tmp_fsec = NULL;
    filesec_t fsec_dst = filesec_init();

    if (fsec_dst == NULL)
	return -1;


    if (COPYFILE_ACL & s->flags)
    {
	if (filesec_get_property(s->fsec, FILESEC_ACL, &acl_src))
	{
	    if (errno == ENOENT)
		acl_src = NULL;
	    else
		goto error_exit;
	}

/* grab the destination acl
    cannot assume it's empty due to inheritance
*/
	if(fstatx_np(s->dst_fd, &sb, fsec_dst))
	    goto error_exit;

	if (filesec_get_property(fsec_dst, FILESEC_ACL, &acl_dst))
	{
	    if (errno == ENOENT)
		acl_dst = NULL;
	    else
		goto error_exit;
	}
	else
	{
		acl_t tmp = acl_init(4);
		acl_entry_t ace = NULL;
		int count = 0;

		if (tmp == NULL)
			goto error_exit;


		for (; acl_get_entry(acl_dst,
			ace == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY,
			&ace) == 0;)
		{
			if (count++ == 0 && is_uberace(ace)) {
				if ((ret = acl_create_entry(&tmp, &entry_dst)) == -1)
					break;
				if ((ret = acl_copy_entry(entry_dst, ace)) == -1)
					break;
				has_uberace = 1;
				continue;
			}
			acl_get_flagset_np(ace, &flags);
			if (acl_get_flag_np(flags, ACL_ENTRY_INHERITED))
			{
				if ((ret = acl_create_entry(&tmp, &entry_dst)) == -1)
					break;
				if ((ret = acl_copy_entry(entry_dst, ace)) == -1)
					break;
			}
		}
		acl_free(acl_dst);
		acl_dst = tmp;

		if (ret == -1)
			goto error_exit;
	}

	if (acl_src == NULL && acl_dst == NULL)
		goto no_acl;

	if (acl_src) {
		if (acl_dst == NULL)
			acl_dst = acl_init(4);
		for (copied = 0;acl_get_entry(acl_src,
		    entry_src == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY,
		    &entry_src) == 0;)
		{
		    acl_get_flagset_np(entry_src, &flags);
		    if (!acl_get_flag_np(flags, ACL_ENTRY_INHERITED))
		    {
			if ((ret = acl_create_entry(&acl_dst, &entry_dst)) == -1)
			    goto error_exit;

			if ((ret = acl_copy_entry(entry_dst, entry_src)) == -1)
			    goto error_exit;

			copyfile_debug(2, "copied acl entry from %s to %s",
				s->src ? s->src : "(null src)",
				s->dst ? s->dst : "(null dst)");
			copied++;
		    }
		}
	}
	if (!has_uberace && (s->internal_flags & cfDelayAce)) {
		if (add_uberace(&acl_dst))
			goto error_exit;
	}
	if (!filesec_set_property(s->fsec, FILESEC_ACL, &acl_dst))
	{
	    copyfile_debug(3, "altered acl");
	}
    }
no_acl:
    /*
     * The following code is attempting to ensure that only the requested
     * security information gets copied over to the destination file.
     * We essentially have four cases:  COPYFILE_ACL, COPYFILE_STAT,
     * COPYFILE_(STAT|ACL), and none (in which case, we wouldn't be in
     * this function).
     *
     * If we have both flags, we copy everything; if we have ACL but not STAT,
     * we remove the POSIX information from the filesec object, and apply the
     * ACL; if we have STAT but not ACL, then we just use fchmod(), and ignore
     * the extended version.
     */
    tmp_fsec = filesec_dup(s->fsec);
    if (tmp_fsec == NULL) {
	goto error_exit;
    }

    switch (COPYFILE_SECURITY & s->flags) {
	case COPYFILE_ACL:
	    copyfile_unset_posix_fsec(tmp_fsec);
	    /* FALLTHROUGH */
	case COPYFILE_ACL | COPYFILE_STAT:
	    if (fchmodx_np(s->dst_fd, tmp_fsec) < 0) {
		acl_t acl = NULL;
		/*
		 * The call could have failed for a number of reasons, since
		 * it does a number of things:  it changes the mode of the file,
		 * sets the owner and group, and applies an ACL (if one exists).
		 * The typical failure is going to be trying to set the group of
		 * the destination file to match the source file, when the process
		 * doesn't have permission to put files in that group.  We try to
		 * work around this by breaking the steps out and doing them
		 * discretely.  We don't care if the fchown fails, but we do care
		 * if the mode or ACL can't be set.  For historical reasons, we
		 * simply log those failures, however.
		 */

#define NS(x)	((x) ? (x) : "(null string)")
		if (fchmod(s->dst_fd, s->sb.st_mode) == -1) {
			copyfile_warn("could not change mode of destination file %s to match source file %s", NS(s->dst), NS(s->src));
		}
		(void)fchown(s->dst_fd, s->sb.st_uid, s->sb.st_gid);
		if (filesec_get_property(tmp_fsec, FILESEC_ACL, &acl) == 0) {
			if (acl_set_fd(s->dst_fd, acl) == -1) {
				copyfile_warn("could not apply acl to destination file %s from source file %s", NS(s->dst), NS(s->src));
			}
			acl_free(acl);
		}
	    }
#undef NS
	    break;
	case COPYFILE_STAT:
	    (void)fchmod(s->dst_fd, s->sb.st_mode);
	    break;
    }
    filesec_free(tmp_fsec);
exit:
    filesec_free(fsec_dst);
    if (acl_src) acl_free(acl_src);
    if (acl_dst) acl_free(acl_dst);

    return ret;

error_exit:
    ret = -1;
goto exit;

}

/*
 * Attempt to set the destination file's stat information -- including
 * flags and time-related fields -- to the source's.
 */
static int copyfile_stat(copyfile_state_t s)
{
    struct timeval tval[2];
    /*
     * NFS doesn't support chflags; ignore errors unless there's reason
     * to believe we're losing bits.  (Note, this still won't be right
     * if the server supports flags and we were trying to *remove* flags
     * on a file that we copied, i.e., that we didn't create.)
     */
    if (fchflags(s->dst_fd, (u_int)s->sb.st_flags))
	if (errno != EOPNOTSUPP || s->sb.st_flags != 0)
	    copyfile_warn("%s: set flags (was: 0%07o)", s->dst ? s->dst : "(null dst)", s->sb.st_flags);

    /* If this fails, we don't care */
    (void)fchown(s->dst_fd, s->sb.st_uid, s->sb.st_gid);

    /* This may have already been done in copyfile_security() */
    (void)fchmod(s->dst_fd, s->sb.st_mode & ~S_IFMT);

    tval[0].tv_sec = s->sb.st_atime;
    tval[1].tv_sec = s->sb.st_mtime;
    tval[0].tv_usec = tval[1].tv_usec = 0;
    if (futimes(s->dst_fd, tval))
	    copyfile_warn("%s: set times", s->dst ? s->dst : "(null dst)");
    return 0;
}

/*
 * Similar to copyfile_security() in some ways; this
 * routine copies the extended attributes from the source,
 * and sets them on the destination.
 * The procedure is pretty simple, even if it is verbose:
 * for each named attribute on the destination, get its name, and
 * remove it.  We should have none after that.
 * For each named attribute on the source, get its name, get its
 * data, and set it on the destination.
 */
static int copyfile_xattr(copyfile_state_t s)
{
    char *name;
    char *namebuf, *end;
    ssize_t xa_size;
    void *xa_dataptr;
    ssize_t bufsize = 4096;
    ssize_t asize;
    ssize_t nsize;
    int ret = 0;

    /* delete EAs on destination */
    if ((nsize = flistxattr(s->dst_fd, 0, 0, 0)) > 0)
    {
	if ((namebuf = (char *) malloc(nsize)) == NULL)
	    return -1;
	else
	    nsize = flistxattr(s->dst_fd, namebuf, nsize, 0);

	if (nsize > 0) {
	    /*
	     * With this, end points to the last byte of the allocated buffer
	     * This *should* be NUL, from flistxattr, but if it's not, we can
	     * set it anyway -- it'll result in a truncated name, which then
	     * shouldn't match when we get them later.
	     */
	    end = namebuf + nsize - 1;
	    if (*end != 0)
		*end = 0;
	    for (name = namebuf; name <= end; name += strlen(name) + 1) {
		/* If the quarantine information shows up as an EA, we skip over it */
		if (strncmp(name, XATTR_QUARANTINE_NAME, end - name) == 0) {
		    continue;
		}
		fremovexattr(s->dst_fd, name,0);
	    }
	}
	free(namebuf);
    } else
    if (nsize < 0)
    {
	if (errno == ENOTSUP || errno == EPERM)
	    return 0;
	else
	    return -1;
    }

    /* get name list of EAs on source */
    if ((nsize = flistxattr(s->src_fd, 0, 0, 0)) < 0)
    {
	if (errno == ENOTSUP || errno == EPERM)
	    return 0;
	else
	    return -1;
    } else
    if (nsize == 0)
	return 0;

    if ((namebuf = (char *) malloc(nsize)) == NULL)
	return -1;
    else
	nsize = flistxattr(s->src_fd, namebuf, nsize, 0);

    if (nsize <= 0) {
	free(namebuf);
	return (int)nsize;
    }

    /*
     * With this, end points to the last byte of the allocated buffer
     * This *should* be NUL, from flistxattr, but if it's not, we can
     * set it anyway -- it'll result in a truncated name, which then
     * shouldn't match when we get them later.
     */
    end = namebuf + nsize - 1;
    if (*end != 0)
	*end = 0;

    if ((xa_dataptr = (void *) malloc(bufsize)) == NULL) {
	free(namebuf);
	return -1;
    }

    for (name = namebuf; name <= end; name += strlen(name) + 1)
    {
	/* If the quarantine information shows up as an EA, we skip over it */
	if (strncmp(name, XATTR_QUARANTINE_NAME, end - name) == 0)
	    continue;

	if ((xa_size = fgetxattr(s->src_fd, name, 0, 0, 0, 0)) < 0)
	{
	    ret = -1;
	    continue;
	}

	if (xa_size > bufsize)
	{
	    void *tdptr = xa_dataptr;
	    bufsize = xa_size;
	    if ((xa_dataptr =
		(void *) realloc((void *) xa_dataptr, bufsize)) == NULL)
	    {
		free(tdptr);
		ret = -1;
		continue;
	    }
	}

	if ((asize = fgetxattr(s->src_fd, name, xa_dataptr, xa_size, 0, 0)) < 0)
	{
	    ret = -1;
	    continue;
	}

	if (xa_size != asize)
	    xa_size = asize;

	if (fsetxattr(s->dst_fd, name, xa_dataptr, xa_size, 0, 0) < 0)
	{
	    ret = -1;
	    continue;
	}
    }
    if (namebuf)
	free(namebuf);
    free((void *) xa_dataptr);
    return ret;
}

/*
 * API interface into getting data from the opaque data type.
 */
int copyfile_state_get(copyfile_state_t s, uint32_t flag, void *ret)
{
    if (ret == NULL)
    {
	errno = EFAULT;
	return -1;
    }

    switch(flag)
    {
	case COPYFILE_STATE_SRC_FD:
	    *(int*)ret = s->src_fd;
	    break;
	case COPYFILE_STATE_DST_FD:
	    *(int*)ret = s->dst_fd;
	    break;
	case COPYFILE_STATE_SRC_FILENAME:
	    *(char**)ret = s->src;
	    break;
	case COPYFILE_STATE_DST_FILENAME:
	    *(char**)ret = s->dst;
	    break;
	case COPYFILE_STATE_QUARANTINE:
	    *(qtn_file_t*)ret = s->qinfo;
	    break;
#if 0
	case COPYFILE_STATE_STATS:
	    ret = s->stats.global;
	    break;
	case COPYFILE_STATE_PROGRESS_CB:
	    ret = s->callbacks.progress;
	    break;
#endif
#ifdef	COPYFILE_STATE_STATUS_CB
	case COPYFILE_STATE_STATUS_CB:
	    *(copyfile_callback_t*)ret = s->statuscb;
	    break;
	case COPYFILE_STATE_STATUS_CTX:
	    *(void**)ret = s->ctx;
	    break;
	case COPYFILE_STATE_COPIED:
	    *(off_t*)ret = s->totalCopied;
	    break;
#endif
	default:
	    errno = EINVAL;
	    ret = NULL;
	    return -1;
    }
    return 0;
}

/*
 * Public API for setting state data (remember that the state is
 * an opaque data type).
 */
int copyfile_state_set(copyfile_state_t s, uint32_t flag, const void * thing)
{
#define copyfile_set_string(DST, SRC) \
    do {					\
	if (SRC != NULL) {			\
	    DST = strdup((char *)SRC);		\
	} else {				\
	    if (DST != NULL) {			\
		free(DST);			\
	    }					\
	    DST = NULL;				\
	}					\
    } while (0)

    if (thing == NULL)
    {
	errno = EFAULT;
	return  -1;
    }

    switch(flag)
    {
	case COPYFILE_STATE_SRC_FD:
	    s->src_fd = *(int*)thing;
	    break;
	case COPYFILE_STATE_DST_FD:
	     s->dst_fd = *(int*)thing;
	    break;
	case COPYFILE_STATE_SRC_FILENAME:
	    copyfile_set_string(s->src, thing);
	    break;
	case COPYFILE_STATE_DST_FILENAME:
	    copyfile_set_string(s->dst, thing);
	    break;
	case COPYFILE_STATE_QUARANTINE:
	    if (s->qinfo)
	    {
		qtn_file_free(s->qinfo);
		s->qinfo = NULL;
	    }
	    if (*(qtn_file_t*)thing)
		s->qinfo = qtn_file_clone(*(qtn_file_t*)thing);
	    break;
#if 0
	case COPYFILE_STATE_STATS:
	     s->stats.global = thing;
	    break;
	case COPYFILE_STATE_PROGRESS_CB:
	     s->callbacks.progress = thing;
	    break;
#endif
#ifdef COPYFILE_STATE_STATUS_CB
	case COPYFILE_STATE_STATUS_CB:
	    s->statuscb = (copyfile_callback_t)thing;
	    break;
	case COPYFILE_STATE_STATUS_CTX:
	    s->ctx = (void*)thing;
	    break;
#endif
	default:
	    errno = EINVAL;
	    return -1;
    }
    return 0;
#undef copyfile_set_string
}


/*
 * Make this a standalone program for testing purposes by
 * defining _COPYFILE_TEST.
 */
#ifdef _COPYFILE_TEST
#define COPYFILE_OPTION(x) { #x, COPYFILE_ ## x },

struct {char *s; int v;} opts[] = {
    COPYFILE_OPTION(ACL)
    COPYFILE_OPTION(STAT)
    COPYFILE_OPTION(XATTR)
    COPYFILE_OPTION(DATA)
    COPYFILE_OPTION(SECURITY)
    COPYFILE_OPTION(METADATA)
    COPYFILE_OPTION(ALL)
    COPYFILE_OPTION(NOFOLLOW_SRC)
    COPYFILE_OPTION(NOFOLLOW_DST)
    COPYFILE_OPTION(NOFOLLOW)
    COPYFILE_OPTION(EXCL)
    COPYFILE_OPTION(MOVE)
    COPYFILE_OPTION(UNLINK)
    COPYFILE_OPTION(PACK)
    COPYFILE_OPTION(UNPACK)
    COPYFILE_OPTION(CHECK)
    COPYFILE_OPTION(VERBOSE)
    COPYFILE_OPTION(DEBUG)
    {NULL, 0}
};

int main(int c, char *v[])
{
    int i;
    int flags = 0;

    if (c < 3)
	errx(1, "insufficient arguments");

    while(c-- > 3)
    {
	for (i = 0; opts[i].s != NULL; ++i)
	{
	    if (strcasecmp(opts[i].s, v[c]) == 0)
	    {
		printf("option %d: %s <- %d\n", c, opts[i].s, opts[i].v);
		flags |= opts[i].v;
		break;
	    }
	}
    }

    return copyfile(v[1], v[2], NULL, flags);
}
#endif
/*
 * Apple Double Create
 *
 * Create an Apple Double "._" file from a file's extented attributes
 *
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 */


#define offsetof(type, member)	((size_t)(&((type *)0)->member))

#define	XATTR_MAXATTRLEN   (4*1024)


/*
   Typical "._" AppleDouble Header File layout:
  ------------------------------------------------------------
         MAGIC          0x00051607
         VERSION        0x00020000
         FILLER         0
         COUNT          2
     .-- AD ENTRY[0]    Finder Info Entry (must be first)
  .--+-- AD ENTRY[1]    Resource Fork Entry (must be last)
  |  '-> FINDER INFO
  |      /////////////  Fixed Size Data (32 bytes)
  |      EXT ATTR HDR
  |      /////////////
  |      ATTR ENTRY[0] --.
  |      ATTR ENTRY[1] --+--.
  |      ATTR ENTRY[2] --+--+--.
  |         ...          |  |  |
  |      ATTR ENTRY[N] --+--+--+--.
  |      ATTR DATA 0   <-'  |  |  |
  |      ////////////       |  |  |
  |      ATTR DATA 1   <----'  |  |
  |      /////////////         |  |
  |      ATTR DATA 2   <-------'  |
  |      /////////////            |
  |         ...                   |
  |      ATTR DATA N   <----------'
  |      /////////////
  |                      Attribute Free Space
  |
  '----> RESOURCE FORK
         /////////////   Variable Sized Data
         /////////////
         /////////////
         /////////////
         /////////////
         /////////////
            ...
         /////////////
 
  ------------------------------------------------------------

   NOTE: The EXT ATTR HDR, ATTR ENTRY's and ATTR DATA's are
   stored as part of the Finder Info.  The length in the Finder
   Info AppleDouble entry includes the length of the extended
   attribute header, attribute entries, and attribute data.
*/


/*
 * On Disk Data Structures
 *
 * Note: Motorola 68K alignment and big-endian.
 *
 * See RFC 1740 for additional information about the AppleDouble file format.
 *
 */

#define ADH_MAGIC     0x00051607
#define ADH_VERSION   0x00020000
#define ADH_MACOSX    "Mac OS X        "

/*
 * AppleDouble Entry ID's
 */
#define AD_DATA          1   /* Data fork */
#define AD_RESOURCE      2   /* Resource fork */
#define AD_REALNAME      3   /* File's name on home file system */
#define AD_COMMENT       4   /* Standard Mac comment */
#define AD_ICONBW        5   /* Mac black & white icon */
#define AD_ICONCOLOR     6   /* Mac color icon */
#define AD_UNUSED        7   /* Not used */
#define AD_FILEDATES     8   /* File dates; create, modify, etc */
#define AD_FINDERINFO    9   /* Mac Finder info & extended info */
#define AD_MACINFO      10   /* Mac file info, attributes, etc */
#define AD_PRODOSINFO   11   /* Pro-DOS file info, attrib., etc */
#define AD_MSDOSINFO    12   /* MS-DOS file info, attributes, etc */
#define AD_AFPNAME      13   /* Short name on AFP server */
#define AD_AFPINFO      14   /* AFP file info, attrib., etc */
#define AD_AFPDIRID     15   /* AFP directory ID */ 
#define AD_ATTRIBUTES   AD_FINDERINFO


#define ATTR_FILE_PREFIX   "._"
#define ATTR_HDR_MAGIC     0x41545452   /* 'ATTR' */

#define ATTR_BUF_SIZE      4096        /* default size of the attr file and how much we'll grow by */

/* Implementation Limits */
#define ATTR_MAX_SIZE      (128*1024)  /* 128K maximum attribute data size */
#define ATTR_MAX_NAME_LEN  128
#define ATTR_MAX_HDR_SIZE  (65536+18)

/*
 * Note: ATTR_MAX_HDR_SIZE is the largest attribute header
 * size supported (including the attribute entries). All of
 * the attribute entries must reside within this limit.
 */


#define FINDERINFOSIZE	32

typedef struct apple_double_entry
{
	u_int32_t   type;     /* entry type: see list, 0 invalid */ 
	u_int32_t   offset;   /* entry data offset from the beginning of the file. */
	u_int32_t   length;   /* entry data length in bytes. */
} __attribute__((aligned(2), packed)) apple_double_entry_t;


typedef struct apple_double_header
{
	u_int32_t   magic;         /* == ADH_MAGIC */
	u_int32_t   version;       /* format version: 2 = 0x00020000 */ 
	u_int32_t   filler[4];
	u_int16_t   numEntries;	   /* number of entries which follow */ 
	apple_double_entry_t   entries[2];  /* 'finfo' & 'rsrc' always exist */
	u_int8_t    finfo[FINDERINFOSIZE];  /* Must start with Finder Info (32 bytes) */
	u_int8_t    pad[2];        /* get better alignment inside attr_header */
} __attribute__((aligned(2), packed)) apple_double_header_t;


/* Entries are aligned on 4 byte boundaries */
typedef struct attr_entry
{
	u_int32_t   offset;    /* file offset to data */
	u_int32_t   length;    /* size of attribute data */
	u_int16_t   flags;
	u_int8_t    namelen;   /* length of name including NULL termination char */ 
	u_int8_t    name[1];   /* NULL-terminated UTF-8 name (up to 128 bytes max) */
} __attribute__((aligned(2), packed)) attr_entry_t;



/* Header + entries must fit into 64K */
typedef struct attr_header
{
	apple_double_header_t  appledouble;
	u_int32_t   magic;        /* == ATTR_HDR_MAGIC */
	u_int32_t   debug_tag;    /* for debugging == file id of owning file */
	u_int32_t   total_size;   /* total size of attribute header + entries + data */ 
	u_int32_t   data_start;   /* file offset to attribute data area */
	u_int32_t   data_length;  /* length of attribute data area */
	u_int32_t   reserved[3];
	u_int16_t   flags;
	u_int16_t   num_attrs;
} __attribute__((aligned(2), packed)) attr_header_t;

/* Empty Resource Fork Header */
/* This comes by way of xnu's vfs_xattr.c */
typedef struct rsrcfork_header {
	u_int32_t	fh_DataOffset;
	u_int32_t	fh_MapOffset;
	u_int32_t	fh_DataLength;
	u_int32_t	fh_MapLength;
	u_int8_t	systemData[112];
	u_int8_t	appData[128];
	u_int32_t	mh_DataOffset; 
	u_int32_t	mh_MapOffset;
	u_int32_t	mh_DataLength; 
	u_int32_t	mh_MapLength;
	u_int32_t	mh_Next;
	u_int16_t	mh_RefNum;
	u_int8_t	mh_Attr;
	u_int8_t	mh_InMemoryAttr; 
	u_int16_t	mh_Types;
	u_int16_t	mh_Names;
	u_int16_t	typeCount;
} rsrcfork_header_t;
#define RF_FIRST_RESOURCE    256
#define RF_NULL_MAP_LENGTH    30   
#define RF_EMPTY_TAG  "This resource fork intentionally left blank   "

static const rsrcfork_header_t empty_rsrcfork_header = {
	OSSwapHostToBigInt32(RF_FIRST_RESOURCE),	// fh_DataOffset
	OSSwapHostToBigInt32(RF_FIRST_RESOURCE),	// fh_MapOffset
	0,						// fh_DataLength
	OSSwapHostToBigInt32(RF_NULL_MAP_LENGTH),	// fh_MapLength
	{ RF_EMPTY_TAG, },				// systemData
	{ 0 },						// appData
	OSSwapHostToBigInt32(RF_FIRST_RESOURCE),	// mh_DataOffset
	OSSwapHostToBigInt32(RF_FIRST_RESOURCE),	// mh_MapOffset
	0,						// mh_DataLength
	OSSwapHostToBigInt32(RF_NULL_MAP_LENGTH),	// mh_MapLength
	0,						// mh_Next
	0,						// mh_RefNum
	0,						// mh_Attr
	0,						// mh_InMemoryAttr
	OSSwapHostToBigInt16(RF_NULL_MAP_LENGTH - 2),	// mh_Types
	OSSwapHostToBigInt16(RF_NULL_MAP_LENGTH),	// mh_Names
	OSSwapHostToBigInt16(-1),			// typeCount
};

#define SWAP16(x)	OSSwapBigToHostInt16(x)
#define SWAP32(x)	OSSwapBigToHostInt32(x)
#define SWAP64(x)	OSSwapBigToHostInt64(x)

#define ATTR_ALIGN 3L  /* Use four-byte alignment */

#define ATTR_ENTRY_LENGTH(namelen)  \
        ((sizeof(attr_entry_t) - 1 + (namelen) + ATTR_ALIGN) & (~ATTR_ALIGN))

#define ATTR_NEXT(ae)  \
	 (attr_entry_t *)((u_int8_t *)(ae) + ATTR_ENTRY_LENGTH((ae)->namelen))

#define	XATTR_SECURITY_NAME	  "com.apple.acl.text"

/*
 * Endian swap Apple Double header 
 */
static void
swap_adhdr(apple_double_header_t *adh)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	int count;
	int i;

	count = (adh->magic == ADH_MAGIC) ? adh->numEntries : SWAP16(adh->numEntries);

	adh->magic      = SWAP32 (adh->magic);
	adh->version    = SWAP32 (adh->version);
	adh->numEntries = SWAP16 (adh->numEntries);

	for (i = 0; i < count; i++)
	{
		adh->entries[i].type   = SWAP32 (adh->entries[i].type);
		adh->entries[i].offset = SWAP32 (adh->entries[i].offset);
		adh->entries[i].length = SWAP32 (adh->entries[i].length);
	}
#else
	(void)adh;
#endif
}

/*
 * Endian swap extended attributes header 
 */
static void
swap_attrhdr(attr_header_t *ah)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	attr_entry_t *ae;
	int count;
	int i;

	count = (ah->magic == ATTR_HDR_MAGIC) ? ah->num_attrs : SWAP16(ah->num_attrs);

	ah->magic       = SWAP32 (ah->magic);
	ah->debug_tag   = SWAP32 (ah->debug_tag);
	ah->total_size  = SWAP32 (ah->total_size);
	ah->data_start  = SWAP32 (ah->data_start);
	ah->data_length = SWAP32 (ah->data_length);
	ah->flags       = SWAP16 (ah->flags);
	ah->num_attrs   = SWAP16 (ah->num_attrs);

	ae = (attr_entry_t *)(&ah[1]);
	for (i = 0; i < count; i++)
	{
		attr_entry_t *next = ATTR_NEXT(ae);
		ae->offset = SWAP32 (ae->offset);
		ae->length = SWAP32 (ae->length);
		ae->flags  = SWAP16 (ae->flags);
		ae = next;
	}
#else
	(void)ah;
#endif
}

static const u_int32_t emptyfinfo[8] = {0};

/*
 * Given an Apple Double file in src, turn it into a
 * normal file (possibly with multiple forks, EAs, and
 * ACLs) in dst.
 */
static int copyfile_unpack(copyfile_state_t s)
{
    ssize_t bytes;
    void * buffer, * endptr;
    apple_double_header_t *adhdr;
    ssize_t hdrsize;
    int error = 0;

    if (s->sb.st_size < ATTR_MAX_HDR_SIZE)
	hdrsize = (ssize_t)s->sb.st_size;
    else
	hdrsize = ATTR_MAX_HDR_SIZE;

    buffer = calloc(1, hdrsize);
    if (buffer == NULL) {
	copyfile_debug(1, "copyfile_unpack: calloc(1, %u) returned NULL", hdrsize);
	error = -1;
	goto exit;
    } else
	endptr = (char*)buffer + hdrsize;

    bytes = pread(s->src_fd, buffer, hdrsize, 0);

    if (bytes < 0)
    {
	copyfile_debug(1, "pread returned: %d", bytes);
	error = -1;
	goto exit;
    }
    if (bytes < hdrsize)
    {
	copyfile_debug(1,
	    "pread couldn't read entire header: %d of %d",
	    (int)bytes, (int)s->sb.st_size);
	error = -1;
	goto exit;
    }
    adhdr = (apple_double_header_t *)buffer;

    /*
     * Check for Apple Double file. 
     */
    if ((size_t)bytes < sizeof(apple_double_header_t) - 2 ||
	SWAP32(adhdr->magic) != ADH_MAGIC ||
	SWAP32(adhdr->version) != ADH_VERSION ||
	SWAP16(adhdr->numEntries) != 2 ||
	SWAP32(adhdr->entries[0].type) != AD_FINDERINFO)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("Not a valid Apple Double header");
	error = -1;
	goto exit;
    }
    swap_adhdr(adhdr);

    /*
     * Remove any extended attributes on the target.
     */

    if (COPYFILE_XATTR & s->flags)
    {
	if ((bytes = flistxattr(s->dst_fd, 0, 0, 0)) > 0)
	{
	    char *namebuf, *name;

	    if ((namebuf = (char*) malloc(bytes)) == NULL)
	    {
		s->err = ENOMEM;
		goto exit;
	    }
	    bytes = flistxattr(s->dst_fd, namebuf, bytes, 0);

	    if (bytes > 0)
		for (name = namebuf; name < namebuf + bytes; name += strlen(name) + 1)
		    (void)fremovexattr(s->dst_fd, name, 0);

	    free(namebuf);
	}
	else if (bytes < 0)
	{
	    if (errno != ENOTSUP && errno != EPERM)
	    goto exit;
	}
    }

    /*
     * Extract the extended attributes.
     *
     * >>>  WARNING <<<
     * This assumes that the data is already in memory (not
     * the case when there are lots of attributes or one of
     * the attributes is very large.
     */
    if (adhdr->entries[0].length > FINDERINFOSIZE)
    {
	attr_header_t *attrhdr;
	attr_entry_t *entry;
	int count;
	int i;

	if ((size_t)hdrsize < sizeof(attr_header_t)) {
		copyfile_warn("bad attribute header:  %u < %u", hdrsize, sizeof(attr_header_t));
		error = -1;
		goto exit;
	}

	attrhdr = (attr_header_t *)buffer;
	swap_attrhdr(attrhdr);
	if (attrhdr->magic != ATTR_HDR_MAGIC)
	{
	    if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("bad attribute header");
	    error = -1;
	    goto exit;
	}
	count = attrhdr->num_attrs;
	entry = (attr_entry_t *)&attrhdr[1];

	for (i = 0; i < count; i++)
	{
	    void * dataptr;

	    /*
	     * First we do some simple sanity checking.
	     * +) See if entry is within the buffer's range;
	     *
	     * +) Check the attribute name length; if it's longer than the
	     * maximum, we truncate it down.  (We could error out as well;
	     * I'm not sure which is the better way to go here.)
	     *
	     * +) If, given the name length, it goes beyond the end of
	     * the buffer, error out.
	     *
	     * +) If the last byte isn't a NUL, make it a NUL.  (Since we
	     * truncated the name length above, we truncate the name here.)
	     *
	     * +) If entry->offset is so large that it causes dataptr to
	     * go beyond the end of the buffer -- or, worse, so large that
	     * it wraps around! -- we error out.
	     *
	     * +) If entry->length would cause the entry to go beyond the
	     * end of the buffer (or, worse, wrap around to before it),
	     * *or* if the length is larger than the hdrsize, we error out.
	     * (An explanation of that:  what we're checking for there is
	     * the small range of values such that offset+length would cause
	     * it to go beyond endptr, and then wrap around past buffer.  We
	     * care about this because we are passing entry->length down to
	     * fgetxattr() below, and an erroneously large value could cause
	     * problems there.  By making sure that it's less than hdrsize,
	     * which has already been sanity-checked above, we're safe.
	     * That may mean that the check against < buffer is unnecessary.)
	     */
	    if ((void*)entry >= endptr || (void*)entry < buffer) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		error = -1;
		s->err = EINVAL;
		goto exit;
	    }

	    if (((char*)entry + sizeof(*entry)) > (char*)endptr) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		error = -1;
		s->err = EINVAL;
		goto exit;
	    }

	    if (entry->namelen < 2) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Corrupt attribute entry (only %d bytes)", entry->namelen);
		    error = -1;
		    s->err = EINVAL;
		    goto exit;
	    }

	    if (entry->namelen > XATTR_MAXNAMELEN + 1) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Corrupt attribute entry (name length is %d bytes)", entry->namelen);
		error = -1;
		s->err = EINVAL;
		goto exit;
	    }

	    if ((void*)(entry->name + entry->namelen) > endptr) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		error = -1;
		s->err = EINVAL;
		goto exit;
	    }

	    /* Because namelen includes the NUL, we check one byte back */
	    if (entry->name[entry->namelen-1] != 0) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Corrupt attribute entry (name is not NUL-terminated)");
		error = -1;
		s->err = EINVAL;
		goto exit;
	    }

	    copyfile_debug(3, "extracting \"%s\" (%d bytes) at offset %u",
		entry->name, entry->length, entry->offset);

	    dataptr = (char *)attrhdr + entry->offset;

	    if (dataptr > endptr || dataptr < buffer) {
		copyfile_debug(1, "Entry %d overflows:  offset = %u", entry->offset);
		error = -1;
		s->err = EINVAL;	/* Invalid buffer */
		goto exit;
	    }
	    if (((char*)dataptr + entry->length) > (char*)endptr ||
		(((char*)dataptr + entry->length) < (char*)buffer) ||
		(entry->length > (size_t)hdrsize)) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		copyfile_debug(1, "Entry %d length overflows:  offset = %u, length = %u",
			entry->offset, entry->length);
		error = -1;
		s->err = EINVAL;	/* Invalid buffer */
		goto exit;
	    }

	    if (strcmp((char*)entry->name, XATTR_QUARANTINE_NAME) == 0)
	    {
		qtn_file_t tqinfo = NULL;

		if (s->qinfo == NULL)
		{
		    tqinfo = qtn_file_alloc();
		    if (tqinfo)
		    {
			int x;
			if ((x = qtn_file_init_with_data(tqinfo, dataptr, entry->length)) != 0)
			{
			    copyfile_warn("qtn_file_init_with_data failed: %s", qtn_error(x));
			    qtn_file_free(tqinfo);
			    tqinfo = NULL;
			}
		    }
		}
		else
		{
		    tqinfo = s->qinfo;
		}
		if (tqinfo)
		{
			int x;
			x = qtn_file_apply_to_fd(tqinfo, s->dst_fd);
			if (x != 0)
			    copyfile_warn("qtn_file_apply_to_fd failed: %s", qtn_error(x));
		}
		if (tqinfo && !s->qinfo)
		{
		    qtn_file_free(tqinfo);
		}
	    }
	    /* Look for ACL data */
	    else if (COPYFILE_ACL & s->flags && strcmp((char*)entry->name, XATTR_SECURITY_NAME) == 0)
	    {
		acl_t acl;
		struct stat sb;
		int retry = 1;
		char *tcp = dataptr;

		/*
		 * acl_from_text() requires a NUL-terminated string.  The ACL EA,
		 * however, may not be NUL-terminated.  So in that case, we need to
		 * copy it to a +1 sized buffer, to ensure it's got a terminated string.
		 */
		if (tcp[entry->length - 1] != 0) {
			char *tmpstr = malloc(entry->length + 1);
			if (tmpstr == NULL) {
				error = -1;
				goto exit;
			}
			strlcpy(tmpstr, tcp, entry->length + 1);
			acl = acl_from_text(tmpstr);
			free(tmpstr);
		} else {
			acl = acl_from_text(tcp);
		}

		if (acl != NULL)
		{
		    filesec_t fsec_tmp;

		    if ((fsec_tmp = filesec_init()) == NULL)
			error = -1;
		    else if((error = fstatx_np(s->dst_fd, &sb, fsec_tmp)) < 0)
			error = -1;
		    else if (filesec_set_property(fsec_tmp, FILESEC_ACL, &acl) < 0)
			error = -1;
		    else {
			while (fchmodx_np(s->dst_fd, fsec_tmp) < 0)
			{
			    if (errno == ENOTSUP)
			    {
				    if (retry && !copyfile_unset_acl(s))
				    {
					retry = 0;
					continue;
				    }
			    }
			    copyfile_warn("setting security information");
			    error = -1;
			    break;
			}
		    }
		    acl_free(acl);
		    filesec_free(fsec_tmp);

		    if (error == -1)
			goto exit;
		}
	    }
	    /* And, finally, everything else */
	    else if (COPYFILE_XATTR & s->flags) {
		 if (fsetxattr(s->dst_fd, (char *)entry->name, dataptr, entry->length, 0, 0) == -1) {
			if (COPYFILE_VERBOSE & s->flags)
				copyfile_warn("error %d setting attribute %s", error, entry->name);
			error = -1;
			goto exit;
		}
	    }
	    entry = ATTR_NEXT(entry);
	}
    }

    /*
     * Extract the Finder Info.
     */
    if (adhdr->entries[0].offset > (hdrsize - sizeof(emptyfinfo))) {
	error = -1;
	goto exit;
    }

    if (bcmp((u_int8_t*)buffer + adhdr->entries[0].offset, emptyfinfo, sizeof(emptyfinfo)) != 0)
    {
	copyfile_debug(3, " extracting \"%s\" (32 bytes)", XATTR_FINDERINFO_NAME);
	error = fsetxattr(s->dst_fd, XATTR_FINDERINFO_NAME, (u_int8_t*)buffer + adhdr->entries[0].offset, sizeof(emptyfinfo), 0, 0);
	if (error)
	    goto exit;
    }

    /*
     * Extract the Resource Fork.
     */
    if (adhdr->entries[1].type == AD_RESOURCE &&
	adhdr->entries[1].length > 0)
    {
	void * rsrcforkdata = NULL;
	size_t length;
	off_t offset;
	struct stat sb;
	struct timeval tval[2];

	length = adhdr->entries[1].length;
	offset = adhdr->entries[1].offset;
	rsrcforkdata = malloc(length);

	if (rsrcforkdata == NULL) {
		copyfile_debug(1, "could not allocate %u bytes for rsrcforkdata",
			length);
		error = -1;
		goto bad;
	}

	if (fstat(s->dst_fd, &sb) < 0)
	{
		copyfile_debug(1, "couldn't stat destination file");
		error = -1;
		goto bad;
	}

	bytes = pread(s->src_fd, rsrcforkdata, length, offset);
	if (bytes < (ssize_t)length)
	{
	    if (bytes == -1)
	    {
		copyfile_debug(1, "couldn't read resource fork");
	    }
	    else
	    {
		copyfile_debug(1,
		    "couldn't read resource fork (only read %d bytes of %d)",
		    (int)bytes, (int)length);
	    }
	    error = -1;
	    goto bad;
	}
	error = fsetxattr(s->dst_fd, XATTR_RESOURCEFORK_NAME, rsrcforkdata, bytes, 0, 0);
	if (error)
	{
	     /*
	      * For filesystems that do not natively support named attributes,
	      * the kernel creates an AppleDouble file that -- for compatabilty
	      * reasons -- has a resource fork containing nothing but a rsrcfork_header_t
	      * structure that says there are no resources.  So, if fsetxattr has
	      * failed, and the resource fork is that empty structure, *and* the
	      * target file is a directory, then we do nothing with it.
	      */
	    if ((bytes == sizeof(rsrcfork_header_t)) &&
		((sb.st_mode & S_IFMT) == S_IFDIR)  &&
		(memcmp(rsrcforkdata, &empty_rsrcfork_header, bytes) == 0)) {
		    copyfile_debug(2, "not setting empty resource fork on directory");
		    error = errno = 0;
		    goto bad;
	    }
	    copyfile_debug(1, "error %d setting resource fork attribute", error);
	    error = -1;
	    goto bad;
	}
	copyfile_debug(3, "extracting \"%s\" (%d bytes)",
		    XATTR_RESOURCEFORK_NAME, (int)length);

	if (!(s->flags & COPYFILE_STAT))
	{
	    tval[0].tv_sec = sb.st_atime;
	    tval[1].tv_sec = sb.st_mtime;
	    tval[0].tv_usec = tval[1].tv_usec = 0;

	    if (futimes(s->dst_fd, tval))
		copyfile_warn("%s: set times", s->dst ? s->dst : "(null dst)");
	}
bad:
	if (rsrcforkdata)
	    free(rsrcforkdata);
    }

    if (COPYFILE_STAT & s->flags)
    {
	error = copyfile_stat(s);
    }
exit:
    if (buffer) free(buffer);
    return error;
}

static int copyfile_pack_quarantine(copyfile_state_t s, void **buf, ssize_t *len)
{
    int ret = 0;
    char qbuf[QTN_SERIALIZED_DATA_MAX];
    size_t qlen = sizeof(qbuf);

    if (s->qinfo == NULL)
    {
	ret = -1;
	goto done;
    }

    if (qtn_file_to_data(s->qinfo, qbuf, &qlen) != 0)
    {
	ret = -1;
	goto done;
    }

    *buf = malloc(qlen);
    if (*buf)
    {
	memcpy(*buf, qbuf, qlen);
	*len = qlen;
    }
done:
    return ret;
}

static int copyfile_pack_acl(copyfile_state_t s, void **buf, ssize_t *len)
{
    int ret = 0;
    acl_t acl = NULL;
    char *acl_text;

    if (filesec_get_property(s->fsec, FILESEC_ACL, &acl) < 0)
    {
	if (errno != ENOENT)
	{
	    ret = -1;
	    if (COPYFILE_VERBOSE & s->flags)
		copyfile_warn("getting acl");
	}
	*len = 0;
	goto exit;
    }

    if ((acl_text = acl_to_text(acl, len)) != NULL)
    {
	/*
	 * acl_to_text() doesn't include the NUL at the endo
	 * in it's count (*len).  It does, however, promise to
	 * return a valid C string, so we need to up the count
	 * by 1.
	 */
	*len = *len + 1;
	*buf = malloc(*len);
	if (*buf)
	    memcpy(*buf, acl_text, *len);
	else
	    *len = 0;
	acl_free(acl_text);
    }
    copyfile_debug(2, "copied acl (%ld) %p", *len, *buf);
exit:
    if (acl)
	acl_free(acl);
    return ret;
}

static int copyfile_pack_rsrcfork(copyfile_state_t s, attr_header_t *filehdr)
{
    ssize_t datasize;
    char *databuf = NULL;
    int ret = 0;

    /* Get the resource fork size */
    if ((datasize = fgetxattr(s->src_fd, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0)) < 0)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("skipping attr \"%s\" due to error %d", XATTR_RESOURCEFORK_NAME, errno);
	return -1;
    }

    if (datasize > INT_MAX) {
	s->err = EINVAL;
	ret = -1;
	goto done;
    }

    if ((databuf = malloc(datasize)) == NULL)
    {
	copyfile_warn("malloc");
	ret = -1;
	goto done;
    }

    if (fgetxattr(s->src_fd, XATTR_RESOURCEFORK_NAME, databuf, datasize, 0, 0) != datasize)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("couldn't read entire resource fork");
	ret = -1;
	goto done;
    }

    /* Write the resource fork to disk. */
    if (pwrite(s->dst_fd, databuf, datasize, filehdr->appledouble.entries[1].offset) != datasize)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("couldn't write resource fork");
    }
    copyfile_debug(3, "copied %d bytes of \"%s\" data @ offset 0x%08x",
	datasize, XATTR_RESOURCEFORK_NAME, filehdr->appledouble.entries[1].offset);
    filehdr->appledouble.entries[1].length = (u_int32_t)datasize;

done:
    if (databuf)
	free(databuf);

    return ret;
}

/*
 * The opposite of copyfile_unpack(), obviously.
 */
static int copyfile_pack(copyfile_state_t s)
{
    char *attrnamebuf = NULL, *endnamebuf;
    void *databuf = NULL;
    attr_header_t *filehdr, *endfilehdr;
    attr_entry_t *entry;
    ssize_t listsize = 0;
    char *nameptr;
    size_t namelen;
    size_t entrylen;
    ssize_t datasize;
    size_t offset = 0;
    int hasrsrcfork = 0;
    int error = 0;
    int seenq = 0;	// Have we seen any quarantine info already?

    filehdr = (attr_header_t *) calloc(1, ATTR_MAX_SIZE);
    if (filehdr == NULL) {
	error = -1;
	goto exit;
    } else {
	    endfilehdr = (attr_header_t*)(((char*)filehdr) + ATTR_MAX_SIZE);
    }

    attrnamebuf = calloc(1, ATTR_MAX_HDR_SIZE);
    if (attrnamebuf == NULL) {
	error = -1;
	goto exit;
    } else {
	endnamebuf = ((char*)attrnamebuf) + ATTR_MAX_HDR_SIZE;
    }

    /*
     * Fill in the Apple Double Header defaults.
     */
    filehdr->appledouble.magic              = ADH_MAGIC;
    filehdr->appledouble.version            = ADH_VERSION;
    filehdr->appledouble.numEntries         = 2;
    filehdr->appledouble.entries[0].type    = AD_FINDERINFO;
    filehdr->appledouble.entries[0].offset  = (u_int32_t)offsetof(apple_double_header_t, finfo);
    filehdr->appledouble.entries[0].length  = FINDERINFOSIZE;
    filehdr->appledouble.entries[1].type    = AD_RESOURCE;
    filehdr->appledouble.entries[1].offset  = (u_int32_t)offsetof(apple_double_header_t, pad);
    filehdr->appledouble.entries[1].length  = 0;
    bcopy(ADH_MACOSX, filehdr->appledouble.filler, sizeof(filehdr->appledouble.filler));

    /*
     * Fill in the initial Attribute Header.
     */
    filehdr->magic       = ATTR_HDR_MAGIC;
    filehdr->debug_tag   = (u_int32_t)s->sb.st_ino;
    filehdr->data_start  = (u_int32_t)sizeof(attr_header_t);

    /*
     * Collect the attribute names.
     */
    entry = (attr_entry_t *)((char *)filehdr + sizeof(attr_header_t));

    /*
     * Test if there are acls to copy
     */
    if (COPYFILE_ACL & s->flags)
    {
	acl_t temp_acl = NULL;
	if (filesec_get_property(s->fsec, FILESEC_ACL, &temp_acl) < 0)
	{
	    copyfile_debug(2, "no acl entries found (errno = %d)", errno);
	} else
	{
	    offset = strlen(XATTR_SECURITY_NAME) + 1;
	    strcpy(attrnamebuf, XATTR_SECURITY_NAME);
	}
	if (temp_acl)
	    acl_free(temp_acl);
    }

    if (COPYFILE_XATTR & s->flags)
    {
	ssize_t left = ATTR_MAX_HDR_SIZE - offset;
        if ((listsize = flistxattr(s->src_fd, attrnamebuf + offset, left, 0)) <= 0)
	{
	    copyfile_debug(2, "no extended attributes found (%d)", errno);
	}
	if (listsize > left)
	{
	    copyfile_debug(1, "extended attribute list too long");
	    listsize = left;
	}

	listsize += offset;
	endnamebuf = attrnamebuf + listsize;
	if (endnamebuf > (attrnamebuf + ATTR_MAX_HDR_SIZE)) {
		error = -1;
		goto exit;
	}

	for (nameptr = attrnamebuf; nameptr < endnamebuf; nameptr += namelen)
	{
	    namelen = strlen(nameptr) + 1;
	    /* Skip over FinderInfo or Resource Fork names */
	    if (strcmp(nameptr, XATTR_FINDERINFO_NAME) == 0 ||
		strcmp(nameptr, XATTR_RESOURCEFORK_NAME) == 0) {
		    continue;
	    }
	    if (strcmp(nameptr, XATTR_QUARANTINE_NAME) == 0) {
		seenq = 1;
	    }

	    /* The system should prevent this from happening, but... */
	    if (namelen > XATTR_MAXNAMELEN + 1) {
		namelen = XATTR_MAXNAMELEN + 1;
	    }
	    entry->namelen = namelen;
	    entry->flags = 0;
	    if (nameptr + namelen > endnamebuf) {
		error = -1;
		goto exit;
	    }
	    bcopy(nameptr, &entry->name[0], namelen);
	    copyfile_debug(2, "copied name [%s]", entry->name);

	    entrylen = ATTR_ENTRY_LENGTH(namelen);
	    entry = (attr_entry_t *)(((char *)entry) + entrylen);

	    if ((void*)entry >= (void*)endfilehdr) {
		    error = -1;
		    goto exit;
	    }

	    /* Update the attributes header. */
	    filehdr->num_attrs++;
	    filehdr->data_start += (u_int32_t)entrylen;
	}
    }

    /*
     * If we have any quarantine data, we always pack it.
     * But if we've already got it in the EA list, don't put it in again.
     */
    if (s->qinfo && !seenq)
    {
	ssize_t left = ATTR_MAX_HDR_SIZE - offset;
	/* strlcpy returns number of bytes copied, but we need offset to point to the next byte */
	offset += strlcpy(attrnamebuf + offset, XATTR_QUARANTINE_NAME, left) + 1;
    }

    seenq = 0;
    /*
     * Collect the attribute data.
     */
    entry = (attr_entry_t *)((char *)filehdr + sizeof(attr_header_t));

    for (nameptr = attrnamebuf; nameptr < attrnamebuf + listsize; nameptr += namelen + 1)
    {
	namelen = strlen(nameptr);

	if (strcmp(nameptr, XATTR_SECURITY_NAME) == 0)
	    copyfile_pack_acl(s, &databuf, &datasize);
	else if (s->qinfo && strcmp(nameptr, XATTR_QUARANTINE_NAME) == 0)
	{
	    copyfile_pack_quarantine(s, &databuf, &datasize);
	}
	/* Check for Finder Info. */
	else if (strcmp(nameptr, XATTR_FINDERINFO_NAME) == 0)
	{
	    datasize = fgetxattr(s->src_fd, nameptr, (u_int8_t*)filehdr + filehdr->appledouble.entries[0].offset, 32, 0, 0);
	    if (datasize < 0)
	    {
		    if (COPYFILE_VERBOSE & s->flags)
			copyfile_warn("skipping attr \"%s\" due to error %d", nameptr, errno);
	    } else if (datasize != 32)
	    {
		    if (COPYFILE_VERBOSE & s->flags)
			copyfile_warn("unexpected size (%ld) for \"%s\"", datasize, nameptr);
	    } else
	    {
		    if (COPYFILE_VERBOSE & s->flags)
			copyfile_warn(" copied 32 bytes of \"%s\" data @ offset 0x%08x",
			    XATTR_FINDERINFO_NAME, filehdr->appledouble.entries[0].offset);
	    }
	    continue;  /* finder info doesn't have an attribute entry */
	}
	/* Check for Resource Fork. */
	else if (strcmp(nameptr, XATTR_RESOURCEFORK_NAME) == 0)
	{
	    hasrsrcfork = 1;
	    continue;
	} else
	{
	    /* Just a normal attribute. */
	    datasize = fgetxattr(s->src_fd, nameptr, NULL, 0, 0, 0);
	    if (datasize == 0)
		goto next;
	    if (datasize < 0)
	    {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("skipping attr \"%s\" due to error %d", nameptr, errno);
		goto next;
	    }
	    if (datasize > XATTR_MAXATTRLEN)
	    {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("skipping attr \"%s\" (too big)", nameptr);
		goto next;
	    }
	    databuf = malloc(datasize);
	    if (databuf == NULL) {
		error = -1;
		continue;
	    }
	    datasize = fgetxattr(s->src_fd, nameptr, databuf, datasize, 0, 0);
	}

	entry->length = (u_int32_t)datasize;
	entry->offset = filehdr->data_start + filehdr->data_length;

	filehdr->data_length += (u_int32_t)datasize;
	/*
	 * >>>  WARNING <<<
	 * This assumes that the data is fits in memory (not
	 * the case when there are lots of attributes or one of
	 * the attributes is very large.
	 */
	if (entry->offset > ATTR_MAX_SIZE ||
		(entry->offset + datasize > ATTR_MAX_SIZE)) {
		error = 1;
	} else {
		bcopy(databuf, (char*)filehdr + entry->offset, datasize);
	}
	free(databuf);

	copyfile_debug(3, "copied %ld bytes of \"%s\" data @ offset 0x%08x", datasize, nameptr, entry->offset);
next:
	/* bump to next entry */
	entrylen = ATTR_ENTRY_LENGTH(entry->namelen);
	entry = (attr_entry_t *)((char *)entry + entrylen);
    }

    if (filehdr->data_length > 0)
    {
	/* Now we know where the resource fork data starts. */
	filehdr->appledouble.entries[1].offset = (filehdr->data_start + filehdr->data_length);

	/* We also know the size of the "Finder Info entry. */
	filehdr->appledouble.entries[0].length =
	    filehdr->appledouble.entries[1].offset - filehdr->appledouble.entries[0].offset;

	filehdr->total_size  = filehdr->appledouble.entries[1].offset;
    }

    /* Copy Resource Fork. */
    if (hasrsrcfork && (error = copyfile_pack_rsrcfork(s, filehdr)))
	goto exit;

    /* Write the header to disk. */
    datasize = filehdr->appledouble.entries[1].offset;

    swap_adhdr(&filehdr->appledouble);
    swap_attrhdr(filehdr);

    if (pwrite(s->dst_fd, filehdr, datasize, 0) != datasize)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("couldn't write file header");
	error = -1;
	goto exit;
    }
exit:
    if (filehdr) free(filehdr);
    if (attrnamebuf) free(attrnamebuf);

    if (error)
	return error;
    else
	return copyfile_stat(s);
}
