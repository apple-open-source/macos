/*
 * main.c - common main function for lsof
 *
 * V. Abell, Purdue University
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: main.c,v 1.38 2001/11/01 20:19:44 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local definitions
 */

static int GObk[] = { 1, 1 };		/* option backspace values */
static char GOp;			/* option prefix -- '+' or '-' */
static char *GOv = (char *)NULL;	/* option `:' value pointer */
static int GOx1 = 1;			/* first opt[][] index */
static int GOx2 = 0;			/* second opt[][] index */


_PROTOTYPE(static int GetOpt,(int ct, char *opt[], char *rules, int *err));
_PROTOTYPE(static char *sv_fmt_str,(char *f));


/*
 * main() - main function for lsof
 */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c, i, n, rv;
	char *cp;
	int err = 0;
	int ev = 0;
	int fh = 0;
	long l;
	MALLOC_S len;
	struct lfile *lf;
	struct nwad *np, *npn;
	char options[128];
	int rc = 0;
	struct stat sb;
	struct sfile *sfp;
	struct lproc **slp = (struct lproc **)NULL;
	int sp = 0;
	struct str_lst *str;
	int version = 0;
/*
 * Save program name.
 */
	if ((Pn = strrchr(argv[0], '/')))
	    Pn++;
	else
	    Pn = argv[0];

#if	defined(HASSETLOCALE)
/*
 * Set locale to environment's definition.
 */
	(void) setlocale(LC_CTYPE, "");
#endif	/* defined(HASSETLOCALE) */

/*
 * Common initialization.
 */
	Mypid = getpid();
	if ((Mygid = (gid_t)getgid()) != getegid())
	    Setgid = 1;
	if ((Myuid = (uid_t)getuid()) && !geteuid())
	    Setuidroot = 1;
	if (!(Namech = (char *)malloc(MAXPATHLEN + 1))) {
	    (void) fprintf(stderr, "%s: no space for name buffer\n", Pn);
	    Exit(1);
	}
	Namechl = (size_t)(MAXPATHLEN + 1);
/*
 * Create option mask.
 */
	(void) snpf(options, sizeof(options),
	    "?a%sbc:D:d:%sf:F:g:hi:%slL:%sMnNo:Op:Pr:%ssS:tT:u:UvVw%s",

#if	defined(HAS_AFS) && defined(HASAOPT)
	    "A:",
#else	/* !defined(HAS_AFS) || !defined(HASAOPT) */
	    "",
#endif	/* defined(HAS_AFS) && defined(HASAOPT) */

#if	defined(HASNCACHE)
	    "C",
#else	/* !defined(HASNCACHE) */
	    "",
#endif	/* defined(HASNCACHE) */

#if	defined(HASKOPT)
	    "k:",
#else	/* !defined(HASKOPT) */
	    "",
#endif	/* defined(HASKOPT) */

#if	defined(HASMOPT)
	    "m:",
#else	/* !defined(HASMOPT) */
	    "",
#endif	/* defined(HASMOPT) */

#if	defined(HASPPID)
	    "R",
#else	/* !defined(HASPPID) */
	    "",
#endif	/* defined(HASPPID) */

#if	defined(HASXOPT)
# if	defined(HASXOPT_ROOT)
	    (Myuid == 0) ? "X" : ""
# else	/* !defined(HASXOPT_ROOT) */
	    "X"
# endif	/* defined(HASXOPT_ROOT) */
#else	/* !defined(HASXOPT) */
	    ""
#endif	/* defined(HASXOPT) */

	    );
