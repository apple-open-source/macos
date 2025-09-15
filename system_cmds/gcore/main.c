/*
 * Copyright (c) 2025 Apple Inc.  All rights reserved.
 */

#include <System/sys/proc.h>
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
#include <kern/kcdata.h>

#include <os/log.h>
#include <os/log_private.h>

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
#include <spawn.h>
#include <err.h>

#include <mach/mach.h>

os_log_t glog;

static char *
kern_corefile(void)
{
    char *(^sysc_string)(const char *name) = ^(const char *name) {
        char *p = NULL;
        size_t len = 0;

        if (-1 == sysctlbyname(name, NULL, &len, NULL, 0)) {
            os_log_error(glog, "sysctl: %{public}s: %{darwin.errno}d",
                name, errno);
        } else if (0 != len) {
            p = malloc(len);
            if (-1 == sysctlbyname(name, p, &len, NULL, 0)) {
                os_log_error(glog, "sysctl: %{public}s: %{darwin.errno}d",
                    name, errno);
                free(p);
                p = NULL;
            }
        }
        return p;
    };

    char *s = sysc_string("kern.corefile");
    if (NULL == s) {
        s = strdup("/cores/core.%P");
    }
    return s;
}

static const struct proc_bsdinfo *
get_bsdinfo(pid_t pid)
{
    if (0 == pid) {
        return NULL;
    }
    struct proc_bsdinfo *pbi = calloc(1, sizeof (*pbi));
    if (0 != proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, pbi, sizeof (*pbi))) {
        return pbi;
    }
    free(pbi);
    return NULL;
}

static char *
format_gcore_name(const char *fmt, pid_t pid, uid_t uid, const char *nm)
{
    __block size_t resid = MAXPATHLEN;
    __block char *p = calloc(1, resid);
    char *out = p;

    int (^outchar)(int c) = ^(int c) {
        if (resid > 1) {
            *p++ = (char)c;
            resid--;
            return 1;
        } else {
            return 0;
        }
    };

    char (^popchar)(void) = ^(void) {
        if (resid > 1) {
            resid--;
            return *p--;
        } else {
            return (char)0;
        }
    };

    ptrdiff_t (^outstr)(const char *str) = ^(const char *str) {
        const char *s = str;
        while (*s && 0 != outchar(*s++))
            ;
        return s - str;
    };

    ptrdiff_t (^outint)(int value) = ^(int value) {
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

    int (^ends_with)(const char *s, const char *suff) =
        ^(const char *s, const char *suff) {
        const char *dot = strrchr(s, '.');
        return dot ? strcmp(dot, suff) == 0 : false;
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
                        outint(pid);
                        break;
                    case 'U':
						outint(uid);
                        break;
                    case 'N':
						outstr(nm);
                        break;
                    case 'T':
                        outtstamp();	// ISO 8601 format
                        break;
                    default:
                        if (isprint(c)) {
                            os_log_error(glog, "unknown format char: %%%c", c);
                        } else if (c != 0) {
                            os_log_error(glog, "bad format char %%\\%03o", c);
                        } else {
                            os_log_error(glog, "bad format specifier");
                        }
                        exit(EX_DATAERR);
                }
                break;
            case 0:
                outchar(c);
                goto done;
        }
done:
    if (opt->gzip) {
        const char *dotgz = ".gz";
        if (!ends_with(out, dotgz)) {
            const char lastc = popchar();
            outstr(dotgz);
            outchar(lastc);
        }
    }
    return out;
}

static char *
make_gcore_path(char **corefmtp, pid_t pid, uid_t uid, const char *nm)
{
	char *corefmt = *corefmtp;
	if (NULL == corefmt) {
		const char defcore[] = "%N-%P-%T";
        if (NULL == (corefmt = kern_corefile())) {
            corefmt = strdup(defcore);
        } else {
			// use the same directory as kern.corefile
			char *p = strrchr(corefmt, '/');
			if (NULL != p) {
				*p = '\0';
				size_t len = strlen(corefmt) + strlen(defcore) + 2;
				char *buf = malloc(len);
				snprintf(buf, len, "%s/%s", corefmt, defcore);
				free(corefmt);
				corefmt = buf;
			}
            if (OPTIONS_DEBUG(opt, 3)) {
                printf("corefmt '%s'\n", corefmt);
            }
		}
	}
	char *path = format_gcore_name(corefmt, pid, uid, nm);
	free(corefmt);
	*corefmtp = NULL;
	return path;
}

