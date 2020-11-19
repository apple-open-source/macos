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
#include <pthread.h>
#include <spawn.h>
#include <crt_externs.h>

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include <paths.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/cdefs.h>

/* Some APFS specific goop */
#include <copyfile.h>
#include <MediaKit/MKMedia.h>
#include <MediaKit/MKMediaAccess.h>
#include <MediaKit/GPTTypes.h>
#endif

#if TARGET_OS_OSX
// To unmount the BaseSystem disk image.
#include <paths.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#endif

#include "mount_flags.h"
#include "edt_fstab.h"
#include "pathnames.h"
#include "fsck.h"

#if TARGET_OS_OSX
#define APFS_BOOT_UTIL_PATH   "/System/Library/Filesystems/apfs.fs/Contents/Resources/apfs_boot_util"
#define PLATFORM_DATA_VOLUME_MOUNT_POINT "/System/Volumes/Data"
#define BASE_SYSTEM_PATH "/System/Volumes/BaseSystem"
#define RECOVERY_PATH "/System/Volumes/Recovery"
#else
#define APFS_BOOT_UTIL_PATH   "/System/Library/Filesystems/apfs.fs/apfs_boot_util"
#define PLATFORM_DATA_VOLUME_MOUNT_POINT "/private/var"
#endif

#define environ (*_NSGetEnviron())
#define COMMAND_OUTPUT_MAX 1024

int debug;
int verbose;
int bootstrap_macos = 0;
int passno = 0;

int checkvfsname __P((const char *, const char **));
char   *catopt __P((char *, const char *));
struct statfs *getmntpt __P((const char *));
int hasopt __P((const char *, const char *));
const char
      **makevfslist __P((char *));
void    mangle __P((char *, int *, const char **));
void    prmount __P((struct statfs *));
void    usage __P((void));

int     run_command(char **command_argv, char *output, int *rc, int *signal_no);
void    print_mount(const char **vfslist);
int     ismounted(const char *fs_spec, const char *fs_file, long *flags);
int     mountfs(const char *vfstype, const char *fs_spec, const char *fs_file, int flags, const char *options, const char *mntopts);
int     unmount_location(char *mount_point);

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

char* parse_parameter_for_token(char * opts, char * search_string);
int verify_executable_file_existence (char *path);
int verify_file_existence (char *path);
int _verify_file_flags (char *path, int flags);
int preflight_create_mount_ramdisk (char *mnt_opts, size_t *ramdisk_size, char *template);
char* split_ramdisk_params(char *opts);
int create_mount_ramdisk(struct fstab *fs, int init_flags, char *options);
int construct_apfs_volume(char *mounted_device_name);
int create_partition_table(size_t partition_size, char *device);
int attach_device(size_t device_size , char* deviceOut);
void truncate_whitespace(char* str);

#define RAMDISK_BLCK_OFFSET 34
#define RAMDISK_TMP_MOUNT "/mnt2"
#define RAMDISK_BCK_MOUNT "/.mb"
#define RAMDISK_SIZE_TOK  "size="
#define RAMDISK_TPLT_TOK  "template="
#define HDIK_PATH         "/usr/sbin/hdik"

#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

//pull in the optnames array from the mount_flags.c file
extern mountopt_t optnames[];
#if TARGET_OS_OSX
static int booted_rosv(void);
static int booted_apfs(void);
#endif /* TARGET_OS_OSX */
static int upgrade_mount(const char *mountpt, int init_flags, char *options);

/*
 * Map a POSIX error code to a representative sysexits(3) code. Can be disabled
 * to exit with errno error code by passing -e as a command line argument to mount
 */
