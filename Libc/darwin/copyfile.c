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
#include <sys/acl.h>
#include <libkern/OSByteOrder.h>
#include <membership.h>

#include <copyfile.h>

struct _copyfile_state
{
    char *src;
    char *dst;
    int src_fd;
    int dst_fd;
    struct stat sb;
    filesec_t fsec;
    copyfile_flags_t flags;
    void *stats;
    uint32_t debug;
};

static int copyfile_open	(copyfile_state_t);
static int copyfile_close	(copyfile_state_t);
static int copyfile_data	(copyfile_state_t);
static int copyfile_stat	(copyfile_state_t);
static int copyfile_security	(copyfile_state_t);
static int copyfile_xattr	(copyfile_state_t);
static int copyfile_pack	(copyfile_state_t);
static int copyfile_unpack	(copyfile_state_t);

static copyfile_flags_t copyfile_check	(copyfile_state_t);
static int copyfile_fix_perms(copyfile_state_t, filesec_t *, int);

#define COPYFILE_DEBUG (1<<31)

#ifndef _COPYFILE_TEST
# define copyfile_warn(str, ...) syslog(LOG_WARNING, str ": %m", ## __VA_ARGS__)
# define copyfile_debug(d, str, ...) \
    if (s && (d <= s->debug)) {\
	syslog(LOG_DEBUG, "%s:%d:%s() " str "\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__); \
    } else
#else
#define copyfile_warn(str, ...) \
    fprintf(stderr, "%s:%d:%s() " str ": %s\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__, (errno) ? strerror(errno) : "")
# define copyfile_debug(d, str, ...) \
    if (s && (d <= s->debug)) {\
	fprintf(stderr, "%s:%d:%s() " str "\n", __FILE__, __LINE__ , __FUNCTION__, ## __VA_ARGS__); \
    } else
#endif

int copyfile(const char *src, const char *dst, copyfile_state_t state, copyfile_flags_t flags)
{
    int ret = 0;
    copyfile_state_t s = state;
    filesec_t original_fsec;
    int fix_perms = 0;

    original_fsec = filesec_init();
    if (s == NULL && (s = copyfile_init()) == NULL)
	return -1;

    if (src != NULL)
    {
	if (s->src_fd != -2 && s->src && !strncmp(src, s->src, MAXPATHLEN))
	    close(s->src_fd);
	s->src = strdup(src);
    }

    if (dst != NULL)
    {
	if (s->dst_fd != -2 && s->dst && !strncmp(dst, s->dst, MAXPATHLEN))
	    close(s->dst_fd);
	s->dst = strdup(dst);
    }

    s->flags = flags;

    if (COPYFILE_DEBUG & s->flags)
    {
	char *e;
	if ((e = getenv("COPYFILE_DEBUG")))
	{
	    s->debug = strtol(e, NULL, 0);
	    if (s->debug < 1)
		s->debug = 1;
	}
	copyfile_debug(1, "debug value set to: %d\n", s->debug);
    }
    if (COPYFILE_CHECK & flags)
	return copyfile_check(s);

    if (copyfile_open(s) < 0)
	ret = -1;
    else
    {
	if (s->dst_fd == -2 || s->src_fd == -2)
	    return 0;

	if (COPYFILE_PACK & flags)
	{
	    if (copyfile_pack(s) < 0)
	    {
		unlink(s->dst);
		ret = -1;
	    }
	} else if (COPYFILE_UNPACK & flags)
	{
	    if (!(COPYFILE_STAT & flags || COPYFILE_ACL & flags))
		fix_perms = !copyfile_fix_perms(s, &original_fsec, 1);

	    if (copyfile_unpack(s) < 0)
		ret = -1;
	} else
	{
	    if (COPYFILE_SECURITY & flags)
	    {
		if (copyfile_security(s) < 0)
		{
		    copyfile_warn("error processing security information");
		    ret -= 1;
		}
	    } else if (COPYFILE_UNPACK & flags)
	    {
		fix_perms = !copyfile_fix_perms(s, &original_fsec, 1);
		if (copyfile_unpack(s) < 0)
		    ret = -1;
	    } else
	    {
		if (COPYFILE_SECURITY & flags)
		{
		    copyfile_warn("error processing stat information");
		    ret -= 1;
		}
	    }
	    fix_perms = !copyfile_fix_perms(s, &original_fsec, 1);

	    if (COPYFILE_XATTR & flags)
	    {
		if (copyfile_xattr(s) < 0)
		{
		    copyfile_warn("error processing extended attributes");
		    ret -= 1;
		}
	    }
	    if (COPYFILE_DATA & flags)
	    {
		if (copyfile_data(s) < 0)
		{
		    copyfile_warn("error processing data");
		    ret = -1;
		    if (s->dst && unlink(s->dst))
			    copyfile_warn("%s: remove", s->src);
		    goto exit;
		}
	    }
	}
    }
exit:
    if (fix_perms)
	copyfile_fix_perms(s, &original_fsec, 0);

    filesec_free(original_fsec);

    if (state == NULL)
	ret -= copyfile_free(s);
    return ret;
}

copyfile_state_t copyfile_init(void)
{
    copyfile_state_t s = (copyfile_state_t) calloc(1, sizeof(struct _copyfile_state));

    if (s != NULL)
    {
	s->src_fd = -2;
	s->dst_fd = -2;
	s->fsec = filesec_init();
    }

    return s;
}

int copyfile_free(copyfile_state_t s)
{
    if (s != NULL)
    {
	if (s->fsec)
	    filesec_free(s->fsec);

	if (s->dst)
	    free(s->dst);
	if (s->src)
	    free(s->src);
	if (copyfile_close(s) < 0)
	{
	    copyfile_warn("error closing files");
	    return -1;
	}
	free(s);
    }
    return 0;
}

static int copyfile_close(copyfile_state_t s)
{
    if (s->src_fd != -2)
	close(s->src_fd);

    if (s->dst_fd != -2 && close(s->dst_fd))
    {
	copyfile_warn("close on %s", s->dst);
	return -1;
    }
    return 0;
}

static int copyfile_fix_perms(copyfile_state_t s, filesec_t *fsec, int on)
{
    filesec_t tmp_fsec;
    struct stat sb;
    mode_t mode;
    acl_t acl;

    if (on)
    {
	if(statx_np(s->dst, &sb, *fsec))
	    goto error;

	tmp_fsec = filesec_dup(*fsec);

	if (!filesec_get_property(tmp_fsec, FILESEC_ACL, &acl))
	{
	    acl_entry_t entry;
	    acl_permset_t permset;
	    uuid_t qual;

	    if (mbr_uid_to_uuid(getuid(), qual) != 0)
		goto error;

	    if (acl_create_entry_np(&acl, &entry, ACL_FIRST_ENTRY) == -1)
		goto error;
	    if (acl_get_permset(entry, &permset) == -1)
		goto error;
	    if (acl_clear_perms(permset) == -1)
		goto error;
	    if (acl_add_perm(permset, ACL_WRITE_DATA) == -1)
		goto error;
	    if (acl_add_perm(permset, ACL_WRITE_ATTRIBUTES) == -1)
		goto error;
	    if (acl_add_perm(permset, ACL_WRITE_EXTATTRIBUTES) == -1)
		goto error;
	    if (acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) == -1)
		goto error;

	    if(acl_set_permset(entry, permset) == -1)
		goto error;
	    if(acl_set_qualifier(entry, qual) == -1)
		goto error;

	    if (filesec_set_property(tmp_fsec, FILESEC_ACL, &acl) != 0)
		goto error;
	}

	if (filesec_get_property(tmp_fsec, FILESEC_MODE, &mode) == 0)
	{
	    if (~mode & S_IWUSR)
	    {
		mode |= S_IWUSR;
		if (filesec_set_property(tmp_fsec, FILESEC_MODE, &mode) != 0)
		    goto error;
	    }
	}
	if (fchmodx_np(s->dst_fd, tmp_fsec) < 0 && errno != ENOTSUP)
	    copyfile_warn("setting security information");
	filesec_free(tmp_fsec);
    } else
	if (fchmodx_np(s->dst_fd, *fsec) < 0 && errno != ENOTSUP)
	    copyfile_warn("setting security information");

    return 0;
error:
    filesec_free(*fsec);
    return -1;
}


static int copyfile_open(copyfile_state_t s)
{
    int oflags = O_EXCL | O_CREAT;

    if (s->src && s->src_fd == -2)
    {
	if ((COPYFILE_NOFOLLOW_SRC & s->flags ? lstatx_np : statx_np)
		(s->src, &s->sb, s->fsec))
	{
	    copyfile_warn("stat on %s", s->src);
	    return -1;
	}
	if ((s->src_fd = open(s->src, O_RDONLY, 0)) < 0)
	{
		copyfile_warn("open on %s", s->src);
		return -1;
	}
    }

    if (s->dst && s->dst_fd == -2)
    {
	if (COPYFILE_DATA & s->flags || COPYFILE_PACK & s->flags)
	    oflags |= O_WRONLY;

	if (COPYFILE_ACL & ~s->flags)
	{
	    if (filesec_set_property(s->fsec, FILESEC_ACL, NULL) == -1)
	    {
		copyfile_debug(1, "unsetting acl attribute on %s", s->dst);
	    }
	}
	if (COPYFILE_STAT & ~s->flags)
	{
	    if (filesec_set_property(s->fsec, FILESEC_MODE, NULL) == -1)
	    {
		copyfile_debug(1, "unsetting mode attribute on %s", s->dst);
	    }
	}
	if (COPYFILE_PACK & s->flags)
	{
	    mode_t m = S_IRUSR;
	    if (filesec_set_property(s->fsec, FILESEC_MODE, &m) == -1)
	    {
		mode_t m = S_IRUSR | S_IWUSR;
		if (filesec_set_property(s->fsec, FILESEC_MODE, &m) == -1)
		{
		    copyfile_debug(1, "setting mode attribute on %s", s->dst);
		}
	    }
	    if (filesec_set_property(s->fsec, FILESEC_OWNER, NULL) == -1)
	    {
		copyfile_debug(1, "unsetting uid attribute on %s", s->dst);
	    }
	    if (filesec_set_property(s->fsec, FILESEC_UUID, NULL) == -1)
	    {
		copyfile_debug(1, "unsetting uuid attribute on %s", s->dst);
	    }
	    if (filesec_set_property(s->fsec, FILESEC_GROUP, NULL) == -1)
	    {
		copyfile_debug(1, "unsetting gid attribute on %s", s->dst);
	    }
	}

	if (COPYFILE_UNLINK & s->flags && unlink(s->dst) < 0)
	{
	    copyfile_warn("%s: remove", s->dst);
	    return -1;
	}

	while((s->dst_fd = openx_np(s->dst, oflags, s->fsec)) < 0)
	{
	    if (EEXIST == errno)
	    {
		oflags = oflags & ~O_CREAT;
		continue;
	    }
	    copyfile_warn("open on %s", s->dst);
	    return -1;
	}
    }
    return 0;
}

static copyfile_flags_t copyfile_check(copyfile_state_t s)
{
    acl_t acl;
    copyfile_flags_t ret = 0;

    if (!s->src)
	return ret;

    // check EAs
    if (COPYFILE_XATTR & s->flags)
	if (listxattr(s->src, 0, 0, 0) > 0)
	    ret |= COPYFILE_XATTR;

    if (COPYFILE_ACL & s->flags)
    {
	(COPYFILE_NOFOLLOW_SRC & s->flags ? lstatx_np : statx_np)
		(s->src, &s->sb, s->fsec);

	if (filesec_get_property(s->fsec, FILESEC_ACL, &acl) == 0)
	    ret |= COPYFILE_ACL;
    }

    return ret;
}

static int copyfile_data(copyfile_state_t s)
{
    unsigned int blen;
    char *bp;
    int nread;
    int ret;

    if ((bp = malloc((size_t)s->sb.st_blksize)) == NULL)
    {
	blen = 0;
	warnx("malloc failed");
	return -1;
    }
    blen = s->sb.st_blksize;

    while ((nread = read(s->src_fd, bp, (size_t)blen)) > 0)
    {
	if (write(s->dst_fd, bp, (size_t)nread) != nread)
	{
	    copyfile_warn("writing to %s", s->dst);
	    return -1;
	}
    }
    if (nread < 0)
    {
	copyfile_warn("reading from %s", s->src);
	ret = -1;
    }

    free(bp);

    if (ftruncate(s->dst_fd, s->sb.st_size) < 0)
	ret = -1;

    return ret;
}

static int copyfile_security(copyfile_state_t s)
{
    filesec_t fsec_dst = filesec_init();

    int copied = 0;
    acl_flagset_t flags;
    struct stat sb;
    acl_entry_t entry_src = NULL, entry_dst = NULL;
    acl_t acl_src, acl_dst;
    int inited_dst = 0, inited_src = 0, ret = 0;

    if (COPYFILE_ACL & s->flags)
    {
	if(fstatx_np(s->dst_fd, &sb, fsec_dst))
	{
	    goto cleanup;
	}

	if (filesec_get_property(fsec_dst, FILESEC_ACL, &acl_dst))
	{
	    if (errno == ENOENT)
	    {
		acl_dst = acl_init(4);
		inited_dst = 1;
	    }
	    else
	    {
		ret = -1;
		goto cleanup;
	    }
	}

	if (filesec_get_property(s->fsec, FILESEC_ACL, &acl_src))
	{
	    if (errno == ENOENT)
	    {
		if (inited_dst)
		    goto no_acl;
		acl_dst = acl_init(4);
		inited_src = 1;
	    }
	    else
	    {
		ret = -1;
		goto cleanup;
	    }
	}

	for (;acl_get_entry(acl_src,
	    entry_src == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY,
	    &entry_src) == 0;)
	{
	    acl_get_flagset_np(entry_src, &flags);
	    if (!acl_get_flag_np(flags, ACL_ENTRY_INHERITED))
	    {
		if ((ret = acl_create_entry(&acl_dst, &entry_dst)) == -1)
		    goto cleanup;

		if ((ret = acl_copy_entry(entry_dst, entry_src)) == -1)
		    goto cleanup;

		copyfile_debug(1, "copied acl entry from %s to %s", s->src, s->dst);
		copied++;
	    }
	}

	if (!filesec_set_property(s->fsec, FILESEC_ACL, &acl_dst))
	{
	    copyfile_debug(1, "altered acl");
	}
    }
no_acl:
    if (fchmodx_np(s->dst_fd, s->fsec) < 0 && errno != ENOTSUP)
	copyfile_warn("setting security information: %s", s->dst);

cleanup:
    filesec_free(fsec_dst);
    if (inited_src) acl_free(acl_src);
    if (inited_dst) acl_free(acl_dst);

    return ret;
}

static int copyfile_stat(copyfile_state_t s)
{
    struct timeval tval[2];
    /*
     * NFS doesn't support chflags; ignore errors unless there's reason
     * to believe we're losing bits.  (Note, this still won't be right
     * if the server supports flags and we were trying to *remove* flags
     * on a file that we copied, i.e., that we didn't create.)
     */
    if (chflags(s->dst, (u_long)s->sb.st_flags))
	if (errno != EOPNOTSUPP || s->sb.st_flags != 0)
	    copyfile_warn("%s: set flags (was: 0%07o)", s->dst, s->sb.st_flags);

    tval[0].tv_sec = s->sb.st_atime;
    tval[1].tv_sec = s->sb.st_mtime;
    tval[0].tv_usec = tval[1].tv_usec = 0;
    if (utimes(s->dst, tval))
	    copyfile_warn("%s: set times", s->dst);
    return 0;
}

static int copyfile_xattr(copyfile_state_t s)
{
    char *name;
    char *namebuf;
    size_t xa_size;
    void *xa_dataptr;
    size_t bufsize = 4096;
    ssize_t asize;
    ssize_t nsize;
    int ret = 0;
    int flags = 0;

    if (COPYFILE_NOFOLLOW_SRC & s->flags)
	flags |= XATTR_NOFOLLOW;

    /* delete EAs on destination */
    if ((nsize = flistxattr(s->dst_fd, 0, 0, 0)) > 0)
    {
	if ((namebuf = (char *) malloc(nsize)) == NULL)
	    return -1;
	else
	    nsize = flistxattr(s->dst_fd, namebuf, nsize, 0);

	if (nsize > 0)
	    for (name = namebuf; name < namebuf + nsize; name += strlen(name) + 1)
		    fremovexattr(s->dst_fd, name,flags);

	free(namebuf);
    } else if (nsize < 0)
    {
	if (errno == ENOTSUP)
	    return 0;
	else
	    return -1;
    }

    /* get name list of EAs on source */
    if ((nsize = flistxattr(s->src_fd, 0, 0, 0)) < 0)
    {
	if (errno == ENOTSUP)
	    return 0;
	else
	    return -1;
    } else if (nsize == 0)
	return 0;

    if ((namebuf = (char *) malloc(nsize)) == NULL)
	return -1;
    else
	nsize = flistxattr(s->src_fd, namebuf, nsize, 0);

    if (nsize <= 0)
	return nsize;

    if ((xa_dataptr = (void *) malloc(bufsize)) == NULL)
	return -1;

    for (name = namebuf; name < namebuf + nsize; name += strlen(name) + 1)
    {
	if ((xa_size = fgetxattr(s->src_fd, name, 0, 0, 0, flags)) < 0)
	{
	    ret = -1;
	    continue;
	}

	if (xa_size > bufsize)
	{
	    bufsize = xa_size;
	    if ((xa_dataptr =
		(void *) realloc((void *) xa_dataptr, bufsize)) == NULL)
	    {
		ret = -1;
		continue;
	    }
	}

	if ((asize = fgetxattr(s->src_fd, name, xa_dataptr, xa_size, 0, flags)) < 0)
	{
	    ret = -1;
	    continue;
	}

	if (xa_size != asize)
	    xa_size = asize;

	if (fsetxattr(s->dst_fd, name, xa_dataptr, xa_size, 0, flags) < 0)
	{
	    ret = -1;
	    continue;
	}
    }
    free((void *) xa_dataptr);
    return ret;
}


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
#define AD_REALNAME      3   /* FileÕs name on home file system */
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


#pragma options align=mac68k

#define FINDERINFOSIZE	32

typedef struct apple_double_entry
{
	u_int32_t   type;     /* entry type: see list, 0 invalid */ 
	u_int32_t   offset;   /* entry data offset from the beginning of the file. */
	u_int32_t   length;   /* entry data length in bytes. */
} apple_double_entry_t;


typedef struct apple_double_header
{
	u_int32_t   magic;         /* == ADH_MAGIC */
	u_int32_t   version;       /* format version: 2 = 0x00020000 */ 
	u_int32_t   filler[4];
	u_int16_t   numEntries;	   /* number of entries which follow */ 
	apple_double_entry_t   entries[2];  /* 'finfo' & 'rsrc' always exist */
	u_int8_t    finfo[FINDERINFOSIZE];  /* Must start with Finder Info (32 bytes) */
	u_int8_t    pad[2];        /* get better alignment inside attr_header */
} apple_double_header_t;


/* Entries are aligned on 4 byte boundaries */
typedef struct attr_entry
{
	u_int32_t   offset;    /* file offset to data */
	u_int32_t   length;    /* size of attribute data */
	u_int16_t   flags;
	u_int8_t    namelen;   /* length of name including NULL termination char */ 
	u_int8_t    name[1];   /* NULL-terminated UTF-8 name (up to 128 bytes max) */
} attr_entry_t;


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
} attr_header_t;


#pragma options align=reset

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
	for (i = 0; i < count; i++, ae++)
	{
		ae->offset = SWAP32 (ae->offset);
		ae->length = SWAP32 (ae->length);
		ae->flags  = SWAP16 (ae->flags);
	}
#endif
}

static const u_int32_t emptyfinfo[8] = {0};

static int copyfile_unpack(copyfile_state_t s)
{
    ssize_t bytes;
    void * buffer, * endptr;
    apple_double_header_t *adhdr;
    size_t hdrsize;
    int error = 0;

    if (s->sb.st_size < ATTR_MAX_HDR_SIZE)
	hdrsize = s->sb.st_size;
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
    if (bytes < sizeof(apple_double_header_t) - 2 ||
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

	if (hdrsize < sizeof(attr_header_t)) {
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
		goto exit;
	    }

	    if (((void*)entry + sizeof(*entry)) > endptr) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		error = -1;
		goto exit;
	    }

	    if (entry->namelen > ATTR_MAX_NAME_LEN) {
		entry->namelen = ATTR_MAX_NAME_LEN;
	    }
	    if ((void*)(entry->name + entry->namelen) >= endptr) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		error = -1;
		goto exit;
	    }

	    if (entry->name[entry->namelen] != 0) {
		entry->name[entry->namelen] = 0;
	    }

	    copyfile_debug(3, "extracting \"%s\" (%d bytes) at offset %u",
		entry->name, entry->length, entry->offset);

	    dataptr = (char *)attrhdr + entry->offset;

	    if (dataptr >= endptr || dataptr < buffer) {
		copyfile_debug(1, "Entry %d overflows:  offset = %u", entry->offset);
		error = -1;
		goto exit;
	    }
	    if ((dataptr + entry->length) > endptr ||
		((dataptr + entry->length) < buffer) ||
		(entry->length > hdrsize)) {
		if (COPYFILE_VERBOSE & s->flags)
		    copyfile_warn("Incomplete or corrupt attribute entry");
		copyfile_debug(1, "Entry %d length overflows:  dataptr = %u, offset = %u, length = %u, buffer = %u, endptr = %u",
			i, dataptr, entry->offset, entry->length, buffer, endptr);
		error = -1;
		goto exit;
	    }

	    if (COPYFILE_ACL & s->flags && strcmp((char*)entry->name, XATTR_SECURITY_NAME) == 0)
	    copyfile_debug(2, "extracting \"%s\" (%d bytes)",
		entry->name, entry->length);
	    dataptr = (char *)attrhdr + entry->offset;

	    if (COPYFILE_ACL & s->flags && strncmp(entry->name, XATTR_SECURITY_NAME, strlen(XATTR_SECURITY_NAME)) == 0)
	    {
		acl_t acl;
		if ((acl = acl_from_text(dataptr)) != NULL)
		{
		    if (filesec_set_property(s->fsec, FILESEC_ACL, &acl) < 0)
		    {
			    acl_t acl;
			    if ((acl = acl_from_text(dataptr)) != NULL)
			    {
				if (filesec_set_property(s->fsec, FILESEC_ACL, &acl) < 0)
				{
				    copyfile_debug(1, "setting acl");
				}
				else if (fchmodx_np(s->dst_fd, s->fsec) < 0 && errno != ENOTSUP)
					copyfile_warn("setting security information");
				acl_free(acl);
			    }
		    } else if (COPYFILE_XATTR & s->flags && (fsetxattr(s->dst_fd, entry->name, dataptr, entry->length, 0, 0))) {
			    if (COPYFILE_VERBOSE & s->flags)
				    copyfile_warn("error %d setting attribute %s", error, entry->name);
			    goto exit;
		    }
		    else if (fchmodx_np(s->dst_fd, s->fsec) < 0 && errno != ENOTSUP)
			    copyfile_warn("setting security information");
		    acl_free(acl);
		}
	    } else
	    if (COPYFILE_XATTR & s->flags && (fsetxattr(s->dst_fd, entry->name, dataptr, entry->length, 0, 0))) {
		if (COPYFILE_VERBOSE & s->flags)
			copyfile_warn("error %d setting attribute %s", error, entry->name);
		break;
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
	copyfile_debug(1, " extracting \"%s\" (32 bytes)", XATTR_FINDERINFO_NAME);
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

	length = adhdr->entries[1].length;
	offset = adhdr->entries[1].offset;
	rsrcforkdata = malloc(length);

	if (rsrcforkdata == NULL) {
		copyfile_debug(1, "could not allocate %u bytes for rsrcforkdata",
			       length);
		error = -1;
		goto bad;
	}

	bytes = pread(s->src_fd, rsrcforkdata, length, offset);
	if (bytes < length)
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
	    copyfile_debug(1, "error %d setting resource fork attribute", error);
	    error = -1;
	    goto bad;
	}
	    copyfile_debug(1, "extracting \"%s\" (%d bytes)",
		    XATTR_RESOURCEFORK_NAME, (int)length);
bad:
	if (rsrcforkdata)
	    free(rsrcforkdata);
    }
