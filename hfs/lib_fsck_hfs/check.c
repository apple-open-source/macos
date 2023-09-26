//
//  check.c
//
//  Created by Adam Hijaze on 02/11/2022.
//

#include "check.h"
#include "fsck_msgnums.h"
#include "dfalib/CheckHFS.h"

#include <hfs/hfs_mount.h>

static int setup (char *dev);
static int ScanDisk(int fd);
int     reply __P((char *question));

static int printStatus;
static void siginfo(int signo);

Cache_t fscache;

/*
 * Variables used to map physical block numbers to file paths
 */
enum        { BLOCK_LIST_INCREMENT = 512 };
int         gBlkListEntries = 0;
u_int64_t*  gBlockList = NULL;
int         gFoundBlockEntries = 0;
struct      found_blocks *gFoundBlocksList = NULL;

int checkfilesys(char * filesys)
{
    int flags;
    int result = 0;
    int canWrite = fsck_get_writable();
    char *mntonname = state.mountpoint;
    flags = 0;
    int error = 0;
    state.cdevname = filesys;
    state.hotmount = state.hotroot;    // hotroot will be 1 or 0 by this time

    /**
     * initialize the printing/logging without actually printing anything
     * DO NOT DELETE THIS or else you can deadlock during a live fsck
     * when something is printed and we try to create the log file.
     */
    fsck_print(ctx, LOG_TYPE_INFO, "");

    if (state.lflag && !state.detonatorRun) {
        struct stat fs_stat;

        /*
         * Ensure that, if we're doing a live verify, that we're not trying
         * to do input or output to the same device.  This would cause a deadlock.
         */

        if (stat(state.cdevname, &fs_stat) != -1 &&
            (((fs_stat.st_mode & S_IFMT) == S_IFCHR) ||
             ((fs_stat.st_mode & S_IFMT) == S_IFBLK))) {
            struct stat io_stat;

            if (fstat(fileno(stdin), &io_stat) != -1 &&
                (fs_stat.st_rdev == io_stat.st_dev)) {
                fsck_print(ctx, LOG_TYPE_INFO, "ERROR: input redirected from target volume for live verify.\n");
                    return EEXIT;
            }
            if (fstat(fileno(stdout), &io_stat) != -1 &&
                (fs_stat.st_rdev == io_stat.st_dev)) {
                    fsck_print(ctx, LOG_TYPE_INFO, "ERROR:  output redirected to target volume for live verify.\n");
                    return EEXIT;
            }
            if (fstat(fileno(stderr), &io_stat) != -1 &&
                (fs_stat.st_rdev == io_stat.st_dev)) {
                    fsck_print(ctx, LOG_TYPE_INFO, "ERROR:  error output redirected to target volume for live verify.\n");
                    return EEXIT;
            }

        }
    }

    if (state.debug && state.preen) {
        fsck_print(ctx, LOG_TYPE_WARNING, "starting\n");
    }
    
    if (setup(filesys) == 0) {
        if (state.preen) {
            fsck_print(ctx, LOG_TYPE_FATAL, "CAN'T CHECK FILE SYSTEM.");
        }
        result = EEXIT;
        goto ExitThisRoutine;
    }

    if (state.preen == 0) {
        if (state.hotroot && !state.guiControl) {
            fsck_print(ctx, LOG_TYPE_INFO, "** Root file system\n");
        }
    }

    if (state.errorOnExit && state.nflag) {
        state.chkLev = kMajorCheck;
    }

    /*
     * go check HFS volume...
     */

    if (state.rebuildOptions && canWrite == 0) {
        fsck_print(ctx, LOG_TYPE_INFO, "BTree rebuild requested but writing disabled\n");
        result = EEXIT;
        goto ExitThisRoutine;
    }

    if (gBlockList != NULL && state.scanflag != 0) {
        fsck_print(ctx, LOG_TYPE_INFO, "Cannot scan for bad blocks and ask for listed blocks to file mapping\n");
        result = EEXIT;
        goto ExitThisRoutine;
    }
    if (state.scanflag != 0) {
        fsck_print(ctx, LOG_TYPE_INFO, "Scanning entire disk for bad blocks\n");
        error = ScanDisk(state.fsreadfd);
        if (error) {
            return error;
        }
    }

    result = CheckHFS( filesys, state.fsreadfd, state.fswritefd, state.chkLev, state.repLev, ctx,
                       state.lostAndFoundMode, canWrite, &state.fsmodified,
                       state.lflag, state.rebuildOptions );
    if (state.debug) {
        fsck_print(ctx, LOG_TYPE_INFO, "\tCheckHFS returned %d, fsmodified = %d\n", result, state.fsmodified);
    }

    if (!state.hotmount) {
        DestroyCache();
        if (state.quick) {
            if (result == 0) {
                fsck_print(ctx, LOG_TYPE_WARNING, "QUICKCHECK ONLY; FILESYSTEM CLEAN\n");
                result = 0;
                goto ExitThisRoutine;
            } else if (result == R_Dirty) {
                fsck_print(ctx, LOG_TYPE_WARNING, "QUICKCHECK ONLY; FILESYSTEM DIRTY\n");
                result = DIRTYEXIT;
                goto ExitThisRoutine;
            } else if (result == R_BadSig) {
                fsck_print(ctx, LOG_TYPE_WARNING, "QUICKCHECK ONLY; NO HFS SIGNATURE FOUND\n");
                result = DIRTYEXIT;
                goto ExitThisRoutine;
            } else {
                result = EEXIT;
                goto ExitThisRoutine;
            }
        }
    } else {
        struct statfs stfs_buf;

        /*
         * Check to see if root is mounted read-write.
         */
        if (statfs(mntonname, &stfs_buf) == 0) {
            flags = stfs_buf.f_flags;
        }
        else {
            flags = 0;
        }
        DestroyCache();
    }

    /* XXX free any allocated memory here */

    if (state.hotmount && state.fsmodified) {
        struct hfs_mount_args args;
        /*
         * We modified the root.  Do a mount update on
         * it, unless it is read-write, so we can continue.
         */
        if (!state.preen) {
            fsckPrintFormat(ctx, fsckVolumeModified);
        }
        if (flags & MNT_RDONLY) {
            bzero(&args, sizeof(args));
            flags |= MNT_UPDATE | MNT_RELOAD;
            if (state.debug) {
                fsck_print(ctx, LOG_TYPE_STDERR, "doing update / reload mount for %s now\n", mntonname);
            }
            if (mount("hfs", mntonname, flags, &args) == 0) {
                if (result != 0) {
                    result = EEXIT;
                }
                goto ExitThisRoutine;
            } else {
                fsck_print(ctx, LOG_TYPE_STDERR, "update/reload mount for %s failed: %s\n", mntonname, strerror(errno));
            }
        }
        if (!state.preen) {
            fsck_print(ctx, LOG_TYPE_INFO, "\n***** REBOOT NOW *****\n");
        }
        sync();
        result = FIXEDROOTEXIT;
        goto ExitThisRoutine;
    }

    if (result != 0 && result != MAJOREXIT) {
        result = EEXIT;
    }
    
ExitThisRoutine:
    return result;
}

