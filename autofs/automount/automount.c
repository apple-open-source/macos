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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#pragma ident	"@(#)automount.c	1.50	05/06/08 SMI"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <locale.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fstab.h>
#include <mntopts.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <fts.h>

#include <OpenDirectory/OpenDirectory.h>

#include "deflt.h"
#include "autofs.h"
#include "automount.h"
#include "automount_od.h"
#include "umount_by_fsid.h"

static int parse_mntopts(const char *, int *, int *);
static int paths_match(struct autodir *, struct autodir *);
static int mkdir_r(char *);
static void rmdir_r(char *);
static int have_ad(void);
struct autodir *dir_head;
struct autodir *dir_tail;
static int num_current_mounts;
static struct statfs *current_mounts;
static void make_symlink(const char *, const char *);
static struct statfs *find_mount(const char *);
int verbose = 0;
int trace = 0;

static int autofs_control_fd;

static void usage(void);
static void do_unmounts(void);
static int load_autofs(void);

static int mount_timeout = AUTOFS_MOUNT_TIMEOUT;

static char gKextLoadCommand[] = "/sbin/kextload";
static char gKextLoadPath[] = "/System/Library/Extensions/autofs.kext";

/*
 * XXX
 * The following are needed because they're used in auto_subr.c and
 * we link with it. Should avoid this.
 */
pthread_mutex_t cleanup_lock;
pthread_cond_t cleanup_start_cv;
pthread_cond_t cleanup_done_cv;