exit:
    if (buffer) free(buffer);
    return error;
}

static int copyfile_pack_acl(copyfile_state_t s, void **buf, ssize_t *len)
{
    int ret = 0;
    acl_t acl;
    char *acl_text;

    if (filesec_get_property(s->fsec, FILESEC_ACL, &acl) < 0)
    {
	if (errno != ENOENT)
	{
	    ret = -1;
	    if (COPYFILE_VERBOSE & s->flags)
		copyfile_warn("getting acl");
	}
	goto err;
    }

    if ((acl_text = acl_to_text(acl, len)) != NULL)
    {
	*buf = malloc(*len);
	memcpy(*buf, acl_text, *len);
	acl_free(acl_text);
    }
    copyfile_debug(1, "copied acl (%ld) %p", *len, *buf);
err:
    return ret;
}

static int copyfile_pack_rsrcfork(copyfile_state_t s, attr_header_t *filehdr)
{
    int datasize;
    char *databuf;

    /* Get the resource fork size */
    if ((datasize = fgetxattr(s->src_fd, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, 0)) < 0)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("skipping attr \"%s\" due to error %d", XATTR_RESOURCEFORK_NAME, errno);
	return -1;
    }

    if ((databuf = malloc(datasize)) == NULL)
    {
	copyfile_warn("malloc");
	return -1;
    }

    if (fgetxattr(s->src_fd, XATTR_RESOURCEFORK_NAME, databuf, datasize, 0, 0) != datasize)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("couldn't read entire resource fork");
	return -1;
    }

    /* Write the resource fork to disk. */
    if (pwrite(s->dst_fd, databuf, datasize, filehdr->appledouble.entries[1].offset) != datasize)
    {
	if (COPYFILE_VERBOSE & s->flags)
	    copyfile_warn("couldn't write resource fork");
    }
    copyfile_debug(1, "copied %d bytes of \"%s\" data @ offset 0x%08x",
	datasize, XATTR_RESOURCEFORK_NAME, filehdr->appledouble.entries[1].offset);
    filehdr->appledouble.entries[1].length = datasize;
    free(databuf);

    return 0;
}