/*
 * Loop through options.
 */
	while ((c = GetOpt(argc, argv, options, &rv)) != EOF) {
	    if (rv) {
		err = 1;
		continue;
	    }
	    switch(c) {
	    case 'a':
		Fand = 1;
		break;

#if	defined(HAS_AFS) && defined(HASAOPT)
	    case 'A':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    (void) fprintf(stderr, "%s: -A not followed by path\n", Pn);
		    err = 1;
		} else
		    AFSApath = GOv;
		break;
#endif	/* defined(HAS_AFS) && defined(HASAOPT) */

	    case 'b':
		Fblock = 1;
		break;
	    case 'c':
		if (GOv && (*GOv == '/')) {
		    if (enter_cmd_rx(GOv))
			err = 1;
		} else {
		    if (enter_str_lst("c", GOv, &Cmdl))
			err = 1;
		}
		break;

#if	defined(HASNCACHE)
	    case 'C':
		Fncache = (GOp == '-') ? 0 : 1;
		break;
#endif	/* defined(HASNCACHE) */

	    case 'd':
		if (GOp == '+') {
		    if (enter_dir(GOv, 0))
			err = 1;
		    else
			Selflags |= SELNM;
		} else {
		    if (enter_fd(GOv))
			err = 1;
		}
		break;
	    case 'D':
		if (GOp == '+') {
		    if (enter_dir(GOv, 1))
			err = 1;
		    else
			Selflags |= SELNM;
		} else {

#if	defined(HASDCACHE)
		    if (ctrl_dcache(GOv))
			err = 1;
#else	/* !defined(HASDCACHE) */
		    (void) fprintf(stderr, "%s: unsupported option: -D\n", Pn);
		    err = 1;
#endif	/* defined(HASDCACHE) */

		}
		break;
	    case 'f':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    Ffilesys = (GOp == '+') ? 2 : 1;
		    break;
		}

#if	defined(HASFSTRUCT)
		for (; *GOv; GOv++) {
		    switch (*GOv) {
		    case 'c':
		    case 'C':
			if (GOp == '+')
			    Fsv |= FSV_CT;
			else
			    Fsv &= (unsigned char)~FSV_CT;
			break;
		    case 'f':
		    case 'F':
			if (GOp == '+')
			    Fsv |= FSV_FA;
			else
			    Fsv &= (unsigned char)~FSV_FA;
			break;
		    case 'g':
		    case 'G':
			if (GOp == '+')
			    Fsv |= FSV_FG;
			else
			    Fsv &= (unsigned char)~FSV_FG;
			FsvFlagX = (*GOv == 'G') ? 1 : 0;
			break;
		    case 'n':
		    case 'N':
			if (GOp == '+')
			    Fsv |= FSV_NI;
			else
			    Fsv &= (unsigned char)~FSV_NI;
			break;
		    default:
			(void) fprintf(stderr,
			    "%s: unknown file struct option: %c\n", Pn, *GOv);
			err++;
		    }
		}
#else	/* !defined(HASFSTRUCT) */
		(void) fprintf(stderr,
		    "%s: unknown string for %cf: %s\n", Pn, GOp, GOv);
		err++;