static int ret_errno = 0;
static inline int
errno_or_sysexit(int err, int sysexit)
{
    if (sysexit == -1) {
        sysexit = sysexit_np(err);
    }
    return (ret_errno ? err : sysexit);
}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
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
setup_preboot_mounts(int pass)
{
    int err = 0;
    const char *mnt_to;
    char mnt_from[MAXPATHLEN], boot_manifest_hash[BOOT_MANIFEST_HASH_LEN];

    err = get_boot_manifest_hash(boot_manifest_hash, sizeof(boot_manifest_hash));
    if (err) {
        errx(errno_or_sysexit(err, -1), "failed to get boot manifest hash - %s",
             strerror(err));
    }

    const bind_mount_t preboot_mnts[] = {
        {.bm_mnt_prefix = PREBOOT_VOL_MOUNTPOINT,
         .bm_mnt_to = "/usr/standalone/firmware",
         .bm_mandatory = true},
        {.bm_mnt_prefix = PREBOOT_VOL_MOUNTPOINT,
         .bm_mnt_to = "/usr/local/standalone/firmware",
         .bm_mandatory = false}
    };

    const bind_mount_t hw_mnts[] = {
        {.bm_mnt_prefix = HARDWARE_VOL_MOUNTPOINT "/Pearl",
         .bm_mnt_to = "/System/Library/Pearl/ReferenceFrames",
         .bm_mandatory = false},
        {.bm_mnt_prefix = HARDWARE_VOL_MOUNTPOINT "/FactoryData",
         .bm_mnt_to = "/System/Library/Caches/com.apple.factorydata",
         .bm_mandatory = true}
    };

    if (pass == ROOT_PASSNO) {
        for (int i = 0; i < (sizeof(preboot_mnts) / sizeof(preboot_mnts[0])); i++) {
            mnt_to = preboot_mnts[i].bm_mnt_to;
            snprintf(mnt_from, sizeof(mnt_from), "%s/%s%s",
                     preboot_mnts[i].bm_mnt_prefix, boot_manifest_hash, mnt_to);

            do_bindfs_mount(mnt_from, mnt_to, preboot_mnts[i].bm_mandatory);
        }
    } else {
        for (int i = 0; i < (sizeof(hw_mnts) / sizeof(hw_mnts[0])); i++) {
            mnt_to = hw_mnts[i].bm_mnt_to;
            snprintf(mnt_from, sizeof(mnt_from), "%s%s",
                     hw_mnts[i].bm_mnt_prefix, mnt_to);

            do_bindfs_mount(mnt_from, mnt_to, hw_mnts[i].bm_mandatory);
        }
    }
}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

/*
 * mount phases to be used during boot to perform the following operations:
 * first phase:
 *      TARGET_OS_OSX: (call `apfs_boot_util 1`
 *      TARGET_OS_IPHONE: mount System, Preboot and xART volumes (if present)
 *
 * second phase:
 *      TARGET_OS_OSX: unmount base system DMG if needed, upgrade System 
 *		volume to RW, and call `apfs_boot_util 2` (on ROSV config)
 *      TARGET_OS_IPHONE: mount remaining volumes
 *
 * For more info on mount phases, see apfs_boot_util.
 */
#define MOUNT_PHASE_1      1       /* first phase */
#define MOUNT_PHASE_2      2       /* second phase */