void
AddBlockToList(long long block)
{
    size_t blockListCount = BLOCK_LIST_INCREMENT;    /* Number of elements allocated to gBlockList array */
    if (gBlockList == NULL) {
        gBlockList = (u_int64_t *) malloc(blockListCount * sizeof(u_int64_t));
        if (gBlockList == NULL) {
            fsck_print(ctx, LOG_TYPE_FATAL, "Can't allocate memory for block list.\n");
        }
    }

    if ((gBlkListEntries % BLOCK_LIST_INCREMENT) == 0) {
        void *tmp;

        tmp = realloc(gBlockList, (gBlkListEntries + BLOCK_LIST_INCREMENT) * sizeof(u_int64_t));
        if (tmp == NULL) {
            fsck_print(ctx, LOG_TYPE_FATAL, "Can't allocate memory for block list (%llu entries).\n", gBlkListEntries);
        }
        gBlockList = (u_int64_t*)tmp;
    }
    gBlockList[gBlkListEntries++] = block;
    return;
}

/*
 * Setup for I/O to device
 * Return 1 if successful, 0 if unsuccessful.
 * canWrite - 1 if we can safely write to the raw device or 0 if not.
 */
static int
setup(char *dev)
{
    struct stat statb;
    int devBlockSize;
    uint32_t cacheBlockSize;
    uint32_t cacheTotalBlocks;
    int preTouchMem = 0;
    
    if (!state.detonatorRun) {
        if (stat(dev, &statb) < 0) {
            fsck_print(ctx, LOG_TYPE_INFO, "Can't stat %s: %s\n", dev, strerror(errno));
            return 0;
        }
        if ((statb.st_mode & S_IFMT) != S_IFCHR) {
            fsck_print(ctx, LOG_TYPE_FATAL, "%s is not a character device", dev);
            if (reply("CONTINUE") == 0) {
                return 0;
            }
        }
        /* Always attempt to replay the journal */
        if (!state.nflag && !state.quick) {
            // We know we have a character device by now.
            if (strncmp(dev, "/dev/rdisk", 10) == 0) {
                char block_device[MAXPATHLEN+1];
                int rv;
                snprintf(block_device, sizeof(block_device), "/dev/%s", dev + 6);
                rv = journal_replay(block_device);
                if (state.debug) {
                    fsck_print(ctx, LOG_TYPE_INFO, "journal_replay(%s) returned %d\n", block_device, rv);
                }
            }
        }
    } else { // detonator run
        fsck_print(ctx, LOG_TYPE_INFO, "fsck_hfs: detonator_run (%s).\n", dev);

        struct stat info;
        int error = fstat(state.fswritefd, &info);
        if (error) {
            fsck_print(ctx, LOG_TYPE_TERMINATE, "fsck_hfs: fstat %s", dev);
        }
        
        error = (int)lseek(state.fswritefd, 0, SEEK_SET);
        if (error == -1) {
            fsck_print(ctx, LOG_TYPE_TERMINATE, "fsck_hfs: Could not seek %d for dev: %s, errorno %d", state.fswritefd, dev, errno);
        }
        fsck_set_device_writable(1);
    }
    
    if (state.preen == 0 && !state.guiControl) {
        if (state.nflag || state.quick || state.fswritefd == -1) {
            fsck_print(ctx, LOG_TYPE_INFO, "** %s (NO WRITE)\n", dev);
        } else {
            fsck_print(ctx, LOG_TYPE_INFO, "** %s\n", dev);
        }
    }

    /* Get device block size to initialize cache */
    devBlockSize = fsck_get_dev_block_size();
    if (devBlockSize == -1) {
        fsck_print(ctx, LOG_TYPE_INFO, "Device block size was not initialized\n");
        return 0;
    }

     /*
      * Calculate the cache block size and total blocks.
      *
      * If a quick check was requested, we'll only be checking to see if
      * the volume was cleanly unmounted or journalled, so we won't need
      * a lot of cache.  Since lots of quick checks can be run in parallel
      * when a new disk with several partitions comes on line, let's avoid
      * the memory usage when we don't need it.
      */
    if (state.reqCacheSize == 0 && state.quick == 0) {
        /*
         * Auto-pick the cache size.  The cache code will deal with minimum
         * maximum values, so we just need to find out the size of memory, and
         * how much of it we'll use.
         *
         * If we're looking at the root device, and it's not a live verify (lflag),
         * then we will use half of physical memory; otherwise, we'll use an eigth.
         *
         */
        uint64_t memSize;
        size_t dsize = sizeof(memSize);
        int rv;

        rv = sysctlbyname("hw.memsize", &memSize, &dsize, NULL, 0);
        if (rv == -1) {
            fsck_print(ctx, LOG_TYPE_STDERR, "sysctlbyname failed, not auto-setting cache size\n");
        } else {
            int d = (state.hotroot && !state.lflag) ? 2 : 8;
            if (!state.detonatorRun) {
                int safeMode = 0;
                dsize = sizeof(safeMode);
                rv = sysctlbyname("kern.safeboot", &safeMode, &dsize, NULL, 0);
                if (rv != -1 && safeMode != 0 && state.hotroot && !state.lflag) {
    #define kMaxSafeModeMem    ((size_t)2 * 1024 * 1024 * 1024)    /* 2Gbytes, means cache will max out at 1gbyte */
                    if (state.debug) {
                        fsck_print(ctx, LOG_TYPE_STDERR, "Safe mode and single-user, setting memsize to a maximum of 2gbytes\n");
                    }
                    memSize = (memSize < kMaxSafeModeMem) ? memSize : kMaxSafeModeMem;
                }
            }
            state.reqCacheSize = memSize / d;
        }
    }
    
    CalculateCacheSizes(state.reqCacheSize, &cacheBlockSize, &cacheTotalBlocks, state.debug);

    preTouchMem = (state.hotroot != 0) && (state.lflag != 0);
    /* Initialize the cache */
    if (CacheInit (&fscache, state.fsreadfd, state.fswritefd, devBlockSize,
            cacheBlockSize, cacheTotalBlocks, CacheHashSize, preTouchMem) != EOK) {
        fsck_print(ctx, LOG_TYPE_FATAL, "Can't initialize disk cache\n");

        return 0;
    }

    return 1;
}

