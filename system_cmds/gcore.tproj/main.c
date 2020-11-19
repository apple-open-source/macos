/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "corefile.h"
#include "vanilla.h"
#include "sparse.h"
#include "convert.h"

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
        } else
            return 0;
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

static char *
make_gcore_path(char **corefmtp, pid_t pid, uid_t uid, const char *nm)
{
	char *corefmt = *corefmtp;
	if (NULL == corefmt) {
		const char defcore[] = "%N-%P-%T";
		if (NULL == (corefmt = kern_corefile()))
			corefmt = strdup(defcore);
		else {
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
			if (OPTIONS_DEBUG(opt, 3))
				printf("corefmt '%s'\n", corefmt);
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
		(getuid() != uid && -1 == setuid(uid)))
		errc(EX_NOPERM, errno, "insufficient privilege");
	if (uid != getuid() || gid != getgid())
		err(EX_OSERR, "wrong credentials");
}

static int
openout(const char *corefname, char **coretname, struct stat *st)
{
	const int tfd = open(corefname, O_WRONLY);
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
			const int fd = open(tnm, O_WRONLY | O_CREAT | O_TRUNC, 0600);
			if (-1 == fd || -1 == fstat(fd, st))
				errc(EX_CANTCREAT, errno, "%s", tnm);
			if (!S_ISREG(st->st_mode) || 1 != st->st_nlink)
				errx(EX_CANTCREAT, "%s: invalid attributes", tnm);
			*coretname = tnm;
			return fd;
		} else
			errc(EX_CANTCREAT, errno, "%s", corefname);
	} else if (-1 == fstat(tfd, st)) {
		close(tfd);
		errx(EX_CANTCREAT, "%s: fstat", corefname);
	} else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
				/*
				 * Write dump to a device, no rename!
				 */
				*coretname = NULL;
				return tfd;
	} else {
		close(tfd);
		errc(EX_CANTCREAT, EEXIST, "%s", corefname);
	}
}

static int
closeout(int fd, int ecode, char *corefname, char *coretname, const struct stat *st)
{
	if (0 != ecode && !opt->preserve && S_ISREG(st->st_mode))
		ftruncate(fd, 0); // limit large file clutter
	if (0 == ecode && S_ISREG(st->st_mode))
		fchmod(fd, 0400); // protect core files
	if (-1 == close(fd)) {
		warnc(errno, "%s: close", coretname ? coretname : corefname);
		ecode = EX_OSERR;
	}
	if (NULL != coretname) {
		if (0 == ecode && -1 == rename(coretname, corefname)) {
			warnc(errno, "cannot rename %s to %s", coretname, corefname);
			ecode = EX_NOPERM;
		}
		free(coretname);
	}
	if (corefname)
		free(corefname);
	return ecode;
}

const char *pgm;
const struct options *opt;

static const size_t oneK = 1024;
static const size_t oneM = oneK * oneK;

#define LARGEST_CHUNKSIZE               INT32_MAX
#define DEFAULT_COMPRESSION_CHUNKSIZE	(16 * oneM)
#define DEFAULT_NC_THRESHOLD			(17 * oneK)

static struct options options = {
	.corpsify = 0,
	.suspend = 0,
	.preserve = 0,
	.verbose = 0,
#ifdef CONFIG_DEBUG
	.debug = 0,
#endif
	.extended = 0,
	.sizebound = 0,
	.chunksize = 0,
	.calgorithm = COMPRESSION_LZFSE,
	.ncthresh = DEFAULT_NC_THRESHOLD,
	.dsymforuuid = 0,
};