#define NONFS   "nonfs"

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
main(argc, argv)
    int argc;
    char * const argv[];
{
    const char **vfslist, *vfstype;
    struct fstab *fs;
    struct statfs *mntbuf;
    int all, ch, init_flags, rval;
    char *options, *ep;
    int mount_phase = 0;

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

#define BADTYPE(type)                            \
    (strcmp(type, FSTAB_RO) &&                   \
        strcmp(type, FSTAB_RW) && strcmp(type, FSTAB_RQ))

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
                if (!strcmp(fs->fs_vfstype, "nfs")) {
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
			if (os_env != EDT_OS_ENV_OTHER) {
				setup_preboot_mounts(passno);
			}
			/* Hand the rest of the process over to apfs_boot_util */
			if (os_variant_has_internal_content(APFS_BUNDLE_ID) &&
				(mount_phase == MOUNT_PHASE_2)) {
				bootstrap_apfs(MOUNT_PHASE_2);
			}
#endif
		}
		else if (bootstrap_macos) {
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
        if (vfslist != NULL)
            usage();

        if (init_flags & MNT_UPDATE) {

            rval = upgrade_mount (*argv, init_flags, options);

            break;
        }

        if ((fs = getfsfile(*argv)) == NULL &&
            (fs = getfsspec(*argv)) == NULL)
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
        /*
         * If -t flag has not been specified, and spec contains a ':'
         * then assume that an NFS filesystem is being specified.
         */
        if (vfslist == NULL && strchr(argv[0], ':') != NULL) {
            vfstype = "nfs";
            /* check if already mounted */
            if (ismounted(argv[0], argv[1], NULL))
                errx(errno_or_sysexit(EALREADY, EX_CONFIG),
                     "%s is already mounted at %s.",
                     argv[0], argv[1]);
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
            if ((mntbuf = getmntpt(*argv)) == NULL)
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
            rval = mountfs(vfstype,
                    argv[0], argv[1], init_flags, options, NULL);
        }
        break;
    default:
        usage();
        /* NOTREACHED */
    }

    exit(rval);
}

