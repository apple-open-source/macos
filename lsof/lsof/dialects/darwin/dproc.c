/*
 * dproc.c - Darwin process access functions for lsof
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
static char *rcsid = "$Id: dproc.c,v 1.3 2002/06/17 01:41:42 abe Exp $";
#endif

#include "lsof.h"


_PROTOTYPE(static void enter_vn_text,(KA_T va, int *n));
_PROTOTYPE(static void get_kernel_access,(void));


/*
 * Local static values
 */

static MALLOC_S Nv = 0;			/* allocated Vp[] entries */
static KA_T *Vp = NULL;			/* vnode address cache */


/*
 * enter_vn_text() - enter a vnode text reference
 */

static void
enter_vn_text(va, n)
	KA_T va;			/* vnode address */
	int *n;				/* Vp[] entries in use */
{
	int i;
/*
 * Ignore the request if the vnode has already been entered.
 */
	for (i = 0; i < *n; i++) {
	    if (va == Vp[i])
		return;
	}
/*
 * Save the text file information.
 */
	alloc_lfile(" txt", -1);
	Cfp = (struct file *)NULL;
	process_node(va);
	if (Lf->sf)
	    link_lfile();
	if (i >= Nv) {

	/*
	 * Allocate space for remembering the vnode.
	 */
	    Nv += 10;
	    if (!Vp)
		Vp=(KA_T *)malloc((MALLOC_S)(sizeof(struct vnode *)*10));
	    else
		Vp=(KA_T *)realloc((MALLOC_P *)Vp,(MALLOC_S)(Nv*sizeof(KA_T)));
	    if (!Vp) {
		(void) fprintf(stderr, "%s: no txt ptr space, PID %d\n",
		    Pn, Lp->pid);
		Exit(1);
	    }
	}
/*
 * Remember the vnode.
 */
	Vp[*n] = va;
	(*n)++;
}


#if	DARWINV>=700
/*
 * realcmdname() -- get the "real" command name
 *
 * Note: this function returns either a pointer to an allocated copy
 *       of the command associated with the process or NULL if not
 *       available. 
 */


static char *
realcmdname(pid_t pid)
{
	static int	argmax	= -1;
	char		args[ARG_MAX];
	char		*args_p	= args;
	char		*argv0	= NULL;
	int		mib[3];
	size_t		size;
	char		*cp;
	char		*ep;
	char		*sp;

	if (argmax < 0) {
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;

		size = sizeof(argmax);

		if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1) {
			goto done;
		}
	}

	if (argmax > (int)sizeof(args)) {
		args_p = malloc(argmax);
		if (args_p == NULL) {
			goto done;
		}
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROCARGS;
	mib[2] = pid;

	size = (size_t)argmax;
	if (sysctl(mib, 3, args_p, &size, NULL, 0) == -1) {
		goto done;
	}

	/* Skip the saved exec path */
	for (cp = args_p; cp < &args_p[size]; cp++) {
		if( *cp == '\0' ) {
			/* if end of exec_path reached */
			break;
		}
	}
	if (cp == &args_p[size]) {
		goto done;
	}

	/* skip trailing '\0' characters */
	for (; cp < &args_p[size]; cp++) {
		if (*cp != '\0') {
			/* if at the beginning of the first argument */
			break;
		}
	}
	if (cp == &args_p[size]) {
		goto done;
	}
	sp = cp;

	/*
	 * Make sure that the command is '\0'-terminated.  This protects
	 * against malicious programs; under normal operation this never
	 * ends up being a problem..
	 */
	for (; cp < &args_p[size]; cp++) {
		if (*cp == '\0') {
			/* if the end of first argument reached */
			break;
		}
	}
	if (cp == &args_p[size]) {
		goto done;
	}
	ep = cp;

	/* Get the basename of command. */
	for (cp--; cp >= sp; cp--) {
		if (*cp == '/') {
			/* if slash found in command */
			cp++;
			break;
		}
	}

	size = ep - cp + 1;
	argv0 = (char *)malloc(size);
	if (argv0) {
		memcpy(argv0, cp, size);
	}

    done :

	if (args_p != args) {
		free(args_p);
	}

	return argv0;
}
#endif	/* DARWINV>=700


/*
 * gather_proc_info() -- gather process information
 */