#endif	/* defined(HASFSTRUCT) */

		break;
	    case 'F':
		if (!GOv || *GOv == '-' || *GOv == '+'
		||  strcmp(GOv, "0") == 0) {
		    if (GOv) {
			if (*GOv == '-' || *GOv == '+') {
			    GOx1 = GObk[0];
			    GOx2 = GObk[1];
			} else if (*GOv == '0')
			    Terminator = '\0';
		    }
		    for (i = 0; FieldSel[i].nm; i++) {

#if	!defined(HASPPID)
			if (FieldSel[i].id == LSOF_FID_PPID)
			    continue;
#endif	/* !defined(HASPPID) */

#if	!defined(HASFSTRUCT)
			if (FieldSel[i].id == LSOF_FID_CT
			||  FieldSel[i].id == LSOF_FID_FA
			||  FieldSel[i].id == LSOF_FID_FG
			||  FieldSel[i].id == LSOF_FID_NI)
			    continue;
#endif	/* !defined(HASFSTRUCT) */

			if (FieldSel[i].id == LSOF_FID_RDEV)
			    continue;	/* for compatibility */
			FieldSel[i].st = 1;
			if (FieldSel[i].opt && FieldSel[i].ov)
			    *(FieldSel[i].opt) |= FieldSel[i].ov;
		    }

#if	defined(HASFSTRUCT)
		    Ffield = FsvFlagX = 1;
#else	/* !defined(HASFSTRUCT) */
		    Ffield = 1;
#endif	/* defined(HASFSTRUCT) */

		    break;
		}
		if (strcmp(GOv, "?") == 0) {
		    fh = 1;
		    break;
		}
		for (; *GOv; GOv++) {
		    for (i = 0; FieldSel[i].nm; i++) {

#if	!defined(HASPPID)
			if (FieldSel[i].id == LSOF_FID_PPID)
			    continue;
#endif	/* !defined(HASPPID) */

#if	!defined(HASFSTRUCT)
			if (FieldSel[i].id == LSOF_FID_CT
			||  FieldSel[i].id == LSOF_FID_FA
			||  FieldSel[i].id == LSOF_FID_FG
			||  FieldSel[i].id == LSOF_FID_NI)
			    continue;
#endif	/* !defined(HASFSTRUCT) */

			if (FieldSel[i].id == *GOv) {
			    FieldSel[i].st = 1;
			    if (FieldSel[i].opt && FieldSel[i].ov)
				*(FieldSel[i].opt) |= FieldSel[i].ov;

#if	defined(HASFSTRUCT)
			    if (i == LSOF_FIX_FG)
				FsvFlagX = 1;
#endif	/* defined(HASFSTRUCT) */

			    if (i == LSOF_FIX_TERM)
				Terminator = '\0';
			    break;
			}
		    }
		    if ( ! FieldSel[i].nm) {
			(void) fprintf(stderr,
			    "%s: unknown field: %c\n", Pn, *GOv);
			err++;
		    }
		}
		Ffield = 1;
		break;
	    case 'g':
		if (GOv) {
		    if (*GOv == '-' || *GOv == '+') {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    } else if (enter_id(PGID, GOv))
			err = 1;
		}
		Fpgid = 1;
		break;
	    case 'h':
	    case '?':
		Fhelp = 1;
		break;
	    case 'i':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    Fnet = 1;
		    FnetTy = 0;
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    break;
		}
		if (enter_network_address(GOv))
		    err = 1;
		break;

#if	defined(HASKOPT)
	    case 'k':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    (void) fprintf(stderr, "%s: -k not followed by path\n", Pn);
		    err = 1;
		} else
		    Nmlst = GOv;
		break;
#endif	/* defined(HASKOPT) */

	    case 'l':
		Futol = 0;
		break;
	    case 'L':
		Fnlink = (GOp == '+') ? 1 : 0;
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    Nlink = 0l;
		    break;
		}
		for (cp = GOv, l = 0l, n = 0; *cp; cp++) {
		    if (!isdigit((unsigned char)*cp))
			break;
		    l = (l * 10l) + ((long)*cp - (long)'0');
		    n++;
		}
		if (n) {
		    if (GOp != '+') {
			(void) fprintf(stderr,
			    "%s: no number may follow -L\n", Pn);
			err = 1;
		    } else {
			Nlink = l;
			Selflags |= SELNLINK;
		    }
		} else
		    Nlink = 0l;
		if (*cp) {
		    GOx1 = GObk[0];
		    GOx2 = GObk[1] + n;
		}
		break;

#if	defined(HASMOPT)
	    case 'm':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    (void) fprintf(stderr, "%s: -m not followed by path\n", Pn);
		    err = 1;
		} else
		    Memory = GOv;
		break;
