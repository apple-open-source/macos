/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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
/*-
 * Copyright (c) 1980, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <System/sys/reason.h>
#include <err.h>
#include <os/errno.h>
#include <os/bsd.h>
#include <os/variant_private.h>
#include <fstab.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <TargetConditionals.h>
#include <sysexits.h>
#include <sys/sysctl.h>
#include <APFS/APFS.h>
#include <APFS/APFSConstants.h>

#include "edt_fstab.h"
#include "fsck.h"
#include "mount_flags.h"
#include "mount_ramdisk.h"
#include "pathnames.h"
#include "vfslist.h"

// Defines
#if TARGET_OS_OSX
#define APFS_BOOT_UTIL_PATH "/System/Library/Filesystems/apfs.fs/Contents/Resources/apfs_boot_util"
#define PLATFORM_DATA_VOLUME_MOUNT_POINT "/System/Volumes/Data"
#else
#define APFS_BOOT_UTIL_PATH "/System/Library/Filesystems/apfs.fs/apfs_boot_util"
#define PLATFORM_DATA_VOLUME_MOUNT_POINT "/private/var"
#endif
/*
 * mount phases to be used during boot to perform the following operations:
 * first phase:
 *      TARGET_OS_OSX: Call `apfs_boot_util 1`.
 *      TARGET_OS_IPHONE: Mount System, Preboot and xART volumes (if present)
                          and setup required bindfs mounts.
 *
 * second phase:
 *      TARGET_OS_OSX: Upgrade System volume to RW or call `apfs_boot_util 2`
 *                     (on ROSV config).
 *      TARGET_OS_IPHONE: Mount remaining volumes and setup additional bindfs mounts.
 *                        Call `apfs_boot_util 2` if needed.
 *
 * For more info on mount phases, see apfs_boot_util.
 */
#define MOUNT_PHASE_1      1       /* first phase */
#define MOUNT_PHASE_2      2       /* second phase */

#define NONFS   "nonfs"

#define BADTYPE(type) (strcmp(type, FSTAB_RO) && \
                       strcmp(type, FSTAB_RW) && \
                       strcmp(type, FSTAB_RQ))

// Globals
int bootstrap_macos = 0;
int debug = 0;
int passno = 0;
int ret_errno = 0;
int verbose = 0;

// Externs
extern mountopt_t optnames[];

// Function declarations
struct statfs *getmntpt(const char *name);
int hasopt(const char *mntopts, const char *option);
char *catopt(char *s0, const char *s1);
void mangle(char *options, int *argcp, const char **argv);
void usage(void);
void prmount(struct statfs *sfp);
void print_mount(const char **vfslist);
int ismounted(const char *fs_spec, const char *fs_file, long *flags);
int mountfs(const char *vfstype, const char *fs_spec, const char *fs_file,
			int flags, const char *options, const char *mntopts);
int upgrade_mount(const char *mountpt, int init_flags, char *options);

/*
 * Map a POSIX error code to a representative sysexits(3) code. Can be disabled
 * to exit with errno error code by passing -e as a command line argument to mount
 */
int
errno_or_sysexit(int err, int sysexit)
{
	if (sysexit == -1) {
		sysexit = sysexit_np(err);
	}
	return (ret_errno ? err : sysexit);
}

#if TARGET_OS_OSX
static int
booted_apfs(void) {
	struct statfs *mntbuf;

	if ((mntbuf = getmntpt("/")) == NULL) {
		errx(errno_or_sysexit(errno, -1),
			 "failed to lookup root file system");
	}

	return (strcmp(mntbuf->f_fstypename, "apfs") == 0);
}

static int
booted_rosv(void) {
	/* use sysctl to query kernel */
	uint32_t is_rosp = 0;
	size_t rospsize = sizeof(is_rosp);
	int err = sysctlbyname ("vfs.generic.apfs.rosp", &is_rosp, &rospsize, NULL, NULL);

	if (!err && is_rosp) {
		return 1;
	}

	return 0;
}
#else /* !TARGET_OS_OSX */

