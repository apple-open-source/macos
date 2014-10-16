/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 *	autod_mount.c
 *
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2012 Apple Inc.
 */

#pragma ident	"@(#)autod_mount.c	1.71	05/06/08 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <mntopts.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include <bsm/audit.h>

#include "automount.h"
#include "nfs.h"
#include "automountd.h"
#include "auto_mntopts.h"
#include "replica.h"
#include "sysctl_fsid.h"
#include "umount_by_fsid.h"

static void free_action_list(action_list *);
static int unmount_mntpnt(fsid_t, struct mnttab *);
static int fork_exec(char *, char **, uid_t, au_asid_t);
static void remove_browse_options(char *);
static int inherit_options(const char *, char **);

#define ROUND_UP(a, b)	((((a) + (b) - 1)/(b)) * (b))

static uint32_t
countstring(char *string)
{
	uint32_t stringlen;

	if (string != NULL)
		stringlen = (uint32_t)strlen(string);
	else
		stringlen = 0;
	return ((uint32_t)sizeof (uint32_t) + stringlen);
}

static uint8_t *
putstring(uint8_t *outbuf, char *string)
{
	uint32_t stringlen;

	if (string != NULL) {
		stringlen = (uint32_t)strlen(string);
		memcpy(outbuf, &stringlen, sizeof (uint32_t));
		outbuf += sizeof (uint32_t);
		memcpy(outbuf, string, stringlen);
		outbuf += stringlen;
	} else {
		stringlen = 0xFFFFFFFF;
		memcpy(outbuf, &stringlen, sizeof (uint32_t));
		outbuf += sizeof (uint32_t);
	}
	return (outbuf);
}

static uint8_t *
putint(uint8_t *outbuf, int val)
{
	memcpy(outbuf, &val, sizeof (int));
	outbuf += sizeof (int);
	return (outbuf);
}

static uint8_t *
putuint32(uint8_t *outbuf, uint32_t val)
{
	memcpy(outbuf, &val, sizeof (uint32_t));
	outbuf += sizeof (uint32_t);
	return (outbuf);
}