int
main(int argc, char *argv[])
{
	long timeout_val;
	int c;
	int flushcache = 0;
	int unmount_automounted = 0;	// Unmount automounted mounts
	struct autodir *dir, *d;
	char real_mntpnt[PATH_MAX];
	struct stat stbuf;
	char *master_map = "auto_master";
	int null;
	struct statfs *mntp;
	int count = 0;
	char *stack[STACKSIZ];
	char **stkptr;
	char *defval;
	int fd;
	int flags, altflags;
	struct staticmap *static_ent;

	/*
	 * Read in the values from config file first before we check
	 * commandline options so the options override the file.
	 */
	if ((defopen(AUTOFSADMIN)) == 0) {
		if ((defval = defread("AUTOMOUNT_TIMEOUT=")) != NULL) {
			errno = 0;
			timeout_val = strtol(defval, (char **)NULL, 10);
			if (errno == 0 && timeout_val > 0 &&
			    timeout_val <= INT_MAX)
				mount_timeout = (int)timeout_val;
		}
		if ((defval = defread("AUTOMOUNT_VERBOSE=")) != NULL) {
			if (strncasecmp("true", defval, 4) == 0)
				verbose = TRUE;
			else
				verbose = FALSE;
		}

		/* close defaults file */
		defopen(NULL);
	}

	while ((c = getopt(argc, argv, "mM:D:f:t:vcu?")) != EOF) {
		switch (c) {
		case 'm':
			pr_msg("Warning: -m option not supported");
			break;
		case 'M':
			pr_msg("Warning: -M option not supported");
			break;
		case 'D':
			pr_msg("Warning: -D option not supported");
			break;
		case 'f':
			pr_msg("Error: -f option no longer supported");
			usage();
			break;
		case 't':
			if (strchr(optarg, '=')) {
				pr_msg("Error: invalid value for -t");
				usage();
			}
			mount_timeout = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			flushcache++;
			break;
		case 'u':
			unmount_automounted++;
			break;
		default:
			usage();
			break;
		}
	}

	if (optind < argc) {
		pr_msg("%s: command line mountpoints/maps "
			"no longer supported",
			argv[optind]);
		usage();
	}

	/*
	 * Get an array of current system mounts
	 */
	num_current_mounts = getmntinfo(&current_mounts, MNT_NOWAIT);
	if (num_current_mounts == 0) {
		pr_msg("Couldn't get current mounts: %m");
		exit(1);
	}

	autofs_control_fd = open("/dev/" AUTOFS_CONTROL_DEVICE, O_RDONLY);
	if (autofs_control_fd == -1 && errno == ENOENT) {
		/*
		 * Oops, we probably don't have the autofs kext
		 * loaded.
		 */
		FTS *fts;
		static char *const paths[] = { "/Network", NULL };
		FTSENT *ftsent;
		int error;

		/*
		 * This means there can't be any autofs mounts yet, so
		 * this is the first time we're being run since a reboot.
		 * Clean out any stuff left in /Network from the reboot.
		 */
		fts = fts_open(paths, FTS_NOCHDIR|FTS_PHYSICAL|FTS_XDEV,
		    NULL);
		if (fts != NULL) {
			while ((ftsent = fts_read(fts)) != NULL) {
				/*
				 * We only remove directories - if
				 * there are files, we assume they're
				 * there for a purpose.
				 *
				 * We remove directories after we've
				 * removed their children, so we want
				 * to process directories visited in
				 * post-order.
				 *
				 * We don't remove /Network itself.
				 */
				if (ftsent->fts_info == FTS_DP &&
				    ftsent->fts_level > FTS_ROOTLEVEL)
					rmdir(ftsent->fts_accpath);
			}
			fts_close(fts);
		}

		/*
		 * Now load it.
		 */
		error = load_autofs();
		if (error != 0) {
			pr_msg("can't load autofs kext");
			exit(1);
		}

		/*
		 * Try the open again.
		 */
		autofs_control_fd = open("/dev/" AUTOFS_CONTROL_DEVICE,
		    O_RDONLY);
	}
	if (autofs_control_fd == -1) {
		if (errno == EBUSY)
			pr_msg("Another automount is running");
		else
			pr_msg("Couldn't open %s: %m", "/dev/" AUTOFS_CONTROL_DEVICE);
		exit(1);
	}

	/*
	 * Update the mount timeout.
	 */
	if (ioctl(autofs_control_fd, AUTOFS_SET_MOUNT_TO, &mount_timeout) == -1)
		pr_msg("AUTOFS_SET_MOUNT_TO failed: %m");

	/*
	 * Attempt to unmount any non-busy triggered mounts; this includes
	 * not only autofs mounts, but, for example SMB Dfs mounts.
	 *
	 * This is done before sleep, and after a network change, to
	 * try to get rid of as many network mounts as we can; each
	 * unmounted network mount is a network mount on which we
	 * can't hang.
	 */
	if (unmount_automounted) {
		if (verbose)
			pr_msg("Unmounting triggered mounts");
		if (ioctl(autofs_control_fd, AUTOFS_UNMOUNT_TRIGGERED, 0) == -1)
			pr_msg("AUTOFS_UNMOUNT_TRIGGERED failed: %m");
		exit(0);
	}

	if (flushcache) {
		/*
		 * Notify the automounter that it should flush its caches,
		 * as we might be on a different network with different maps.
		 */
		if (ioctl(autofs_control_fd, AUTOFS_NOTIFYCHANGE, 0) == -1)
			pr_msg("AUTOFS_NOTIFYCHANGE failed: %m");
	}

	(void) umask(0);
	ns_setup(stack, &stkptr);

	(void) loadmaster_map(master_map, "", stack, &stkptr);

	/*
	 * Mount the daemon at its mount points.
	 */
	for (dir = dir_head; dir; dir = dir->dir_next) {

		if (realpath(dir->dir_name, real_mntpnt) == NULL) {
			/*
			 * We couldn't get the real path for this,
			 * perhaps because it doesn't exist.
			 * If it's not because it doesn't exist, just
			 * give up on this entry.  Otherwise, just null
			 * out the real path - we'll try creating the
			 * directory later, and will set dir_realpath
			 * then, if that succeeds.
			 */
			if (errno != ENOENT) {
				pr_msg("%s: Can't convert to real path: %m",
				    dir->dir_name);
				continue;
			}
			dir->dir_realpath = NULL;
		} else {
			dir->dir_realpath = strdup(real_mntpnt);
			if (dir->dir_realpath == NULL) {
				pr_msg("Couldn't allocate real path: %m");
				exit(1);
			}
		}

		/*
		 * Skip null entries
		 */
		if (strcmp(dir->dir_map, "-null") == 0)
			continue;

		/*
		 * Skip null'ed entries
		 */
		null = 0;
		for (d = dir->dir_prev; d; d = d->dir_prev) {
			if (paths_match(dir, d))
				null = 1;
		}
		if (null)
			continue;

		/*
		 * If this is -fstab, and there are no fstab "net" entries,
		 * skip this map if our directory search path doesn't
		 * include Active Directory.  We don't want /Network/Servers
		 * (or wherever it shows up) to exist if this system isn't
		 * using AD (AD supplies fstab entries on the fly, so they
		 * might not exist right now) and we don't have any fstab
		 * entries.
		 */
		if (strcmp(dir->dir_map, "-fstab") == 0) {
			if (!have_ad() && !havefstabkeys()) {
				/*
				 * We're not using AD, and fstab is
				 * inaccessible or devoid of "net" entries.
				 */
				free(dir->dir_map);
				dir->dir_map = strdup("-null");
				continue;
			}
			endfsent();
		}

		/*
		 * If this is -fstab or -static, and there's another entry
		 * that's supposed to mount something on the same directory
		 * and isn't "-fstab" or "-static", ignore this; we might
		 * have a server that's supplying real automounter maps for
		 * the benefit of OS X systems with autofs and also supplying
		 * fstab entries for the benefit of older OS X systems, and
		 * we want to mount the real automounter map, not the -fstab
		 * or -static map, in that case.
		 */
		if (strcmp(dir->dir_map, "-fstab") == 0 ||
		    strcmp(dir->dir_map, "-static") == 0) {
			for (d = dir_head; d; d = d->dir_next) {
				if (paths_match(dir, d) &&
				    strcmp(d->dir_map, "-fstab") != 0 &&
				    strcmp(d->dir_map, "-static") != 0) {
					pr_msg("%s: ignoring redundant %s map",
					    dir->dir_name, dir->dir_map);
					continue;
				}
			}
		}

		/*
		 * Parse the mount options and get additional flags to pass
		 * to mount() (standard mount options) and autofs mount
		 * options.
		 *
		 * XXX - we ignore flags on an update; if they're different
		 * from the current flags for that mount, we'd need to do a
		 * remount.
		 */
		if (!parse_mntopts(dir->dir_opts, &flags, &altflags)) {
			/*
			 * Failed.
			 */
			continue;
		}

		/*
		 * If this is -static, check whether the entry refers
		 * to this host; if so, make the appropriate symlink
		 * exist at the "mount point" path.
		 */
		if (strcmp(dir->dir_map, "-static") == 0) {
			static_ent = get_staticmap_entry(dir->dir_name);
			if (static_ent == NULL) {
				/*
				 * Whiskey tango foxtrot?  There should
				 * be an entry here.  Log an error and
				 * ignore this mount.
				 */
				pr_msg("can't find fstab entry for %s",
				    dir->dir_name);
				continue;
			}
			if (host_is_us(static_ent->host, strlen(static_ent->host)) ||
			    self_check(static_ent->host)) {
				/*
				 * Yup, this is us.
				 * Try to make the appropriate symlink.
				 */
				make_symlink(static_ent->localpath,
				    dir->dir_name);
				release_staticmap_entry(static_ent);
				continue;
			}
			release_staticmap_entry(static_ent);
		}

		/*
		 * Check whether there's already an entry
		 * in the mnttab for this mountpoint.
		 */
		if (dir->dir_realpath != NULL &&
		    (mntp = find_mount(dir->dir_realpath)) != NULL) {
			struct autofs_update_args au;

			/*
			 * If it's not an autofs mount - don't
			 * mount over it.
			 */
			if (strcmp(mntp->f_fstypename, MNTTYPE_AUTOFS) != 0) {
				pr_msg("%s: already mounted on %s",
					mntp->f_mntfromname, dir->dir_realpath);
				continue;
			}

			/*
			 * This is already mounted, so just update it.
			 * We don't bother to check whether any options are
			 * changing, as we'd have to make a trip into the
			 * kernel to get the current options to check them,
			 * so we might as well just make a trip to do the
			 * update.
			 */
			au.fsid		= mntp->f_fsid;
			au.opts		= dir->dir_opts;
			au.map		= dir->dir_map;
			au.mntflags	= altflags;
			au.direct 	= dir->dir_direct;
			au.node_type	= dir->dir_direct ? NT_TRIGGER : 0;

			if (ioctl(autofs_control_fd, AUTOFS_UPDATE_OPTIONS,
			    &au) < 0) {
				pr_msg("update %s: %m", dir->dir_realpath);
				continue;
			}
			if (verbose)
				pr_msg("%s updated", dir->dir_realpath);
		} else {
			struct autofs_args ai;
			int st_flags = 0;

			/*
			 * This trigger isn't already mounted; either
			 * the path doesn't exist at all, or it
			 * exists but nothing is mounted on it.
			 *
			 * Create a mount point if necessary
			 * If the path refers to an existing symbolic
			 * link, refuse to mount on it.  This avoids
			 * future problems.  (We don't use dir->dir_realpath
			 * because that's never a symbolic link.)
			 */
			if (lstat(dir->dir_name, &stbuf) == 0) {
				if ((stbuf.st_mode & S_IFMT) != S_IFDIR) {
					pr_msg("%s: Not a directory", dir->dir_name);
					continue;
				}
				st_flags = stbuf.st_flags;

				/*
				 * Either realpath() succeeded or it
				 * failed with ENOENT; otherwise, we
				 * would have quit before getting here.
				 *
				 * If it failed, report an error, as
				 * the problem isn't that "dir->dir_name"
				 * doesn't exist, the problem is that,
				 * somehow, we got ENOENT even though
				 * it exists.
				 */
				if (dir->dir_realpath == NULL) {
					errno = ENOENT;
					pr_msg("%s: Can't convert to real path: %m",
					    dir->dir_name);
					continue;
				}
			} else {
				if (mkdir_r(dir->dir_name)) {
					pr_msg("%s: %m", dir->dir_name);
					continue;
				}

				/*
				 * realpath() presumably didn't succeed,
				 * as dir->dir_name couldn't be statted.
				 * Call it again, to get the real path
				 * corresponding to the newly-created
				 * mount point.
				 */
				if (realpath(dir->dir_name, real_mntpnt) == NULL) {
					/*
					 * Failed.
					 */
					pr_msg("%s: Can't convert to real path: %m",
					    dir->dir_name);
					continue;
				}
				dir->dir_realpath = strdup(real_mntpnt);
				if (dir->dir_realpath == NULL) {
					pr_msg("Couldn't allocate real path for %s: %m",
					    dir->dir_name);
					continue;
				}
			}

			/*
			 * If the "hidefromfinder" option is set for
			 * this autofs mountpoint then also set the
			 * UF_HIDDEN bit on the directory so it'll still
			 * be invisible to the Finder even if not mounted on.
			 */
			if (altflags & AUTOFS_MNT_HIDEFROMFINDER)
				st_flags |= UF_HIDDEN;
			else
				st_flags &= ~UF_HIDDEN;
			if (chflags(dir->dir_name, st_flags) < 0)
				pr_msg("%s: can't set hidden", dir->dir_name);

			/*
			 * Mount it.  Use the real path (symlink-free),
			 * for reasons mentioned above.
			 */
			ai.version	= AUTOFS_ARGSVERSION;
			ai.path 	= dir->dir_realpath;
			ai.opts		= dir->dir_opts;
			ai.map		= dir->dir_map;
			ai.subdir	= "";
			ai.direct 	= dir->dir_direct;
			if (dir->dir_direct)
				ai.key = dir->dir_name;
			else
				ai.key = "";
			ai.mntflags	= altflags;
			ai.mount_type	= MOUNT_TYPE_MAP;	/* top-level autofs mount */
			ai.node_type	= dir->dir_direct ? NT_TRIGGER : 0;

			if (mount(MNTTYPE_AUTOFS, dir->dir_realpath,
			    MNT_DONTBROWSE | MNT_AUTOMOUNTED | flags,
			    &ai) < 0) {
				pr_msg("mount %s: %m", dir->dir_realpath);
				continue;
			}
			if (verbose)
				pr_msg("%s mounted", dir->dir_realpath);
		}

		count++;
	}

	if (verbose && count == 0)
		pr_msg("no mounts");

	/*
	 * Now compare the /etc/mnttab with the master
	 * map.  Any autofs mounts in the /etc/mnttab
	 * that are not in the master map must be
	 * unmounted
	 *
	 * XXX - if there are no autofs mounts left, should we
	 * unload autofs, or arrange that it be unloaded?
	 */
	do_unmounts();

	/*
	 * Let PremountHomeDirectoryWithAuthentication() know that we're
	 * done.
	 */
	fd = open("/var/run/automount.initialized", O_CREAT|O_WRONLY, 0600);
	close(fd);

	return (0);
}