#define BINDFS_MOUNT_TYPE       "bindfs"
#define PREBOOT_VOL_MOUNTPOINT  "/private/preboot"
#define HARDWARE_VOL_MOUNTPOINT "/private/var/hardware"

#define BOOT_MANIFEST_HASH_LEN  256

typedef struct bind_mount {
	char *bm_mnt_prefix;
	char *bm_mnt_to;
	bool bm_mandatory;
} bind_mount_t;

static void
do_bindfs_mount(char *from, const char *to, bool required)
{
	struct statfs sfs;
	uint32_t mnt_flags = MNT_RDONLY | MNT_NODEV | MNT_NOSUID | MNT_DONTBROWSE;
	int err = 0;
	
	if (debug) {
		printf("call: mount %s %s %x %s", BINDFS_MOUNT_TYPE, to, mnt_flags, from);
		return;
	}
	
	if ((statfs(from, &sfs) == 0) &&
		(strncmp(sfs.f_fstypename, BINDFS_MOUNT_TYPE, sizeof(BINDFS_MOUNT_TYPE)) == 0)) {
		return;
	}
	
	err = mount(BINDFS_MOUNT_TYPE, to, mnt_flags, from);
	if (err) {
		if ((errno == ENOENT) && !required) {
			err = 0;
		} else {
			errx(errno_or_sysexit(errno, -1), "failed to mount %s -> %s - %s(%d)",
				 from, to, strerror(errno), errno);
		}
	}
}

static void
setup_preboot_mounts(int pass, uint32_t os_env)
{
	int err = 0;
	const char *mnt_to;
	char mnt_from[MAXPATHLEN], boot_manifest_hash[BOOT_MANIFEST_HASH_LEN];
	// For other boot environments allow these to fail
	// as they aren't necessarily needed there.
	bool required = (os_env == EDT_OS_ENV_MAIN);

	err = get_boot_manifest_hash(boot_manifest_hash, sizeof(boot_manifest_hash));
	if (err) {
		errx(errno_or_sysexit(err, -1), "failed to get boot manifest hash - %s",
			 strerror(err));
	}

	const bind_mount_t preboot_mnts[] = {
		{.bm_mnt_prefix = PREBOOT_VOL_MOUNTPOINT,
			.bm_mnt_to = "/usr/standalone/firmware",
			.bm_mandatory = true}
	};

	const bind_mount_t hw_mnts[] = {
		{.bm_mnt_prefix = HARDWARE_VOL_MOUNTPOINT "/Pearl",
			.bm_mnt_to = "/System/Library/Pearl/ReferenceFrames",
			.bm_mandatory = false},
		{.bm_mnt_prefix = HARDWARE_VOL_MOUNTPOINT "/FactoryData",
			.bm_mnt_to = "/System/Library/Caches/com.apple.factorydata",
			.bm_mandatory = true}
	};

	// Skip preboot bindfs mounts for EDT_OS_ENV_OTHER,
	// they aren't needed in that boot environment.
	if (pass == ROOT_PASSNO && (os_env != EDT_OS_ENV_OTHER)) {
		for (int i = 0; i < (sizeof(preboot_mnts) / sizeof(preboot_mnts[0])); i++) {
			mnt_to = preboot_mnts[i].bm_mnt_to;
			snprintf(mnt_from, sizeof(mnt_from), "%s/%s%s",
					 preboot_mnts[i].bm_mnt_prefix, boot_manifest_hash, mnt_to);

			do_bindfs_mount(mnt_from, mnt_to, required && preboot_mnts[i].bm_mandatory);
		}
	} else {
		for (int i = 0; i < (sizeof(hw_mnts) / sizeof(hw_mnts[0])); i++) {
			mnt_to = hw_mnts[i].bm_mnt_to;
			snprintf(mnt_from, sizeof(mnt_from), "%s%s",
					 hw_mnts[i].bm_mnt_prefix, mnt_to);

			do_bindfs_mount(mnt_from, mnt_to, required && hw_mnts[i].bm_mandatory);
		}
	}
}
#endif /* !TARGET_OS_OSX */