int
do_mount1(const autofs_pathname mapname, const char *key,
    const autofs_pathname subdir, const autofs_opts mapopts,
    const autofs_pathname path, boolean_t isdirect, boolean_t issubtrigger,
    fsid_t mntpnt_fsid, uid_t sendereuid, au_asid_t asid, fsid_t *fsidp,
    uint32_t *retflags, byte_buffer *actions,
    mach_msg_type_number_t *actionsCnt)
{
	struct mapent *me, *mapents;
	char mntpnt[MAXPATHLEN];
	char spec_mntpnt[MAXPATHLEN];
	int err;
	char *private;	/* fs specific data. eg prevhost in case of nfs */
	ssize_t len;
	action_list *alp, *alphead, *prev, *tmp;
	char root[MAXPATHLEN];
	char next_subdir[MAXPATHLEN];
	bool_t mount_access = TRUE;
	bool_t isrestricted = hasrestrictopt(mapopts);
	kern_return_t ret;
	size_t bufsize;
	vm_address_t buffer_vm_address;
	uint8_t *outbuf;

retry:
	mapents = parse_entry(key, mapname, mapopts, subdir, isdirect,
		NULL, isrestricted, mount_access, &err);
	if (mapents == NULL) {
		/* Return the error parse_entry handed back. */
		return (err);
	}

	if (trace > 1) {
		struct mapfs *mfs;
		trace_prt(1, "  do_mount1:\n");
		for (me = mapents; me; me = me->map_next) {
			trace_prt(1, "  (%s,%s)\t%s%s%s -%s\n",
				me->map_fstype ? me->map_fstype : "",
				me->map_mounter ? me->map_mounter : "",
				path ? path : "",
				me->map_root  ? me->map_root : "",
				me->map_mntpnt ? me->map_mntpnt : "",
				me->map_mntopts ? me->map_mntopts : "");

			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				trace_prt(1, "\t\t%s:%s\tpenalty=%d\n",
					mfs->mfs_host ? mfs->mfs_host: "",
					mfs->mfs_dir ? mfs->mfs_dir : "",
					mfs->mfs_penalty);
		}
	}

	alphead = NULL;

	/*
	 * Each mapent in the list describes a mount to be done.
	 * Normally there's just a single entry, though in the
	 * case of /net mounts there may be many entries, that
	 * must be mounted as a hierarchy.  For each mount the
	 * automountd must make sure the required mountpoint
	 * exists and invoke the appropriate mount command for
	 * the fstype.
	 */
	private = "";
	for (me = mapents; me && !err; me = me->map_next) {
		/*
		 * For subtrigger mounts, path is the mount path of
		 * the subtrigger, which is the path atop which we
		 * mount the remote object, so there's no subdirectory
		 * underneath the root of the mount.  subdir is
		 * relative to the root of the top-level autofs mount,
		 * not relative to the root of the subtrigger mount,
		 * so we don't include it in the path.  (subdir is
		 * used to look in the map entry for mapents at or
		 * below that directory, as the topmost of those
		 * tells us what to mount, and the entries below
		 * it tell us what triggers need to be planted.)
		 */
		if (isdirect) {
			len = snprintf(mntpnt, sizeof (mntpnt), "%s%s%s",
			    path, issubtrigger ? "" : subdir, me->map_mntpnt);
		} else {
			len = snprintf(mntpnt, sizeof (mntpnt), "%s%s%s%s",
			    path, mapents->map_root,
			    issubtrigger ? "" : subdir, me->map_mntpnt);
		}

		if (len < 0) {
			free_mapent(mapents);
			return (EINVAL);
		}
		if ((size_t)len >= sizeof (mntpnt)) {
			free_mapent(mapents);
			return (ENAMETOOLONG);
		}
		/*
		 * remove trailing /'s from mountpoint to avoid problems
		 * stating a directory with two or more trailing slashes.
		 * This will let us mount directories from machines
		 * which export with two or more slashes (apollo for instance).
		 */
		len -= 1;
		while (mntpnt[len] == '/')
			mntpnt[len--] = '\0';

		(void) strlcpy(spec_mntpnt, mntpnt, sizeof(spec_mntpnt));

		if (isrestricted &&
		    inherit_options(mapopts, &me->map_mntopts) != 0) {
			syslog(LOG_ERR, "malloc of options failed");
			free_mapent(mapents);
			return (ENOMEM);
		}

		if (strcmp(me->map_fstype, MNTTYPE_NFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err =
			    mount_nfs(me, spec_mntpnt, private, isdirect,
				mntpnt_fsid, asid, fsidp, retflags);
			/*
			 * We must retry if we don't have access to the
			 * root file system and there are other
			 * following mapents. The reason we can't
			 * continue because the rest of the mapent list
			 * depends on whether mount_access is TRUE or FALSE.
			 */
			if (err == EACCES && me->map_next != NULL) {
				/*
				 * don't expect mount_access to be
				 * FALSE here, but we do a check
				 * anyway.
				 */
				if (mount_access == TRUE) {
					mount_access = FALSE;
					free_mapent(mapents);
					goto retry;
				}
			}
		} else if (strcmp(me->map_fstype, MNTTYPE_AUTOFS) == 0) {
			if (isdirect) {
				len = strlcpy(root, path, sizeof (root));
			} else {
				len = snprintf(root, sizeof (root), "%s/%s",
				    path, key);
			}
			if (len < 0) {
				free_mapent(mapents);
				return (EINVAL);
			}
			if ((size_t)len >= sizeof (root)) {
				free_mapent(mapents);
				return (ENAMETOOLONG);
			}

			/*
			 * get the next subdir
			 */
			len = snprintf(next_subdir, sizeof (next_subdir),
				"%s%s", subdir, me->map_mntpnt);

			if (trace > 2)
				trace_prt(1, "  root=%s\t next_subdir=%s\n", root, next_subdir);
			if (len < 0) {
				err = EINVAL;
			} else if ((size_t)len < sizeof (next_subdir)) {
				err = mount_autofs(mapname, me, spec_mntpnt,
					mntpnt_fsid, &alp, root,
					next_subdir, key, fsidp, retflags);
			} else {
				err = ENAMETOOLONG;
			}
			if (alp != NULL) {
				/*
				 * We were given an action list entry to
				 * append to the action list; do so.
				 */
				if (alphead == NULL)
					alphead = alp;
				else {
					for (tmp = alphead; tmp != NULL;
					    tmp = tmp->next)
						prev = tmp;
					prev->next = alp;
				}
			}
#ifdef HAVE_LOFS
		} else if (strcmp(me->map_fstype, MNTTYPE_LOFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err = loopbackmount(me->map_fs->mfs_dir, spec_mntpnt,
					    me->map_mntopts);
#endif
		} else {
			remove_browse_options(me->map_mntopts);
			err = mount_generic(me->map_fs->mfs_dir,
					    me->map_fstype, me->map_mntopts, 0,
					    spec_mntpnt, isdirect, FALSE,
					    mntpnt_fsid, sendereuid, asid,
					    fsidp, retflags);
		}
	}
	if (mapents)
		free_mapent(mapents);

	if (!err) {
		/*
		 * Serialize the action list and supply it to the
		 * caller.
		 */
		bufsize = 0;
		for (tmp = alphead; tmp != NULL; tmp = tmp->next) {
			bufsize += countstring(tmp->mounta.dir);
			bufsize += countstring(tmp->mounta.opts);
			bufsize += countstring(tmp->mounta.path);
			bufsize += countstring(tmp->mounta.map);
			bufsize += countstring(tmp->mounta.subdir);
			bufsize += countstring(tmp->mounta.trig_mntpnt);
			bufsize += sizeof (int);	/* tmp->mounta.flags */
			bufsize += sizeof (int);	/* tmp->mounta.mntflags */
			bufsize += sizeof (uint32_t);	/* tmp->mounta.isdirect */
			bufsize += sizeof (uint32_t);	/* tmp->mounta.needs_subtrigger */
			bufsize += countstring(tmp->mounta.key);
		}
		if (bufsize != 0) {
			ret = vm_allocate(current_task(), &buffer_vm_address,
			    bufsize, VM_FLAGS_ANYWHERE);
			if (ret != KERN_SUCCESS) {
				syslog(LOG_ERR, "memory allocation error: %s",
				    mach_error_string(ret));
				free_action_list(alphead);
				return (ENOMEM);
			}
			outbuf = (uint8_t *)buffer_vm_address;
			*actions = outbuf;
			for (tmp = alphead; tmp != NULL; tmp = tmp->next) {
				outbuf = putstring(outbuf, tmp->mounta.dir);
				outbuf = putstring(outbuf, tmp->mounta.opts);
				outbuf = putstring(outbuf, tmp->mounta.path);
				outbuf = putstring(outbuf, tmp->mounta.map);
				outbuf = putstring(outbuf, tmp->mounta.subdir);
				outbuf = putstring(outbuf, tmp->mounta.trig_mntpnt);
				outbuf = putint(outbuf, tmp->mounta.flags);
				outbuf = putint(outbuf, tmp->mounta.mntflags);
				outbuf = putuint32(outbuf, tmp->mounta.isdirect);
				outbuf = putuint32(outbuf, tmp->mounta.needs_subtrigger);
				outbuf = putstring(outbuf, tmp->mounta.key);
			}
			free_action_list(alphead);
		}
		*actionsCnt = (mach_msg_type_number_t)bufsize;
	}
	return (err);
}

static void
free_action_list(action_list *alp)
{
	action_list *p, *next = NULL;

	for (p = alp; p != NULL; p = next) {
		free_action_list_fields(p);
		next = p->next;
		free(p);
	}
}

#define	ARGV_MAX	23
#define	VFS_PATH	"/sbin"
#define MOUNT_PATH	"/sbin/mount"
#define MOUNT_URL_PATH	"/usr/libexec/mount_url"

struct attr_buffer {
	uint32_t		length;
	fsid_t			fsid;
	uint32_t		mountflags;
	vol_capabilities_attr_t	capabilities;
};

/*
 * TRUE if two fsids are equal.
 */
#define FSIDS_EQUAL(fsid1, fsid2)	\
	((fsid1).val[0] == (fsid2).val[0] && \
	 (fsid1).val[1] == (fsid2).val[1])

int
mount_generic(char *special, char *fstype, char *opts, int nfsvers,
    char *mntpnt, boolean_t isdirect, boolean_t usenetauth, fsid_t mntpnt_fsid,
    uid_t sendereuid, au_asid_t asid, fsid_t *fsidp,
    uint32_t *retflags)
{
	struct stat stbuf;
	int i, res;
	char *newargv[ARGV_MAX];
	static struct mntopt mopts_soft[] = {
		{ "soft", 0, 1, 1 },
		{ NULL, 0, 0, 0 }
	};
	mntoptparse_t mp;
	int flags, altflags;
	char *opts_copy;
	size_t mapped_opts_buflen;
	char *mapped_opts = NULL;
	char *p;
	char *optp;

	if (trace > 1) {
		trace_prt(1, "  mount: %s %s %s %s\n", special, mntpnt, fstype, opts);
	}

	/*
	 * XXX - if we do a stat() on the mount point of a direct
	 * mount, that'll trigger the mount, so do that only for
	 * an indirect mount.
	 *
	 * XXX - why bother doing it at all?  Won't the program
	 * we run just fail if it doesn't exist?
	 */
	if (!isdirect && stat(mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (ENOENT);
	}

	i = 1;

#if 0
	/*
	 *  Use "quiet" option to suppress warnings about unsupported
	 *  mount options.
	 */
	newargv[i++] = "-q";
#endif

	/*
	 * If this should go through the NetAuth agent, say so.
	 */
	if (usenetauth)
		newargv[i++] = "-n";

	/*
	 * Flag it as not to show up as a "mounted volume" in the
	 * Finder/File Manager sense; we put this first so that
	 * it can be overridden if somebody wants it to show up.
	 */
	newargv[i++] = "-o";
	newargv[i++] = "nobrowse";

	if (strcmp(fstype, "nfs") == 0) {
		/*
		 * Is "soft" set?
		 */
		flags = altflags = 0;
		getmnt_silent = 1;
		mp = getmntopts(opts, mopts_soft, &flags, &altflags);
		if (mp == NULL) {
			syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
			    opts);
			return (ENOENT);
		}
		freemntopts(mp);
		if (!altflags) {
			/*
			 * The only bit in altflags is the bit for "soft",
			 * so if nothing is set, it's a hard mount.
			 *
			 * Therefore, we don't want to preemptively unmount
			 * it on a sleep or network change, because that
			 * unmount might hang forever, causing a more severe
			 * hang than if we just let it stay mounted and hang
			 * in a regular NFS operation, as, in the latter case,
			 * you can ^C out of it from the command line, and
			 * should get a "server not responding" dialog from
			 * the GUI, which, if you click the unmount button,
			 * will trigger a forced unmount, which will cause
			 * the hanging operations to time out and won't
			 * itself block.
			 */
			*retflags |= MOUNT_RETF_DONTPREUNMOUNT;
		}

		/*
		 * Add a "-t nfs" option, as we'll be running "mount"
		 * rather than "mount_nfs".
		 */
		newargv[i++] = "-t";
		newargv[i++] = "nfs";

		/*
		 * Turn down mount_nfs's aggressiveness about
		 * trying to mount.
		 */
		newargv[i++] = "-o";
		newargv[i++] = "retrycnt=0";

		/*
		 * Specify the NFS version, if a version was given.
		 */
		switch (nfsvers) {

		case NFS_VER4:
			newargv[i++] = "-o";
			newargv[i++] = "vers=4";
			break;

		case NFS_VER3:
			newargv[i++] = "-o";
			newargv[i++] = "vers=3";
			break;

		case NFS_VER2:
			newargv[i++] = "-o";
			newargv[i++] = "vers=2";
			break;
		}
	}
	if (automountd_defopts != NULL) {
		/*
		 * Add the default mount options.
		 * The options for this particular entry come later,
		 * so they can override the defaults.
		 */
		newargv[i++] = "-o";
		newargv[i++] = automountd_defopts;
	}
	if (opts && *opts) {
		/*
		 * Map "findervol" to "browse", and "nofindervol"
		 * to "nobrowse".  In autofs, we use "findervol"
		 * and "nofindervol" to control whether a mount
		 * should show up as a "mounted volume" in the
		 * Finder/File Manager sense, to avoid collisions
		 * with the autofs "browse"/"nobrowse" options.
		 *
		 * Remove any automounter-specific options that the
		 * mount command may warn about such as "hidefromfinder"
		 * or any commonly-encountered Solaris options.
		 *
		 * Scan a copy of the options, as strsep() will
		 * modify what it's passed.
		 */
		opts_copy = strdup(opts);
		if (opts_copy == NULL) {
			syslog(LOG_ERR, "Can't mount \"%s\" - out of memory",
			    special);
			return (ENOMEM);
		}
		mapped_opts_buflen = strlen(opts_copy) + 1;
		mapped_opts = malloc(mapped_opts_buflen);
		*mapped_opts = '\0';
		opts = opts_copy;
		while ((p = strsep(&opts, ",")) != NULL) {

			/*
			 * Edit out any automounter specific options
			 * or commonly encountered but unsupported options
			 * that the mount command might warn about.
			 */
			if (strcmp(p, MNTOPT_HIDEFROMFINDER) == 0 ||
				strcmp(p, "grpid") == 0)
				continue;
			if (automountd_nosuid && strcmp(p, "suid") == 0)
				continue;
			/*
			 * Now handle mappings
			 */
			if (strcmp(p, "findervol") == 0)
				optp = "browse";
			else if (strcmp(p, "nofindervol") == 0)
				optp = "nobrowse";
			else
				optp = p;

			/*
			 * "findervol" is longer than "browse", and
			 * the target string is long enough for the
			 * options before we map "findervol" to
			 * "browse", so we know the options will fit.
			 */
			if (mapped_opts[0] != '\0') {
				/*
				 * We already have mount options; add a
				 * comma before this one.
				 */
				strlcat(mapped_opts, ",", mapped_opts_buflen);
			}
			strlcat(mapped_opts, optp, mapped_opts_buflen);
		}
		free(opts_copy);
		newargv[i++] = "-o";
		newargv[i++] = mapped_opts;
	}

	/*
	 * Make sure we flag it as automounted; we put this last so
	 * that it can't be overridden (the automounter is mounting
	 * it, so it is by definition automounted).
	 */
	newargv[i++] = "-o";
	newargv[i++] = "automounted";

	/*
	 * If forcing "nosuid" then append it too
	 */
	if (automountd_nosuid) {
		newargv[i++] = "-o";
		newargv[i++] = "nosuid";
	}

	/*
	 * XXX - not all our mount commands support "--" as an
	 * end-of-flags indication, so we just reject attempts
	 * to mount anything that begins with "-".
	 */
	if (special[0] == '-') {
		syslog(LOG_ERR,
		    "Can't mount \"%s\", as its name begins with \"-\"\n",
		    special);
		if (mapped_opts != NULL)
			free(mapped_opts);
		return (ENOENT);
	}
	newargv[i++] = special;
	newargv[i++] = mntpnt;
	newargv[i] = NULL;

	res = fork_exec(fstype, newargv, sendereuid, asid);
	if (res == 0) {
		res = get_triggered_mount_info(mntpnt, mntpnt_fsid,
		    fsidp, retflags);

		if (trace > 1) {
			if (stat(mntpnt, &stbuf) == 0) {
				trace_prt(1, "  mount of %s dev=%x rdev=%x OK\n",
					mntpnt, stbuf.st_dev, stbuf.st_rdev);
			} else {
				trace_prt(1, "  failed to stat %s\n", mntpnt);
			}
		}
	}
	if (mapped_opts != NULL)
		free(mapped_opts);
	return (res);
}

int
get_triggered_mount_info(const char *mntpnt, fsid_t mntpnt_fsid,
    fsid_t *fsidp, uint32_t *retflags)
{
	struct attrlist attrs;
	struct attr_buffer attrbuf;

	memset(&attrs, 0, sizeof (attrs));
	attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	attrs.commonattr = ATTR_CMN_FSID;
	attrs.volattr = ATTR_VOL_INFO|ATTR_VOL_MOUNTFLAGS|ATTR_VOL_CAPABILITIES;
	if (getattrlist(mntpnt, &attrs, &attrbuf, sizeof (attrbuf), 0) == -1) {
		/* Failed. */
		return errno;
	}
	if (FSIDS_EQUAL(mntpnt_fsid, attrbuf.fsid)) {
		/*
		 * OK, the file system there has the same fsid
		 * as the file system containing the mount
		 * point, so there's nothing mounted there.
		 * This presumably means somebody unmounted it
		 * out from under us; return EAGAIN to force
		 * the mount to be re-triggered.
		 */
		return EAGAIN;
	}

	/*
	 * Get the FSID of what's presumably the
	 * newly-mounted file system.
	 */
	*fsidp = attrbuf.fsid;

	/*
	 * If this is a VolFS file system, we don't want to unmount it
	 * ourselves, as, if somebody wanted to reopen a file from it,
	 * using /.vol, that would fail, as the /.vol path would use the
	 * fsid the file system had then, and if we unmount it, that fsid
	 * would no longer be valid and the /.vol lookup would fail - even
	 * though this was a triggered mount; the /.vol lookup would not
	 * trigger a remount.
	 *
	 * If either the VOL_CAP_FMT_PATH_FROM_ID capability is present
	 * or the MNT_DOVOLFS flag is set, it's a VolFS file system.
	 */
	if ((attrbuf.capabilities.capabilities[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_PATH_FROM_ID) &&
	    (attrbuf.capabilities.valid[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_PATH_FROM_ID))
		*retflags |= MOUNT_RETF_DONTUNMOUNT;
	else if (attrbuf.mountflags & MNT_DOVOLFS)
		*retflags |= MOUNT_RETF_DONTUNMOUNT;
	return 0;
}

/*
 * Given an audit session id, Try and join that session
 * Return 0 on success else -1
 */
int
join_session(au_asid_t asid)
{
	int err;
	au_asid_t asid2;
	mach_port_name_t session_port;

	err = audit_session_port(asid, &session_port);
	if (err) {
		syslog(LOG_ERR, "Could not get audit session port %d for %m", asid);
		return (-1);
	}
	asid2 = audit_session_join(session_port);
	(void) mach_port_deallocate(current_task(), session_port);

	if (asid2 < 0) {
		syslog(LOG_ERR, "Could not join audit session %d", asid);
		return (-1);
	}

	if (asid2 != asid) {
		syslog(LOG_ERR, "Joined session %d but wound up in session %d", asid, asid2);
		return (-1);
	}
	return (0);
}
#define CFENVFORMATSTRING "0x%X:0:0"

static int
fork_exec(char *fstype, char **newargv, uid_t sendereuid, au_asid_t asid)
{
	char *path;
	volatile int path_is_allocated;
	struct stat stbuf;
	int i;
	char CFUserTextEncodingEnvSetting[sizeof(CFENVFORMATSTRING) + 20]; /* Extra bytes for expansion of %X uid field */
	int child_pid;
	int stat_loc;
	int res;

	/*
	 * Build the full path name of the fstype dependent command.
	 * If fstype is "url", however, we run our mount_url helper, and,
	 * if it's "nfs", we just run "mount" with a "-t nfs" option, so
	 * that mount options in the form of flags (i.e., "-s" instead of
	 * "soft") work.
	 */
	if (strcmp(fstype, "url") == 0) {
		path = MOUNT_URL_PATH;
		path_is_allocated = 0;
	} else if (strcmp(fstype, "nfs") == 0) {
		path = MOUNT_PATH;
		path_is_allocated = 0;
	} else {
		if (strcmp(fstype, "smb") == 0)
			fstype = "smbfs";	/* mount_smbfs, not mount_smb */
		if (asprintf(&path, "%s/mount_%s", VFS_PATH, fstype) == -1) {
			res = errno;
			syslog(LOG_ERR, "Can't construct pathname of mount program: %m");
			return (res);
		}
		path_is_allocated = 1;
	}

	if (stat(path, &stbuf) != 0) {
		res = errno;
		syslog(LOG_ERR, "Can't stat mount program %s: %m", path);
		goto done;
	}

	if (trace > 1) {
		char *bufp;
		int c = 0;

		for (i = 1; newargv[i]; i++) // sum arg length + space
			c += strlen(newargv[i]) + 1;
		bufp = malloc(c + 1);
		if (bufp) {
			char *p = bufp;
			for (i = 1; newargv[i]; i++) {
				c -= snprintf(p, c, "%s ", newargv[i]);
				p += strlen(newargv[i]) + 1;
			}
			trace_prt(1, "  fork_exec: %s %s\n", path, bufp);
			free(bufp);
		}
	}

	newargv[0] = path;

	switch ((child_pid = fork())) {
	case -1:
		/*
		 * Fork failure. Log an error, and quit.
		 */
		res = errno;
		syslog(LOG_ERR, "Cannot fork: %m");
		goto done;
	case 0:
		/*
		 * Child.
		 *
		 * We need to join the right audit session
		 */
		if (join_session(asid))
			_exit(EPERM);

		/*
		 * We leave most of our environment as it is; we assume
		 * that launchd has made the right thing happen for us,
		 * and that this is also the right thing for the processes
		 * we run.
		 *
		 * One change we make is that we do the mount as the user
		 * who triggered the mount, so that if it's a file system
		 * such as AFP or SMB where a mount has a single session
		 * and user identity associated with it, the right user
		 * identity gets associated with it, and if it's a file
		 * system such as NFS that doesn't, but that might require
		 * user credentials, the UID matches the credentials that
		 * the GSSD it talks to has.
		 */
		if (setuid(sendereuid) == -1) {
			res = errno;
			syslog(LOG_ERR, "Can't set mount subprocess UID: %s",
			    strerror(res));
			_exit(res);
		}

		/*
		 * Create a new environment with a definition of
		 * __CF_USER_TEXT_ENCODING to work around CF's interest
		 * in the user's home directory.  We're a child of the
		 * automountd process, so those references won't deadlock
		 * us by blocking waiting for the very mount that we're
		 * trying to do, but it still means that CF makes a
		 * pointless attempt to find the user's home directory.
		 * The program we run to do the mount, if linked with
		 * CF - as mount_url is - will do that.
		 *
		 * Make sure we include the UID since CF will check for
		 * this when deciding whether to look in the home directory.
		 */
		snprintf(CFUserTextEncodingEnvSetting,
		    sizeof(CFUserTextEncodingEnvSetting), CFENVFORMATSTRING,
		    getuid());
		setenv("__CF_USER_TEXT_ENCODING", CFUserTextEncodingEnvSetting,
		    1);
		(void) execv(path, newargv);
		res = errno;
		syslog(LOG_ERR, "exec %s: %m", path);
		_exit(res);
	default:
		/*
		 * Parent.
		 *
		 * Now wait for the child to finish.
		 */
		while (waitpid(child_pid, &stat_loc, WUNTRACED) < 0) {
			if ((res = errno) == EINTR)
				continue;
			syslog(LOG_ERR, "waitpid %d failed - error %d", child_pid, errno);
			goto done;
		}

		if (WIFEXITED(stat_loc)) {
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns exit status %d\n",
				    WEXITSTATUS(stat_loc));
			}

			res = WEXITSTATUS(stat_loc);
		} else if (WIFSIGNALED(stat_loc)) {
			syslog(LOG_ERR, "Mount subprocess terminated with %s",
			    strsignal(WTERMSIG(stat_loc)));
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns signal status %d\n",
				    WTERMSIG(stat_loc));
			}
			res = EIO;
		} else if (WIFSTOPPED(stat_loc)) {
			syslog(LOG_ERR, "Mount subprocess stopped with %s",
			    strsignal(WSTOPSIG(stat_loc)));
			res = EIO;
		} else {
			syslog(LOG_ERR, "Mount subprocess got unknown status 0x%08x",
			    stat_loc);
			if (trace > 1)
				trace_prt(1,
				    "  fork_exec: returns unknown status\n");
			res = EIO;
		}
	}

done:
	if (path_is_allocated)
		free(path);
	return (res);
}

