/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PROCFS_H
#define	_SYS_PROCFS_H

#pragma ident	"@(#)procfs.h	1.37	05/06/08 SMI"

/* From Sun's procfs_isa.h */
/*
 * Possible values of pr_dmodel.
 * This isn't isa-specific, but it needs to be defined here for other reasons.
 */
#define PR_MODEL_UNKNOWN 0
#define PR_MODEL_ILP32  1       /* process data model is ILP32 */
#define PR_MODEL_LP64   2       /* process data model is LP64 */

/*
 * APPLE NOTE: This is a VERY cut down copy of Sun's procfs.h. KEEP IT IN ORDER!
 * We want to be able to diff this file against newer versions of libproc.h
 * and see where changes have been made.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Control codes (long values) for messages written to ctl and lwpctl files.
 */
#define PCNULL   0L     /* null request, advance to next message */
#define PCSTOP   1L     /* direct process or lwp to stop and wait for stop */
#define PCDSTOP  2L     /* direct process or lwp to stop */
#define PCWSTOP  3L     /* wait for process or lwp to stop, no timeout */
#define PCTWSTOP 4L     /* wait for stop, with long millisecond timeout arg */
#define PCRUN    5L     /* make process/lwp runnable, w/ long flags argument */
#define PCCSIG   6L     /* clear current signal from lwp */
#define PCCFAULT 7L     /* clear current fault from lwp */
#define PCSSIG   8L     /* set current signal from siginfo_t argument */
#define PCKILL   9L     /* post a signal to process/lwp, long argument */
#define PCUNKILL 10L    /* delete a pending signal from process/lwp, long arg */
#define PCSHOLD  11L    /* set lwp signal mask from sigset_t argument */
#define PCSTRACE 12L    /* set traced signal set from sigset_t argument */
#define PCSFAULT 13L    /* set traced fault set from fltset_t argument */
#define PCSENTRY 14L    /* set traced syscall entry set from sysset_t arg */
#define PCSEXIT  15L    /* set traced syscall exit set from sysset_t arg */
#define PCSET    16L    /* set modes from long argument */
#define PCUNSET  17L    /* unset modes from long argument */
#define PCSREG   18L    /* set lwp general registers from prgregset_t arg */
#define PCSFPREG 19L    /* set lwp floating-point registers from prfpregset_t */
#define PCSXREG  20L    /* set lwp extra registers from prxregset_t arg */
#define PCNICE   21L    /* set nice priority from long argument */
#define PCSVADDR 22L    /* set %pc virtual address from long argument */
#define PCWATCH  23L    /* set/unset watched memory area from prwatch_t arg */
#define PCAGENT  24L    /* create agent lwp with regs from prgregset_t arg */
#define PCREAD   25L    /* read from the address space via priovec_t arg */
#define PCWRITE  26L    /* write to the address space via priovec_t arg */
#define PCSCRED  27L    /* set process credentials from prcred_t argument */
#define PCSASRS  28L    /* set ancillary state registers from asrset_t arg */
#define PCSPRIV  29L    /* set process privileges from prpriv_t argument */
#define PCSZONE  30L    /* set zoneid from zoneid_t argument */
#define PCSCREDX 31L    /* as PCSCRED but with supplemental groups */
    
/*
 * process status file.  /proc/<pid>/status
 */
typedef struct pstatus {
	int	pr_flags;	/* flags (see below) */
//	int	pr_nlwp;	/* number of active lwps in the process */
	pid_t	pr_pid;		/* process id */
//	pid_t	pr_ppid;	/* parent process id */
//	pid_t	pr_pgid;	/* process group id */
//	pid_t	pr_sid;		/* session id */
//	id_t	pr_aslwpid;	/* historical; now always zero */
//	id_t	pr_agentid;	/* lwp id of the /proc agent lwp, if any */
//	sigset_t pr_sigpend;	/* set of process pending signals */
//	uintptr_t pr_brkbase;	/* address of the process heap */
//	size_t	pr_brksize;	/* size of the process heap, in bytes */
//	uintptr_t pr_stkbase;	/* address of the process stack */
//	size_t	pr_stksize;	/* size of the process stack, in bytes */
//	timestruc_t pr_utime;	/* process user cpu time */
//	timestruc_t pr_stime;	/* process system cpu time */
//	timestruc_t pr_cutime;	/* sum of children's user times */
//	timestruc_t pr_cstime;	/* sum of children's system times */
//	sigset_t pr_sigtrace;	/* set of traced signals */
//	fltset_t pr_flttrace;	/* set of traced faults */
//	sysset_t pr_sysentry;	/* set of system calls traced on entry */
//	sysset_t pr_sysexit;	/* set of system calls traced on exit */
	char	pr_dmodel;	/* data model of the process (see below) */
//	char	pr_pad[3];
//	taskid_t pr_taskid;	/* task id */
//	projid_t pr_projid;	/* project id */
//	int	pr_nzomb;	/* number of zombie lwps in the process */
//	zoneid_t pr_zoneid;	/* zone id */
//	int	pr_filler[15];	/* reserved for future use */
//	lwpstatus_t pr_lwp;	/* status of the representative lwp */
} pstatus_t;