void
gather_proc_info()
{
	struct filedesc fd;
	int i, nf;
	MALLOC_S nb;
	static struct file **ofb = NULL;
	static int ofbb = 0;
	int pgid, pid;
	int ppid = 0;
	short pss, sf;
	int px;
	uid_t uid;

	struct proc kp;
	struct kinfo_proc *p;

	static char *pof = (char *)NULL;
	static int pofb = 0;

/*
 * Read the process table.
 */

	if ((P = kvm_getprocs(Kd, KERN_PROC_ALL, 0, &Np)) == NULL)
	{
	    (void) fprintf(stderr, "%s: can't read process table: %s\n",
		Pn,

		kvm_geterr(Kd)

	    );
	    Exit(1);
	}

/*
 * Examine proc structures and their associated information.
 */

	for (p = P, px = 0; px < Np; p++, px++)

	{
	    char *cmd;

	    if (p->P_STAT == 0 || p->P_STAT == SZOMB)
		continue;
	    pgid = p->P_PGID;
	    uid = p->kp_eproc.e_ucred.cr_uid;

#if	defined(HASPPID)
	    ppid = p->P_PPID;
#endif	/* defined(HASPPID) */

	/*
	 * See if process is excluded.
	 *
	 * Read file structure pointers.
	 */
	    if (is_proc_excl(p->P_PID, pgid, (UID_ARG)uid, &pss, &sf))
		    continue;
	    if (!p->P_ADDR ||  kread((KA_T)p->P_ADDR, (char *)&kp, sizeof(kp)))
		    continue;
	    if (!kp.P_FD ||  kread((KA_T)kp.P_FD, (char *)&fd, sizeof(fd)))
		    continue;
	    if (!fd.fd_refcnt || fd.fd_lastfile > fd.fd_nfiles)
		    continue;

	/*
	 * Identfy the command name for the process.  For CFM applications,
	 * this requires some extra work since the basename of the first
	 * program argument is the actual command name.
	 */
#if	DARWINV<700
	    cmd = p->P_COMM;
#else	/* DARWINV>=700 */
	    if (strcmp(p->P_COMM, "LaunchCFMApp") != 0) {
		/* if a "normal" program" */
		cmd = p->P_COMM;
	    } else {
		/* if "LaunchCFM" */
		cmd = realcmdname(p->P_PID);
		if (!cmd)
		    cmd = p->P_COMM;
	    }
#endif	/* DARWINV<700 */

	/*
	 * Allocate a local process structure.
	 */
	    if (is_cmd_excl(cmd, &pss, &sf)) {
		if (cmd != p->P_COMM)
		    free(cmd);
		continue;
	    }
	    alloc_lproc(p->P_PID, pgid, ppid, (UID_ARG)uid, cmd,
		(int)pss, (int)sf);
	    Plf = (struct lfile *)NULL;

	    if (cmd != p->P_COMM)
		free(cmd);

	/*
	 * Save current working directory information.
	 */
	    if (fd.fd_cdir) {
		alloc_lfile(CWD, -1);
		Cfp = (struct file *)NULL;
		process_node((KA_T)fd.fd_cdir);
		if (Lf->sf)
		    link_lfile();
	    }
	/*
	 * Save root directory information.
	 */
	    if (fd.fd_rdir) {
		alloc_lfile(RTD, -1);
		Cfp = (struct file *)NULL;
		process_node((KA_T)fd.fd_rdir);
		if (Lf->sf)
		    link_lfile();
	    }
	/*
	 * Read open file structure pointers.
	 */
	    if (!fd.fd_ofiles || (nf = fd.fd_nfiles) <= 0)
		continue;
	    nb = (MALLOC_S)(sizeof(struct file *) * nf);
	    if (nb > ofbb) {
		if (!ofb)
		    ofb = (struct file **)malloc(nb);
		else
		    ofb = (struct file **)realloc((MALLOC_P *)ofb, nb);
		if (!ofb) {
		    (void) fprintf(stderr, "%s: PID %d, no file * space\n",
			Pn, p->P_PID);
		    Exit(1);
		}
		ofbb = nb;
	    }
	    if (kread((KA_T)fd.fd_ofiles, (char *)ofb, nb))
		continue;

	    nb = (MALLOC_S)(sizeof(char) * nf);
	    if (nb > pofb) {
		if (!pof)
		    pof = (char *)malloc(nb);
		else
		    pof = (char *)realloc((MALLOC_P *)pof, nb);
		if (!pof) {
		    (void) fprintf(stderr, "%s: PID %d, no file flag space\n",
			Pn, p->P_PID);
		    Exit(1);
		}
		pofb = nb;
	    }
	    if (!fd.fd_ofileflags || kread((KA_T)fd.fd_ofileflags, pof, nb))
		zeromem(pof, nb);

	/*
	 * Save information on file descriptors.
	 */
	    for (i = 0; i < nf; i++) {
		if (ofb[i] && !(pof[i] & UF_RESERVED)) {
		    alloc_lfile(NULL, i);
		    process_file((KA_T)(Cfp = ofb[i]));
		    if (Lf->sf) {

#if	defined(HASFSTRUCT)
			if (Fsv & FSV_FG)
			    Lf->pof = (long)pof[i];
#endif	/* defined(HASFSTRUCT) */

			link_lfile();
		    }
		}
	    }
	/*
	 * Examine results.
	 */
	    if (examine_lproc())
		return;
	}
}