static const struct mntopt mopts_nfs[] = {
	MOPT_NFS
};

int
do_unmount1(fsid_t mntpnt_fsid, autofs_pathname mntresource,
    autofs_pathname mntpnt, autofs_component fstype, autofs_opts mntopts)
{

	struct mnttab m;
	int res = 0;
	mntoptparse_t mp;
	int flags;
	int altflags;

	m.mnt_special = mntresource;
	m.mnt_mountp = mntpnt;
	m.mnt_fstype = fstype;
	m.mnt_mntopts = mntopts;
	/*
	 * Special case for NFS mounts.
	 * Don't want to attempt unmounts from
	 * a dead server.  If any member of a
	 * hierarchy belongs to a dead server
	 * give up (try later).
	 */
	if (strcmp(fstype, MNTTYPE_NFS) == 0) {
		struct replica *list;
		int i, n;
		long nfs_port;

		/*
		 * See if a port number was specified.  If one was
		 * specified that is too large to fit in 16 bits, truncate
		 * the high-order bits (for historical compatibility).  Use
		 * zero to indicate "no port specified".
		 */
		flags = altflags = 0;
		getmnt_silent = 1;
		mp = getmntopts(mntopts, mopts_nfs, &flags, &altflags);
		if (mp != NULL) {
			if (altflags & NFS_MNT_PORT) {
				nfs_port = getmntoptnum(mp, "port");
				if (nfs_port != -1)
					nfs_port &= USHRT_MAX;
				else {
					syslog(LOG_ERR, "Couldn't parse port= option in \"%s\": %m",
					    mntopts);
					nfs_port = 0;	/* error */
				}
			} else
				nfs_port = 0;	/* option not present */
			freemntopts(mp);
		} else {
			syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
			    mntopts);
			nfs_port = 0;
		}

		list = parse_replica(mntresource, &n);
		if (list == NULL) {
			if (n >= 0)
				syslog(LOG_ERR, "Memory allocation failed: %m");
			res = 1;
			goto done;
		}

		for (i = 0; i < n; i++) {
			if (pingnfs(list[i].host, NULL, 0, nfs_port,
			    list[i].path, NULL) != RPC_SUCCESS) {
				res = 1;
				free_replica(list, n);
				goto done;
			}
		}
		free_replica(list, n);
	}

	res = unmount_mntpnt(mntpnt_fsid, &m);