static int
ScanDisk(int fd)
{
    uint32_t devBlockSize = 512;
    uint64_t devBlockTotal;
    off_t diskSize;
    uint8_t *buffer = NULL;
    size_t bufSize = 1024 * 1024;
    ssize_t nread;
    off_t curPos = 0;
    void (*oldhandler)(int);
    uint32_t numErrors = 0;
    uint32_t maxErrors = 40;    // Something more variable?

    oldhandler = signal(SIGINFO, &siginfo);

#define PRSTAT \
    do { \
        if (diskSize) { \
            fsck_print(ctx, LOG_TYPE_ERROR, "Scanning offset %lld of %lld (%d%%)\n", \
                curPos, diskSize, (int)((curPos * 100) / diskSize)); \
        } else { \
            fsck_print(ctx, LOG_TYPE_ERROR, "Scanning offset %lld\n", curPos); \
        } \
        printStatus = 0; \
    } while (0)
    
    if (state.devBlockSize == -1) {
        devBlockSize = 512;
    } else {
        devBlockSize = state.devBlockSize;
    }

    devBlockTotal = state.blockCount;
    if (devBlockTotal == 0) {
        diskSize = 0;
    } else {
        diskSize = devBlockTotal * devBlockSize;
    }

    while (buffer == NULL && bufSize >= devBlockSize) {
        buffer = malloc(bufSize);
        if (buffer == NULL) {
            bufSize /= 2;
        }
    }
    if (buffer == NULL) {
        fsck_print(ctx, LOG_TYPE_FATAL, "Cannot allocate buffer for disk scan.\n");
    }

loop:

    if (printStatus) {
        PRSTAT;
    }
    while ((nread = pread(fd, buffer, bufSize, curPos)) == bufSize) {
        curPos += bufSize;
        if (printStatus) {
            PRSTAT;
        }
    }

    if (nread == 0) {
        /* We're done with the disk */
        goto done;
    }
    if (nread == -1) {
        if (errno == EIO) {
            /* Try reading devBlockSize blocks */
            size_t total;
            for (total = 0; total < bufSize; total += devBlockSize) {
                nread = pread(fd, buffer, devBlockSize, curPos + total);
                if (nread == -1) {
                    if (errno == EIO) {
                        if (state.debug) {
                            fsck_print(ctx, LOG_TYPE_STDERR, "Bad block at offset %lld\n", curPos + total);
                        }
                        AddBlockToList((curPos + total) / fsck_get_block_size());
                        if (++numErrors > maxErrors) {
                            if (state.debug) {
                                fsck_print(ctx, LOG_TYPE_STDERR, "Got %u errors, maxing out so stopping scan\n", numErrors);
                            }
                            goto done;
                        }
                        continue;
                    } else {
                        fsck_print(ctx, LOG_TYPE_FATAL, "Got a non I/O error reading disk at offset %llu: %s\n",
                                   curPos + total, strerror(errno));
                        // Hey, pfatal wasn't fatal!
                        // But that seems to work out for us for some reason.
                    }
                }
                if (nread == 0) {
                    /* End of disk, somehow. */
                    goto done;
                }
                if (nread != devBlockSize) {
                    fsck_print(ctx, LOG_TYPE_WARNING, "During disk scan, did not get block size (%zd) read, got %zd instead.  Skipping rest of this block.\n", (size_t)devBlockSize, nread);
                    continue;
                }
            }
            curPos += total;
            goto loop;
        } else if (errno == EINTR) {
            goto loop;
        } else {
            fsck_print(ctx, LOG_TYPE_FATAL, "Got a non I/O error reading disk at offset %llu:  %s\n", curPos, strerror(errno));
            return EEXIT;
        }
    }
    if (nread < bufSize) {
        if ((nread % devBlockSize) == 0) {
            curPos += nread;
        } else {
            curPos = curPos + (((nread % devBlockSize) + 1) * devBlockSize);
        }
        goto loop;
    }
    goto loop;
done:
    if (buffer) {
        free(buffer);
    }
    signal(SIGINFO, oldhandler);
    return 0;

}