static void
make_symlink(const char *target, const char *path)
{
	struct stat stbuf;
	char linktarget[PATH_MAX + 1];
	ssize_t pathlength;
	struct statfs *mnt;
	struct stat st;

	/*
	 * Does the target exist?
	 */
	if (lstat(path, &stbuf) == 0) {
		/*
		 * Yes.  What is it?
		 */
		if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
			/*
			 * It's a symlink.
			 * What does it point to?
			 */
			pathlength = readlink(path, linktarget, PATH_MAX);
			if (pathlength == -1) {
				/*
				 * FAIL.
				 */
				pr_msg("can't read target of %s: %m", path);
				return;
			}
			linktarget[pathlength] = '\0';

			/*
			 * Does it point to the same place that we
			 * want it to point to?
			 *
			 * XXX - case-sensitivity?  That's hard to
			 * handle, given that the path might cross
			 * multiple file systems with different case-
			 * sensitivities.
			 */
			if (strcmp(linktarget, target) == 0) {
				/*
				 * Yes, it does.
				 * We don't need to do anything.
				 */
				if (verbose)
					pr_msg("link %s unchanged", path);
				return;
			}

			/*
			 * Get rid of the existing symlink.
			 */
			if (unlink(path) == -1) {
				pr_msg("can't unlink %s: %m", path);
				return;
			}
		} else if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
			/*
			 * It's a directory.
			 * Is there an autofs mount atop it?
			 */
			mnt = find_mount(path);
			if (mnt != NULL) {
				/*
				 * Something's mounted atop it; is it
				 * autofs?
				 */
				if (strcmp(mnt->f_fstypename, MNTTYPE_AUTOFS) == 0) {
					/*
					 * Yes.  Try to unmount it (and
					 * everything under it).
					 */
					if (ioctl(autofs_control_fd,
					    AUTOFS_UNMOUNT, &mnt->f_fsid) != 0) {
						/*
						 * Failed.
						 * Leave it alone for now.
						 */
						return;
					}
				}
			}

			/*
			 * Now try to remove the directory.
			 */
			if (rmdir(path) != 0) {
				/*
				 * Failed.  Leave it alone.
				 */
				return;
			}
		} else {
			/*
			 * Neither a symlink nor a directory.
			 * Leave it alone.
			 */
			return;
		}
	} else {
		/*
		 * lstat() failed; is it because the target doesn't
		 * exist, or because we couldn't get its
		 * information?
		 */
		if (errno != ENOENT) {
			/*
			 * We couldn't get its information.
			 * Leave it alone.
			 */
			return;
		}
	}

	/*
	 * OK, the target should not exist.
	 * Make the symlink.
	 */
	if (symlink(target, path) == -1) {
		pr_msg("can't create symlink from %s to %s: %m",
		    path, target);
	}

	/*
	 * Validate the symlink in case the path and target
	 * don't match but still yield an ELOOP.
	 */
	if (stat(path, &st) < 0) {
		pr_msg("Invalid symbolic link: %s to %s: %m", path, target);
		(void) unlink(path);
	}
}