done:	return (res);
}

static int
unmount_mntpnt(fsid_t mntpnt_fsid, struct mnttab *mnt)
{
	int res = 0;

	if (umount_by_fsid(&mntpnt_fsid, 0) < 0)
		res = errno;

	if (trace > 1)
		trace_prt(1, "  unmount %s %s\n",
			mnt->mnt_mountp, res ? "failed" : "OK");
	return (res);
}

/*
 * Remove the autofs specific options 'browse', 'nobrowse' and
 * 'restrict' from 'opts'.
 */
static void
remove_browse_options(char *opts)
{
	char *p, *pb;
	char buf[MAXOPTSLEN], new[MAXOPTSLEN];
	char *placeholder;

	new[0] = '\0';
	CHECK_STRCPY(buf, opts, sizeof buf);
	pb = buf;

	while ((p = (char *)strtok_r(pb, ",", &placeholder)) != NULL) {
		pb = NULL;
		if (strcmp(p, "nobrowse") != 0 &&
		    strcmp(p, "browse") != 0 &&
		    strcmp(p, MNTOPT_RESTRICT) != 0) {
			if (new[0] != '\0')
				(void) strlcat(new, ",", sizeof(new));
			(void) strlcat(new, p, sizeof(new));
		}
	}

	(void) strlcpy(opts, new, AUTOFS_MAXOPTSLEN);
}