#endif	/* defined(HASMOPT) */

	    case 'M':
		FportMap = (GOp == '+') ? 1 : 0;
		break;
	    case 'n':
		Fhost = (GOp == '-') ? 0 : 1;
		break;
	    case 'N':
		Fnfs = 1;
		break;
	    case 'o':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    Foffset = 1;
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    break;
		}
		for (cp = GOv, i = n = 0; *cp; cp++) {
		    if (!isdigit((unsigned char)*cp))
			break;
		    i = (i * 10) + ((int)*cp - '0');
		    n++;
		}
		if (n)
		    OffDecDig = i;
		else
		    Foffset = 1;
		if (*cp) {
		    GOx1 = GObk[0];
		    GOx2 = GObk[1] + n;
		}
		break;
	    case 'O':
		Fovhd = (GOp == '-') ? 1 : 0;
		break;
	    case 'p':
		if (enter_id(PID, GOv))
		    err = 1;
		break;
	    case 'P':
		Fport = (GOp == '-') ? 0 : 1;
		break;
	    case 'r':
		if (GOp == '+')
		    ev = rc = 1;
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    RptTm = RPTTM;
		    break;
		}
		for (cp = GOv, i = n = 0; *cp; cp++) {
		    if (!isdigit((unsigned char)*cp))
			break;
		    i = (i * 10) + ((int)*cp - '0');
		    n++;
		}
		if (n)
		    RptTm = i;
		else
		    RptTm = RPTTM;
		if (*cp) {
		    GOx1 = GObk[0];
		    GOx2 = GObk[1] + n;
		}
		break;

#if	defined(HASPPID)
	    case 'R':
		Fppid = 1;
		break;
#endif	/* defined(HASPPID) */

	    case 's':
		Fsize = 1;
		break;
	    case 'S':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    TmLimit = TMLIMIT;
		    break;
		}
		for (cp = GOv, i = n = 0; *cp; cp++) {
		    if (!isdigit((unsigned char)*cp))
			break;
		    i = (i * 10) + ((int)*cp - '0');
		    n++;
		}
		if (n)
		    TmLimit = i;
		else
		    TmLimit = TMLIMIT;
		if (*cp) {
		    GOx1 = GObk[0];
		    GOx2 = GObk[1] + n;
		}
		if (TmLimit < TMLIMMIN) {
		    (void) fprintf(stderr,
			"%s: WARNING: -S time (%d) changed to %d\n",
			Pn, TmLimit, TMLIMMIN);
		    TmLimit = TMLIMMIN;
		}
		break;
	    case 't':
		Fterse = Fwarn = 1;
		break;
	    case 'T':
		if (!GOv || *GOv == '-' || *GOv == '+') {
		    if (GOv) {
			GOx1 = GObk[0];
			GOx2 = GObk[1];
		    }
		    Ftcptpi = (GOp == '-') ? 0 : TCPTPI_STATE;
		    break;
		}
		for (Ftcptpi = 0; *GOv; GOv++) {
		    switch (*GOv) {

#if	defined(HASTCPTPIQ)
		    case 'q':
			Ftcptpi |= TCPTPI_QUEUES;
			break;
#endif	/* defined(HASTCPTPIQ) */

		    case 's':
			Ftcptpi |= TCPTPI_STATE;
			break;

#if	defined(HASTCPTPIW)
		    case 'w':
			Ftcptpi |= TCPTPI_WINDOWS;
			break;
#endif	/* defined(HASTCPTPIW) */

		    default:
			(void) fprintf(stderr,
			"%s: unsupported TCP/TPI info selection: %c\n",
			    Pn, *GOv);
			err = 1;
		    }
		}
		break;
	    case 'u':
		if (enter_uid(GOv))
		    err = 1;
		break;
	    case 'U':
		Funix = 1;
		break;
	    case 'v':
		version = 1;
		break;
	    case 'V':
		Fverbose = 1;
		break;
	    case 'w':
		Fwarn = (GOp == '+') ? 0 : 1;
		break;

#if	defined(HASXOPT)
	    case 'X':
		Fxopt = Fxopt ? 0 : 1;
		break;