int
reply(char *question)
{
    int persevere;
    char c;

    if (state.preen) {
        fsck_print(ctx, LOG_TYPE_FATAL, "INTERNAL ERROR: GOT TO reply()");
    }
    persevere = !strcmp(question, "CONTINUE");
    fsck_print(ctx, LOG_TYPE_INFO, "\n");
    if (!persevere && (state.nflag || state.fswritefd < 0)) {
        fsck_print(ctx, LOG_TYPE_INFO, "%s? no\n\n", question);
        return 0;
    }
    if (state.yflag || (persevere && state.nflag)) {
        fsck_print(ctx, LOG_TYPE_INFO, "%s? yes\n\n", question);
        return 1;
    }
    do  {
        fsck_print(ctx, LOG_TYPE_INFO, "%s? [yn] ", question);
        (void) fflush(stdout);
        c = getc(stdin);
        while (c != '\n' && getc(stdin) != '\n') {
            if (feof(stdin)) {
                return 0;
            }
        }
            
    } while (c != 'y' && c != 'Y' && c != 'n' && c != 'N');
    fsck_print(ctx, LOG_TYPE_INFO, "\n");
    if (c == 'y' || c == 'Y') {
        return 1;
    }
    return 0;
}

static int printStatus;
static void
siginfo(int signo)
{
    printStatus = 1;
}

