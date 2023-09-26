/*
 * Copyright (c) 1999-2000, 2002-2008 Apple Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <err.h>
#include <setjmp.h>

#include <hfs/hfs_mount.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

#include <TargetConditionals.h>

#include "fsck_hfs.h"
#include "fsck_messages.h"

static void usage __P((void));
static char*    check_path(char *name);
static int      add_file_block(const char *filepath);
static int      open_device(char * dev);
static int      read_disk_info();
static void     close_device(void);
static char*    get_mount_point(const char *cdev);
static void     get_write_access (char *dev);
static void     fsck_print(LogMessageType level, const char *fmt, ...);

/*
 * These definitions are duplicated from xnu's hfs_readwrite.c, and could live
 * in a shared header file if desired. On the other hand, the freeze and thaw
 * commands are not really supposed to be public.
 */
#ifndef    F_FREEZE_FS
#define F_FREEZE_FS     53              /* "freeze" all fs operations */
#define F_THAW_FS       54              /* "thaw" all fs operations */
#endif  // F_FREEZE_FS


/* Function: DPRINTF
 *
 * Description: Debug function similar to printf except the first parameter
 * which indicates the type of message to be printed by DPRINTF. Based on
 * current debug level and the type of message, the function decides
 * whether to print the message or not.
 *
 * Each unique message type has a bit assigned to it. The message type
 * passed to DPRINTF can be one or combination (OR-ed value) of pre-defined
 * debug message types.  Only the messages whose type have one or more similar
 * bits set in comparison with current global debug level are printed.
 *
 * For example, if cur_debug_level = 0x11 (d_info|d_xattr)
 * ----------------------------------------
 *    message type    -     printed/not printed
 * ----------------------------------------
 *    d_info            -    printed
 *    d_error|d_xattr    -    printed
 *    d_error            -     not printed
 *    d_overlap        -     not printed
 *
 * Input:
 *    message_type - type of message, to determine when to print the message
 *    variable arguments - similar to printfs
 *
 * Output:
 *    Nothing
 */
void DPRINTF(__unused fsck_ctx_t c, unsigned long type, const char *fmt, va_list ap)
{
    if (fsck_get_debug_level() & type) {
        plog("\t");
        vplog(fmt, ap);
    }
}
static void
vprint(__unused fsck_client_ctx_t client, LogMessageType level, const char *fmt, va_list ap)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    switch (level)
    {
        case LOG_TYPE_INFO:
            vplog(fmt, ap);
            break;
        case LOG_TYPE_ERROR:
            perror(fmt);
            break;
        case LOG_TYPE_WARNING:
            pwarn((char *)fmt, ap);
            break;
        case LOG_TYPE_FATAL:
            pfatal((char *)fmt, ap);
            break;
        case LOG_TYPE_TERMINATE:
            verr(1, fmt, ap);
            break;
        case LOG_TYPE_STDERR:
            fplog(stderr, fmt, ap);
            break;
        case LOG_TYPE_WARN:
            vwarn(fmt, ap);
        default:
            break;
    }
#pragma clang diagnostic pop
}

static void fsck_print(LogMessageType level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprint(NULL, level, fmt, ap);
    va_end(ap);
}