#endif	/* defined(HASXOPT) */

	    default:
		(void) fprintf(stderr, "%s: unknown option (%c)\n", Pn, c);
		err = 1;
	    }
	}
	if (Fsize && Foffset) {
	    (void) fprintf(stderr, "%s: -o and -s are mutually exclusive\n",
		Pn);
	    err++;
	}
	if (Ffield) {
	    if (Fterse) {
		(void) fprintf(stderr,
		    "%s: -f and -t are mutually exclusive\n", Pn);
		err++;
	    }
	    FieldSel[LSOF_FIX_PID].st = 1;
	}
	if (DChelp || err || Fhelp || fh || version)
	    usage(err ? 1 : 0, fh, version);
/*
 * Reduce the size of Suid[], if necessary.
 */
	if (Suid && Nuid && Nuid < Mxuid) {
	    if (!(Suid = (struct seluid *)realloc((MALLOC_P *)Suid,
			 (MALLOC_S)(sizeof(struct seluid) * Nuid))))
	    {
		(void) fprintf(stderr, "%s: can't realloc UID table\n", Pn);
		Exit(1);
	    }
	    Mxuid = Nuid;
	}
/*
 * Compute the selection flags.
 */
	if (Cmdl || CmdRx)
	    Selflags |= SELCMD;
	if (Fdl)
	    Selflags |= SELFD;
	if (Fnet)
	    Selflags |= SELNET;
	if (Fnfs)
	    Selflags |= SELNFS;
	if (Funix)
	    Selflags |= SELUNX;
	if (Npgid)
	    Selflags |= SELPGID;
	if (Npid)
	    Selflags |= SELPID;
	if (Nuid && Nuidincl)
	    Selflags |= SELUID;
	if (Nwad)
	    Selflags |= SELNA;
	if (GOx1 < argc)
	    Selflags |= SELNM;
	if (Selflags == 0) {
	    if (Fand) {
		(void) fprintf(stderr,
		    "%s: no select options to AND via -a\n", Pn);
		usage(1, 0, 0);
	    }
	    Selflags = SELALL;
	} else {
	    if (GOx1 >= argc && (Selflags & (SELNA|SELNET)) != 0
	    &&  (Selflags & ~(SELNA|SELNET)) == 0)
		Selinet = 1;
	    Selall = 0;
	}
/*
 * Get the device for /dev.
 */
	if (stat("/dev", &sb)) {
	    (void) fprintf(stderr, "%s: can't stat(/dev): %s\n", Pn,
		strerror(errno));
	    Exit(1);
	}
	DevDev = sb.st_dev;
/*
 * Process the file arguments.
 */
	if (GOx1 < argc) {
	    if (ck_file_arg(GOx1, argc, argv, Ffilesys, 0, (struct stat *)NULL))
		usage(1, 0, 0);
	}
/*
 * Do dialect-specific initialization.
 */
	initialize();
	if (Sfile)
	    (void) hashSfile();

#if	defined(WILLDROPGID)
/*
 * If this process isn't setuid(root), but it is setgid(not_real_gid),
 * relinquish the setgid power.  (If it hasn't already been done.)
 */
	(void) dropgid();
#endif	/* defined(WILLDROPGID) */


#if	defined(HASDCACHE)
/*
 * If there is a device cache, prepare the device table.
 */
	if (DCstate)
	    readdev(0);
#endif	/* defined(HASDCACHE) */

/*
 * Define the size and offset print formats.
 */
	(void) snpf(options, sizeof(options), "0t%%%su", SZOFFPSPEC);
	SzOffFmt_0t = sv_fmt_str(options);
	(void) snpf(options, sizeof(options), "%%%su", SZOFFPSPEC);
	SzOffFmt_d = sv_fmt_str(options);
	(void) snpf(options, sizeof(options), "%%*%su", SZOFFPSPEC);
	SzOffFmt_dv = sv_fmt_str(options);
	(void) snpf(options, sizeof(options), "%%#%sx", SZOFFPSPEC);
	SzOffFmt_x = sv_fmt_str(options);
