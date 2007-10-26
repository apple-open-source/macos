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
 * Portions Copyright 2007 Apple Inc.
 *
 * $Id$
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

#include "automount.h"
#include "automountd.h"
#include "auto_mntopts.h"
#include "replica.h"
#include "sysctl_fsid.h"
#include "umount_by_fsid.h"

static void free_action_list(action_list *);
static int unmount_mntpnt(int32_t, int32_t, struct mnttab *, autofs_component);
static int fork_exec(char *, char **, uid_t, mach_port_t);
static void remove_browse_options(char *);
static int inherit_options(char *, char **);

#define ROUND_UP(a, b)	((((a) + (b) - 1)/(b)) * (b))

pthread_mutex_t gssd_port_lock;

static uint32_t
countstring(char *string)
{
	uint32_t stringlen;

	if (string != NULL)
		stringlen = strlen(string);
	else
		stringlen = 0;
	return (sizeof (uint32_t) + stringlen);
}

static uint8_t *
putstring(uint8_t *outbuf, char *string)
{
	uint32_t stringlen;

	if (string != NULL) {
		stringlen = strlen(string);
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

int
do_mount1(autofs_pathname mapname, char *key, autofs_pathname subdir,
    autofs_opts mapopts, autofs_pathname path, boolean_t isdirect,
    uid_t sendereuid, mach_port_t gssd_port,
    byte_buffer *actions, mach_msg_type_number_t *actionsCnt)
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
	bool_t iswildcard;
	bool_t isrestricted = hasrestrictopt(mapopts);
	kern_return_t ret;
	size_t bufsize;
	vm_address_t buffer_vm_address;
	uint8_t *outbuf;

retry:
	iswildcard = FALSE;

	mapents = parse_entry(key, mapname, mapopts, subdir, isdirect,
		&iswildcard, isrestricted, mount_access, &err);
	if (mapents == NULL) {
		/* Return the error parse_entry handed back. */
		return (err);
	}

	if (trace > 1) {
		struct mapfs *mfs;
		trace_prt(1, "  do_mount1:\n");
		for (me = mapents; me; me = me->map_next) {
			trace_prt(1, "  (%s,%s)\t%s%s%s\n",
			me->map_fstype ? me->map_fstype : "",
			me->map_mounter ? me->map_mounter : "",
			path ? path : "",
			me->map_root  ? me->map_root : "",
			me->map_mntpnt ? me->map_mntpnt : "");
			trace_prt(0, "\t\t-%s\n",
			me->map_mntopts ? me->map_mntopts : "");

			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				trace_prt(0, "\t\t%s:%s\tpenalty=%d\n",
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
		len = snprintf(mntpnt, sizeof (mntpnt), "%s%s%s", path,
		    mapents->map_root, me->map_mntpnt);

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

		(void) strcpy(spec_mntpnt, mntpnt);

		if (isrestricted &&
		    inherit_options(mapopts, &me->map_mntopts) != 0) {
			syslog(LOG_ERR, "malloc of options failed");
			free_mapent(mapents);
			return (EAGAIN);
		}

		if (strcmp(me->map_fstype, MNTTYPE_NFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err =
			    mount_nfs(me, spec_mntpnt, private, isdirect,
			        gssd_port);
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

			alp = (action_list *)malloc(sizeof (action_list));
			if (alp == NULL) {
				syslog(LOG_ERR, "malloc of alp failed");
				continue;
			}
			memset(alp, 0, sizeof (action_list));

			/*
			 * get the next subidr, but only if its a modified
			 * or faked autofs mount
			 */
			if (me->map_modified || me->map_faked) {
				len = snprintf(next_subdir,
					sizeof (next_subdir), "%s%s", subdir,
					me->map_mntpnt);
			} else {
				next_subdir[0] = '\0';
				len = 0;
			}

			if (trace > 2)
				trace_prt(1, "  root=%s\t next_subdir=%s\n",
						root, next_subdir);
			if (len < 0) {
				err = EINVAL;
			} else if ((size_t)len < sizeof (next_subdir)) {
				err = mount_autofs(me, spec_mntpnt, alp,
					root, next_subdir, key);
			} else {
				err = ENAMETOOLONG;
			}
			if (err == 0) {
				/*
				 * append to action list
				 */
				if (alphead == NULL)
					alphead = alp;
				else {
					for (tmp = alphead; tmp != NULL;
					    tmp = tmp->next)
						prev = tmp;
					prev->next = alp;
				}
			} else
				free(alp);
#if 0
		} else if (strcmp(me->map_fstype, MNTTYPE_LOFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err = loopbackmount(me->map_fs->mfs_dir, spec_mntpnt,
					    me->map_mntopts);
#endif
		} else {
			remove_browse_options(me->map_mntopts);
			err = mount_generic(me->map_fs->mfs_dir,
					    me->map_fstype, me->map_mntopts,
					    spec_mntpnt, isdirect, sendereuid,
					    gssd_port);
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
			bufsize += sizeof (uint32_t);
			bufsize += countstring(tmp->mounta.key);
		}
		if (bufsize != 0) {
			ret = vm_allocate(mach_task_self(), &buffer_vm_address,
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
				memcpy(outbuf, &tmp->mounta.isdirect,
				    sizeof (uint32_t));
				outbuf += sizeof (uint32_t);
				outbuf = putstring(outbuf, tmp->mounta.key);
			}
			free_action_list(alphead);
		}
		*actionsCnt = bufsize;
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

int
do_check_trigger(autofs_pathname mapname, char *key, autofs_pathname subdir,
    autofs_opts mapopts, autofs_pathname path, boolean_t isdirect,
    boolean_t *istrigger)
{
	struct mapent *me, *mapents = NULL;
	int err = 0;
	bool_t iswildcard;
	bool_t isrestricted = hasrestrictopt(mapopts);

	iswildcard = FALSE;

	/*
	 * Start out assuming it's not a trigger.
	 */
	*istrigger = FALSE;

	mapents = parse_entry(key, mapname, mapopts, subdir, isdirect,
		&iswildcard, isrestricted, TRUE, &err);
	if (mapents == NULL)
		return (err);

	if (trace > 1) {
		struct mapfs *mfs;
		trace_prt(1, "  do_check_trigger:\n");
		for (me = mapents; me; me = me->map_next) {
			trace_prt(1, "  (%s,%s)\t%s%s%s\n",
			me->map_fstype ? me->map_fstype : "",
			me->map_mounter ? me->map_mounter : "",
			path ? path : "",
			me->map_root  ? me->map_root : "",
			me->map_mntpnt ? me->map_mntpnt : "");
			trace_prt(0, "\t\t-%s\n",
			me->map_mntopts ? me->map_mntopts : "");

			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				trace_prt(0, "\t\t%s:%s\tpenalty=%d\n",
					mfs->mfs_host ? mfs->mfs_host: "",
					mfs->mfs_dir ? mfs->mfs_dir : "",
					mfs->mfs_penalty);
		}
	}

	/*
	 * Each mapent in the list describes a mount to be done.
	 * See whether any of them aren't autofs mounts.
	 */
	for (me = mapents; me; me = me->map_next) {
		if (strcmp(me->map_fstype, MNTTYPE_AUTOFS) != 0)
			*istrigger = TRUE;	/* non-autofs mount */
	}
	free_mapent(mapents);
	return (0);
}

#define	ARGV_MAX	20
#define	VFS_PATH	"/sbin"
#define MOUNT_PATH	"/sbin/mount"
#define MOUNT_URL_PATH	"/usr/libexec/mount_url"

int
mount_generic(char *special, char *fstype, char *opts, char *mntpnt,
    boolean_t isdirect, uid_t sendereuid, mach_port_t gssd_port)
{
	struct stat stbuf;
	int i, res;
	char *newargv[ARGV_MAX];

	if (trace > 1) {
		trace_prt(1, "  mount: %s %s %s %s\n",
			special, mntpnt, fstype, opts);
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

	if (strcmp(fstype, "nfs") == 0) {
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
		 * XXX - handle Solaris-style mount options we don't
		 * support, or for which we have a different name,
		 * here.
		 */
		newargv[i++] = "-o";
		newargv[i++] = opts;
	}

	/*
	 * Make sure we flag it as automounted.
	 */
	newargv[i++] = "-o";
	newargv[i++] = "automounted";

	/*
	 * XXX - not all our mount commands support "--" as an
	 * end-of-flags indication, so we just reject attempts
	 * to mount anything that begins with "-".
	 */
	if (special[0] == '-') {
		syslog(LOG_ERR,
		    "Can't mount \"%s\", as its name begins with \"-\"\n",
		    special);
		return (ENOENT);
	}
	newargv[i++] = special;
	newargv[i++] = mntpnt;
	newargv[i] = NULL;

	/*
	 * We'll be doing the mount as user "sendereuid"; give them the mount
	 * point, so they can do the mount.  If that fails, log a
	 * message but drive on, just in case the mount would
	 * succeed anyway.
	 */
	if (chown(mntpnt, sendereuid, -1) == -1)
		syslog(LOG_ERR, "Can't change ownership of %s", mntpnt);

	res = fork_exec(fstype, newargv, sendereuid, gssd_port);

	if (res == 0 && trace > 1) {
		if (stat(mntpnt, &stbuf) == 0) {
			trace_prt(1, "  mount of %s dev=%x rdev=%x OK\n",
				mntpnt, stbuf.st_dev, stbuf.st_rdev);
		} else {
			trace_prt(1, "  failed to stat %s\n", mntpnt);
		}
	}
	return (res);
}

static int
fork_exec(char *fstype, char **newargv, uid_t sendereuid, mach_port_t gssd_port)
{
	char *path;
	int path_is_allocated;
	struct stat stbuf;
	int i;
	kern_return_t ret;
	mach_port_t save_gssd_port;
	int child_pid;
	int stat_loc;
	int fd = 0;
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
		if (asprintf(&path, "%s/mount_%s", VFS_PATH, fstype) == -1)
			return (errno);
		path_is_allocated = 1;
	}

	if (stat(path, &stbuf) != 0) {
		res = errno;
		goto done;
	}

	if (trace > 1) {
		trace_prt(1, "  fork_exec: %s ", path);
		for (i = 1; newargv[i]; i++)
			trace_prt(0, "%s ", newargv[i]);
		trace_prt(0, "\n");
	}

	newargv[0] = path;

	/*
	 * We set the gssd task special port to the one we were handed,
	 * which is the one for the task that triggered this mount; we
	 * want the mount to talk to that task's gssd, so it can get
	 * that task's credentials to do the mount.
	 *
	 * XXX - we can't just pass the supplied port to the child; we
	 * have to save our current GSSD task special port, set that
	 * port to the specified port, fork, and then set it back in
	 * the parent after the fork.
	 *
	 * The task special port isn't per-thread, so we grab a mutex
	 * to make sure only one thread is doing anything with the port
	 * at a time.
	 */
	pthread_mutex_lock(&gssd_port_lock);
	ret = task_get_gssd_port(current_task(), &save_gssd_port);
	if (ret != KERN_SUCCESS) {
		pthread_mutex_unlock(&gssd_port_lock);
		syslog(LOG_ERR, "Cannot get gssd port: %s\n",
		    mach_error_string(ret));
		res = EIO;
		goto done;
	}
	ret = task_set_gssd_port(current_task(), gssd_port);
	if (ret != KERN_SUCCESS) {
		pthread_mutex_unlock(&gssd_port_lock);
		syslog(LOG_ERR, "Cannot set gssd port: %s\n",
		    mach_error_string(ret));
		res = EIO;
		goto done;
	}
	switch ((child_pid = fork1())) {
	case -1:
		/*
		 * Fork failure.
		 *
		 * Put the GSSD port back, and release the mutex.
		 */
		ret = task_set_gssd_port(current_task(), save_gssd_port);
		if (ret != KERN_SUCCESS) {
			pthread_mutex_unlock(&gssd_port_lock);
			syslog(LOG_ERR, "Cannot restore gssd port: %s\n",
			    mach_error_string(ret));
		}
		pthread_mutex_unlock(&gssd_port_lock);
		res = errno;
		syslog(LOG_ERR, "Cannot fork: %m");
		goto done;
	case 0:
		/*
		 * Child.
		 *
		 * We leave most of our environment as it is; we assume
		 * that launchd has made the right thing happen for us,
		 * and that this is also the right thing for the processes
		 * we run.
		 */
		if (!verbose) {
			/*
			 * Pitch all output from the mount program
			 * down /dev/null.
			 */
			fd = open("/dev/null", O_WRONLY);
			if (fd != -1) {
				(void) dup2(fd, 1);
				(void) dup2(fd, 2);
				(void) close(fd);
			}
		}

		/*
		 * Do the mount as the user who triggered the mount, so
		 * that if it's a file system such as AFP or SMB where a
		 * mount has a single session and user identity associated
		 * with it, the right user identity get associated with it.
		 */
		res = setuid(sendereuid);
		if (res) {
			syslog(LOG_ERR, "Can't set URL subprocess UID: %s\n",
			    strerror(res));
			_exit(res);
		}

		(void) execv(path, newargv);
		res = errno;
		syslog(LOG_ERR, "exec %s: %m", path);
		_exit(res);
	default:
		/*
		 * Parent.
		 *
		 * Put the GSSD port back, and release the mutex.
		 */
		ret = task_set_gssd_port(current_task(), save_gssd_port);
		if (ret != KERN_SUCCESS) {
			pthread_mutex_unlock(&gssd_port_lock);
			syslog(LOG_ERR, "Cannot restore gssd port: %s\n",
			    mach_error_string(ret));
		}
		pthread_mutex_unlock(&gssd_port_lock);

		/*
		 * Now wait for the child to finish.
		 */
		(void) waitpid(child_pid, &stat_loc, WUNTRACED);

		if (WIFEXITED(stat_loc)) {
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns exit status %d\n",
				    WEXITSTATUS(stat_loc));
			}

			res = WEXITSTATUS(stat_loc);
		} else if (WIFSIGNALED(stat_loc)) {
			syslog(LOG_ERR, "Mount subprocess terminated with %s\n",
			    strsignal(WTERMSIG(stat_loc)));
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns signal status %d\n",
				    WTERMSIG(stat_loc));
			}
		} else if (WIFSTOPPED(stat_loc)) {
			syslog(LOG_ERR, "Mount subprocess stopped with %s\n",
			    strsignal(WSTOPSIG(stat_loc)));
		} else {
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
do_unmount1(int32_t fsid_val0, int32_t fsid_val1,
    autofs_pathname mntresource, autofs_pathname mntpnt,
    autofs_component fstype, autofs_opts mntopts)
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

	res = unmount_mntpnt(fsid_val0, fsid_val1, &m, fstype);

done:	return (res);
}

static int
unmount_mntpnt(int32_t fsid_val0, int32_t fsid_val1, struct mnttab *mnt,
    autofs_component fstype)
{
	fsid_t fsid;
	int res;

	fsid.val[0] = fsid_val0;
	fsid.val[1] = fsid_val1;
	if (strcmp(fstype, MNTTYPE_NFS) == 0) {
		res = nfsunmount(&fsid, mnt);
	} else {
		if ((res = umount_by_fsid(&fsid, 0)) < 0)
			res = errno;
	}

	if (trace > 1)
		trace_prt(1, "  unmount %s %s\n",
			mnt->mnt_mountp, res ? "failed" : "OK");
	return (res);
}

/*
 * Remove the autofs specific options 'browse', 'nobrowse' and
 * 'restrict' from 'opts'.
 * This means a map can't force our "nobrowse" option to be set
 * on a file system, but that's life.
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

	/*
	 * "new" is as big as "buf", and we're not copying anything
	 * to "new" that's not in "buf", so the "strcat()" calls are
	 * safe.
	 */
	while ((p = (char *)strtok_r(pb, ",", &placeholder)) != NULL) {
		pb = NULL;
		if (strcmp(p, "nobrowse") != 0 &&
		    strcmp(p, "browse") != 0 &&
		    strcmp(p, MNTOPT_RESTRICT) != 0) {
			if (new[0] != '\0')
				(void) strcat(new, ",");
			(void) strcat(new, p);
		}
	}

	/*
	 * The string copied to "buf" was "opts", and we copied nothing
	 * to "new" that wasn't in "buf", so "new" is no bigger than "opts",
	 * and this copy is safe.
	 */
	(void) strcpy(opts, new);
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
inherit_options(char *opts, char **mapentopts)
{
	u_int i;
	char *new;
	mntoptparse_t mtmap;
	int mtmapflags, mtmapaltflags;
	mntoptparse_t mtopt;
	int mtoptflags, mtoptaltflags;
	bool_t addopt;
	size_t len;

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

	CHECK_STRCPY(new, *mapentopts, len);

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
			if (*new != '\0')
				CHECK_STRCAT(new, ",", len);
			if (mopts_restrict[i].m_inverse)
				CHECK_STRCAT(new, "no", len);
			CHECK_STRCAT(new, mopts_restrict[i].m_option, len);
		}
	}
	free(*mapentopts);
	*mapentopts = new;
	return (0);
}

bool_t
hasrestrictopt(char *opts)
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