int main(int argc, char *argv[])
{
	int ch;
	int ret = 0;
    int err = 0;
	extern int optind;
	extern char *optarg;
	char * lastChar;
    char * progName;
	long mode;
    long blockSize;
    int rebuilOptions;
    uint64_t cacheSize;
    int logLev;
    
    fsck_ctx_t msgsContext = fsckMsgsCreate();
    
    fsck_init_state();
    fsck_set_context_properties(vprint, fsckPrint, DPRINTF, NULL, msgsContext);
        
    fsck_set_progname(strrchr(*argv, '/'));
    if (fsck_get_progname()) {
        progName = fsck_get_progname();
        fsck_set_progname(++progName);
    } else {
        fsck_set_progname(*argv);
    }
    
	while ((ch = getopt(argc, argv, "b:B:c:D:e:Edfglm:npqrR:SuyxJ")) != EOF) {
		switch (ch) {
		case 'b':
            blockSize = atoi(optarg);
            fsck_set_block_size(blockSize);
			if ((blockSize < 512) || (blockSize & (blockSize-1))) {
				fsck_print(LOG_TYPE_STDERR, "%s invalid block size %d\n",
                           fsck_get_progname(), blockSize);
				exit(2);
			}
			break;
		case 'S':
            fsck_set_scanflag(1);
			break;
		case 'B':
            add_file_block(optarg);
			break;
		case 'c':
			/* Cache size to use in fsck_hfs */
            fsck_set_cache_size(strtoull(optarg, &lastChar, 0));
			if (*lastChar) {
                cacheSize = fsck_get_cachesize();
                switch (tolower(*lastChar)) {
                    case 'g':
                        cacheSize *= 1024*1024*1024;
                        break;
                    case 'm':
                        cacheSize *= 1024*1024;
                        break;
                    case 'k':
                        cacheSize *= 1024;
                        break;
                    default:
                        cacheSize = 0;
                        break;
                };
			}
            fsck_set_cache_size(cacheSize);
            break;
		case 'd':
            fsck_set_debug(1);
			break;

		case 'J':
            fsck_set_disable_journal(1);
			break;
		case 'D':
            /* Input value should be in hex example: -D 0x5 */
            fsck_set_debug_level(strtoul(optarg, NULL, 0));
            if (fsck_get_debug_level() == 0) {
                fsck_print(LOG_TYPE_STDERR, "%s: invalid debug development argument.  Assuming zero\n", fsck_get_progname());
            }
            break;
		case 'e':
            if (optarg) {
                if (strcasecmp(optarg, "embedded") == 0) {
                    fsck_set_embedded(1);
                }
                else if (strcasecmp(optarg, "desktop") == 0) {
                    fsck_set_embedded(0);
                }
            }
            break;
		case 'E':
			/* Exit on first error, after logging it */
            fsck_set_error_on_exit(1);
			break;
		case 'f':
            fsck_set_force(1);
			break;

		case 'g':
            fsck_set_guicontrol(1);
			break;
		case 'x':
            fsck_set_xmlcontrol(1);
            fsck_set_guicontrol(1);
			break;
		case 'l':
            fsck_set_lflag(1);
            fsck_set_nflag(1);
            fsck_set_yflag(0);
            fsck_set_force(1);
            break;
		case 'm':
            fsck_set_mode_setting(1);
            mode = strtol(optarg, NULL, 8 );
            fsck_set_lostAndFoundMode((int)mode);
            if (fsck_get_lostAndFoundMode() == 0 || mode < INT_MIN || mode > INT_MAX) {
                fsck_print(LOG_TYPE_STDERR, "%s: %ld is invalid mode argument\n", fsck_get_progname(), mode);
                usage();
            }
            break;
		case 'n':
            fsck_set_nflag(1);
            fsck_set_yflag(0);
            fsck_set_repair_level(kNeverRepair);
        break;
		case 'p':
            fsck_set_preen(1);
            break;
		case 'q':
            fsck_set_quick(1);
			break;
		case 'r':
			// rebuild catalog btree
            rebuilOptions = fsck_get_rebuild_options() | REBUILD_CATALOG;
            fsck_set_rebuild_btree(1);
            fsck_set_rebuild_options(rebuilOptions);
			break;
		case 'R':
			if (optarg) {
				char *cp = optarg;
				while (*cp) {
					switch (*cp) {
						case 'a':	
                            // rebuild attribute btree
                            rebuilOptions = fsck_get_rebuild_options() | REBUILD_ATTRIBUTE;
                            fsck_set_rebuild_btree(1);
                            fsck_set_rebuild_options(rebuilOptions);
							break;
						case 'c':
                            // rebuild catalog btree
                            rebuilOptions = fsck_get_rebuild_options() | REBUILD_CATALOG;
                            fsck_set_rebuild_btree(1);
                            fsck_set_rebuild_options(rebuilOptions);
                            break;
						case 'e':
                            // rebuild extents overflow btree
                            rebuilOptions = fsck_get_rebuild_options() | REBUILD_EXTENTS;
                            fsck_set_rebuild_btree(1);
                            fsck_set_rebuild_options(rebuilOptions);
                            break;
						default:	
                            fsck_print(LOG_TYPE_STDERR, "%s: unknown btree rebuild code `%c' (%#x)\n", fsck_get_progname(), *cp, *cp);
                            exit(2);
					}
					cp++;
				}
				break;
			}

		case 'y':
            fsck_set_yflag(1);
            fsck_set_nflag(0);
            fsck_set_repair_level(kMajorRepairs);
			break;

		case 'u':
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	
    if (fsck_get_debug() == 0 && fsck_get_disable_journal() != 0) {
        fsck_set_disable_journal(0);
    }

    if (fsck_get_block_size() == 0) {
        fsck_set_block_size(512);
    }
	
    if (fsck_get_guicontrol()) {
        fsck_set_debug(0);  /* debugging is for command line only */
    }
    
    if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
        (void)signal(SIGINT, catch);
    }

	if (argc < 1) {
		fsck_print(LOG_TYPE_STDERR, "%s: missing special-device\n", fsck_get_progname());
		usage();
	}
    
    /* start with defaults for dfa back-end */
    fsck_set_chek_level(kAlwaysCheck);
    fsck_set_repair_level(kMajorRepairs);
    logLev = kVerboseLog;

    if (fsck_get_yflag()) {
        fsck_set_repair_level(kMajorRepairs);
    }

    if (fsck_get_quick()) {
        fsck_set_chek_level(kNeverCheck);
        fsck_set_repair_level(kNeverRepair);
        logLev = kFatalLog;
    } else if (fsck_get_force()) {
        fsck_set_chek_level(kForceCheck);
    }
    if (fsck_get_preen()) {
        fsck_set_repair_level(kMinorRepairs);
        fsck_get_force() ? fsck_set_chek_level(kAlwaysCheck) : fsck_set_chek_level(kDirtyCheck);
        logLev = kFatalLog;
    }
    if (fsck_get_debug()) {
        logLev = kDebugLog;
    }

    if (fsck_get_nflag()) {
        fsck_set_repair_level(kNeverRepair);
    }
        
    if (fsck_get_rebuild_btree()) {
        fsck_set_chek_level(kPartialCheck);
        fsck_set_repair_level(kForceRepairs); // this will force rebuild of B-Tree file
    }
    
    fsck_set_verbosity_level(logLev);
    /* All of fsck_hfs' output should go thorugh logstring */
    fsckSetOutput(msgsContext, NULL);
    /* Setup writer that will output to standard out */
    fsckSetWriter(msgsContext, &outstring);
    
    /* Setup logger that will write to log file */
    fsckSetLogger(msgsContext, &logstring);
    if (fsck_get_guicontrol()) {
        if (fsck_get_xmlcontrol()) {
            fsckSetOutputStyle(msgsContext, fsckOutputXML);
        }
        else {
            fsckSetOutputStyle(msgsContext, fsckOutputGUI);
        }
    } else {
        fsckSetOutputStyle(msgsContext, fsckOutputTraditional);
    }
    while (argc-- > 0) {
        char *devPath = check_path(*argv++);
        fsck_set_cdevname(devPath);
        
        err = open_device(devPath);
        if (err) {
            exit(err);
        }
        
        err = read_disk_info(devPath);
        if (err) {
            exit(err);
        }
        
        /*
         * attempt to get write access to the block device and if not check if volume is
         * mounted read-only.
        */
        if (fsck_get_detonator_run() == 0 && fsck_get_nflag() == 0 && fsck_get_quick() == 0) {
            get_write_access(devPath);
        }
		ret |= checkfilesys(devPath);
        close_device();
    }
    
    if (msgsContext) {
        fsckMsgsDestroy(msgsContext);
    }
	exit(ret);
}

static void
close_device(void) {
    int readfd = fsck_get_fsreadfd();
    int writefd = fsck_get_fswritefd();
    
    if (readfd > 0) {
        close(readfd);
    }
    if (writefd) {
        close(writefd);
    }
}

static int
open_device(char* dev) {
    int fd = -1;
    char *mntonname = NULL;
    
    /*
     * If the device is mounted somewhere, then we need to make sure that it's
     * a read-only device, or that a live-verify has been requested.
     */
    mntonname = get_mount_point(fsck_get_cdevname());
    if (fsck_get_hotroot()) {
        if (mntonname) {
            free(mntonname);
        }
        mntonname = strdup("/");
    }
    
    if (mntonname != NULL) {
        struct statfs stfs_buf;

        if (statfs(mntonname, &stfs_buf) == 0) {
            if (fsck_get_lflag()) {
                // Need to try to freeze it
                fsck_set_fd(open(mntonname, O_RDONLY));
                if (fsck_get_fd() < 0) {
                    fsck_print(LOG_TYPE_INFO, "ERROR: could not open %s to freeze the volume.\n", mntonname);
                    free(mntonname);
                    goto exit;
                }
    
                if (fcntl(fsck_get_fd(), F_FREEZE_FS, NULL) != 0) {
                    free(mntonname);
                    fsck_print(LOG_TYPE_INFO, "ERROR: could not freeze volume (%s)\n", strerror(errno));
                    goto exit;
                }
            } else if (stfs_buf.f_flags & MNT_RDONLY) {
                fsck_set_hotmount(1);
            } else {
                /* MNT_RDONLY is not set and this is not a live verification */
                fsck_print(LOG_TYPE_INFO, "ERROR: volume %s is mounted with write access. Re-run with (-l) to freeze volume.\n", mntonname);
                goto exit;
            }
        }
    }
    fsck_set_mount_point(mntonname);
    
    if (fsck_get_detonator_run()) {
        char *end_ptr;
        fd = (int)strtol(dev+8, &end_ptr, 10);
        if (*end_ptr) {
            fsck_print(LOG_TYPE_TERMINATE, "fsck_hfs: Invalid file descriptor path: %s", dev);
        }
        fsck_set_fswritefd(fd);
    } else {
        // Open device for writing - acquire exclusive lock if the target is mounted
        fd = open(dev, O_RDWR | (fsck_get_hotmount() ? 0 : O_EXLOCK));
        if (fsck_get_nflag() || fsck_get_quick() || fd < 0 ) {
            fsck_set_fswritefd(-1);
            if (fsck_get_preen()) {
                fsck_print(LOG_TYPE_FATAL, "** %s (NO WRITE ACCESS)\n", dev);
            }
        }
        fsck_set_fswritefd(fd);
    }
    
    // Aquire fd for reading from target device
    if (fsck_get_fswritefd() > 0) {
        fsck_set_fsreadfd(dup(fsck_get_fswritefd()));
        if (fsck_get_fsreadfd() < 0) {
            fsck_print(LOG_TYPE_INFO, "Can't dup fd for reading on %s: %s\n", dev, strerror(errno));
            close(fsck_get_fswritefd());
            goto exit;
        }
    } else {
        // Try openning device for read only
        fd = open(dev, O_RDONLY);
        fsck_print(LOG_TYPE_INFO, "Can't open %s: %s\n", dev, strerror(errno));
        if (fd < 0) {
            goto exit;
        }
        fsck_set_fsreadfd(fd);
    }
    return 0;
    
exit:
    if (fsck_get_preen()) {
        fsck_print(LOG_TYPE_FATAL, "CAN'T CHECK FILE SYSTEM.");
    } else {
        if (fsck_get_yflag()) {
            cleanup_fs_fd();
        }
    }
    return EEXIT;
}
static int
read_disk_info() {
    int fd = fsck_get_fsreadfd();
    uint64_t devBlockCount = 0;
    uint32_t devBlockSize = 0;
    
    // Get device block size
    if (ioctl(fd, DKIOCGETBLOCKSIZE, &devBlockSize) < 0) {
        fsck_print(LOG_TYPE_INFO, "Can't get device block size (%s)\n", strerror(errno));
        goto exit;
    } else {
        fsck_set_dev_block_size(devBlockSize);
    }
    
    // Get device block count
    if (ioctl(fd, DKIOCGETBLOCKCOUNT, &devBlockCount) < 0) {
        fsck_print(LOG_TYPE_INFO, "Can't get device block count (%s)\n", strerror(errno));
        goto exit;
    } else {
        fsck_set_dev_block_count(devBlockCount);
    }
    
    
    return 0;
exit:
    if (fsck_get_preen()) {
        fsck_print(LOG_TYPE_FATAL, "CAN'T CHECK FILE SYSTEM.");
    } else {
        if (fsck_get_yflag()) {
            cleanup_fs_fd();
        }
    }
    return EEXIT;
}

void
cleanup_fs_fd(void)
{
    if (fsck_get_fd() >= 0) {
        fcntl(fsck_get_fd(), F_THAW_FS, NULL);
        close(fsck_get_fd());
        fsck_set_fd(-1);
    }
}

static void
usage(void)
{
	fsck_print(LOG_TYPE_STDERR, "usage: %s [-b [size] B [path] c [size] e [mode] ESdfglx m [mode] npqruy] special-device\n", fsck_get_progname());
	fsck_print(LOG_TYPE_STDERR, "  b size = size of physical blocks (in bytes) for -B option\n");
	fsck_print(LOG_TYPE_STDERR, "  B path = file containing physical block numbers to map to paths\n");
	fsck_print(LOG_TYPE_STDERR, "  c size = cache size (ex. 512m, 1g)\n");
	fsck_print(LOG_TYPE_STDERR, "  e mode = emulate 'embedded' or 'desktop'\n");
	fsck_print(LOG_TYPE_STDERR, "  E = exit on first major error\n");
	fsck_print(LOG_TYPE_STDERR, "  d = output debugging info\n");
	fsck_print(LOG_TYPE_STDERR, "  f = force fsck even if clean (preen only) \n");
	fsck_print(LOG_TYPE_STDERR, "  g = GUI output mode\n");
	fsck_print(LOG_TYPE_STDERR, "  x = XML output mode\n");
	fsck_print(LOG_TYPE_STDERR, "  l = live fsck (lock down and test-only)\n");
	fsck_print(LOG_TYPE_STDERR, "  m arg = octal mode used when creating lost+found directory \n");
	fsck_print(LOG_TYPE_STDERR, "  n = assume a no response \n");
	fsck_print(LOG_TYPE_STDERR, "  p = just fix normal inconsistencies \n");
	fsck_print(LOG_TYPE_STDERR, "  q = quick check returns clean, dirty, or failure \n");
	fsck_print(LOG_TYPE_STDERR, "  r = rebuild catalog btree \n");
	fsck_print(LOG_TYPE_STDERR, "  S = Scan disk for bad blocks\n");
	fsck_print(LOG_TYPE_STDERR, "  u = usage \n");
	fsck_print(LOG_TYPE_STDERR, "  y = assume a yes response \n");
	
	exit(1);
}

static int
add_file_block(const char *filepath)
{
    FILE * file;
    long long block;

    if ((file = fopen(filepath, "r")) == NULL) {
        fsck_print(LOG_TYPE_FATAL, "Can't open %s\n", filepath);
    }

    while (fscanf(file, "%lli", &block) > 0) {
        AddBlockToList(block);
    }

    (void) fclose(file);
    
    return 0;
}

static char *
check_path(char *origname)
{
    struct stat stslash, stblock, stchar;
    char *newname, *raw = NULL;
    int retried = 0;
    fsck_set_hotroot(0);
    if (stat("/", &stslash) < 0) {
        fsck_print(LOG_TYPE_ERROR, "/");
        fsck_print(LOG_TYPE_INFO, "Can't stat root\n");
        return origname;
    }
    newname = origname;
retry:
    if (!strncmp(newname, "/dev/fd/", 8)) {
        fsck_set_detonator_run(1);
        return origname;
    } else {
        fsck_set_detonator_run(0);
    }
    
    if (stat(newname, &stblock) < 0) {
        fsck_print(LOG_TYPE_ERROR, newname);
        fsck_print(LOG_TYPE_INFO, "Can't stat %s\n", newname);
        return origname;
    }
    if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
        if (stslash.st_dev == stblock.st_rdev) {
            fsck_set_hotroot(1);
        }
        raw = rawname(newname);
        if (stat(raw, &stchar) < 0) {
            fsck_print(LOG_TYPE_ERROR, raw);
            fsck_print(LOG_TYPE_INFO, "Can't stat %s\n", raw);
            return origname;
        }
        if ((stchar.st_mode & S_IFMT) == S_IFCHR) {
            return raw;
        } else {
            fsck_print(LOG_TYPE_INFO, "%s is not a character device\n", raw);
            return origname;
        }
    } else if ((stblock.st_mode & S_IFMT) == S_IFCHR && !retried) {
        newname = unrawname(newname);
        retried++;
        goto retry;
    }
    /*
     * Not a block or character device, just return name and
     * let the caller decide whether to use it.
     */
    return origname;
}