static void
bootstrap_apfs(int phase)
{
	char * const apfs_util_argv[] = {
		APFS_BOOT_UTIL_PATH,
		((phase == MOUNT_PHASE_1) ? "1" : ((phase == MOUNT_PHASE_2) ? "2" : NULL)),
		NULL,
	};
	execv(APFS_BOOT_UTIL_PATH, apfs_util_argv);
	errx(errno_or_sysexit(errno, -1), "apfs_boot_util exec failed");
}

int
main(int argc, char *argv[])
{
	const char **vfslist, *vfstype;
	struct fstab *fs;
	struct statfs *mntbuf;
	int all, ch, init_flags, rval;
	char *options, *ep;
	int mount_phase = 0;
	char fs_file[MAXPATHLEN] = {};
	char *fs_spec = NULL;

	all = init_flags = 0;
	options = NULL;
	vfslist = NULL;
	vfstype = NULL;

	while ((ch = getopt(argc, argv, "headfo:rwt:uvP:")) != EOF)
		switch (ch) {
			case 'a':
				all = 1;
				break;
			case 'd':
				debug = 1;
				break;
			case 'f':
				init_flags |= MNT_FORCE;
				break;
			case 'o':
				if (*optarg) {
					options = catopt(options, optarg);
					if (strstr(optarg, "union"))
						init_flags |= MNT_UNION;
				}
				break;
			case 'r':
				init_flags |= MNT_RDONLY;
				break;
			case 't':
				if (vfslist != NULL)
					errx(errno_or_sysexit(EINVAL, EX_USAGE),
						 "only one -t option may be specified.");
				vfslist = makevfslist(optarg);
				vfstype = optarg;
				break;
			case 'u':
				init_flags |= MNT_UPDATE;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'w':
				init_flags &= ~MNT_RDONLY;
				break;
			case 'e':
				ret_errno = 1;
				break;
			case 'P':
				/* only allowed to specify 1 or 2 as argument here */
				mount_phase = (int)strtol(optarg, &ep, 10);
				if ((ep == optarg) || (*ep) ||
					(mount_phase < MOUNT_PHASE_1) ||
					(mount_phase > MOUNT_PHASE_2)) {
					errx(errno_or_sysexit(EINVAL, EX_USAGE),
						 "-P flag requires a valid mount phase number");
				}
				break;
			case 'h':
			case '?':
			default:
				usage();
				/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	// mount boot tasks
	if (mount_phase != 0) {
#if TARGET_OS_OSX
		bootstrap_macos = 1;
#else /* !TARGET_OS_OSX */
		if (mount_phase == MOUNT_PHASE_1) {
			/* mount -vat -nonfs -P 1 */
			passno = ROOT_PASSNO;
		} else if (mount_phase == MOUNT_PHASE_2) {
			/* mount -vat -nonfs -R 2 */
			passno = NONROOT_PASSNO;
		}
		verbose = 1;
		all = 1;
		vfslist = makevfslist(NONFS);
		vfstype = NONFS;
#endif /* !TARGET_OS_OSX */
	}
	
	rval = 0;
	switch (argc) {
		case 0:
			/*
			 * Note - mount should never be called with "-a" on OSX
			 *        as per fstab(5) - you may insert entries with UUID=,LABEL=
			 *        and mount(8) has no knowledge of these entries
			 */
			if (all) {
				int err = 0;
				long fs_flags = 0;

				if ((setfsent() == 0)) {
					errx(errno_or_sysexit(errno ? errno : ENXIO, -1),
						 "mount: can't get filesystem checklist");
				}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
				uint32_t os_env;
				const char *container_dev = get_boot_container(&os_env);
				const char *data_vol_dev = get_data_volume();
				/*
				 * It is a given that the boot container must be present
				 * in order to locate the data volume.
				 *
				 * The data volume is required if it is present in the EDT.
				 * This is always the case when booting the main OS (EDT_OS_ENV_MAIN),
				 * however there are some exceptions.
				 *
				 * When the data volume is required,
				 * we defer checking it is present until the second mount-phase,
				 * when the data volume is actually mounted, in order to allow
				 * MobileObliteration the opportunity to fix the container or fail gracefully.
				 */
				if (data_vol_dev) {
					fprintf(stdout, "mount: found boot container: %s, data volume: %s env: %u\n",
							container_dev, data_vol_dev, os_env);
				} else if ((os_env == EDT_OS_ENV_MAIN) &&
						   (passno == MOUNT_PHASE_2)) {
					errx(errno_or_sysexit(errno ? errno : ENXIO, -1),
						 "mount: missing data volume");
				} else {
					fprintf(stdout, "mount: data volume missing, but not required in env: %u\n",
							os_env);
				}
#endif

				while ((fs = getfsent()) != NULL) {
					int ro_mount = !strcmp(fs->fs_type, FSTAB_RO);
					
					if (BADTYPE(fs->fs_type))
						continue;
					if (checkvfsname(fs->fs_vfstype, vfslist))
						continue;
					if (hasopt(fs->fs_mntops, "noauto"))
						continue;
					if (!strcmp(fs->fs_vfstype, "nfs") || !strcmp(fs->fs_vfstype, "url")) {
						if (hasopt(fs->fs_mntops, "net"))
							continue;
						/* check if already mounted */
						if (fs->fs_spec == NULL ||
							fs->fs_file == NULL ||
							ismounted(fs->fs_spec, fs->fs_file, NULL))
							continue;
					}

					/* If this volume is not needed for this pass, skip it. */
					if (passno && fs->fs_passno != passno)
						continue;

					/*
					 * Check if already mounted:
					 *  1) If mounted RW this is either an attempt to
					 *      downgrade (RW -> RO) or someone else already
					 *      mounted this volume as RW.
					 *  2) If mounted RO and not upgrading to RW nothing
					 *      nothing need to be done so we should skip this entry.
					 * Skip this entry in both cases (basically only keep going
					 * if this is an acctual mount RW upgrade).
					 */
					if (ismounted(fs->fs_spec, fs->fs_file, &fs_flags) &&
						(!(fs_flags & MNT_RDONLY) || ro_mount))
						continue;

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
					if (!strcmp(fs->fs_spec, RAMDISK_FS_SPEC)) {
						if (verbose) {
							fprintf(stdout, "mount: encountered ramdisk\n");
						}
						rval = create_mount_ramdisk(fs, init_flags, options);
						continue;
					} else if (fs->fs_passno > ROOT_PASSNO &&
							   !strcmp(fs->fs_vfstype, EDTVolumeFSType) &&
							   !strcmp(fs->fs_type, FSTAB_RW)) {
						/*
						 * Perform media keys migration if this is the data volume
						 * of the main OS environment
						 */
						if (!debug && data_vol_dev &&
							(os_env == EDT_OS_ENV_MAIN) &&
							(strcmp(data_vol_dev, fs->fs_spec) == 0)) {
							kern_return_t mig_err = APFSContainerMigrateMediaKeys(container_dev);
							if (mig_err) {
								fprintf(stderr, "mount: failed to migrate Media Keys, error = %x\n", mig_err);
							} else {
								fprintf(stdout, "mount: successfully migrated Media Keys\n");
							}
						}
					}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

					if ((err = mountfs(fs->fs_vfstype, fs->fs_spec,
									   fs->fs_file, init_flags, options,
									   fs->fs_mntops)))
						rval = err;
				}
				endfsent();

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
				/* Setup bindfs mounts */
				setup_preboot_mounts(passno, os_env);

				/* Hand the rest of the process over to apfs_boot_util */
				if (os_variant_has_internal_content(APFS_BUNDLE_ID) &&
					(mount_phase == MOUNT_PHASE_2)) {
					bootstrap_apfs(MOUNT_PHASE_2);
				}
#endif
			} else if (bootstrap_macos) {
#if TARGET_OS_OSX
				if (mount_phase == MOUNT_PHASE_1) {
					if (booted_apfs()) {
						bootstrap_apfs(MOUNT_PHASE_1);
					} else {
						fprintf(stdout, "Not booted from APFS, skipping apfs_boot_util\n");
						exit(0);
					}
				} else if (mount_phase == MOUNT_PHASE_2) {
					/*
					 * We centralize the logic for dealing with a read-only system (ROSV) volume here.
					 * If it is not set up, then default to the old logic of a `mount -uw /`
					 *
					 * Note: We can safely do this boot-task even if we are
					 *       already mounted RW (e.g. boot from single user mode).
					 *       In that case this will effectively be a no-op.
					 */
					if (booted_rosv()) {
						/* Hand the rest of the process over to apfs_boot_util */
						bootstrap_apfs(MOUNT_PHASE_2);
					} else {
						/* upgrade mount "/" read-write */
						rval = upgrade_mount("/", MNT_UPDATE, options);
					}
				}
#endif /* TARGET_OS_OSX */
			} else {
				print_mount(vfslist);
			}
			exit(rval);
		case 1:
			/* make sure the device node or mount point exists */
			if (strlen(*argv) > sizeof(fs_file))
				errx(errno_or_sysexit(EINVAL, ENAMETOOLONG),
					 "special file or file system %s too long.",
					 *argv);
			if (realpath(*argv, fs_file) == NULL) {
				errx(errno_or_sysexit(errno , -1),
					 "%s: invalid special file or file system.",
					 *argv);
			}

			if (vfslist != NULL)
				usage();

			if (init_flags & MNT_UPDATE) {
				rval = upgrade_mount (fs_file, init_flags, options);
				break;
			}

			if ((fs = getfsfile(fs_file)) == NULL &&
				(fs = getfsspec(fs_file)) == NULL)
				errx(errno_or_sysexit(errno , -1),
					 "%s: unknown special file or file system.",
					 *argv);
			if (BADTYPE(fs->fs_type))
				errx(errno_or_sysexit(EINVAL, EX_DATAERR),
					 "%s has unknown file system type.",
					 *argv);
			if (!strcmp(fs->fs_vfstype, "nfs")) {
				if (hasopt(fs->fs_mntops, "net"))
					errx(errno_or_sysexit(EINVAL, EX_DATAERR),
						 "%s is owned by the automounter.",
						 *argv);
				if (ismounted(fs->fs_spec, fs->fs_file, NULL))
					errx(errno_or_sysexit(EALREADY, EX_CONFIG),
						 "%s is already mounted at %s.",
						 fs->fs_spec, fs->fs_file);
			}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
			if (!strcmp(fs->fs_spec, RAMDISK_FS_SPEC)) {
				if (verbose) {
					fprintf(stdout, "Found a ramdisk entry\n");
				}
				rval = create_mount_ramdisk(fs, init_flags, options);
				break;
			}
#endif
			rval = mountfs(fs->fs_vfstype, fs->fs_spec, fs->fs_file,
						   init_flags, options, fs->fs_mntops);
			break;
		case 2:
			fs_spec = argv[0];
			/* make sure the mount point exists */
			if (strlen(argv[1]) > sizeof(fs_file)) {
				errx(errno_or_sysexit(ENAMETOOLONG, EX_DATAERR),
					 "file system %s too long.", argv[1]);
			}
			if (realpath(argv[1], fs_file) == NULL) {
				errx(errno_or_sysexit(errno , -1),
					 "%s: invalid file system.", argv[1]);
			}

			/*
			 * If -t flag has not been specified, and spec contains a ':'
			 * then assume that an NFS filesystem is being specified.
			 */
			if (vfslist == NULL && strchr(argv[0], ':') != NULL) {
				vfstype = "nfs";
				/* check if already mounted */
				if (ismounted(fs_spec, fs_file, NULL))
					errx(errno_or_sysexit(EALREADY, EX_CONFIG),
						 "%s is already mounted at %s.",
						 fs_spec, fs_file);
			}

			/* If we have both a devnode and a pathname, and an update mount was requested,
			 * then figure out where the devnode is mounted.  We will need to run
			 * an update mount on its path.  It wouldn't make sense to do an
			 * update mount on a path other than the one it's already using.
			 *
			 * XXX: Should we implement the same workaround for updating the
			 * root file system at boot time?
			 */
			if (init_flags & MNT_UPDATE) {
				if ((mntbuf = getmntpt(fs_file)) == NULL)
					errx(errno_or_sysexit(errno ? errno : ENOENT, -1),
						 "unknown special file or file system %s.",
						 *argv);
				rval = mountfs(mntbuf->f_fstypename, mntbuf->f_mntfromname,
							   mntbuf->f_mntonname, init_flags, options, 0);
			}
			else {
				/*
				 * If update mount not requested, then go with the vfstype and arguments
				 * specified.  If no vfstype specified, then error out.
				 */
				if (vfstype == NULL) {
					errx (errno_or_sysexit(EINVAL, EX_USAGE),
						  "You must specify a filesystem type with -t.");
				}
				rval = mountfs(vfstype, fs_spec, fs_file, init_flags, options, NULL);
			}
			break;
		default:
			usage();
			/* NOTREACHED */
	}

	exit(rval);
}

int
upgrade_mount(const char *mountpt, int init_flags, char *options)
{
	struct statfs *mntbuf = NULL;
	const char *mntfromname = NULL;
	struct fstab *fs = NULL;

	if ((mntbuf = getmntpt(mountpt)) == NULL) {
		errx(errno_or_sysexit(errno, -1),
			 "unknown special file or file system %s.",
			 mountpt);
	}

	/*
	 * Handle the special case of upgrading the root file
	 * system from read-only to read/write.  The root file
	 * system was originally mounted with a "mount from" name
	 * of "root_device".  The getfsfile("/") returns non-
	 * NULL at this point, with fs_spec set to the true
	 * path to the root device (regardless of what either the real
	 * or synthesized /etc/fstab contained).
	 */
	mntfromname = mntbuf->f_mntfromname;
	if (strchr(mntfromname, '/') == NULL) {
		fs = getfsfile(mntbuf->f_mntonname);
		if (fs != NULL)
			mntfromname = fs->fs_spec;
	}

	/*
	 * Handle the special case of upgrading a content protected
	 * file system from read-only to read/write. While our caller
	 * is nominally required to pass the protect option to maintain
	 * content protection, the kernel requires it anyway, so just add it
	 * in.
	 */
	if (mntbuf->f_flags & MNT_CPROTECT) {
		init_flags |= MNT_CPROTECT;
	}

	/* Do the update mount */
	return mountfs(mntbuf->f_fstypename, mntfromname,
				   mntbuf->f_mntonname, init_flags, options, 0);
}

int
hasopt(const char *mntopts, const char *option)
{
	int negative, found;
	char *opt, *optbuf;

	if (option[0] == 'n' && option[1] == 'o') {
		negative = 1;
		option += 2;
	} else
		negative = 0;
		optbuf = strdup(mntopts);
		found = 0;
		for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
			if (opt[0] == 'n' && opt[1] == 'o') {
				if (!strcasecmp(opt + 2, option))
					found = negative;
			} else if (!strcasecmp(opt, option))
				found = !negative;
		}
	free(optbuf);
	return (found);
}