static int
upgrade_mount (const char *mountpt, int init_flags, char *options) {

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
hasopt(mntopts, option)
    const char *mntopts, *option;
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
#endif /* TARGET_OS_OSX */

// prints currently mounted filesystems
void
print_mount(const char **vfslist)
{
    struct statfs *mntbuf;
    int mntsize;

    if ( (mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0 )
        err(errno_or_sysexit(errno , -1), "getmntinfo");
    for (int i = 0; i < mntsize; i++) {
        if ( checkvfsname(mntbuf[i].f_fstypename, vfslist) )
            continue;
        prmount(&mntbuf[i]);
    }
}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
int
verify_executable_file_existence (char *path)
{
    return _verify_file_flags(path, F_OK | X_OK);
}

int
verify_file_existence (char *path)
{
    return _verify_file_flags(path, F_OK);
}

int
_verify_file_flags (char *path, int flags)
{

    if ( access(path, flags) ) {
        fprintf(stderr, "Failed access check for %s with issue %s\n", path, strerror(errno));
        return errno;
    }
    return 0;
}

int
preflight_create_mount_ramdisk (char *mnt_opts, size_t *ramdisk_size, char *template)
{
    char *special_ramdisk_params;

    if ( mnt_opts == NULL ) {
        fprintf(stderr, "No mnt_opts provided to ramdisk preflight.\n");
        return EINVAL;
    }

    if ( verify_executable_file_existence(HDIK_PATH) != 0 ) {
        fprintf(stderr, "Failed to find executable hdik at location %s \n", HDIK_PATH);
        return ENOENT;
    }

    special_ramdisk_params = split_ramdisk_params(mnt_opts);
    if ( special_ramdisk_params == NULL ) {
        fprintf(stderr, "Ramdisk fstab not in expected format.\n");
        return EINVAL;
    }

    if ( ramdisk_size ) {
        char *ramdisk_size_str = parse_parameter_for_token(special_ramdisk_params, RAMDISK_SIZE_TOK);

        if ( ramdisk_size_str != NULL ) {
            *ramdisk_size = atoi(ramdisk_size_str);
            free(ramdisk_size_str);
        }

        if ( *ramdisk_size == 0 ) {
            fprintf(stderr, "Unexpected ramdisk size %zu\n", *ramdisk_size);
            return EINVAL;
        }
    }

    if ( template ) {
        char *template_str = parse_parameter_for_token(special_ramdisk_params, RAMDISK_TPLT_TOK);
        if (template_str != NULL) {
            strlcpy(template, template_str, PATH_MAX);
            free(template_str);
        }

        if ( template == NULL ) {
            fprintf(stderr, "Ramdisk template path not found\n");
            return EINVAL;
        }

    }

    return 0;
}
#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */

/*
 * Helper function that posix_spawn a child process
 * as defined by command_argv[0].
 * If `output` is non-null, then the command's stdout will be read
 * into that buffer.
 * If `rc` is non-null, then the command's return code will be set
 * there.
 * If `signal_no` is non-null, then if the command is signaled, the
 * signal number will be set there.
 *
 *
 * This function returns
 *  -1, if there's an internal error. errno will be set
 *   0, if command exit normally with 0 as return code
 *   1, if command exit abnormally or with a non-zero return code
 */
int
run_command(char **command_argv, char *output, int *rc, int *signal_no)
{
    int error = -1;
    int faulting_errno = 0;
    int status = -1;
    int internal_result = -1;
    pid_t pid;
    posix_spawn_file_actions_t actions = NULL;
    int output_pipe[2] = {-1, -1};
    char *command_out = NULL;
    FILE *stream = NULL;

    if ( !command_argv ) {
        fprintf(stderr, "command_argv is NULL\n");
        errno = EINVAL;
        goto done;
    }

    if ( pipe(output_pipe) ) {
        fprintf(stderr, "Failed to create pipe for command output: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( (internal_result = posix_spawn_file_actions_init(&actions)) != 0 ) {
        errno = internal_result;
        fprintf(stderr, "posix_spawn_file_actions_init failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( (internal_result = posix_spawn_file_actions_addclose(&actions, output_pipe[0])) != 0 ) {
        errno = internal_result;
        fprintf(stderr, "posix_spawn_file_actions_addclose output_pipe[0] failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( (internal_result = posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO)) != 0 ) {
        errno = internal_result;
        fprintf(stderr, "posix_spawn_file_actions_adddup2 output_pipe[1] failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( (internal_result = posix_spawn_file_actions_addclose(&actions, output_pipe[1])) != 0 ) {
        errno = internal_result;
        fprintf(stderr, "posix_spawn_file_actions_addclose output_pipe[1] failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( verbose ) {
        fprintf(stdout, "Executing command: ");
        for (char **command_segment = command_argv; *command_segment; command_segment++) {
            fprintf(stdout, "%s ", *command_segment);
        }
        fprintf(stdout, "\n");
    }

    if ( (internal_result = posix_spawn(&pid, command_argv[0], &actions, NULL, command_argv, environ)) != 0 ) {
        errno = internal_result;
        fprintf(stderr, "posix_spawn failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    // Close out our side of the pipe
    close(output_pipe[1]);
    output_pipe[1] = -1;

    // If caller specified the output buffer, we'll use that
    // Otherwise allocate a buffer and capture the output ourselves for verbose logging
    if ( output != NULL ) {
        command_out = output;
    } else {
        command_out = calloc(COMMAND_OUTPUT_MAX, sizeof(char));
        if (!command_out) {
            fprintf(stderr, "calloc failed: %d (%s)\n", errno, strerror(errno));
            goto done;
        }
    }

    stream = fdopen(output_pipe[0], "r");
    if ( !stream ) {
        fprintf(stderr, "fdopen failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    size_t length;
    size_t count = 0;
    char *line;
    while ( (line = fgetln(stream, &length)) && (count < COMMAND_OUTPUT_MAX - length - 1) ) {
        strncat(command_out, line, length);
        count += length;
    }

    if ( ferror(stream) ) {
        fprintf(stderr, "fgetln failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( fclose(stream) ) {
        fprintf(stderr, "fclose failed: %d (%s)\n", errno, strerror(errno));
        stream = NULL;
        goto done;
    }
    stream = NULL;
    close(output_pipe[0]);
    output_pipe[0] = -1;

    while ( waitpid(pid, &status, 0) < 0 ) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "waitpid failed: %d (%s)\n", errno, strerror(errno));
        goto done;
    }

    if ( verbose ) {
        fprintf(stdout, "Command output:\n%s\n", command_out);
    }

    if ( WIFEXITED(status) ) {
        int exit_status = WEXITSTATUS(status);
        if (rc) *rc = exit_status;
        if (signal_no) *signal_no = 0;

        if (exit_status != 0) {
            error = 1;
            fprintf(stderr, "Command failed: %d\n", exit_status);
            goto done;
        }
    }

    if ( WIFSIGNALED(status) ) {
        if (rc) *rc = 0;
        if (signal_no) *signal_no = WTERMSIG(status);

        error = 1;
        fprintf(stderr, "Command signaled: %d\n", WTERMSIG(status));
        goto done;
    }

    error = 0;
done:
    // we don't care much about the errno set by the clean up routine
    // so save the errno here and return to caller
    faulting_errno = errno;

    if ( actions ) {
        posix_spawn_file_actions_destroy(&actions);
    }

    if ( stream ) {
        fclose(stream);
    }

    if ( output_pipe[0] >= 0 ) {
        close(output_pipe[0]);
    }

    if ( output_pipe[1] >= 0 ) {
        close(output_pipe[1]);
    }

    if ( !output && command_out ) {
        free(command_out);
    }

    errno = faulting_errno;
    return error;
}

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
// Helper function that truncates whitespaces
void
truncate_whitespace(char* str)
{
    size_t idx = strcspn(str, " \n");
    if ( idx != 0 ) {
        str[idx] = '\0';
    }
}

// Creates a new unmounted ramdisk of size device_size
int
attach_device(size_t device_size , char* deviceOut)
{
    int return_val = -1;
    char ram_define [PATH_MAX];
    snprintf(ram_define, sizeof(ram_define), "ram://%zu", device_size);

    char *command[4] = { HDIK_PATH, "-nomount", ram_define, NULL };

    int status = run_command(command, deviceOut, &return_val, NULL);
    if ( status == 1 ) {
        fprintf(stderr, "Failed to create ramdisk. HDIK returned %d.\n", return_val);
        exit(errno_or_sysexit(errno, -1));
    } else if (status != 0) {
        fprintf(stderr, "Failed to execute command %s\n", command[0]);
        exit(errno_or_sysexit(errno, -1));
    }

    truncate_whitespace(deviceOut);
    return return_val;
}

// Creates the partition table directly through MediaKit.
int
create_partition_table(size_t partition_size, char *device)
{

    MKStatus err = -1;
    MKMediaRef gpt_ref               = NULL;
    CFMutableArrayRef schemes        = NULL;
    CFMutableArrayRef partitionArray = NULL;
    CFDictionaryRef partition        = NULL;
    CFMutableDictionaryRef options   = NULL;
    CFMutableDictionaryRef layout    = NULL;
    CFMutableDictionaryRef media     = NULL;
    CFMutableDictionaryRef map       = NULL;

    layout  = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (!layout || !options) {
        fprintf(stderr, "Failed to create necessary CFDictionaries\n");
        err = errno;
        goto done;
    }

    CFDictionarySetValue(options, kMKMediaPropertyWritableKey, kCFBooleanTrue);

    gpt_ref = MKMediaCreateWithPath(kCFAllocatorDefault, device, options, &err);
    CFRelease(options);

    if (gpt_ref) {
        MKStatus mediaErr = 0;
        partition = MKCFBuildPartition(PMGPTTYPE, apple_apfs, CFSTR(EDTVolumeFSType), CFSTR(RAMDISK_FS_SPEC), 0, RAMDISK_BLCK_OFFSET, &mediaErr, NULL);

        if (!partition) {
            fprintf(stderr, "Failed to create partition with err %d\n", mediaErr);
            err = mediaErr;
            goto done;
        }

        partitionArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

        if (!partitionArray) {
            fprintf(stderr, "Failed to create partitionArray\n");
            err = errno;
            CFRelease(partition);
            goto done;
        }

        CFArrayAppendValue(partitionArray, partition);
        CFRelease(partition);

        CFDictionaryAddValue(layout, CFSTR(MK_PARTITIONS_KEY), partitionArray);
        CFRelease(partitionArray);


        media = MKCFCreateMedia(&schemes, &mediaErr);

        if (!media) {
            fprintf(stderr, "Failed to create Schemes with error %d\n", mediaErr);
            err = mediaErr;
            goto done;
        }

        map = MKCFCreateMap(PMGPTTYPE, schemes, layout, NULL, NULL, NULL, NULL, NULL, gpt_ref, &mediaErr);
        if (!map) {
            fprintf(stderr, "Failed to create map with error %d\n", mediaErr);
            err = mediaErr;
            goto done;
        }

        err = MKCFWriteMedia(media, layout, NULL, NULL, gpt_ref);
        if (err) {
            fprintf(stderr, "Failed to WriteMedia with error %d\n", err);
            goto done;
        }

    } else {
        fprintf(stderr, "Failed to create gpt_ref with error %d\n", err);
        goto done;
    }

    if (verbose) {
        fprintf(stderr, "Releasing MediaKit objects\n");
    }
    err = 0;

done:
    if (media) {
        MKCFDisposeMedia(media);
    }

    if (layout) {
        CFRelease(layout);
    }

    if (gpt_ref) {
        CFRelease(gpt_ref);
    }

    return err;
}

// Triggers newfs_apfs for the target device
int
construct_apfs_volume(char *mounted_device_name)
{
    int return_val = -1;
    int status = -1;
    char *command[5] = { "/sbin/newfs_apfs", "-v", "Var", mounted_device_name, NULL };

    status = run_command(command, NULL, &return_val, NULL);
    if ( status >= 0 ) {
        return return_val;
    } else {
        fprintf(stderr, "Failed to execute command %s\n", command[0]);
        errno_or_sysexit(errno, -1);
    }

    // shouldn't reach here. This is to satisfy the compiler
    return -1;
}

// unmounts device at location
int
unmount_location(char *mount_point)
{
    int return_val = -1;
    int status = -1;
    char *command[4] = { "/sbin/umount", "-f", mount_point, NULL };

    status = run_command(command, NULL, &return_val, NULL);
    if ( status >= 0 ) {
        return return_val;
    } else {
        fprintf(stderr, "Failed to execute command %s\n", command[0]);
        return errno_or_sysexit(errno, -1);
    }
}

// The mnt_opts for fstab are standard across the different
// mount_fs implementations. To create and mount an ephemeral
// filesystem, it is necessary to provide additional non-standard
// values in filesystem definition - mainly size and location of
// the seed files.
// The fstab definition for a ramdisk fs requires two new parameters:
// 'size=%zu' and 'template=%s'. To keep the fstab structure
// consistent with that of other filesystem types, these
// parameters are appended at the end of the mnt_opts string.
// It is necessary to split the mnt_opts into two strings, the
// standard mountfs parameters that are used in the fs-specifnc mount
// and the ramdisk definition parameters.
char*
split_ramdisk_params(char *opts)
{
    char* opt             = NULL;
    char* target_str      = NULL;
    char* size_tok        = RAMDISK_SIZE_TOK;
    char* tplt_tok        = RAMDISK_TPLT_TOK;
    char* optbuf          = NULL;
    size_t size_tok_len   = strlen(size_tok);
    size_t tplt_tok_len   = strlen(tplt_tok);

    optbuf = strdup(opts);
    for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
        size_t opt_len = strlen(opt);
        if ( (opt_len > size_tok_len && !strncmp(size_tok, opt, size_tok_len) ) ||
            (opt_len > tplt_tok_len && !strncmp(tplt_tok, opt, tplt_tok_len) ) ) {
            size_t start_index = opt - optbuf;
            target_str = opts + start_index;
            opts[start_index - 1 ] = '\0'; // Break original into two strings.
            break;
        }
    }
    free(optbuf);
    return target_str;
}

// returns string for the parameter after the '=' in the search_string
// part of the special ramdisk parameters
char*
parse_parameter_for_token(char * opts, char * search_string)
{
    char *return_str = NULL;
    char *tmp_str    = NULL;
    char *target_str = strstr(opts, search_string);
    size_t len = strlen(search_string);

    if ( target_str && strlen(target_str) > len ) {
        tmp_str = target_str + len;
        size_t idx = strcspn(tmp_str, ",\0");
        if ( idx != 0 && (idx < MAXPATHLEN) ) {
            return_str = calloc(1, idx+1); //for null terminator
            strncpy(return_str, tmp_str, idx);
        }
    }
    return return_str;
}


static int _copyfile_status(int what, int stage, copyfile_state_t state, const char * src, const char * dst, void * ctx) {

    if (verbose && stage == COPYFILE_START) {
        if (what == COPYFILE_RECURSE_FILE) {
            fprintf(stderr, "Copying %s -> %s\n", src, dst);
        } else if (what == COPYFILE_RECURSE_DIR) {
            fprintf(stderr, "Creating %s/\n", dst);
        }
    }

    return COPYFILE_CONTINUE;
}


// returns 0 upon success and a valid sysexit or errno code upon failure
int
create_mount_ramdisk(struct fstab *fs, int init_flags, char *options)
{
    int     default_flags                    =  init_flags;
    int     mount_return                     =  0;
    char    ramdisk_partition   [PATH_MAX]   =  { 0 };
    char    ramdisk_volume      [PATH_MAX]   =  { 0 };
    char    ramdisk_container   [PATH_MAX]   =  { 0 };
    char    seed_location       [PATH_MAX]   =  { 0 };
    char    *mnt_point                       =  RAMDISK_TMP_MOUNT; // intermediate
    char    *target_dir                      =  fs->fs_file; // target
    size_t  ram_size                         =  0;

    if ( verify_file_existence(mnt_point) != 0 ) {
        if (verbose) {
            fprintf(stderr, "Default mount %s is not available. Using backup %s.\n", mnt_point, RAMDISK_BCK_MOUNT);
        }
        mnt_point = RAMDISK_BCK_MOUNT;
        if ( verify_file_existence(mnt_point) != 0 ) {
            fprintf(stderr, "Mountpoints not available. Exiting.\n");
            return ENOENT;
        }
    }

    if ( preflight_create_mount_ramdisk(fs->fs_mntops, &ram_size, seed_location) != 0 ) {
        fprintf(stderr, "Failed ramdisk preflight. Exiting.\n");
        return EINVAL;
    }

    if ( verbose ) {
        fprintf(stdout, "Attaching device of size %zu\n", ram_size);
    }

    if( attach_device(ram_size, ramdisk_partition) != 0 ){
        fprintf(stderr, "Failed to attach the ramdisk.\n");
        exit(errno_or_sysexit(ECHILD, -1));
    }

    if ( verbose ) {
        fprintf(stdout, "Creating partition table for device %s \n", ramdisk_partition);
    }

    if ( create_partition_table(ram_size, ramdisk_partition) !=0 ) {
        fprintf(stderr, "Failed to create partition table.\n");
        exit(errno_or_sysexit(ECHILD, -1));
    }

    snprintf(ramdisk_container, sizeof(ramdisk_container), "%ss1", ramdisk_partition);

    if ( verbose ) {
        fprintf(stdout, "Creating apfs volume on partition %s\n", ramdisk_container);
    }

    if ( construct_apfs_volume(ramdisk_container) != 0 ) {
        fprintf(stderr, "Failed to construct the apfs volume on the ramdisk.\n");
        exit(errno_or_sysexit(ECHILD, -1));
    }

    snprintf(ramdisk_volume, sizeof(ramdisk_volume), "%ss1", ramdisk_container);

    if ( verify_file_existence(ramdisk_volume) != 0 ) {
        fprintf(stderr, "Failed to verify %s with issue %s\n", ramdisk_volume, strerror(errno));
        exit(errno_or_sysexit(errno, -1));
    }

    // Mount volume to RAMDISK_TMP_MOUNT
    if ( verbose ) {
        fprintf(stdout, "Mounting to tmp location %s\n", mnt_point);
    }

    mount_return = mountfs(EDTVolumeFSType, ramdisk_volume, mnt_point, default_flags, NULL, fs->fs_mntops);
    if ( mount_return > 0 ) {
        fprintf(stderr, "Initial mount to %s failed with %d\n", mnt_point, mount_return);
        exit(errno_or_sysexit(errno, -1));
    }

    // ditto contents of RAMDISK_TMP_MOUNT to /private/var
    copyfile_state_t state = copyfile_state_alloc();
    copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, _copyfile_status);
    if( copyfile(seed_location, mnt_point, state, COPYFILE_ALL | COPYFILE_RECURSIVE) < 0 ) {
        fprintf(stderr, "Failed to copy contents from %s to %s with error: %s\n", seed_location, mnt_point, strerror(errno));
        exit(errno_or_sysexit(errno, -1));
    }
    copyfile_state_free(state);

    // unount RAMDISK_TMP_MOUNT
    if( unmount_location(mnt_point) != 0 ){
        fprintf(stderr, "Failed to unmount device mounted at %s.\n", mnt_point);
        exit(errno_or_sysexit(ECHILD, -1));
    }

    if( verbose ) {
        fprintf(stdout, "Mounting apfs volume %s to %s\n", ramdisk_volume, target_dir);
    }

    mount_return = mountfs(EDTVolumeFSType, ramdisk_volume, target_dir, default_flags, options, fs->fs_mntops);
    if ( mount_return > 0 ) {
        fprintf(stderr, "Followup mount to %s failed with %d\n", target_dir, mount_return);
        exit(errno_or_sysexit(errno, -1));
    }

    // Verify contents in stdout
    if ( verbose ) {
        print_mount(NULL);
    }

    return mount_return;
}
#endif // (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

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
prmount(sfp)
    struct statfs *sfp;
{
    int flags;
    mountopt_t *o;
    struct passwd *pw;

    (void)printf("%s on %s (%s", sfp->f_mntfromname, sfp->f_mntonname,
        sfp->f_fstypename);

    if (is_sealed(sfp->f_mntonname))
        (void)printf(", sealed");
    flags = sfp->f_flags & MNT_VISFLAGMASK;
    for (o = optnames; flags && o->o_opt; o++)
        if (flags & o->o_opt) {
            (void)printf(", %s", o->o_name);
            flags &= ~o->o_opt;
        }
    if (sfp->f_owner) {
        (void)printf(", mounted by ");
        if ((pw = getpwuid(sfp->f_owner)) != NULL)
            (void)printf("%s", pw->pw_name);
        else
            (void)printf("%d", sfp->f_owner);
    }
    (void)printf(")\n");
}

struct statfs *
getmntpt(name)
    const char *name;
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
catopt(s0, s1)
    char *s0;
    const char *s1;
{
    size_t i;
    char *cp;

    if (s0 && *s0) {
        i = strlen(s0) + strlen(s1) + 1 + 1;
        if ((cp = malloc(i)) == NULL)
            err(errno_or_sysexit(errno, EX_TEMPFAIL),
                "failed to allocate memory for arguments");
        (void)snprintf(cp, i, "%s,%s", s0, s1);
    } else
        cp = strdup(s1);

    if (s0)
        free(s0);
    return (cp);
}

void
mangle(options, argcp, argv)
    char *options;
    int *argcp;
    const char **argv;
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
usage()
{

    (void)fprintf(stderr,
        "usage: mount %s %s\n       mount %s\n       mount %s\n",
        "[-dfruvw] [-o options] [-t external_type]",
            "special mount_point",
        "[-adfruvw] [-t external_type]",
        "[-dfruvw] special | mount_point");
    exit(errno_or_sysexit(EINVAL, EX_USAGE));
}