/*
 * Find the first mount entry given the mountpoint path.
 */
static struct statfs *
find_mount(mntpnt)
	const char *mntpnt;
{
	int i;
	struct statfs *mnt;

	for (i = 0; i < num_current_mounts; i++) {
		mnt = &current_mounts[i];
		if (strcmp(mntpnt, mnt->f_mntonname) == 0)
			return (mnt);
	}

	return (NULL);
}

static void
usage()
{
	pr_msg("Usage: automount  [ -vcu ]  [ -t duration ]");
	exit(1);
	/* NOTREACHED */
}

/*
 * Given a mount options string, get the flags argument to pass to mount()
 * and the autofs mount options.
 *
 * We put "nobrowse" in front of MOPT_STDOPTS so that the mount
 * options "browse" and "nobrowse" are interpreted as autofs
 * mount options controlling whether you can get a directory listing,
 * not OS X mount options controlling whether the mounts are treated as
 * "real" volumes or not (we force MNT_NOBROWSE on for any mounts we do).
 */
static const struct mntopt mopts_autofs[] = {
	{ "browse",			1, AUTOFS_MNT_NOBROWSE, 1 },
	MOPT_STDOPTS,
	{ MNTOPT_RESTRICT,		0, AUTOFS_MNT_RESTRICT, 1 },
	{ MNTOPT_HIDEFROMFINDER,	0, AUTOFS_MNT_HIDEFROMFINDER, 1 },
	{ NULL,				0, 0, 0 }
};