/*
 * Gather and report process information every RptTm seconds.
 */
	if (RptTm)
	    CkPasswd = 1;
	do {

	/*
	 * Gather information about processes.
	 */
	    gather_proc_info();
	/*
	 * If the local process table has more than one entry, sort it by PID.
	 */
	    if (Nlproc > 1) {
		if (Nlproc > sp) {
		    len = (MALLOC_S)(Nlproc * sizeof(struct lproc *));
		    sp = Nlproc;
		    if (!slp)
			slp = (struct lproc **)malloc(len);
		    else
			slp = (struct lproc **)realloc((MALLOC_P *)slp, len);
		    if (!slp) {
			(void) fprintf(stderr,
			    "%s: no space for %d sort pointers\n", Pn, Nlproc);
			Exit(1);
		    }
		}
		for (i = 0; i < Nlproc; i++) {
		    slp[i] = &Lproc[i];
		}
		(void) qsort((QSORT_P *)slp, (size_t)Nlproc,
			     (size_t)sizeof(struct lproc *), comppid);
	    }
	    if ((n = Nlproc)) {

#if	defined(HASNCACHE)
	    /*
	     * If using the kernel name cache, force its reloading.
	     */
		NcacheReload = 1;
#endif	/* defined(HASNCACHE) */

	    /*
	     * Print the selected processes and count them.
	     *
	     * Lf contents must be preserved, since they may point to a
	     * malloc()'d area, and since Lf is used throughout the print
	     * process.
	     */
		for (lf = Lf, print_init(); PrPass < 2; PrPass++) {
		    for (i = n = 0; i < Nlproc; i++) {
			Lp = (Nlproc > 1) ? slp[i] : &Lproc[i];
			if (Lp->pss) {
			    if (print_proc())
				n++;
			}
			if (RptTm && PrPass)
			    (void) free_lproc(Lp);
		    }
		}
		Lf = lf;
	    }
	/*
	 * If a repeat time is set, sleep for the specified time.
	 *
	 * If conditional repeat mode is in effect, see if it's time to exit.
	 */
	    if (RptTm) {
		if (rc) {
		    if (!n)
			break;
		    else
			ev = 0;
		}
		if (Ffield) {
		    putchar(LSOF_FID_MARK);
		    putchar('\n');
		} else
		    puts("=======");
		(void) fflush(stdout);
		(void) childx();
		(void) sleep(RptTm);
		Hdr = Nlproc = 0;
		CkPasswd = 1;
	    }
	} while (RptTm);
/*
 * See if all requested information was displayed.  Return zero if it
 * was; one, if not.  If -V was specified, report what was not displayed.
 */
	(void) childx();
	rv = 0;
	for (str = Cmdl; str; str = str->next) {

	/*
	 * Check command specifications.
	 */
	    if (str->f)
		continue;
	    rv = 1;
	    if (Fverbose) {
		(void) printf("%s: command not located: ", Pn);
		safestrprt(str->str, stdout, 1);
	    }
	}
	for (i = 0; i < NCmdRxU; i++) {
	
	/*
	 * Check command regular expressions.
	 */
	    if (CmdRx[i].mc)
		continue;
	    rv = 1;
	    if (Fverbose) {
		(void) printf("%s: no command found for regex: ", Pn);
		safestrprt(CmdRx[i].exp, stdout, 1);
	    }
	}
	for (sfp = Sfile; sfp; sfp = sfp->next) {

	/*
	 * Check file specifications.
	 */
	    if (sfp->f)
		continue;
	    rv = 1;
	    if (Fverbose) {
		(void) printf("%s: no file%s use located: ", Pn,
		    sfp->type ? "" : " system");
		safestrprt(sfp->aname, stdout, 1);
	    }
	}