static int
gcore_main(int argc, char *const *argv)
{
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
                    "usage:\t%s [-s] [-v] [[-o file] | [-c pathfmt ]] [-b size] "
#if DEBUG
#ifdef CONFIG_DEBUG
                    "[-d] "
#endif
					"[-x] [-C] "
                    "[-Z compression-options] "
					"[-t size] "
                    "[-F] "
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

    int c;
    char *sopts, *value;

    while ((c = getopt(argc, argv, "vdsxCFZ:o:c:b:t:")) != -1) {
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
#ifdef CONFIG_DEBUG
            case 'd':   /* debugging */
                options.debug++;
                options.verbose++;
                options.preserve++;
                break;
#endif
                /*
                 * Remaining options are experimental and/or
                 * affect the content of the core file
                 */
            case 'x':	/* write extended format (small) core files */
                options.extended++;
                options.chunksize = DEFAULT_COMPRESSION_CHUNKSIZE;
                break;
            case 'C':   /* forcibly corpsify rather than suspend */
                options.corpsify++;
                break;
            case 'Z':   /* control compression options */
                /*
                 * Only LZFSE and LZ4 seem practical.
                 * (Default to LZ4 compression when the
                 * process is suspended, LZFSE when corpsed?)
                 */
                if (0 == options.extended)
					errx(EX_USAGE, "illegal flag combination");
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
                                options.calgorithm = COMPRESSION_LZ4;
                            else if (strcmp(value, "zlib") == 0)
                                options.calgorithm = COMPRESSION_ZLIB;
                            else if (strcmp(value, "lzma") == 0)
                                options.calgorithm = COMPRESSION_LZMA;
                            else if (strcmp(value, "lzfse") == 0)
                                options.calgorithm = COMPRESSION_LZFSE;
                            else
                                errx(EX_USAGE, "unknown algorithm '%s'"
                                     " for %s suboption",
                                     value, zoptkeys[ZOPT_ALG]);
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
			case 't':	/* set the F_NOCACHE threshold */
				if (NULL != optarg) {
					size_t tsize = atoi(optarg) * oneK;
					if (tsize > 0)
						options.ncthresh = tsize;
					else
						errx(EX_USAGE, "invalid nc threshold");
				} else
					errx(EX_USAGE, "no threshold specified");
				break;
            case 'F':   /* maximize filerefs */
                options.allfilerefs++;
                break;
            default:
                errx(EX_USAGE, "unknown flag");
        }
    }

	if (optind == argc)
		errx(EX_USAGE, "no pid specified");
	if (optind < argc-1)
		errx(EX_USAGE, "too many arguments");

	opt = &options;
    if (NULL != corefname && NULL != corefmt)
        errx(EX_USAGE, "specify only one of -o and -c");
    if (!opt->extended && opt->allfilerefs)
        errx(EX_USAGE, "unknown flag");

    setpageshift();

	if (opt->ncthresh < ((vm_offset_t)1 << pageshift_host))
		errx(EX_USAGE, "threshold %lu less than host pagesize", opt->ncthresh);

    const pid_t apid = atoi(argv[optind]);
	pid_t pid = apid;
	mach_port_t corpse = MACH_PORT_NULL;
	kern_return_t ret;

	if (0 == apid) {
		/* look for corpse - dead or alive */
		mach_port_array_t parray = NULL;
		mach_msg_type_number_t pcount = 0;
		ret = mach_ports_lookup(mach_task_self(), &parray, &pcount);
		if (KERN_SUCCESS == ret && pcount > 0) {
			task_t tcorpse = parray[0];
			mig_deallocate((vm_address_t)parray, pcount * sizeof (*parray));
			pid_t tpid = 0;
			ret = pid_for_task(tcorpse, &tpid);
			if (KERN_SUCCESS == ret && tpid != getpid()) {
				corpse = tcorpse;
				pid = tpid;
			}
		}
	}

	if (pid < 1 || getpid() == pid)
		errx(EX_DATAERR, "invalid pid: %d", pid);

	if (0 == apid && MACH_PORT_NULL == corpse)
		errx(EX_DATAERR, "missing or bad corpse from parent");

	task_t task = TASK_NULL;
	const struct proc_bsdinfo *pbi = NULL;
	const int rc = kill(pid, 0);

	if (rc == 0) {
		/* process or corpse that may respond to signals */
		pbi = get_bsdinfo(pid);
	}

	if (rc == 0 && pbi != NULL) {
		/* process or corpse that responds to signals */

		/* make our data model match the data model of the target */
		if (-1 == reexec_to_match_lp64ness(pbi->pbi_flags & PROC_FLAG_LP64))
			errc(1, errno, "cannot match data model of %d", pid);

		if (!proc_same_data_model(pbi))
			errx(EX_OSERR, "cannot match data model of %d", pid);

		if (pbi->pbi_ruid != pbi->pbi_svuid ||
			pbi->pbi_rgid != pbi->pbi_svgid)
			errx(EX_NOPERM, "pid %d - not dumping a set-id process", pid);

		if (NULL == corefname)
			corefname = make_gcore_path(&corefmt, pbi->pbi_pid, pbi->pbi_uid, pbi->pbi_name[0] ? pbi->pbi_name : pbi->pbi_comm);

		if (MACH_PORT_NULL == corpse) {
			ret = task_for_pid(mach_task_self(), pid, &task);
			if (KERN_SUCCESS != ret) {
				if (KERN_FAILURE == ret)
					errx(EX_NOPERM, "insufficient privilege");
				else
					errx(EX_NOPERM, "task_for_pid: %s", mach_error_string(ret));
			}
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
				errx(EX_OSERR, "cannot get process info for %d", pid);
			}
			switch (errno) {
				case ESRCH:
					errc(EX_DATAERR, errno, "no process with pid %d", pid);
				default:
					errc(EX_DATAERR, errno, "pid %d", pid);
			}
		}
		/* a corpse with no live process backing it */

		assert(0 == apid && TASK_NULL == task);

		task_flags_info_data_t tfid;
		mach_msg_type_number_t count = TASK_FLAGS_INFO_COUNT;
		ret = task_info(corpse, TASK_FLAGS_INFO, (task_info_t)&tfid, &count);
		if (KERN_SUCCESS != ret)
			err_mach(ret, NULL, "task_info");
		if (!task_same_data_model(&tfid))
			errx(EX_OSERR, "data model mismatch for target corpse");

		if (opt->suspend || opt->corpsify)
			errx(EX_USAGE, "cannot use -s or -C option with a corpse");
		if (NULL != corefmt)
			errx(EX_USAGE, "cannot use -c with a corpse");
		if (NULL == corefname)
			corefname = make_gcore_path(&corefmt, pid, -2, "corpse");

		/*
		 * Only have a corpse, thus no process credentials.
		 * Switch to nobody.
		 */
		change_credentials(-2, -2);
	}

	struct stat cst;
	char *coretname = NULL;
	const int fd = openout(corefname, &coretname, &cst);

	if (opt->verbose) {
        printf("Dumping core ");
        if (OPTIONS_DEBUG(opt, 1)) {
            printf("(%s", opt->extended ? "extended" : "vanilla");
            if (0 != opt->sizebound) {
                hsize_str_t hstr;
                printf(", <= %s", str_hsize(hstr, opt->sizebound));
            }
            printf(") ");
        }
		printf("for pid %d to %s\n", pid, corefname);
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
		int badcorpse_is_fatal = 1; /* default: failure to create is an error */

		if (!opt->suspend && !opt->corpsify) {
			/* try a corpse dump, else dump the live process */
			badcorpse_is_fatal = 0;
		} else if (opt->suspend) {
			trycorpse = opt->corpsify;
			/* if suspended anyway, ignore corpse-creation errors */
			badcorpse_is_fatal = 0;
		}

		if (opt->suspend)
			task_suspend(task);

		if (trycorpse) {
			/*
			 * Create a corpse from the image before dumping it
			 */
			ret = task_generate_corpse(task, &corpse);
			switch (ret) {
				case KERN_SUCCESS:
					if (OPTIONS_DEBUG(opt, 1))
						printf("Corpse generated on port %x, task %x\n",
							   corpse, task);
					ecode = coredump(corpse, fd, pbi);
					mach_port_deallocate(mach_task_self(), corpse);
					break;
				default:
					if (badcorpse_is_fatal || opt->verbose) {
						warnx("failed to snapshot pid %d: %s\n",
							  pid, mach_error_string(ret));
						if (badcorpse_is_fatal) {
							ecode = KERN_RESOURCE_SHORTAGE == ret ? EX_TEMPFAIL : EX_OSERR;
							goto out;
						}
					}
					ecode = coredump(task, fd, pbi);
					break;
			}
		} else {
			/*
			 * Examine the task directly
			 */
			ecode = coredump(task, fd, pbi);
		}

	out:
		if (opt->suspend)
			task_resume(task);
	} else {
		/*
		 * Handed a corpse by our parent.
		 */
		ecode = coredump(corpse, fd, pbi);
		mach_port_deallocate(mach_task_self(), corpse);
	}

	ecode = closeout(fd, ecode, corefname, coretname, &cst);
	if (ecode)
		errx(ecode, "failed to dump core for pid %d", pid);
    return 0;
}