static int
parse_mntopts(const char *opts, int *flags, int *altflags)
{
	mntoptparse_t mp;

	/*
	 * Parse the mount options and fill in "flags" and "altflags".
	 */
	*flags = *altflags = 0;
	getmnt_silent = 1;
	mp = getmntopts(opts, mopts_autofs, flags, altflags);
	if (mp == NULL) {
		pr_msg("memory allocation failure");
		return (0);
	}
	freemntopts(mp);

	return (1);
}

/*
 * Unmount any autofs mounts that
 * aren't in the master map
 */
static void
do_unmounts(void)
{
	int i;
	struct statfs *mnt;
	struct autodir *dir;
	int current;
	int count = 0;
	static const char triggered[] = "triggered";

	for (i = 0; i < num_current_mounts; i++) {
		mnt = &current_mounts[i];
		if (strcmp(mnt->f_fstypename, MNTTYPE_AUTOFS) != 0)
			continue;
		/*
		 * Don't unmount autofs mounts done
		 * from the autofs mount command.
		 * How do we tell them apart ?
		 * Autofs mounts not eligible for auto-unmount
		 * have an f_mntfromname of "subtrigger" (those
		 * are subtriggers on top of a non-autofs mount)
		 * or an f_mntfromname beginning with "triggered"
		 * (those are autofs maps specified by map entries
		 * to be automounted).
		 * They will be unmounted, if possible, when the
		 * top-level autofs mount they're under is
		 * unmounted - as will any other mounts under
		 * that top-level autofs mount.
		 */
		if (strcmp(mnt->f_mntfromname, "subtrigger") == 0 ||
		    strncmp(mnt->f_mntfromname, triggered, sizeof (triggered) - 1) == 0)
			continue;

		current = 0;
		for (dir = dir_head; dir; dir = dir->dir_next) {
			if (dir->dir_realpath != NULL &&
			    strcmp(dir->dir_realpath, mnt->f_mntonname) == 0) {
				current = strcmp(dir->dir_map, "-null");
				break;
			}
		}
		if (current)
			continue;

		/*
		 * Mark this as being unmounted, and try to unmount it.
		 */
		if (ioctl(autofs_control_fd, AUTOFS_UNMOUNT,
		    &mnt->f_fsid) == 0) {
			static const char slashnetwork[] = "/Network/";

			if (verbose) {
				pr_msg("%s unmounted",
					mnt->f_mntonname);
			}
			count++;

			/*
			 * If the path to this was under /Network,
			 * try to remove the directory it was
			 * mounted on, to keep /Network clean.
			 *
			 * (The system "owns" /Network, so we can get rid
			 * of stuff as we choose.  For other trigger mount
			 * points, we don't know whether we created the
			 * mount point, so we don't know whether we should
			 * remove it.)
			 */
			if (strncmp(mnt->f_mntonname, slashnetwork,
			    sizeof slashnetwork - 1) == 0)
				rmdir_r(mnt->f_mntonname);
		}
	}
	if (verbose && count == 0)
		pr_msg("no unmounts");
}