#if	defined(HASPROCFS)
	/*
	 * Report on proc file system search results.
	 */
	    if (Procsrch && !Procfind) {
		rv = 1;
		if (Fverbose) {
		    (void) printf("%s: no file system use located: ", Pn);
		    safestrprt(Mtprocfs ? Mtprocfs->dir : HASPROCFS, stdout, 1);
		}
	    }
	    {
		struct procfsid *pfi;

		for (pfi = Procfsid; pfi; pfi = pfi->next) {
		    if (!pfi->f) {
			rv = 1;
			if (Fverbose) {
			    (void) printf("%s: no file use located: ", Pn);
			    safestrprt(pfi->nm, stdout, 1);
			}
		    }
		}
	    }
#endif	/* defined(HASPROCFS) */

	if ((np = Nwad)) {

	/*
	 * Check Internet address specifications.
	 *
	 * If any Internet address derived from the same argument was found,
	 * consider all derivations found.  If no derivation from the same
	 * argument was found, report only the first failure.
	 *
	 */
	    for (; np; np = np->next) {
		if (!(cp = np->arg))
		    continue;
		for (npn = np->next; npn; npn = npn->next) {
		    if (!npn->arg)
			continue;
		    if (!strcmp(cp, npn->arg)) {

		    /*
		     * If either of the duplicate specifications was found,
		     * mark them both found.  If neither was found, mark all
		     * but the first one found.
		     */
			if (np->f)
			    npn->f = np->f;
			else if (npn->f)
			    np->f = npn->f;
			else
			    npn->f = 1;
		    }
		}
	    }
	    for (np = Nwad; np; np = np->next) {
		if (!np->f && (cp = np->arg)) {
		    rv = 1;
		    if (Fverbose) {
			(void) printf("%s: Internet address not located: ", Pn);
			safestrprt(cp ? cp : "(unknown)", stdout, 1);
		    }
		}
	    }
	}
	if (Fnet && Fnet < 2) {

	/*
	 * Report no Internet files located.
	 */
	    rv = 1;
	    if (Fverbose)
		(void) printf("%s: no Internet files located\n", Pn);
	}
	if (Fnfs && Fnfs < 2) {

	/*
	 * Report no NFS files located.
	 */
	    rv = 1;
	    if (Fverbose)
		(void) printf("%s: no NFS files located\n", Pn);
	}
	for (i = 0; i < Npid; i++) {

	/*
	 * Check process ID specifications.
	 */
	    if (Spid[i].f)
		continue;
	    rv = 1;
	    if (Fverbose)
		(void) printf("%s: process ID not located: %d\n",
		    Pn, Spid[i].i);
	}
	for (i = 0; i < Npgid; i++) {

	/*
	 * Check process group ID specifications.
	 */
	    if (Spgid[i].f)
		continue;
	    rv = 1;
	    if (Fverbose)
		(void) printf("%s: process group ID not located: %d\n",
		    Pn, Spgid[i].i);
	}
	for (i = 0; i < Nuid; i++) {

	/*
	 * Check inclusionary user ID specifications.
	 */
	    if (Suid[i].excl || Suid[i].f)
		continue;
	    rv = 1;
	    if (Fverbose) {
		if (Suid[i].lnm) {
		    (void) printf("%s: login name (UID %lu) not located: ",
			Pn, (unsigned long)Suid[i].uid);
		    safestrprt(Suid[i].lnm, stdout, 1);
		} else
		    (void) printf("%s: user ID not located: %lu\n", Pn,
			(unsigned long)Suid[i].uid);
	    }
	}
	if (!rv && rc)
	    rv = ev;
	if (!rv && ErrStat)
	    rv = 1;
	Exit(rv);
	return(rv);		/* to make code analyzers happy */
}


/*
 * GetOpt() -- Local get option
 *
 * Liberally adapted from the public domain AT&T getopt() source,
 * distributed at the 1985 UNIFORM conference in Dallas
 *
 * The modifications allow `?' to be an option character and allow
 * the caller to decide that an option that may be followed by a
 * value doesn't have one -- e.g., has a default instead.
 */