#if defined(CONFIG_GCORE_FREF) || defined(CONFIG_GCORE_MAP) || defined(GCONFIG_GCORE_CONV)

static int
getcorefd(const char *infile)
{
	const int fd = open(infile, O_RDONLY | O_CLOEXEC);
	if (-1 == fd)
		errc(EX_DATAERR, errno, "cannot open %s", infile);

	struct mach_header mh;
	if (-1 == pread(fd, &mh, sizeof (mh), 0))
		errc(EX_OSERR, errno, "cannot read mach header from %s", infile);

	static const char cant_match_data_model[] = "cannot match the data model of %s";

	if (-1 == reexec_to_match_lp64ness(MH_MAGIC_64 == mh.magic))
		errc(1, errno, cant_match_data_model, infile);

	if (NATIVE_MH_MAGIC != mh.magic)
		errx(EX_OSERR, cant_match_data_model, infile);
	if (MH_CORE != mh.filetype)
		errx(EX_DATAERR, "%s is not a mach core file", infile);
	return fd;
}

#endif

#ifdef CONFIG_GCORE_FREF

static int
gcore_fref_main(int argc, char *argv[])
{
	err_set_exit_b(^(int eval) {
		if (EX_USAGE == eval) {
			fprintf(stderr, "usage:\t%s %s corefile\n", pgm, argv[1]);
		}
	});
	if (2 == argc)
		errx(EX_USAGE, "no input corefile");
	if (argc > 3)
		errx(EX_USAGE, "too many arguments");
	opt = &options;
	return gcore_fref(getcorefd(argv[2]));
}