#define kProgressToggle    "kern.progressmeterenable"
#define    kProgress    "kern.progressmeter"

void
start_progress(void)
{
    int rv;
    int enable = 1;
    if (state.hotroot == 0) {
        return;
    }
    rv = sysctlbyname(kProgressToggle, NULL, NULL, &enable, sizeof(enable));
    if (state.debug && rv == -1 && errno != ENOENT) {
        fsck_print(ctx, LOG_TYPE_WARN, "sysctl(%s) failed", kProgressToggle);
    }
}

void
draw_progress(int pct)
{
    int rv;
    if (state.hotroot == 0) {
        return;
    }
    rv = sysctlbyname(kProgress, NULL, NULL, &pct, sizeof(pct));
    if (state.debug && rv == -1 && errno != ENOENT) {
        fsck_print(ctx, LOG_TYPE_WARN, "sysctl(%s) failed", kProgress);
    }
}

void
end_progress(void)
{
    int rv;
    int enable = 0;
    if (state.hotroot == 0) {
        return;
    }
    rv = sysctlbyname(kProgressToggle, NULL, NULL, &enable, sizeof(enable));
    if (state.debug && rv == -1 && errno != ENOENT) {
        fsck_print(ctx, LOG_TYPE_WARN, "sysctl(%s) failed", kProgressToggle);
    }
}

/*
 * DestroyCache
 *
 *  Shutdown the cache.
 */
void DestroyCache()
{
    CacheFlush(&fscache);
    
#if CACHE_DEBUG
    /* Print cache report */
    fsck_print(ctx, LOG_TYPE_INFO, "Cache Report:\n");
    fsck_print(ctx, LOG_TYPE_INFO, "\tRead Requests:  %d\n", cache->ReqRead);
    fsck_print(ctx, LOG_TYPE_INFO, "\tWrite Requests: %d\n", cache->ReqWrite);
    fsck_print(ctx, LOG_TYPE_INFO, "\tDisk Reads:     %d\n", cache->DiskRead);
    fsck_print(ctx, LOG_TYPE_INFO, "\tDisk Writes:    %d\n", cache->DiskWrite);
    fsck_print(ctx, LOG_TYPE_INFO, "\tSpans:          %d\n", cache->Span);
#endif
}