static bool proc_same_data_model(const struct proc_bsdinfo *pbi) {
#if defined(__LP64__)
	return (pbi->pbi_flags & PROC_FLAG_LP64) != 0;
#else
	return (pbi->pbi_flags & PROC_FLAG_LP64) == 0;
#endif
}

static bool task_same_data_model(const task_flags_info_data_t *tfid) {
#if defined(__LP64__)
	return (tfid->flags & TF_LP64) != 0;
#else
	return (tfid->flags & TF_LP64) == 0;
#endif
}

/*
 * Change credentials for writing out the file
 */
static void
change_credentials(gid_t uid, uid_t gid)
{
	if ((getgid() != gid && -1 == setgid(gid)) ||
        (getuid() != uid && -1 == setuid(uid))) {
        os_log_error(glog, "insufficient privilege: %{public}s",
            strerror(errno));
        exit(EX_NOPERM);
    }
    if (uid != getuid() || gid != getgid()) {
        os_log_error(glog, "wrong credentials");
        exit(EX_OSERR);
    }
}

static int
openout(const char *corefname, char **coretname, struct stat *st)
{
#if DEBUG
    // enable corefile mmap if we're going to validate the contents
    // of the various notes that are written out
    const int oflags = O_RDWR;
#else
    const int oflags = O_WRONLY;
#endif
    const int tfd = open(corefname, oflags);
    if (-1 == tfd) {
        if (ENOENT == errno) {
            /*
             * Arrange for a core file to appear "atomically": write the data
             * to the file + ".tmp" suffix, then fchmod and rename it into
             * place once the dump completes successfully.
             */
            const size_t nmlen = strlen(corefname) + 4 + 1;
            char *tnm = malloc(nmlen);
            snprintf(tnm, nmlen, "%s.tmp", corefname);
            const int fd = open(tnm,
                oflags | O_CREAT | O_EXCL | O_NOFOLLOW | O_TRUNC, 0600);
            if (-1 == fd) {
                os_log_error(glog, "open: %{public}s: %{public}s",
                    tnm, strerror(errno));
                exit(EX_CANTCREAT);
            }
            if (-1 == fstat(fd, st)) {
                os_log_error(glog, "fstat: %{public}s: %{darwin.errno}d",
                    tnm, errno);
                exit(EX_OSERR);
            }
            if (!S_ISREG(st->st_mode) || 1 != st->st_nlink) {
                os_log_error(glog, "%{public}s: invalid attributes", tnm);
                exit(EX_CANTCREAT);
            }
            *coretname = tnm;
            return fd;
        }
        os_log_error(glog, "open: %{public}s: %{public}s",
            corefname, strerror(errno));
    } else if (0 == fstat(tfd, st) && S_ISCHR(st->st_mode)) {
        struct stat ns;
        if (0 == stat("/dev/null", &ns) && ns.st_rdev == st->st_rdev) {
            /*
             * Write dump to /dev/null, no rename!
             */
            *coretname = NULL;
            return tfd;
        }
        close(tfd);
        os_log_error(glog, "%{public}s forbidden", corefname);
    } else {
        close(tfd);
        os_log_error(glog, "%{public}s already exists", corefname);
    }
    exit(EX_CANTCREAT);
}