/*
 * get_kernel_access() - get access to kernel memory
 */

static void
get_kernel_access()
{

/*
 * Check kernel version.
 */
	(void) ckkv("Darwin", LSOF_VSTR, (char *)NULL, (char *)NULL);
/*
 * Set name list file path.
 */
	if (!Nmlst)

#if	defined(N_UNIX)
	    Nmlst = N_UNIX;
#else	/* !defined(N_UNIX) */
	{
	    if (!(Nmlst = get_nlist_path(1))) {
		(void) fprintf(stderr,
		    "%s: can't get kernel name list path\n", Pn);
		Exit(1);
	    }
	}
#endif	/* defined(N_UNIX) */

#if	defined(WILLDROPGID)
/*
 * If kernel memory isn't coming from KMEM, drop setgid permission
 * before attempting to open the (Memory) file.
 */
	if (Memory)
	    (void) dropgid();
#else	/* !defined(WILLDROPGID) */
/*
 * See if the non-KMEM memory and the name list files are readable.
 */
	if ((Memory && !is_readable(Memory, 1))
	||  (Nmlst && !is_readable(Nmlst, 1)))
	    Exit(1);
#endif	/* defined(WILLDROPGID) */

/*
 * Open kernel memory access.
 */

	if ((Kd = kvm_open(Nmlst, Memory, NULL, O_RDONLY, NULL)) == NULL)
	{
	    (void) fprintf(stderr,
		"%s: kvm_open%s (namelist=%s, core = %s): %s\n",
		Pn,
		"",
		Nmlst ? Nmlst : "default",
		Memory  ? Memory  : "default",
		strerror(errno));
	    Exit(1);
	}
	(void) build_Nl(Drive_Nl);
	if (kvm_nlist(Kd, Nl) < 0) {
	    (void) fprintf(stderr, "%s: can't read namelist from %s\n",
		Pn, Nmlst);
	    Exit(1);
	}

#if	defined(WILLDROPGID)
/*
 * Drop setgid permission, if necessary.
 */
	if (!Memory)
	    (void) dropgid();
#endif	/* defined(WILLDROPGID) */

}


#if	!defined(N_UNIX)
/*
 * get_nlist_path() - get kernel name list path
 */

char *
get_nlist_path(ap)
	int ap;				/* on success, return an allocated path
					 * string pointer if 1; return a
					 * constant character pointer if 0;
					 * return NULL if failure */
{
        int m[2];
        char *bf;
        static char *bfc;
        MALLOC_S bfl;

       /*
        * Get bootfile name.
        */
        m[0] = CTL_KERN;
        m[1] = KERN_BOOTFILE;
        sysctl(m, 2, NULL, &bfl, NULL, 0);
        if (bfl) {
                bf = malloc((MALLOC_S)bfl);
                if (sysctl(m, 2, bf, &bfl, NULL, 0)) {
                        (void) fprintf(stderr, "%s: CTL_KERN, KERN_BOOTFILE: %s\n", Pn, strerror(errno));
                        Exit(1);
                }
                return(bf);
        }
        return((char *)NULL);

}
#endif	/* !defined(N_UNIX) */


/*
 * initialize() - perform all initialization
 */

void
initialize()
{
	get_kernel_access();
}


/*
 * kread() - read from kernel memory
 */

int
kread(addr, buf, len)
	KA_T addr;			/* kernel memory address */
	char *buf;			/* buffer to receive data */
	READLEN_T len;			/* length to read */
{
	int br;

	br = kvm_read(Kd, (u_long)addr, buf, len);
	return((br == len) ? 0 : 1);
}