/*
 * Check whether two entries refer to the same directory; we compare
 * both the path name and the realpath()ed path name.
 */
static int
paths_match(struct autodir *d1, struct autodir *d2)
{
	if (strcmp(d1->dir_name, d2->dir_name) == 0)
		return (1);
	if (d1->dir_realpath != NULL) {
		if (strcmp(d1->dir_realpath, d2->dir_name) == 0)
			return (1);
		if (d2->dir_realpath != NULL) {
			if (strcmp(d1->dir_realpath, d2->dir_realpath) == 0)
				return (1);
		}
	}
	if (d2->dir_realpath != NULL) {
		if (strcmp(d1->dir_name, d2->dir_realpath) == 0)
			return (1);
	}
	return (0);
}

static int
mkdir_r(dir)
	char *dir;
{
	int err;
	char *slash;

	if (mkdir(dir, 0555) == 0) {
		/*
		 * We created the directory.
		 */
		return (0);
	}
	if (errno == EEXIST) {
		/*
		 * Something already existed there; we'll assume it's
		 * a directory.  (If it's not, something will fail later.)
		 */
		return (0);
	}
	if (errno != ENOENT) {
		/*
		 * We failed to create it for some reason other than
		 * the absence of a directory in the path leading up to
		 * it.  Give up.
		 */
		return (-1);
	}

	/*
	 * Create the parent directory (creating any directories leading
	 * up to it).
	 */
	slash = strrchr(dir, '/');
	if (slash == NULL)
		return (-1);
	*slash = '\0';
	err = mkdir_r(dir);
	*slash++ = '/';
	if (err || !*slash)
		return (err);
	return (mkdir(dir, 0555));
}