/*
 * A "struct mntopt" table is terminated with an entry with a null
 * m_option pointer; therefore, the number of real entries in the
 * table is one fewer than the total number of entries.
 */
static const struct mntopt mopts_restrict[] = {
	RESTRICTED_MNTOPTS
};
#define	NROPTS	((sizeof (mopts_restrict)/sizeof (mopts_restrict[0])) - 1)

static int
inherit_options(const char *opts, char **mapentopts)
{
	u_int i;
	char *new = NULL;
	mntoptparse_t mtmap;
	int mtmapflags, mtmapaltflags;
	mntoptparse_t mtopt;
	int mtoptflags, mtoptaltflags;
	bool_t addopt;
	size_t len;
        int error = 0;

	len = strlen(*mapentopts);

	/*
	 * Compute the maximum amount of space needed to add all of the
	 * restricted options.
	 */
	for (i = 0; i < NROPTS; i++) {
		/*
		 * Count the space for the option name.
		 */
		len += strlen(mopts_restrict[i].m_option);

		/*
		 * If this is a negative option, and the option should be
		 * set, the name will be preceded with "no".
		 */
		if (mopts_restrict[i].m_inverse)
			len += 2;
	}

	/* "," for each new option plus the trailing NUL */
	len += NROPTS + 1;

	new = malloc(len);
	if (new == 0)
		return (-1);

	if ((error = CHECK_STRCPY(new, *mapentopts, len))) {
                goto DONE;
        }

	mtmapflags = mtmapaltflags = 0;
	getmnt_silent = 1;
	mtmap = getmntopts(*mapentopts, mopts_restrict, &mtmapflags, &mtmapaltflags);
	if (mtmap == NULL) {
		syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
		    opts);
		return (-1);
	}
	freemntopts(mtmap);

	mtoptflags = mtoptaltflags = 0;
	getmnt_silent = 1;
	mtopt = getmntopts(opts, mopts_restrict, &mtoptflags, &mtoptaltflags);
	if (mtopt == NULL) {
		syslog(LOG_ERR, "Couldn't parse mount options \"%s\": %m",
		    opts);
		return (-1);
	}
	freemntopts(mtopt);

	for (i = 0; i < NROPTS; i++) {
		if (mopts_restrict[i].m_altloc) {
			addopt = ((mtoptaltflags & mopts_restrict[i].m_flag) &&
				 !(mtmapaltflags & mopts_restrict[i].m_flag));
		} else {
			addopt = ((mtoptflags & mopts_restrict[i].m_flag) &&
				 !(mtmapflags & mopts_restrict[i].m_flag));
		}
		if (addopt) {
			if (*new != '\0') {
				if ((error = CHECK_STRCAT(new, ",", len))) {
                                        goto DONE;
                                }
                        }
			if (mopts_restrict[i].m_inverse) {
				if ((error = CHECK_STRCAT(new, "no", len))) {
                                        goto DONE;
                                }
                        }
			error = CHECK_STRCAT(new, mopts_restrict[i].m_option, len);
		}
	}
DONE:
        free(*mapentopts);
        if (error) {
        	if (new) {
                	free(new);
                }
                new = NULL;
        }
        *mapentopts = new;
        
	return error;
}

bool_t
hasrestrictopt(const char *opts)
{
	mntoptparse_t mp;
	int flags, altflags;

	flags = altflags = 0;
	getmnt_silent = 1;
	mp = getmntopts(opts, mopts_restrict, &flags, &altflags);
	if (mp == NULL)
		return (FALSE);
	freemntopts(mp);
	return ((altflags & AUTOFS_MNT_RESTRICT) != 0);
}
