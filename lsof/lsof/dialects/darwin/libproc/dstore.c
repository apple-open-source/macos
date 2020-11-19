/*
 * dstore.c -- Darwin global storage for libproc-based lsof
 */


/*
 * Portions Copyright 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Copyright 2005 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Allan Nathanson, Apple Computer, Inc., and Victor A.
 * Abell, Purdue University.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors, nor Apple Computer, Inc. nor Purdue University
 *    are responsible for any consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either
 *    by explicit claim or by omission.  Credit to the authors, Apple
 *    Computer, Inc. and Purdue University must appear in documentation
 *    and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


#ifndef lint
static char copyright[] =
"@(#) Copyright 2005 Apple Computer, Inc. and Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dstore.c,v 1.5 2018/02/14 14:27:57 abe Exp $";
#endif


#include "lsof.h"

struct file *Cfp;			/* current file's file struct pointer */


#if	defined(HASFSTRUCT)
/*
 * Pff_tab[] - table for printing file flags
 */

struct pff_tab Pff_tab[] = {
	{ (long)FREAD,		FF_READ		},	// 0x0001
	{ (long)FWRITE,		FF_WRITE	},	// 0x0002
	{ (long)FNONBLOCK,	FF_NBLOCK	},	// 0x0004 (O_NONBLOCK)
	{ (long)FAPPEND,	FF_APPEND	},	// 0x0008 (O_APPEND)

	{ (long)FASYNC,		FF_ASYNC	},	// 0x0040 (O_ASYNC)
	{ (long)FFSYNC,		FF_FSYNC	},	// 0x0080 (O_FSYNC, O_SYNC)

# if	defined(FHASLOCK)
	{ (long)FHASLOCK,	FF_HASLOCK	},	// 0x4000
# endif	/* defined(FHASLOCK) */

# if	defined(O_EVTONLY)
	{ (long)O_EVTONLY,	FF_EVTONLY	},	// 0x8000
# endif	/* defined(O_EVTONLY) */

	{ (long)O_NOCTTY,	FF_NOCTTY	},	// 0x20000

# if	defined(FNOCACHE)
	{ (long)FNOCACHE,	FF_NOCACHE	},	// 0x40000
# endif	/* defined(FNOCACHE) */

# if	defined(FNORDAHEAD)
	{ (long)FNORDAHEAD,	"NRA"		},	// 0x80000
# endif	/* !defined(FNORDAHEAD) */

# if	defined(FFDSYNC)
	{ (long)FFDSYNC,	FF_DSYNC	},	// 0x400000 (O_DSYNC)
# endif	/* defined(FFDSYNC) */

# if	defined(FNODIRECT)
	{ (long)FNODIRECT,	"NDR"		},	// 0x800000
# endif	/* !defined(FNODIRECT) */

# if	defined(FSINGLE_WRITER)
	{ (long)FSINGLE_WRITER,	"1W"		},	// 0x4000000
# endif	/* !defined(FSINGLE_WRITER) */

	{ (long)0,		NULL 		}
};


/*
 * Pof_tab[] - table for print process open file flags
 */

struct pff_tab Pof_tab[] = {

# if	defined(PROC_FP_SHARED)
	{ (long)PROC_FP_SHARED,	"SH"		},	// 1
# endif	/* defined(PROC_FP_SHARED) */

# if	defined(PROC_FP_CLFORK)
	{ (long)PROC_FP_CLFORK,	"CF"		},	// 8
# endif	/* defined(PROC_FP_CLFORK) */

# if	defined(PROC_FP_CLEXEC)
	{ (long)PROC_FP_CLEXEC,	POF_CLOEXEC	},	// 2
# endif	/* defined(PROC_FP_CLEXEC) */

# if	defined(PROC_FP_GUARDED)
	{ (long)PROC_FP_GUARDED,"GRD"		},	// 4
# endif	/* defined(PROC_FP_GUARDED) */

	{ (long)0,		NULL		}
};
#endif	/* defined(HASFSTRUCT) */


#if	defined(PROC_FP_GUARDED)
/*
 * Pgf_tab[] - table for print process open file guard flags
 */

struct pff_tab Pgf_tab[] = {
	{ (long)PROC_FI_GUARD_CLOSE,		"CLOSE"		},
	{ (long)PROC_FI_GUARD_DUP,		"DUP"		},
	{ (long)PROC_FI_GUARD_SOCKET_IPC,	"SOCKET"	},
	{ (long)PROC_FI_GUARD_FILEPORT,		"FILEPORT"	},

	{ (long)0,				NULL		}
};
#endif	/* defined(PROC_FP_GUARDED) */