static char *
get_mount_point(const char *cdev)
{
    char *retval = NULL;
    struct statfs *fsinfo;
    char *unraw = NULL;
    int result;
    int i;
    
    if (fsck_get_detonator_run()) {
        return NULL;
    }

    unraw = strdup(cdev);
    unrawname(unraw);

    if (unraw == NULL) {
        goto done;
    }

    result = getmntinfo(&fsinfo, MNT_NOWAIT);

    for (i = 0; i < result; i++) {
        if (strcmp(unraw, fsinfo[i].f_mntfromname) == 0) {
            retval = strdup(fsinfo[i].f_mntonname);
            break;
        }
    }

done:
    if (unraw) {
        free(unraw);
    }

    return retval;
}


/**
 * This routine will attempt to open the block device with write access for the target
 * volume in order to block others from mounting the volume with write access while we
 * check / repair it.  If we cannot get write access then we check to see if the volume
 * has been mounted read-only.  If it is read-only then we should be OK to write to
 * the raw device.  Note that this does not protect use from someone upgrading the mount
 * from read-only to read-write.
 */
static void get_write_access( char *dev)
{
    int                    i;
    int                    myMountsCount;
    void *                myPtr;
    char *                myCharPtr;
    struct statfs *            myBufPtr;
    void *                myNamePtr;
    int                blockDevice_fd = -1;

    myPtr = NULL;
    myNamePtr = malloc( strlen(dev) + 2 );
    if ( myNamePtr == NULL )
        return;
        
    strcpy( (char *)myNamePtr, dev );
    if ( (myCharPtr = strrchr( (char *)myNamePtr, '/' )) != 0 ) {
        if ( myCharPtr[1] == 'r' ) {
            memmove(&myCharPtr[1], &myCharPtr[2], strlen(&myCharPtr[2]) + 1);
            blockDevice_fd = open( (char *)myNamePtr, O_WRONLY | (fsck_get_hotmount() ? 0 : O_EXLOCK) );
        }
    }
    
    fsck_set_device_writable(0);
    
    if ( blockDevice_fd > 0 ) {
        // we got write access to the block device so we can safely write to raw device
        fsck_set_device_writable(1);
        goto ExitThisRoutine;
    }
    
    // get count of mounts then get the info for each
    myMountsCount = getfsstat( NULL, 0, MNT_NOWAIT );
    if ( myMountsCount < 0 )
        goto ExitThisRoutine;

    myPtr = (void *) malloc( sizeof(struct statfs) * myMountsCount );
    if ( myPtr == NULL )
        goto ExitThisRoutine;
    myMountsCount = getfsstat(     myPtr,
                                (int)(sizeof(struct statfs) * myMountsCount),
                                MNT_NOWAIT );
    if ( myMountsCount < 0 )
        goto ExitThisRoutine;

    myBufPtr = (struct statfs *) myPtr;
    for ( i = 0; i < myMountsCount; i++ )
    {
        if ( strcmp( myBufPtr->f_mntfromname, myNamePtr ) == 0 ) {
            if ( myBufPtr->f_flags & MNT_RDONLY )
                fsck_set_device_writable(1);
            goto ExitThisRoutine;
        }
        myBufPtr++;
    }
    fsck_set_device_writable(1);   // single user will get us here, f_mntfromname is not /dev/diskXXXX
    
ExitThisRoutine:
    if (myPtr != NULL){
        free(myPtr);
    }
        
    if (myNamePtr != NULL) {
        free(myNamePtr);
    }
    
    if (blockDevice_fd != -1) {
        close(blockDevice_fd);
    }

    return;
    
} /* get_write_access */

void
ckfini()
{
    DestroyCache();
    
    if (fsck_get_fswritefd() < 0) {
        (void)close(fsck_get_fsreadfd());
        return;
    }
    (void)close(fsck_get_fsreadfd());
    (void)close(fsck_get_fswritefd());
}

char *
rawname(char *name)
{
    static char rawbuf[32];
    char *dp;

    if ((dp = strrchr(name, '/')) == 0) {
        return 0;
    }
    *dp = 0;
    (void)strlcpy(rawbuf, name, sizeof(rawbuf));
    *dp = '/';
    (void)strlcat(rawbuf, "/r", sizeof(rawbuf));
    (void)strlcat(rawbuf, &dp[1], sizeof(rawbuf));

    return (rawbuf);
}

char *
unrawname(char *name)
{
    char *dp;
    struct stat stb;

    if ((dp = strrchr(name, '/')) == 0)
        return (name);
    if (stat(name, &stb) < 0)
        return (name);
    if ((stb.st_mode & S_IFMT) != S_IFCHR)
        return (name);
    if (dp[1] != 'r')
        return (name);
    memmove(&dp[1], &dp[2], strlen(&dp[2]) + 1);

    return (name);
}
