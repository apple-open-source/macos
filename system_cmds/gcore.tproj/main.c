/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "corefile.h"
#include "vanilla.h"
#include "sparse.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libproc.h>

#include <sys/kauth.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <assert.h>
#include <libutil.h>

#include <mach/mach.h>

static char *
kern_corefile(void)
{
    char *(^sysc_string)(const char *name) = ^(const char *name) {
        char *p = NULL;
        size_t len = 0;

        if (-1 == sysctlbyname(name, NULL, &len, NULL, 0)) {
            warnc(errno, "sysctl: %s", name);
        } else if (0 != len) {
            p = malloc(len);
            if (-1 == sysctlbyname(name, p, &len, NULL, 0)) {
                warnc(errno, "sysctl: %s", name);
                free(p);
                p = NULL;
            }
        }
        return p;
    };

    char *s = sysc_string("kern.corefile");
    if (NULL == s)
        s = strdup("/cores/core.%P");
    return s;
}

static const struct proc_bsdinfo *
get_bsdinfo(pid_t pid)
{
    if (0 == pid)
        return NULL;
    struct proc_bsdinfo *pbi = calloc(1, sizeof (*pbi));
    if (0 != proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, pbi, sizeof (*pbi)))
        return pbi;
    free(pbi);
    return NULL;
}

static char *
format_gcore_name(const char *fmt, const struct proc_bsdinfo *pbi)
{
    __block size_t resid = MAXPATHLEN;
    __block char *p = calloc(1, resid);
    char *out = p;

    int (^outchar)(int c) = ^(int c) {
        if (resid > 1) {
            *p++ = (char)c;
            resid--;
            return 1;
        } else
            return 0;
    };

    ptrdiff_t (^outstr)(const char *str) = ^(const char *str) {
        const char *s = str;
        while (*s && 0 != outchar(*s++))
            ;
        return s - str;
    };

    ptrdiff_t (^outint)(int value)= ^(int value) {
        char id[11];
        snprintf(id, sizeof (id), "%u", value);
        return outstr(id);
    };

    ptrdiff_t (^outtstamp)(void) = ^(void) {
        time_t now;
        time(&now);
        struct tm tm;
        gmtime_r(&now, &tm);
        char tstamp[50];
        strftime(tstamp, sizeof (tstamp), "%Y%m%dT%H%M%SZ", &tm);
        return outstr(tstamp);
    };

    int c;

    for (int i = 0; resid > 0; i++)
        switch (c = fmt[i]) {
            default:
                outchar(c);
                break;
            case '%':
                i++;
                switch (c = fmt[i]) {
                    case '%':
                        outchar(c);
                        break;
                    case 'P':
                        outint(pbi->pbi_pid);
                        break;
                    case 'U':
                        outint(pbi->pbi_uid);
                        break;
                    case 'N':
                        outstr(pbi->pbi_name[0] ?
                               pbi->pbi_name : pbi->pbi_comm);
                        break;
                    case 'T':
                        outtstamp();	// ISO 8601 format
                        break;
                    default:
                        if (isprint(c))
                            err(EX_DATAERR, "unknown format char: %%%c", c);
                        else if (c != 0)
                            err(EX_DATAERR, "bad format char %%\\%03o", c);
                        else
                            err(EX_DATAERR, "bad format specifier");
                }
                break;
            case 0:
                outchar(c);
                goto done;
        }
done:
    return out;
}

const char *pgm;
const struct options *opt;