static int
GetOpt(ct, opt, rules, err)
	int ct;				/* option count */
	char *opt[];			/* options */
	char *rules;			/* option rules */
	int *err;			/* error return */
{
	register int c;
	register char *cp = (char *)NULL;

	if (GOx2 == 0) {

	/*
	 * Move to a new entry of the option array.
	 *
	 * EOF if:
	 *
	 *	Option list has been exhausted;
	 *	Next option doesn't start with `-' or `+';
	 *	Next option has nothing but `-' or `+';
	 *	Next option is ``--'' or ``++''.
	 */
	    if (GOx1 >= ct
	    ||  (opt[GOx1][0] != '-' && opt[GOx1][0] != '+')
	    ||  !opt[GOx1][1])
		 return(EOF);
	    if (strcmp(opt[GOx1], "--") == 0 || strcmp(opt[GOx1], "++") == 0) {
		GOx1++;
		return(EOF);
	    }
	    GOp = opt[GOx1][0];
	    GOx2 = 1;
	}
/*
 * Flag `:' option character as an error.
 *
 * Check for a rule on this option character.
 */
	*err = 0;
	if ((c = opt[GOx1][GOx2]) == ':') {
	    (void) fprintf(stderr,
		"%s: colon is an illegal option character.\n", Pn);
	    *err = 1;
	} else if (!(cp = strchr(rules, c))) {
	    (void) fprintf(stderr, "%s: illegal option character: %c\n", Pn, c);
	    *err = 2;
	}
	if (*err) {

	/*
	 * An error was detected.
	 *
	 * Advance to the next option character.
	 *
	 * Return the character causing the error.
	 */
	    if (opt[GOx1][++GOx2] == '\0') {
		GOx1++;
		GOx2 = 0;
	    }
	    return(c);
	}
	if (*(cp + 1) == ':') {

	/*
	 * The option may have a following value.  The caller decides
	 * if it does.
	 *
	 * Save the position of the possible value in case the caller
	 * decides it does not belong to the option and wants it
	 * reconsidered as an option character.  The caller does that
	 * with:
	 *		GOx1 = GObk[0]; GOx2 = GObk[1];
	 *
	 * Don't indicate that an option of ``--'' is a possible value.
	 *
	 * Finally, on the assumption that the caller will decide that
	 * the possible value belongs to the option, position to the
	 * option following the possible value, so that the next call
	 * to GetOpt() will find it.
	 */
	    if(opt[GOx1][GOx2 + 1] != '\0') {
		GObk[0] = GOx1;
		GObk[1] = ++GOx2;
		GOv = &opt[GOx1++][GOx2];
	    } else if (++GOx1 >= ct)
		GOv = (char *)NULL;
	    else {
		GObk[0] = GOx1;
		GObk[1] = 0;
		GOv = opt[GOx1];
		if (strcmp(GOv, "--") == 0)
		    GOv = (char *)NULL;
		else
		    GOx1++;
	    }
	    GOx2 = 0;
	} else {

	/*
	 * The option character stands alone with no following value.
	 *
	 * Advance to the next option character.
	 */
	    if (opt[GOx1][++GOx2] == '\0') {
		GOx2 = 0;
		GOx1++;
	    }
	    GOv = (char *)NULL;
	}
/*
 * Return the option character.
 */
	return(c);
}


/*
 * sv_fmt_str() - save format string
 */

static char *
sv_fmt_str(f)
	char *f;			/* format string */
{
	char *cp;
	MALLOC_S l;

	l = (MALLOC_S)(strlen(f) + 1);
	if (!(cp = (char *)malloc(l))) {
	    (void) fprintf(stderr,
		"%s: can't allocate %d bytes for format: %s\n", Pn, (int)l, f);
	    Exit(1);
	}
	(void) snpf(cp, l, "%s", f);
	return(cp);
}