static int
closeout(int fd, int ecode, char *corefname, char *coretname, const struct stat *st)
{
    if (0 != ecode && !opt->preserve && S_ISREG(st->st_mode)) {
        (void) ftruncate(fd, 0);    // limit space clutter
        (void) unlink(coretname);   // limit name clutter
    }
    if (0 == ecode && S_ISREG(st->st_mode)) {
        fchmod(fd, 0400); // protect core files
    }
    if (-1 == close(fd)) {
        char *filename = coretname ? coretname : corefname;
        if (filename) {
            os_log_error(glog, "%{public}s: close: %{public}s",
                filename, strerror(errno));
        } else {
            os_log_error(glog, "close: %{public}s", strerror(errno));
        }
        ecode = EX_OSERR;
    }
    if (NULL != coretname) {
        if (0 == ecode && -1 == rename(coretname, corefname)) {
            os_log_error(glog, "cannot rename %{public}s to %{public}s: %{public}s",
                coretname, corefname, strerror(errno));
            ecode = EX_NOPERM;
        }
        free(coretname);
    }
    if (corefname) {
        free(corefname);
    }
    return ecode;
}

static pid_t
run_gzip(int *outfd, int filefd)
{
    int fdpair[2];
    if (pipe(fdpair) == -1) {
        os_log_error(glog, "gzip pipe: %{darwin.errno}d", errno);
        exit(EX_CANTCREAT);
    }
    *outfd = fdpair[1]; // write to fdpair[1], read from fdpair[0]

    posix_spawn_file_actions_t fa;
    if (posix_spawn_file_actions_init(&fa)) {
        os_log_error(glog, "file actions: %{darwin.errno}d", errno);
        exit(EX_OSERR);
    }
    /*
     * Close off our end of the pipe, and set the other end as stdin.
     * Set stdout to be the output corefile and inherit stderr.
     */
    posix_spawn_file_actions_addclose(&fa, fdpair[1]);
    posix_spawn_file_actions_adddup2(&fa, fdpair[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&fa, filefd, STDOUT_FILENO);
    posix_spawn_file_actions_addinherit_np(&fa, STDERR_FILENO);

    posix_spawnattr_t sa;
    if (posix_spawnattr_init(&sa)) {
        os_log_error(glog, "spawn attrs: %{darwin.errno}d", errno);
        exit(EX_OSERR);
    }
    posix_spawnattr_setflags(&sa, POSIX_SPAWN_CLOEXEC_DEFAULT);

#define GZIPCMD     "gzip"
#define GZIPDIR     "/usr/bin"
    char *argv[] = {
        GZIPCMD,
        "-1",   // aka --fast; everything else is mind-numbingly slow
        NULL,
    };

    extern char **environ;
    const char gzippath[] = GZIPDIR "/" GZIPCMD;
    pid_t cpid = -1;
    if (posix_spawn(&cpid, gzippath, &fa, &sa, argv, environ) == -1) {
        os_log_error(glog, "%{public}s: %{public}s", gzippath, strerror(errno));
        exit(EX_OSERR);
    }
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&sa);
    return cpid;
}

static int
wait_for_gzip(pid_t pid)
{
    int status = 0;
    pid_t wpid;
    do {
        wpid = waitpid(pid, &status, 0);
    } while (wpid < 0 && errno == EINTR);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return -1;
    }
    (void) kill(pid, SIGKILL);
    return -1;
}

const char *pgm;
const struct options *opt;

static const size_t oneK = 1024;
static const size_t oneM = oneK * oneK;

#define DEFAULT_NC_THRESHOLD			(17 * oneK)

static struct options options = {
	.corpsify = 0,
	.suspend = 0,
	.preserve = 0,
	.verbose = 0,
#ifdef CONFIG_DEBUG
	.debug = 0,
#endif
	.sizebound = 0,
	.ncthresh = DEFAULT_NC_THRESHOLD,
	.notes = 0,
    .content = CONTENT_FULL,
    .quiet = 0,
};