int
main(int argc, char *const *argv)
{
    if (NULL == (pgm = strrchr(*argv, '/')))
        pgm = *argv;
    else
        pgm++;

#define	ZOPT_ALG	(0)
#define	ZOPT_CHSIZE	(ZOPT_ALG + 1)

    static char *const zoptkeys[] = {
        [ZOPT_ALG] = "algorithm",
        [ZOPT_CHSIZE] = "chunksize",
        NULL
    };

    err_set_exit_b(^(int eval) {
        if (EX_USAGE == eval) {
            fprintf(stderr,
                    "usage:\n\t%s [-s] [-v] [[-o file] | [-c pathfmt ]] [-b size] "
#if DEBUG
                    "[-d] [-n] [-i] [-p] [-S] [-z] [-C] "
                    "[-Z compression-options] "
#ifdef CONFIG_REFSC
                    "[-R] "
#endif
#endif
                    "pid\n", pgm);
#if DEBUG
            fprintf(stderr, "where compression-options:\n");
            const char zvalfmt[] = "\t%s=%s\t\t%s\n";
            fprintf(stderr, zvalfmt, zoptkeys[ZOPT_ALG], "alg",
                    "set compression algorithm");
            fprintf(stderr, zvalfmt, zoptkeys[ZOPT_CHSIZE], "size",
                    "set compression chunksize, Mib");
#endif
        }
    });

    char *corefmt = NULL;
    char *corefname = NULL;
    const size_t oneM = 1024 * 1024;

#define	LARGEST_CHUNKSIZE               INT32_MAX
#define	DEFAULT_COMPRESSION_CHUNKSIZE	(16 * oneM)

    struct options options = {
        .corpse = 0,
        .suspend = 0,
        .preserve = 0,
        .verbose = 0,
        .debug = 0,
        .dryrun = 0,
        .sparse = 0,
        .sizebound = 0,
        .coreinfo = 0,
#ifdef OPTIONS_REFSC
        .scfileref = 0,
#endif
        .compress = 0,
        .chunksize = LARGEST_CHUNKSIZE,
        .calgorithm = COMPRESSION_LZFSE,
    };

    int c;
    char *sopts, *value;

    while ((c = getopt(argc, argv, "inmvdszpCSRZ:o:c:b:")) != -1) {
        switch (c) {

                /*
                 * documented options
                 */
            case 's':   /* FreeBSD compat: stop while gathering */
                options.suspend = 1;
                break;
            case 'o':   /* Linux (& SunOS) compat: basic name */
                corefname = strdup(optarg);
                break;
            case 'c':   /* FreeBSD compat: basic name */
                /* (also allows pattern-based naming) */
                corefmt = strdup(optarg);
                break;

            case 'b':   /* bound the size of the core file */
                if (NULL != optarg) {
                    off_t bsize = atoi(optarg) * oneM;
                    if (bsize > 0)
                        options.sizebound = bsize;
                    else
                        errx(EX_USAGE, "invalid bound");
                } else
                    errx(EX_USAGE, "no bound specified");
                break;
            case 'v':   /* verbose output */
                options.verbose++;
                break;

                /*
                 * dev and debugging help
                 */
            case 'n':   /* write the core file to /dev/null */
                options.dryrun++;
                break;
            case 'd':   /* debugging */
                options.debug++;
                options.verbose++;
                options.preserve++;
                break;
            case 'p':   /* preserve partial core file (even if errors) */
                options.preserve++;
                break;

                /*
                 * Remaining options are experimental and/or
                 * affect the content of the core file
                 */
            case 'i':   /* include LC_COREINFO data */
                options.coreinfo++;
                break;
            case 'C':   /* corpsify rather than suspend */
                options.corpse++;
                break;
#ifdef CONFIG_REFSC
            case 'R':   /* include the shared cache by reference */
                options.scfileref++;
                options.coreinfo++;
                break;
#endif
            case 'S':   /* use dyld info to control the content */
                options.sparse++;
                options.coreinfo++;
                break;
            case 'z':   /* create compressed LC_SEGMENT* segments */
                if (0 == options.compress) {
                    options.compress++;
                    options.chunksize = DEFAULT_COMPRESSION_CHUNKSIZE;
                }
                options.coreinfo++;
                break;
            case 'Z':   /* control compression options */
                /*
                 * Only LZFSE and LZ4 seem practical.
                 * (Default to LZ4 compression when the
                 * process is suspended, LZFSE when corpsed?)
                 */
                if (0 == options.compress) {
                    options.compress++;
                    options.chunksize = DEFAULT_COMPRESSION_CHUNKSIZE;
                }
                sopts = optarg;
                while (*sopts) {
                    size_t chsize;

                    switch (getsubopt(&sopts, zoptkeys, &value)) {
                        case ZOPT_ALG:	/* change the algorithm */
                            if (NULL == value)
                                errx(EX_USAGE, "missing algorithm for "
                                     "%s suboption",
                                     zoptkeys[ZOPT_ALG]);
                            if (strcmp(value, "lz4") == 0)
                                options.calgorithm =
                                COMPRESSION_LZ4;
                            else if (strcmp(value, "zlib") == 0)
                                options.calgorithm =
                                COMPRESSION_ZLIB;
                            else if (strcmp(value, "lzma") == 0)
                                options.calgorithm =
                                COMPRESSION_LZMA;
                            else if (strcmp(value, "lzfse") == 0)
                                options.calgorithm =
                                COMPRESSION_LZFSE;
                            else
                                errx(EX_USAGE, "unknown algorithm '%s'"
                                     " for %s suboption",
                                     value,
                                     zoptkeys[ZOPT_ALG]);
                            break;
                        case ZOPT_CHSIZE:     /* set the chunksize */
                            if (NULL == value)
                                errx(EX_USAGE, "no value specified for "
                                     "%s suboption",
                                     zoptkeys[ZOPT_CHSIZE]);
                            if ((chsize = atoi(value)) < 1)
                                errx(EX_USAGE, "chunksize %lu too small", chsize);
                            if (chsize > (LARGEST_CHUNKSIZE / oneM))
                                errx(EX_USAGE, "chunksize %lu too large", chsize);
                            options.chunksize = chsize * oneM;
                            break;
                        default:
                            if (suboptarg)
                                errx(EX_USAGE, "illegal suboption '%s'",
                                     suboptarg);
                            else
                                errx(EX_USAGE, "missing suboption");
                    }
                }
                break;
            default:
                errx(EX_USAGE, "unknown flag");
        }
    }
    if (optind == argc)
        errx(EX_USAGE, "no pid specified");

    opt = &options;

    if ((opt->dryrun ? 1 : 0) +
        (NULL != corefname ? 1 : 0) +
        (NULL != corefmt ? 1 : 0) > 1)
        errx(EX_USAGE, "specify only one of -n, -o and -c");

    setpageshift();

    const pid_t pid = atoi(argv[optind]);
    if (pid < 1 || getpid() == pid)
        errx(EX_DATAERR, "invalid pid: %d", pid);
    if (-1 == kill(pid, 0)) {
        switch (errno) {
            case ESRCH:
                errc(EX_DATAERR, errno, "no process with pid %d", pid);
            default:
                errc(EX_DATAERR, errno, "pid %d", pid);
        }
    }

    const struct proc_bsdinfo *pbi = get_bsdinfo(pid);
    if (NULL == pbi)
        errx(EX_OSERR, "cannot get bsdinfo about %d", pid);

    /*
     * make our data model match the data model of the target
     */
    if (-1 == reexec_to_match_lp64ness(pbi->pbi_flags & PROC_FLAG_LP64))
        errc(1, errno, "cannot match data model of %d", pid);

#if defined(__LP64__)
    if ((pbi->pbi_flags & PROC_FLAG_LP64) == 0)
#else
    if ((pbi->pbi_flags & PROC_FLAG_LP64) != 0)
#endif
        errx(EX_OSERR, "cannot match data model of %d", pid);

    /*
     * These are experimental options for the moment.
     * These will likely change.
     * Some may become defaults, some may be removed altogether.
     */
    if (opt->sparse ||
#ifdef CONFIG_REFSC
        opt->scfileref ||
#endif
        opt->compress ||
        opt->corpse ||
        opt->coreinfo)
        warnx("experimental option(s) used, "
              "resulting corefile may be unusable.");

    if (pbi->pbi_ruid != pbi->pbi_svuid ||
        pbi->pbi_rgid != pbi->pbi_svgid)
        errx(EX_NOPERM, "pid %d - not dumping a set-id process", pid);

    if (NULL == corefname) {
        if (NULL == corefmt) {
            const char defcore[] = "%N-%P-%T";
            if (NULL == (corefmt = kern_corefile()))
                corefmt = strdup(defcore);
            else {
                // use the same directory as kern.corefile
                char *p = strrchr(corefmt, '/');
                if (NULL != p) {
                    *p = '\0';
                    size_t len = strlen(corefmt) +
                    strlen(defcore) + 2;
                    char *buf = malloc(len);
                    snprintf(buf, len, "%s/%s", corefmt, defcore);
                    free(corefmt);
                    corefmt = buf;
                }
                if (opt->debug)
                    printf("corefmt '%s'\n", corefmt);
            }
        }
        corefname = format_gcore_name(corefmt, pbi);
        free(corefmt);
    }

    task_t task;
    kern_return_t ret = task_for_pid(mach_task_self(), pid, &task);
    if (KERN_SUCCESS != ret) {
        if (KERN_FAILURE == ret)
            errx(EX_NOPERM, "insufficient privilege");
        else
            errx(EX_NOPERM, "task_for_pid: %s", mach_error_string(ret));
    }

    /*
     * Now that we have the task port, we adopt the credentials of
     * the target process, *before* opening the core file, and
     * analyzing the address space.
     *
     * If we are unable to match the target credentials, bail out.
     */
    if (getgid() != pbi->pbi_gid &&
        setgid(pbi->pbi_gid) == -1)
        errc(EX_NOPERM, errno, "insufficient privilege");

    if (getuid() != pbi->pbi_uid &&
        setuid(pbi->pbi_uid) == -1)
        errc(EX_NOPERM, errno, "insufficient privilege");

    int fd;

    if (opt->dryrun) {
        free(corefname);
        corefname = strdup("/dev/null");
        fd = open(corefname, O_RDWR);
    } else {
        fd = open(corefname, O_RDWR | O_CREAT | O_EXCL, 0400);

        struct stat st;

        if (-1 == fd || -1 == fstat(fd, &st))
            errc(EX_CANTCREAT, errno, "%s", corefname);
        if ((st.st_mode & S_IFMT) != S_IFREG || 1 != st.st_nlink) {
            close(fd);
            errx(EX_CANTCREAT, "%s: invalid file", corefname);
        }
    }

    if (opt->verbose) {
        printf("Dumping core ");
        if (opt->debug) {
            printf("(%s%s%s",
                   opt->sparse ? "sparse" : "normal",
                   opt->compress ? ", compressed" : "",
#ifdef CONFIG_REFSC
                   opt->scfileref ? ", scfilerefs" :
#endif
                   "");
            if (0 != opt->sizebound) {
                hsize_str_t hstr;
                printf(", %s", str_hsize(hstr, opt->sizebound));
            }
            printf(") ");
        }
        printf("for pid %d to %s\n", pid, corefname);
    }
    int ecode;

    /*
     * The traditional way to capture a consistent core dump is to
     * suspend the process while processing it and writing it out.
     * Yet suspending a large process for a long time can have
     * unpleasant side-effects.  Alternatively dumping from the live
     * process can lead to an inconsistent state in the core file.
     *
     * Instead we can ask xnu to create a 'corpse' - the process is transiently
     * suspended while a COW snapshot of the address space is constructed
     * in the kernel and dump from that.  This vastly reduces the suspend
     * time, but it is more resource hungry and thus may fail.
     *
     * The -s flag (opt->suspend) causes a task_suspend/task_resume
     * The -C flag (opt->corpse) causes a corpse be taken, exiting if that fails.
     * Both flags can be specified, in which case corpse errors are ignored.
     *
     * With no flags, we imitate traditional behavior though more
     * efficiently: we try to take a corpse-based dump, in the event that
     * fails, dump the live process.
     */

    int trycorpse = 1;          /* default: use corpses */
    int badcorpse_is_fatal = 1; /* default: failure to create a corpse is an error */

    if (!opt->suspend && !opt->corpse) {
        /* try a corpse dump, else dump the live process */
        badcorpse_is_fatal = 0;
    } else if (opt->suspend) {
        trycorpse = opt->corpse;
        /* if suspended anyway, ignore corpse-creation errors */
        badcorpse_is_fatal = 0;
    }

    if (opt->suspend)
        task_suspend(task);

    if (trycorpse) {
        /*
         * Create a corpse from the image before dumping it
         */
        mach_port_t corpse = MACH_PORT_NULL;
        ret = task_generate_corpse(task, &corpse);
        switch (ret) {
            case KERN_SUCCESS:
                if (opt->debug)
                    printf("corpse generated on port %x, task %x\n",
                           corpse, task);
                ecode = coredump(corpse, fd);
                mach_port_deallocate(mach_task_self(), corpse);
                break;
            default:
                if (badcorpse_is_fatal || opt->debug) {
                    warnx("failed to snapshot pid %d: %s\n",
                        pid, mach_error_string(ret));
                    if (badcorpse_is_fatal) {
                        ecode = KERN_RESOURCE_SHORTAGE == ret ? EX_TEMPFAIL : EX_OSERR;
                        goto out;
                    }
                }
                ecode = coredump(task, fd);
                break;
        }
    } else {
        /*
         * Examine the task directly
         */
        ecode = coredump(task, fd);
    }

out:
    if (opt->suspend)
        task_resume(task);

    if (0 != ecode && !opt->preserve && !opt->dryrun) {
        /*
         * try not to leave a half-written mess occupying
         * blocks on the filesystem
         */
        ftruncate(fd, 0);
        unlink(corefname);
    }
    if (-1 == close(fd))
        ecode = EX_OSERR;
    if (ecode)
        errx(ecode, "failed to dump core for pid %d", pid);
    free(corefname);

    return 0;
}