#endif /* CONFIG_GCORE_FREF */

#ifdef CONFIG_GCORE_MAP

static int
gcore_map_main(int argc, char *argv[])
{
	err_set_exit_b(^(int eval) {
		if (EX_USAGE == eval) {
			fprintf(stderr, "usage:\t%s %s corefile\n", pgm, argv[1]);
		}
	});
	if (2 == argc)
		errx(EX_USAGE, "no input corefile");
	if (argc > 3)
		errx(EX_USAGE, "too many arguments");
	opt = &options;
	return gcore_map(getcorefd(argv[2]));
}

#endif

#ifdef CONFIG_GCORE_CONV

static int
gcore_conv_main(int argc, char *argv[])
{
	err_set_exit_b(^(int eval) {
		if (EX_USAGE == eval)
			fprintf(stderr,
				"usage:\t%s %s [-v] [-L searchpath] [-z] [-s] incore outcore\n", pgm, argv[1]);
	});

	char *searchpath = NULL;
	bool zf = false;

	int c;
	optind = 2;
	while ((c = getopt(argc, argv, "dzvL:s")) != -1) {
		switch (c) {
				/*
				 * likely documented options
				 */
			case 'L':
				searchpath = strdup(optarg);
				break;
			case 'z':
				zf = true;
				break;
			case 'v':
				options.verbose++;
				break;
			case 's':
				options.dsymforuuid++;
				break;
				/*
				 * dev and debugging help
				 */
#ifdef CONFIG_DEBUG
			case 'd':
				options.debug++;
				options.verbose++;
				options.preserve++;
				break;
#endif
			default:
				errx(EX_USAGE, "unknown flag");
		}
	}
	if (optind == argc)
		errx(EX_USAGE, "no input corefile");
    if (optind == argc - 1)
        errx(EX_USAGE, "no output corefile");
    if (optind < argc - 2)
        errx(EX_USAGE, "too many arguments");

    const char *incore = argv[optind];
    char *corefname = strdup(argv[optind+1]);

	opt = &options;

	setpageshift();

	if (opt->ncthresh < ((vm_offset_t)1 << pageshift_host))
		errx(EX_USAGE, "threshold %lu less than host pagesize", opt->ncthresh);

	const int infd = getcorefd(incore);
	struct stat cst;
	char *coretname = NULL;
	const int fd = openout(corefname, &coretname, &cst);
	int ecode = gcore_conv(infd, searchpath, zf, fd);
	ecode = closeout(fd, ecode, corefname, coretname, &cst);
	if (ecode)
		errx(ecode, "failed to convert core file successfully");
	return 0;
}
#endif

int
main(int argc, char *argv[])
{
	if (NULL == (pgm = strrchr(*argv, '/')))
		pgm = *argv;
	else
		pgm++;
#ifdef CONFIG_GCORE_FREF
	if (argc > 1 && 0 == strcmp(argv[1], "fref")) {
		return gcore_fref_main(argc, argv);
	}
#endif
#ifdef CONFIG_GCORE_MAP
	if (argc > 1 && 0 == strcmp(argv[1], "map")) {
		return gcore_map_main(argc, argv);
	}
#endif
#ifdef CONFIG_GCORE_CONV
	if (argc > 1 && 0 == strcmp(argv[1], "conv")) {
		return gcore_conv_main(argc, argv);
	}
#endif
	return gcore_main(argc, argv);
}
