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
 * Portions Copyright 2007 Apple Inc.  All rights reserved.
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
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fts.h>

#include <DirectoryService/DirectoryService.h>

#include "deflt.h"
#include "autofs.h"
#include "automount.h"
#include "automount_ds.h"

static int mkdir_r(char *);
static void rmdir_r(char *);
static int have_ad(void);
struct autodir *dir_head;
struct autodir *dir_tail;
static int num_current_mounts;
static struct statfs *current_mounts;
static struct statfs *find_mount(char *);
int verbose = 0;
int trace = 0;

static void usage(void);
static void do_unmounts(int);
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
	int c;
	int flushcache = 0;
	struct autodir *dir, *d;
	struct stat stbuf;
	char *master_map = "auto_master";
	int autofs_control_fd;
	int null;
	struct statfs *mntp;
	int count = 0;
	char *stack[STACKSIZ];
	char **stkptr;
	char *defval;
	int fd;

	/*
	 * Read in the values from config file first before we check
	 * commandline options so the options override the file.
	 */
	if ((defopen(AUTOFSADMIN)) == 0) {
		if ((defval = defread("AUTOMOUNT_TIMEOUT=")) != NULL) {
			errno = 0;
			mount_timeout = strtol(defval, (char **)NULL, 10);
			if (errno != 0)
				mount_timeout = AUTOFS_MOUNT_TIMEOUT;
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

	while ((c = getopt(argc, argv, "mM:D:f:t:vc?")) != EOF) {
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
			pr_msg("mount %s: can't load autofs kext: %s",
			    strerror(errno));
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

	num_current_mounts = getmntinfo(&current_mounts, MNT_NOWAIT);
	if (num_current_mounts == 0) {
		pr_msg("Couldn't get current mounts: %m");
		exit(1);
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
			if (strcmp(dir->dir_name, d->dir_name) == 0)
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
				if (strcmp(dir->dir_name, d->dir_name) == 0 &&
				    strcmp(d->dir_map, "-fstab") != 0 &&
				    strcmp(d->dir_map, "-static") != 0) {
					pr_msg("%s: ignoring redundant %s map",
					    dir->dir_name, dir->dir_map);
					continue;
				}
			}
		}

		/*
		 * Check whether there's already an entry
		 * in the mnttab for this mountpoint.
		 */
		if ((mntp = find_mount(dir->dir_name)) != NULL) {
			struct autofs_update_args au;

			/*
			 * If it's not an autofs mount - don't
			 * mount over it.
			 */
			if (strcmp(mntp->f_fstypename, MNTTYPE_AUTOFS) != 0) {
				pr_msg("%s: already mounted",
					mntp->f_mntfromname);
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
			au.mntflags	= 0;	/* XXX - right value? */
			au.mount_to	= mount_timeout;
#if 0
			au.mach_to	= AUTOFS_RPC_TIMEOUT;
#else
			au.mach_to	= 0;	/* XXX */
#endif
			au.direct 	= dir->dir_direct;

			if (ioctl(autofs_control_fd, AUTOFS_UPDATE_OPTIONS,
			    &au) < 0) {
				pr_msg("update %s: %m", dir->dir_name);
				continue;
			}
			if (verbose)
				pr_msg("%s updated", dir->dir_name);
		} else {
			struct autofs_args ai;

			/*
			 * This trigger isn't already mounted.
			 *
			 * Create a mount point if necessary
			 * If the path refers to an existing symbolic
			 * link, refuse to mount on it.  This avoids
			 * future problems.
			 */
			if (lstat(dir->dir_name, &stbuf) == 0) {
				if ((stbuf.st_mode & S_IFMT) != S_IFDIR) {
					pr_msg("%s: Not a directory", dir->dir_name);
					continue;
				}
			} else {
				if (mkdir_r(dir->dir_name)) {
					pr_msg("%s: %m", dir->dir_name);
					continue;
				}
			}

			/*
			 * Mount it.
			 */
			ai.version	= AUTOFS_ARGSVERSION;
			ai.path 	= dir->dir_name;
			ai.opts		= dir->dir_opts;
			ai.map		= dir->dir_map;
			ai.subdir	= "";
			ai.direct 	= dir->dir_direct;
			if (dir->dir_direct)
				ai.key = dir->dir_name;
			else
				ai.key = "";
			ai.mntflags	= 0;	/* XXX - right value? */
			ai.mount_to	= mount_timeout;
#if 0
			ai.mach_to	= AUTOFS_RPC_TIMEOUT;
#else
			ai.mach_to	= 0;	/* XXX */
#endif
			ai.trigger	= 0;	/* not a special trigger-point submount */

			if (mount(MNTTYPE_AUTOFS, dir->dir_name,
			    MNT_DONTBROWSE | MNT_AUTOMOUNTED, &ai) < 0) {
				pr_msg("mount %s: %m", dir->dir_name);
				continue;
			}
			if (verbose)
				pr_msg("%s mounted", dir->dir_name);
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
	do_unmounts(autofs_control_fd);

	/*
	 * Let PremountHomeDirectoryWithAuthentication() know that we're
	 * done.
	 */
	fd = open("/var/run/automount.initialized", O_CREAT|O_WRONLY, 0600);
	close(fd);

	return (0);
}

/*
 * Find the first mount entry given the mountpoint path.
 */
static struct statfs *
find_mount(mntpnt)
	char *mntpnt;
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
	pr_msg("Usage: automount  [ -vc ]  [ -t duration ]");
	exit(1);
	/* NOTREACHED */
}

/*
 * Unmount any autofs mounts that
 * aren't in the master map
 */
static void
do_unmounts(int autofs_control_fd)
{
	int i;
	struct statfs *mnt;
	struct autodir *dir;
	int current;
	int count = 0;

	for (i = 0; i < num_current_mounts; i++) {
		mnt = &current_mounts[i];
		if (strcmp(mnt->f_fstypename, MNTTYPE_AUTOFS) != 0)
			continue;
		/*
		 * Don't unmount autofs mounts done
		 * from the autofs mount command.
		 * How do we tell them apart ?
		 * Autofs mounts not eligible for auto-unmount
		 * have an f_mntfromname of "trigger".
		 */
		if (strcmp(mnt->f_mntfromname, "trigger") == 0)
			continue;

		current = 0;
		for (dir = dir_head; dir; dir = dir->dir_next) {
			if (strcmp(dir->dir_name, mnt->f_mntonname) == 0) {
				current = strcmp(dir->dir_map, "-null");
				break;
			}
		}
		if (current)
			continue;

		/*
		 * Try to unmount everything under this mount, so that
		 * we can unmount it.  Then flag this mount so references
		 * to it won't trigger any further mounts, wait for any
		 * in-progress mount attempts to complete, and unmount
		 * it.
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
	tDirReference session;
	tDirNodeReference node_ref;
	tDirStatus status;
	tDataListPtr attribute_type = NULL;
	static unsigned long attr_bufsize = 2*1024;
	tDataBufferPtr buffer = NULL;
	unsigned long num_results;
	tAttributeListRef attr_list_ref;
	tContextData context;
	tAttributeValueListRef value_list_ref;
	tAttributeEntry *attr_entry_p;
	unsigned long i;
	tAttributeValueEntry *value_entry_p;
	char *value;
	unsigned long value_len;
	static const char ad_prefix[] = "/Active Directory";
	size_t ad_prefix_len = sizeof ad_prefix - 1;

	/* Open an Open Directory session. */
	if (dsOpenDirService(&session) != eDSNoErr)
		return (0);

	/*
	 * Get the search node.
	 */
	if (ds_get_root_level_node(session, &node_ref) != __NSW_SUCCESS) {
		dsCloseDirService(session);
		return (0);
	}

	/*
	 * Build the tDataList containing the attribute type that we are
	 * searching for.
	 */
	attribute_type = dsBuildListFromStrings(session, kDS1AttrSearchPath,
	    NULL);
	if (attribute_type == NULL) {
		pr_msg(
		    "have_ad: can't build attribute type list: %s (%d)",
		    dsCopyDirStatusName(status), status);
		goto done;
	}

	/*
	 * Get the information about that attribute.
	 */
	for (;;) {
		/* Allocate a buffer. */
		buffer = dsDataBufferAllocate(session, attr_bufsize);
		if (buffer == NULL) {
			pr_msg("have_ad: malloc failed");
			goto done;
		}

		/* Get the node info. */
		status = dsGetDirNodeInfo(node_ref, attribute_type, buffer,
		    FALSE, &num_results, &attr_list_ref, &context);
		if (status != eDSBufferTooSmall) {
			/* Well, the buffer wasn't too small */
			break;
		}

		/*
		 * The buffer was too small; free the buffer, and try one
		 * twice as big.
		 */
		dsDataBufferDeAllocate(session, buffer);
		attr_bufsize = 2*attr_bufsize;
	}

	if (status != eDSNoErr) {
		pr_msg("have_ad: can't get root node info: %s (%d)",
		    dsCopyDirStatusName(status), status);
		goto done;
	}
	if (num_results == 0) {
		/* We didn't find any attribute values. */
		goto done;
	}

	/*
	 * We only care about the first attribute entry, as we only asked
	 * for one attribute.
	 */
	status = dsGetAttributeEntry(node_ref, buffer, attr_list_ref,
	    1, &value_list_ref, &attr_entry_p);
	if (status != eDSNoErr) {
		pr_msg("have_ad: dsGetAttributeEntry failed: %s (%d)",
		    dsCopyDirStatusName(status), status);
		goto done;
	}

	/*
	 * Scan the values for this attribute looking for an Active
	 * Directory search path entry, i.e. one beginning with
	 * "/Active Directory".
	 */
	for (i = 1; i <= attr_entry_p->fAttributeValueCount && !have_it; i++) {
		status = dsGetAttributeValue(node_ref, buffer, i,
		    value_list_ref, &value_entry_p);
		if (status != eDSNoErr) {
			pr_msg("have_ad: dsGetAttributeValue failed: %s (%d)",
			    dsCopyDirStatusName(status), status);
			dsDeallocAttributeEntry(session, attr_entry_p);
			dsCloseAttributeValueList(value_list_ref);
			goto done;
		}
		value = value_entry_p->fAttributeValueData.fBufferData;
		value_len = value_entry_p->fAttributeValueData.fBufferLength;

		/*
		 * Check for a value *not* in the local_search_dirs list.
		 * It indicates that there's an entry in the DS search path
		 * that could conceivably provide fstab entries.
		 */
		if (value_len >= ad_prefix_len &&
		    memcmp(value, ad_prefix, ad_prefix_len) == 0) {
			/*
			 * This entry begins with "/Active Directory".
			 */
			have_it = 1;
		}

		dsDeallocAttributeValueEntry(session, value_entry_p);
	}
	dsDeallocAttributeEntry(session, attr_entry_p);
	dsCloseAttributeValueList(value_list_ref);

done:
	if (buffer != NULL)
		dsDataBufferDeAllocate(session, buffer);
	if (attribute_type != NULL) {
		dsDataListDeallocate(session, attribute_type);
		free(attribute_type);
	}
	dsCloseDirNode(node_ref);
	dsCloseDirService(session);
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

	(void) strcpy(buf, "automount: ");
	p2 = buf + strlen(buf);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			if (errno < sys_nerr) {
				(void) strcpy(p2, sys_errlist[errno]);
				p2 += strlen(p2);
			}
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