int
ismounted(const char *fs_spec, const char *fs_file, long *flags)
{
	int i, mntsize;
	struct statfs *mntbuf;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		err(errno_or_sysexit(errno , -1), "getmntinfo");
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mntbuf[i].f_mntfromname, fs_spec))
			continue;
		if (strcmp(mntbuf[i].f_mntonname, fs_file))
			continue;
		if (flags)
			*flags = mntbuf[i].f_flags;
		return 1;
	}
	return 0;
}

// prints currently mounted filesystems
void
print_mount(const char **vfslist)
{
	struct statfs *mntbuf;
	int mntsize;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		err(errno_or_sysexit(errno , -1), "getmntinfo");
	for (int i = 0; i < mntsize; i++) {
		if (checkvfsname(mntbuf[i].f_fstypename, vfslist))
			continue;
		prmount(&mntbuf[i]);
	}
}

// returns 0 upon success and a valid sysexit or errno code upon failure
int
mountfs(const char *vfstype, const char *fs_spec, const char *fs_file, int flags,
		const char *options, const char *mntopts)
{
	/* List of directories containing mount_xxx subcommands. */
	static const char *edirs[] = {
		_PATH_SBIN,
		_PATH_USRSBIN,
		NULL
	};
	static const char *bdirs[] = {
		_PATH_FSBNDL,
		_PATH_USRFSBNDL,
		NULL
	};
	const char *argv[100], **edir, **bdir;
	struct statfs sf;
	pid_t pid;
	int argc, i, status;
	char *optbuf, execname[MAXPATHLEN + 1], mntpath[MAXPATHLEN];

	if (realpath(fs_file, mntpath) == NULL) {
		/* Attempt to create missing mountpoints on Data volume */
		if ((passno == NONROOT_PASSNO) &&
			(!strncmp(mntpath, PLATFORM_DATA_VOLUME_MOUNT_POINT,
					  MIN(strlen(mntpath), strlen(PLATFORM_DATA_VOLUME_MOUNT_POINT))))) {
			if (mkdir(mntpath, S_IRWXU)) {
				warn("mkdir %s", mntpath);
				return (errno_or_sysexit(errno , -1));
			}
		} else {
			warn("realpath %s", mntpath);
			return (errno_or_sysexit(errno , -1));
		}
	}
	fs_file = mntpath;

	if (mntopts == NULL)
		mntopts = "";
	if (options == NULL) {
		if (*mntopts == '\0') {
			options = "";
		} else {
			options = mntopts;
			mntopts = "";
		}
	}
	optbuf = catopt(strdup(mntopts), options);

	if ((strcmp(fs_file, "/") == 0) && !(flags & MNT_UNION))
		flags |= MNT_UPDATE;
	if (flags & MNT_FORCE)
		optbuf = catopt(optbuf, "force");
	if (flags & MNT_RDONLY)
		optbuf = catopt(optbuf, "ro");
	if (flags & MNT_UPDATE)
		optbuf = catopt(optbuf, "update");
	if (flags & MNT_DONTBROWSE)
		optbuf = catopt(optbuf, "nobrowse");
	if (flags & MNT_CPROTECT)
		optbuf = catopt(optbuf, "protect");

	argc = 0;
	argv[argc++] = vfstype;
	mangle(optbuf, &argc, argv);
	argv[argc++] = fs_spec;
	argv[argc++] = fs_file;
	argv[argc] = NULL;

	if (debug) {
		(void)printf("exec: mount_%s", vfstype);
		for (i = 1; i < argc; i++)
			(void)printf(" %s", argv[i]);
		(void)printf("\n");
		return (0);
	}

	switch (pid = fork()) {
		case -1:                /* Error. */
			warn("fork");
			free(optbuf);
			return (errno_or_sysexit(errno, EX_OSERR));
		case 0:                    /* Child. */
			/* Go find an executable. */
			edir = edirs;
			do {
				(void)snprintf(execname, sizeof(execname),
							   "%s/mount_%s", *edir, vfstype);

				argv[0] = execname;
				execv(execname, (char * const *)argv);
				if (errno != ENOENT)
					warn("exec %s for %s", execname, fs_file);
			} while (*++edir != NULL);

			bdir = bdirs;
			do {
				/* Special case file system bundle executable path */
				(void)snprintf(execname, sizeof(execname),
							   "%s/%s.fs/%s/mount_%s", *bdir,
							   vfstype, _PATH_FSBNDLBIN, vfstype);
				
				argv[0] = execname;
				execv(execname, (char * const *)argv);
				if (errno != ENOENT)
					warn("exec %s for %s", execname, fs_file);
			} while (*++bdir != NULL);

			if (errno == ENOENT) {
				warn("exec %s for %s", execname, fs_file);
				return (errno_or_sysexit(errno, EX_OSFILE));
			}
			exit(errno_or_sysexit(errno , -1));
			/* NOTREACHED */
		default:                /* Parent. */
			free(optbuf);

			if (waitpid(pid, &status, 0) < 0) {
				warn("waitpid");
				return (errno_or_sysexit(errno , -1));
			}

			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					warnx("%s failed with %d", fs_file, WEXITSTATUS(status));
					return (errno_or_sysexit(EINTR, WEXITSTATUS(status)));
				}
			} else if (WIFSIGNALED(status)) {
				warnx("%s: %s", fs_file, sys_siglist[WTERMSIG(status)]);
				return (errno_or_sysexit(EINTR, EX_UNAVAILABLE));
			}

			if (verbose) {
				if (statfs(fs_file, &sf) < 0) {
					warn("statfs %s", fs_file);
					return (errno_or_sysexit(errno , -1));
				}
				prmount(&sf);
			}
			break;
	}

	return (EX_OK);
}