/*
 * pr_flags (same values appear in both pstatus_t and lwpstatus_t pr_flags).
 *
 * These flags do *not* apply to psinfo_t.pr_flag or lwpsinfo_t.pr_flag
 * (which are both deprecated).
 */
/* The following flags apply to the specific or representative lwp */
#define PR_STOPPED 0x00000001   /* lwp is stopped */
#define PR_ISTOP   0x00000002   /* lwp is stopped on an event of interest */
#define PR_DSTOP   0x00000004   /* lwp has a stop directive in effect */
#define PR_STEP    0x00000008   /* lwp has a single-step directive in effect */
#define PR_ASLEEP  0x00000010   /* lwp is sleeping in a system call */
#define PR_PCINVAL 0x00000020   /* contents of pr_instr undefined */
#define PR_ASLWP   0x00000040   /* obsolete flag; never set */
#define PR_AGENT   0x00000080   /* this lwp is the /proc agent lwp */
#define PR_DETACH  0x00000100   /* this is a detached lwp */
#define PR_DAEMON  0x00000200   /* this is a daemon lwp */
/* The following flags apply to the process, not to an individual lwp */
#define PR_ISSYS   0x00001000   /* this is a system process */
#define PR_VFORKP  0x00002000   /* process is the parent of a vfork()d child */
#define PR_ORPHAN  0x00004000   /* process's process group is orphaned */
/* The following process flags are modes settable by PCSET/PCUNSET */
#define PR_FORK    0x00100000   /* inherit-on-fork is in effect */
#define PR_RLC     0x00200000   /* run-on-last-close is in effect */
#define PR_KLC     0x00400000   /* kill-on-last-close is in effect */
#define PR_ASYNC   0x00800000   /* asynchronous-stop is in effect */
#define PR_MSACCT  0x01000000   /* micro-state usage accounting is in effect */
#define PR_BPTADJ  0x02000000   /* breakpoint trap pc adjustment is in effect */
#define PR_PTRACE  0x04000000   /* ptrace-compatibility mode is in effect */
#define PR_MSFORK  0x08000000   /* micro-state accounting inherited on fork */
#define PR_IDLE    0x10000000   /* lwp is a cpu's idle thread */
    
/*
 * Memory-map interface.  /proc/<pid>/map /proc/<pid>/rmap
 */
// #define	PRMAPSZ	64
typedef struct prmap {
        /* APPLE NOTE: Changed to 64 bit to handle 32 bit dtrace looking at 64 bit procs */
	uint64_t pr_vaddr;	/* virtual address of mapping */
//	uintptr_t pr_vaddr;	/* virtual address of mapping */
        
//	size_t	pr_size;	/* size of mapping in bytes */
//	char	pr_mapname[PRMAPSZ];	/* name in /proc/<pid>/object */
//	offset_t pr_offset;	/* offset into mapped object, if any */
	int	pr_mflags;	/* protection and attribute flags (see below) */
//	int	pr_pagesize;	/* pagesize (bytes) for this mapping */
//	int	pr_shmid;	/* SysV shmid, -1 if not SysV shared memory */
//	int	pr_filler[1];	/* filler for future expansion */
} prmap_t;

/* Protection and attribute flags */
#define MA_READ         0x04    /* readable by the traced process */
#define MA_WRITE        0x02    /* writable by the traced process */
#define MA_EXEC         0x01    /* executable by the traced process */
#define MA_SHARED       0x08    /* changes are shared by mapped object */
#define MA_ANON         0x40    /* anonymous memory (e.g. /dev/zero) */
#define MA_ISM          0x80    /* intimate shared mem (shared MMU resources) */
#define MA_NORESERVE    0x100   /* mapped with MAP_NORESERVE */
#define MA_SHM          0x200   /* System V shared memory */
#define MA_RESERVED1    0x400   /* reserved for future use */
    
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_H */