static int copyfile_pack(copyfile_state_t s)
{
    char *attrnamebuf = NULL, *endnamebuf;
    void *databuf = NULL;
    attr_header_t *filehdr, *endfilehdr;
    attr_entry_t *entry;
    ssize_t listsize = 0;
    char *nameptr;
    int namelen;
    int entrylen;
    ssize_t datasize;
    int offset = 0;
    int hasrsrcfork = 0;
    int error = 0;

    filehdr = (attr_header_t *) calloc(1, ATTR_MAX_SIZE);
    if (filehdr == NULL) {
	error = -1;
	goto exit;
    } else {
	    endfilehdr = ((void*)filehdr) + ATTR_MAX_SIZE;
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
    filehdr->appledouble.magic              = SWAP32 (ADH_MAGIC);
    filehdr->appledouble.version            = SWAP32 (ADH_VERSION);
    filehdr->appledouble.numEntries         = SWAP16 (2);
    filehdr->appledouble.entries[0].type    = SWAP32 (AD_FINDERINFO);
    filehdr->appledouble.entries[0].offset  = SWAP32 (offsetof(apple_double_header_t, finfo));
    filehdr->appledouble.entries[0].length  = SWAP32 (FINDERINFOSIZE);
    filehdr->appledouble.entries[1].type    = SWAP32 (AD_RESOURCE);
    filehdr->appledouble.entries[1].offset  = SWAP32 (offsetof(apple_double_header_t, pad));
    filehdr->appledouble.entries[1].length  = 0;
    bcopy(ADH_MACOSX, filehdr->appledouble.filler, sizeof(filehdr->appledouble.filler));

    /*
     * Fill in the initial Attribute Header.
     */
    filehdr->magic       = SWAP32 (ATTR_HDR_MAGIC);
    filehdr->debug_tag   = SWAP32 (s->sb.st_ino);
    filehdr->data_start  = SWAP32 (sizeof(attr_header_t));

    /*
     * Collect the attribute names.
     */
    entry = (attr_entry_t *)((char *)filehdr + sizeof(attr_header_t));

    /*
     * Test if there are acls to copy
     */
    if (COPYFILE_ACL & s->flags)
    {
	if (filesec_get_property(s->fsec, FILESEC_ACL, &datasize) < 0)
	{
	    copyfile_debug(1, "no acl entries found (%d)", datasize < 0 ? errno : 0);
	} else
	{
	    offset = strlen(XATTR_SECURITY_NAME) + 1;
	    strcpy(attrnamebuf, XATTR_SECURITY_NAME);
	}
    }

    if (COPYFILE_XATTR & s->flags)
    {
	ssize_t left = ATTR_MAX_HDR_SIZE - offset;
        if ((listsize = flistxattr(s->src_fd, attrnamebuf + offset, left, 0)) <= 0)
	{
	    copyfile_debug(1, "no extended attributes found (%d)", errno);
	}
	if (listsize > left)
	{
	    copyfile_debug(1, "extended attribute list too long");
	    listsize = ATTR_MAX_HDR_SIZE;
	}

	listsize += offset;
	endnamebuf = attrnamebuf + listsize;
	if (endnamebuf > (attrnamebuf + ATTR_MAX_HDR_SIZE)) {
	    error = -1;
	    goto exit;
	}

	for (nameptr = attrnamebuf; nameptr <endnamebuf; nameptr += namelen)
	{
	    namelen = strlen(nameptr) + 1;
	    /* Skip over FinderInfo or Resource Fork names */
	    if (strcmp(nameptr, XATTR_FINDERINFO_NAME) == 0 ||
		strcmp(nameptr, XATTR_RESOURCEFORK_NAME) == 0)
		    continue;

	    /* The system should prevent this from happening, but... */
	    if (namelen > XATTR_MAXNAMELEN + 1) {
	        namelen = XATTR_MAXNAMELEN + 1;
	    }
	    entry->namelen = namelen;
	    entry->flags = 0;
	    bcopy(nameptr, &entry->name[0], namelen);
	    copyfile_debug(2, "copied name [%s]", entry->name);

	    entrylen = ATTR_ENTRY_LENGTH(namelen);
	    entry = (attr_entry_t *)(((char *)entry) + entrylen);

	    if ((void*)entry > (void*)endfilehdr) {
		    error = -1;
		    goto exit;
	    }

	    /* Update the attributes header. */
	    filehdr->num_attrs++;
	    filehdr->data_start += entrylen;
	}
    }

    /*
     * Collect the attribute data.
     */
    entry = (attr_entry_t *)((char *)filehdr + sizeof(attr_header_t));

    for (nameptr = attrnamebuf; nameptr < attrnamebuf + listsize; nameptr += namelen + 1)
    {
	namelen = strlen(nameptr);

	if (strcmp(nameptr, XATTR_SECURITY_NAME) == 0)
	    copyfile_pack_acl(s, &databuf, &datasize);
	else
	/* Check for Finder Info. */
	if (strcmp(nameptr, XATTR_FINDERINFO_NAME) == 0)
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
	} else
	/* Check for Resource Fork. */
	if (strcmp(nameptr, XATTR_RESOURCEFORK_NAME) == 0)
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

	entry->length = datasize;
	entry->offset = filehdr->data_start + filehdr->data_length;

	filehdr->data_length += datasize;
	/*
	 * >>>  WARNING <<<
	 * This assumes that the data is fits in memory (not
	 * the case when there are lots of attributes or one of
	 * the attributes is very large.
	 */
	if (entry->offset > ATTR_MAX_SIZE ||
		(entry->offset + datasize > ATTR_MAX_SIZE)) {
		error = -1;
	} else {
		bcopy(databuf, (char*)filehdr + entry->offset, datasize);
	}
	free(databuf);

	copyfile_debug(1, "copied %ld bytes of \"%s\" data @ offset 0x%08x", datasize, nameptr, entry->offset);
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

	filehdr->total_size  = SWAP32 (filehdr->appledouble.entries[1].offset);
    }

    /* Copy Resource Fork. */
    if (hasrsrcfork && (error = copyfile_pack_rsrcfork(s, filehdr)))
	goto exit;

    /* Write the header to disk. */
    datasize = filehdr->appledouble.entries[1].offset;

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