static bool
is_sealed(const char *mntpt)
{
	struct vol_attr {
		uint32_t len;
		vol_capabilities_attr_t vol_cap;
	} vol_attrs = {};

	struct attrlist vol_attr_list = {
		.bitmapcount = ATTR_BIT_MAP_COUNT,
		.volattr = ATTR_VOL_CAPABILITIES
	};

	if (!getattrlist(mntpt, &vol_attr_list, &vol_attrs, sizeof(vol_attrs), 0) &&
		vol_attrs.vol_cap.valid[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_SEALED) {
		return (vol_attrs.vol_cap.capabilities[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_SEALED);
	}
	return false;
}

void
prmount(struct statfs *sfp)
{
	int flags;
	mountopt_t *o;
	struct passwd *pw;

	printf("%s on %s (%s", sfp->f_mntfromname, sfp->f_mntonname, sfp->f_fstypename);

	if (is_sealed(sfp->f_mntonname))
		printf(", sealed");
		flags = sfp->f_flags & MNT_VISFLAGMASK;
		for (o = optnames; flags && o->o_opt; o++)
			if (flags & o->o_opt) {
				(void)printf(", %s", o->o_name);
				flags &= ~o->o_opt;
			}
	if (sfp->f_owner) {
		printf(", mounted by ");
		if ((pw = getpwuid(sfp->f_owner)) != NULL)
			printf("%s", pw->pw_name);
		else
			printf("%d", sfp->f_owner);
	}
	printf(")\n");
}

struct statfs *
getmntpt(const char *name)
{
	struct statfs *mntbuf;
	int i, mntsize;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mntbuf[i].f_mntfromname, name) == 0 ||
			strcmp(mntbuf[i].f_mntonname, name) == 0)
			return (&mntbuf[i]);
	}
	return (NULL);
}