int
main(int argc, char *const *argv)
{
    if (NULL == (pgm = strrchr(*argv, '/'))) {
        pgm = *argv;
    } else {
        pgm++;
    }

    glog = os_log_create("com.apple.gcore", "gcore");

    const void (^usage)(void) = ^(void) {
        fprintf(stderr,
                "usage: %s [-v] [-s] [[-o file] | [-c pathfmt ]] [-b size]\n"
                "%*s[-x [stack|compact|full]] "
#if DEBUG
#ifdef CONFIG_DEBUG
                "[-d] "
#endif
                "[-q] "
                "[-C] "
                "[-S] "
                "[-t size] "
                "[-f outfd] "
#endif
                "pid\n", pgm, (int)strlen(pgm) + 8, " ");
    };

    err_set_exit_b(^(int eval) {
        if (EX_USAGE == eval) {
            usage();
        }
    });
    
#define Usage(...)    errx(EX_USAGE, __VA_ARGS__)

    char *corefmt = NULL;
    char *corefname = NULL;
    int fd_arg = -1;
    
    int c;

    while ((c = getopt(argc, argv, "vdskNgqCSx:o:c:b:t:f:")) != -1) {
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
                    const off_t bsize = (off_t)atoi(optarg) * oneM;
                    if (bsize > 0) {
                        options.sizebound = bsize;
                    } else {
                        Usage("invalid bound");
                    }
                } else {
                    Usage("no bound specified");
                }
                break;
            case 'v':   /* verbose output */
                options.verbose++;
                break;
            case 'x':
                if (optarg) {
                    if (strcmp(optarg, "stack") == 0) {
                        options.content = CONTENT_STACK;
                    } else if (strcmp(optarg, "compact") == 0) {
                        options.content = CONTENT_COMPACT;
                    } else if (strcmp(optarg, "full") == 0) {
                        // also the historic default
                        options.content = CONTENT_FULL;
                    } else {
                        Usage("invalid content type");
                    }
                } else {
                    Usage("no content type specified");
                }
                break;
                
                /*
                 * dev and debugging help
                 */
#ifdef CONFIG_DEBUG
            case 'd':   /* debugging */
                options.debug++;
                options.verbose++;
                options.preserve++;
                break;
#endif
                /*
                 * Experimental / SPI options
                 */
            case 'f': {  /* write the core file to an fd (not std{in,out,err}) */
                struct stat st;
                if ((fd_arg = atoi(optarg)) < 3 ||
                    fstat(fd_arg, &st) == -1 || !S_ISREG(st.st_mode)) {
                    Usage("invalid fd: %s", optarg);
                }
                break;
            }
            case 'q':
                options.quiet++;
                break;
            case 'C':   /* forcibly corpsify rather than suspend */
                options.corpsify++;
                break;
            case 'g':   /* compress whole core file via gzip -1 */
                options.gzip++;
                options.stream++;
                break;
            case 'S':   /* use streaming writes instead of pwrites */
                options.stream++;
                break;
            case 't':	/* set the F_NOCACHE threshold */
                if (NULL != optarg) {
                    size_t tsize = atoi(optarg) * oneK;
                    if (tsize > 0) {
                        options.ncthresh = tsize;
                    } else {
                        Usage("invalid nc threshold");
                    }
                } else {
                    Usage("no threshold specified");
                }
                break;
            case 'N':
                options.notes = true;
                break;
            default:
                usage();
                if (corefmt) {
                    free(corefmt);
                }
                if (corefname) {
                    free(corefname);
                }
                return EX_USAGE;
        }
    }


    opt = &options;

    /*
     * All usage-related errors go to stderr directly.
     * All verbose (-v) output goes to stdout directly via printf.
     * The disposition of os_log*() messages depends on
     * the "quiet" flag.
     */
    if (!opt->quiet) {
        static os_log_hook_t chain = NULL;
        chain = os_log_set_hook(OS_LOG_TYPE_DEFAULT,
            ^(os_log_type_t type, os_log_message_t msg) {
            if (chain) {
                chain(type, msg);
            }
            char *message = os_log_copy_message_string(msg);
            switch (type) {
                case OS_LOG_TYPE_DEBUG:
                case OS_LOG_TYPE_INFO:
                case OS_LOG_TYPE_DEFAULT:
                    printf("%s\n", message);
                    break;
                case OS_LOG_TYPE_ERROR:
                case OS_LOG_TYPE_FAULT:
                    fprintf(stderr, "%s: %s\n", pgm, message);
                    break;
            }
            free(message);
        });
    }

    if (optind < argc-1) {
        Usage("too many arguments");
    }
    if (optind >= argc) {
        Usage("no process specified");
    }
    if (NULL != corefname && NULL != corefmt) {
        Usage("specify only one of -o and -c");
    }
    if (opt->sizebound) {
        if (opt->stream) {
            Usage("specify only one of -b and -S");
        }
        if (opt->gzip) {
            Usage("specify only one of -b and -g");
        }
    }
    if (opt->notes) {
        if (opt->gzip) {
            /* gzip requires streaming */
            Usage("specify only one of -N and -g");
        }
        if (opt->stream) {
            Usage("specify only one of -N and -S");
        }
    }
    if (opt->content != CONTENT_FULL) {
        /*
         * XXX This really should work.
         * Streaming and gzipped core files were originally added as
         * for DriverKit. Why don't other types of content work?
         * The issue is that the streaming core files can't write out
         * the requisite "all image infos", "addrable bits" and optional
         * "process metadata" notes out, as they currently depend on pwrite.
         * Which means that only CONTENT_FULL streamed core files work
         * with lldb (since they're just like kernel-generated core files).
         * Ideally, we should make the notes be written without requiring
         * pwrite ... though is anyone actually actively this capability,
         * and if they are, would CONTENT_COMPACT ones actually be useful?
         */
        if (opt->gzip) {
            /* gzip requires streaming */
            Usage("gzip requires full core content");
        }
        if (opt->stream) {
            Usage("streaming requires full core content");
        }
    }
    if (fd_arg != -1 && corefname != NULL) {
        Usage("specify only one of -f and -o");
    }
    setpageshift();

    if (opt->ncthresh < ((vm_offset_t)1 << pageshift_host)) {
        Usage("threshold %lu less than host pagesize", opt->ncthresh);
    }

    const pid_t apid = atoi(argv[optind]);
	pid_t pid = apid;
	mach_port_t corpse = MACH_PORT_NULL;
	task_t task = TASK_NULL;
	kern_return_t ret;

	if (0 == apid) {
		/* look for task or corpse - dead or alive */
		mach_port_array_t parray = NULL;
		mach_msg_type_number_t pcount = 0;
		ret = mach_ports_lookup(mach_task_self(), &parray, &pcount);
		if (KERN_SUCCESS == ret && pcount > 0) {
			task_t ttask = parray[0];
			mig_deallocate((vm_address_t)parray, pcount * sizeof (*parray));
			pid_t tpid = 0;
			ret = pid_for_task(ttask, &tpid);
			if (KERN_SUCCESS == ret && tpid != getpid()) {
				pid = tpid;
				if (task_port_is_corpse(ttask)) {
					corpse = ttask;
				} else {
					task = ttask;
				}
			}
		}
	}

    if (pid < 1 || getpid() == pid) {
        os_log_error(glog, "invalid pid: %d", pid);
        exit(EX_DATAERR);
    }
    if (0 == apid && MACH_PORT_NULL == corpse && MACH_PORT_NULL == task) {
        os_log_error(glog, "missing or bad task/corpse from parent");
        exit(EX_DATAERR);
    }

	const struct proc_bsdinfo *pbi = NULL;
	const int rc = kill(pid, 0);

	if (rc == 0) {
		/* process or corpse that may respond to signals */
		pbi = get_bsdinfo(pid);
	}

	if (rc == 0 && pbi != NULL) {
		/* process or corpse that responds to signals */

		/* make our data model match the data model of the target */
        if (-1 == reexec_to_match_lp64ness(pbi->pbi_flags & PROC_FLAG_LP64)) {
            os_log_error(glog, "cannot match data model of %d %{darwin.errno}d",
                pid, errno);
            exit(EX_CONFIG);
        }
        if (!proc_same_data_model(pbi)) {
            os_log_error(glog, "did not match data model of %d", pid);
            exit(EX_CONFIG);
        }
		if (pbi->pbi_ruid != pbi->pbi_svuid ||
            pbi->pbi_rgid != pbi->pbi_svgid) {
            os_log_error(glog, "not dumping set-id process %d", pid);
            exit(EX_NOPERM);
        }
        if (NULL == corefname && fd_arg == -1) {
            corefname = make_gcore_path(&corefmt, pbi->pbi_pid,
                pbi->pbi_uid, pbi->pbi_name[0] ? pbi->pbi_name : pbi->pbi_comm);
        }
		if (MACH_PORT_NULL == corpse && MACH_PORT_NULL == task &&
            -1 == task_read_for_pid(mach_task_self(), pid, &task)) {
            os_log_error(glog, "insufficient privilege: %{public}s",
                strerror(errno));
            exit(EX_NOPERM);
		}

		/*
		 * Have either the corpse port or the task port so adopt the
		 * credentials of the target process, *before* opening the
		 * core file, and analyzing the address space.
		 *
		 * If we are unable to match the target credentials, bail out.
		 */
		change_credentials(pbi->pbi_uid, pbi->pbi_gid);
	} else {
		if (MACH_PORT_NULL == corpse) {
			if (rc == 0) {
                os_log_error(glog, "cannot get process info for %d", pid);
                exit(EX_OSERR);
			}
			os_log_error(glog, "pid %d: %{public}s", pid, strerror(errno));
            exit(EX_DATAERR);
        }
		/* a corpse with no live process backing it */

		assert(0 == apid && TASK_NULL == task);

		task_flags_info_data_t tfid;
		mach_msg_type_number_t count = TASK_FLAGS_INFO_COUNT;
		ret = task_info(corpse, TASK_FLAGS_INFO, (task_info_t)&tfid, &count);
        if (KERN_SUCCESS != ret) {
            err_mach(ret, NULL, "task_info");
        }
        if (!task_same_data_model(&tfid)) {
            os_log_error(glog, "data model mismatch for target corpse");
            exit(EX_OSERR);
        }
        if (opt->suspend || opt->corpsify) {
            Usage("cannot use -s or -C option with a corpse");
        }

        uid_t cuid = -2;
        gid_t cgid = -2;
        pid_t cpid = pid;
        char cname[MAXCOMLEN + 1] = "corpse";

        // Extract pid, uid, gid and name so the core
        // file gets the right name and ownership

        mach_vm_address_t blob;
        mach_vm_size_t blob_size;
        const kern_return_t kr = task_map_corpse_info_64(mach_task_self(),
            corpse, &blob, &blob_size);
        if (kr != KERN_SUCCESS) {
            err_mach(ret, NULL, "task_map_corpse_info_64");
        } else {
            kcdata_iter_t iter = kcdata_iter((void *)blob, (unsigned long)blob_size);
            KCDATA_ITER_FOREACH(iter) {
                const void *data = kcdata_iter_payload(iter);
                switch (kcdata_iter_type(iter)) {
                    case TASK_CRASHINFO_PID:
                        cpid = *(pid_t *)data;
                        break;
                    case TASK_CRASHINFO_UID:
                        cuid = *(uid_t *)data;
                        break;
                    case TASK_CRASHINFO_GID:
                        cgid = *(gid_t *)data;
                        break;
                    case TASK_CRASHINFO_PROC_NAME:  // p->p_comm in reality
                        memcpy(cname, data, MAXCOMLEN);
                        cname[sizeof(cname) - 1] = '\0';
                        break;
                    default:
                        break;
                }
            }
            if (KCDATA_ITER_FOREACH_FAILED(iter)) {
                os_log_error(glog, "failed to iterate kcdata for corpse");
            }
            mach_vm_deallocate(mach_task_self(), blob, blob_size);
        }

        if (NULL == corefname && fd_arg == -1) {
            corefname = make_gcore_path(&corefmt, cpid, cuid, cname);
        }

        /*
         * Adopt the credentials of the target process, *before* opening the
         * core file, and analyzing the address space.
         *
         * If we are unable to match the target change_credentials() will exit.
         */
        change_credentials(cuid, cgid);
    }

    struct stat cst;
    char *coretname = NULL;
    const int fd = (fd_arg != -1) ? fd_arg : openout(corefname, &coretname, &cst);
    int outfd = fd;
    pid_t gpid = -1;

    if (opt->gzip) {
        /*
         * spawn a gzip instance reading from 'outfd' and writing to 'fd'
         */
        assert(opt->stream);
        gpid = run_gzip(&outfd, fd);
    }

    if (opt->verbose) {
        printf("Dumping core ");
        if (OPTIONS_DEBUG(opt, 1)) {
            char *type = "full";
            switch (opt->content) {
                case CONTENT_STACK:
                    type = "stack";
                    break;
                case CONTENT_COMPACT:
                    type = "compact";
                    break;
                case CONTENT_FULL:
                    break;
            }
            printf("(%s", type);
            if (0 != opt->sizebound) {
                hsize_str_t hstr;
                printf(", <= %s", str_hsize(hstr, opt->sizebound));
            }
            if (opt->gzip) {
                printf(", gzip");
            }
            printf(") ");
        }
        if (corefname) {
            printf("for pid %d to %s\n", pid, corefname);
        } else {
            printf("for pid %d\n", pid);
        }
    }

    int ecode;

	if (MACH_PORT_NULL == corpse) {
		assert(TASK_NULL != task);

		/*
		 * The "traditional" way to capture a consistent core dump is to
		 * suspend the process while examining it and writing it out.
		 * Yet suspending a large process for a long time can have
		 * unpleasant side-effects.  Alternatively dumping from the live
		 * process can lead to an inconsistent state in the core file.
		 *
		 * Instead ask xnu to create a 'corpse' - the process is transiently
		 * suspended while a COW snapshot of the address space is constructed
		 * in the kernel - and dump from that.  This vastly reduces the suspend
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
		int badcorpse_is_fatal = 1; /* default: failure to create is an error */

		if (!opt->suspend && !opt->corpsify) {
			/* try a corpse dump, else dump the live process */
			badcorpse_is_fatal = 0;
		} else if (opt->suspend) {
			trycorpse = opt->corpsify;
			/* if suspended anyway, ignore corpse-creation errors */
			badcorpse_is_fatal = 0;
		}

        if (opt->suspend) {
            task_suspend(task);
        }
		if (trycorpse) {
			/*
			 * Create a corpse from the image before dumping it
			 */
			ret = task_generate_corpse(task, &corpse);
			switch (ret) {
				case KERN_SUCCESS:
                    if (OPTIONS_DEBUG(opt, 1)) {
                        os_log(glog, "Process snapshot generated on port %x, task %x",
                            corpse, task);
                    }
					ecode = coredump(corpse, outfd, pbi);
					mach_port_deallocate(mach_task_self(), corpse);
					break;
				default:
					if (badcorpse_is_fatal || opt->verbose) {
						os_log_error(glog, "failed to snapshot pid %d: %{public}s",
							  pid, mach_error_string(ret));
						if (badcorpse_is_fatal) {
							ecode = KERN_RESOURCE_SHORTAGE == ret ? EX_TEMPFAIL : EX_OSERR;
							goto out;
						}
					}
					ecode = coredump(task, outfd, pbi);
					break;
			}
		} else {
			/*
			 * Examine the task directly
			 */
			ecode = coredump(task, outfd, pbi);
		}

	out:
        if (opt->suspend) {
            task_resume(task);
        }
	} else {
		/*
		 * Handed a corpse by our parent.
		 */
		ecode = coredump(corpse, outfd, pbi);
		mach_port_deallocate(mach_task_self(), corpse);
	}

    if (gpid > 0) {
        assert(opt->gzip && opt->stream);
        assert(outfd != fd);
        close(outfd);
        if (wait_for_gzip(gpid) != 0) {
            os_log_error(glog, "failed to compress core dump stream");
            exit(EX_OSERR);
        }
    }

	ecode = closeout(fd, ecode, corefname, coretname, &cst);
    if (ecode) {
        os_log_error(glog, "failed to dump core for pid %d", pid);
    }
    return ecode;
}