/*
 * Inverse of mkdir_r() - removes a directory, and then removes all
 * parent directories that it can, except for the one right under the
 * root directory, stopping when it can't remove one.
 * Modifies the path argument as it goes.
 */
void
rmdir_r(char *path)
{
	char *p;

	for (;;) {
		/*
		 * Look for the separator between us and the parent
		 * directory.
		 */
		p = strrchr(path, '/');
		if (p == NULL) {
			/*
			 * Not found; this is not an absolute path, so
			 * just give up.
			 */
			break;
		}
		if (p == path) {
			/*
			 * Our parent is the root directory, so we
			 * shouldn't be removed (we want /Network to
			 * stick around).
			 */
			break;
		}
		if (rmdir(path) == -1)
			break;	/* failed */

		/*
		 * Cut off our name, leaving the name of the parent
		 * directory.
		 */
		*p = '\0';
	}
}

/*
 * Check whether there are any Active Directory entries in the
 * Directory Services search path.
 */
static int
have_ad(void)
{
	int have_it = 0;
	CFErrorRef error;
	char *errstring;
	ODNodeRef node_ref;
	CFArrayRef paths;
	CFIndex num_paths;
	CFIndex i;
	CFStringRef path;

	/*
	 * Create the search node.
	 */
	error = NULL;
	node_ref = ODNodeCreateWithNodeType(kCFAllocatorDefault, kODSessionDefault, 
		kODNodeTypeAuthentication, &error);
	if (node_ref == NULL) {
		errstring = od_get_error_string(error);
		pr_msg("have_ad: can't create search node for /Search: %s",
		    errstring);
		free(errstring);
		return (0);
	}

	/*
	 * Get the search paths from the node.
	 */
	paths = ODNodeCopySubnodeNames(node_ref, &error);
	if (paths == NULL) {
		errstring = od_get_error_string(error);
		pr_msg("have_ad: can't get subnode names for /Search: %s",
		    errstring);
		free(errstring);
		return (0);
	}

	/*
	 * Scan the paths in that array looking for an Active
	 * Directory search path entry, i.e. one beginning with
	 * "/Active Directory".
	 */
	num_paths = CFArrayGetCount(paths);
	for (i = 0; i < num_paths && !have_it; i++) {
		path = CFArrayGetValueAtIndex(paths, i);

		/*
		 * Check whether this entry begins with "/Active Directory".
		 */
		have_it = CFStringHasPrefix(path, CFSTR("/Active Directory"));
	}
	CFRelease(paths);
	CFRelease(node_ref);
	return (have_it);
}

/*
 * Print an error.
 * It works like printf
 * (fmt string and variable args) except that it will prepend "automount:"
 * and substitute an error message for a "%m" string (like syslog).
 */
/* VARARGS1 */
void
pr_msg(const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ], *p2;
	const char *p1;

	(void) strlcpy(buf, "automount: ", sizeof buf);
	p2 = buf + strlen(buf);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			(void) strlcpy(p2, strerror(errno),
			    sizeof buf - (p2 - buf));
			p2 += strlen(p2);
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > buf && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';


	va_start(ap, fmt);
 	(void) vfprintf(stderr, buf, ap);
	va_end(ap);
}

static int
load_autofs(void)
{
	pid_t pid, terminated_pid;
	int result;
	union wait status;
    
	pid = fork();
	if (pid == 0) {
		result = execl(gKextLoadCommand, gKextLoadCommand, "-q",
		    gKextLoadPath, NULL);
		/* IF WE ARE HERE, WE WERE UNSUCCESSFUL */
		return (result ? result : ECHILD);
	}

	if (pid == -1)
		return (-1);

	/* Success! Wait for completion in-line here */
	while ((terminated_pid = wait4(pid, (int *)&status, 0, NULL)) < 0) {
		/* retry if EINTR, else break out with error */
		if (errno != EINTR)
			break;
	}
    
	if (terminated_pid == pid && WIFEXITED(status))
		result = WEXITSTATUS(status);
	else
		result = -1;

	return (result);
}