char *
catopt(char *s0, const char *s1)
{
	size_t i;
	char *cp;

	if (s0 && *s0) {
		i = strlen(s0) + strlen(s1) + 1 + 1;
		if ((cp = malloc(i)) == NULL)
			err(errno_or_sysexit(errno, EX_TEMPFAIL),
				"failed to allocate memory for arguments");
		(void)snprintf(cp, i, "%s,%s", s0, s1);
	} else {
		cp = strdup(s1);
	}

	if (s0) {
		free(s0);
	}

	return (cp);
}

void
mangle(char *options, int *argcp, const char **argv)
{
	char *p, *s;
	int argc;

	argc = *argcp;
	for (s = options; (p = strsep(&s, ",")) != NULL;)
		if (*p != '\0') {
			if (*p == '-') {
				argv[argc++] = p;
				p = strchr(p, '=');
				if (p) {
					*p = '\0';
					argv[argc++] = p+1;
				}
			} else {
				argv[argc++] = "-o";
				argv[argc++] = p;
			}
		}
	*argcp = argc;
}

void
usage(void)
{
	fprintf(stderr,
			"usage: mount %s %s\n       mount %s\n       mount %s\n",
			"[-dfruvw] [-o options] [-t external_type]",
			"special mount_point",
			"[-adfruvw] [-t external_type]",
			"[-dfruvw] special | mount_point");
	exit(errno_or_sysexit(EINVAL, EX_USAGE));
}
