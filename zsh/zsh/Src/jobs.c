/*
 * jobs.c - job control
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zsh.h"

/* empty job structure for quick clearing of jobtab entries */

static struct job zero;		/* static variables are initialized to zero */

struct timeval dtimeval, now;

/* Diff two timevals for elapsed-time computations */

/**/
struct timeval *
dtime(struct timeval *dt, struct timeval *t1, struct timeval *t2)
{
    dt->tv_sec = t2->tv_sec - t1->tv_sec;
    dt->tv_usec = t2->tv_usec - t1->tv_usec;
    if (dt->tv_usec < 0) {
	dt->tv_usec += 1000000.0;
	dt->tv_sec -= 1.0;
    }
    return dt;
}

/* change job table entry from stopped to running */

/**/
void
makerunning(Job jn)
{
    Process pn;

    jn->stat &= ~STAT_STOPPED;
    for (pn = jn->procs; pn; pn = pn->next)
#if 0
	if (WIFSTOPPED(pn->status) && 
	    (!(jn->stat & STAT_SUPERJOB) || pn->next))
	    pn->status = SP_RUNNING;
#endif
        if (WIFSTOPPED(pn->status))
	    pn->status = SP_RUNNING;

    if (jn->stat & STAT_SUPERJOB)
	makerunning(jobtab + jn->other);
}

/* Find process and job associated with pid.         *
 * Return 1 if search was successful, else return 0. */

/**/
int
findproc(pid_t pid, Job *jptr, Process *pptr)
{
    Process pn;
    int i;

    for (i = 1; i < MAXJOB; i++)
	for (pn = jobtab[i].procs; pn; pn = pn->next)
	    if (pn->pid == pid) {
		*pptr = pn;
		*jptr = jobtab + i;
		return 1;
	    }

    return 0;
}

/* Find the super-job of a sub-job. */

static int
super_job(int sub)
{
    int i;

    for (i = 1; i < MAXJOB; i++)
	if ((jobtab[i].stat & STAT_SUPERJOB) &&
	    jobtab[i].other == sub &&
	    jobtab[i].gleader)
	    return i;
    return 0;
}

static int
handle_sub(int job, int fg)
{
    Job jn = jobtab + job, sj = jobtab + jn->other;

    if ((sj->stat & STAT_DONE) || !sj->procs) {
	struct process *p;
		    
	for (p = sj->procs; p; p = p->next)
	    if (WIFSIGNALED(p->status)) {
		if (jn->gleader != mypgrp && jn->procs->next)
		    killpg(jn->gleader, WTERMSIG(p->status));
		else
		    kill(jn->procs->pid, WTERMSIG(p->status));
		kill(sj->other, SIGCONT);
		kill(sj->other, WTERMSIG(p->status));
		break;
	    }
	if (!p) {
	    int cp;

	    jn->stat &= ~STAT_SUPERJOB;
	    jn->stat |= STAT_WASSUPER;

	    if ((cp = ((WIFEXITED(jn->procs->status) ||
			WIFSIGNALED(jn->procs->status)) &&
		       killpg(jn->gleader, 0) == -1))) {
		Process p;
		for (p = jn->procs; p->next; p = p->next);
		jn->gleader = p->pid;
	    }
	    /* This deleted the job too early if the parent
	       shell waited for a command in a list that will
	       be executed by the sub-shell (e.g.: if we have
	       `ls|if true;then sleep 20;cat;fi' and ^Z the
	       sleep, the rest will be executed by a sub-shell,
	       but the parent shell gets notified for the
	       sleep.
	       deletejob(sj); */
	    /* If this super-job contains only the sub-shell,
	       we have to attach the tty to its process group
	       now. */
	    if ((fg || thisjob == job) &&
		(!jn->procs->next || cp || jn->procs->pid != jn->gleader))
		attachtty(jn->gleader);
	    kill(sj->other, SIGCONT);
	}
	curjob = jn - jobtab;
    } else if (sj->stat & STAT_STOPPED) {
	struct process *p;

	jn->stat |= STAT_STOPPED;
	for (p = jn->procs; p; p = p->next)
	    if (p->status == SP_RUNNING ||
		(!WIFEXITED(p->status) && !WIFSIGNALED(p->status)))
		p->status = sj->procs->status;
	curjob = jn - jobtab;
	printjob(jn, !!isset(LONGLISTJOBS), 1);
	return 1;
    }
    return 0;
}

/* Update status of process that we have just WAIT'ed for */

/**/
void
update_process(Process pn, int status)
{
    struct timezone dummy_tz;
    long childs, childu;

    childs = shtms.tms_cstime;
    childu = shtms.tms_cutime;
    times(&shtms);                          /* get time-accounting info          */

    pn->status = status;                    /* save the status returned by WAIT  */
    pn->ti.st  = shtms.tms_cstime - childs; /* compute process system space time */
    pn->ti.ut  = shtms.tms_cutime - childu; /* compute process user space time   */

    gettimeofday(&pn->endtime, &dummy_tz);  /* record time process exited        */
}

/* Update status of job, possibly printing it */

/**/
void
update_job(Job jn)
{
    Process pn;
    int job;
    int val = 0, status = 0;
    int somestopped = 0, inforeground = 0;
    extern int list_pipe;

    for (pn = jn->procs; pn; pn = pn->next) {
	if (pn->status == SP_RUNNING)      /* some processes in this job are running       */
	    return;                        /* so no need to update job table entry         */
	if (WIFSTOPPED(pn->status))        /* some processes are stopped                   */
	    somestopped = 1;               /* so job is not done, but entry needs updating */
	if (!pn->next)                     /* last job in pipeline determines exit status  */
	    val = (WIFSIGNALED(pn->status)) ? 0200 | WTERMSIG(pn->status) :
		WEXITSTATUS(pn->status);
	if (pn->pid == jn->gleader)        /* if this process is process group leader      */
	    status = pn->status;
    }

    job = jn - jobtab;   /* compute job number */

    if (somestopped) {
	if (shout && job == thisjob) {
	    if (!jn->ty)
		jn->ty = (struct ttyinfo *) zalloc(sizeof(struct ttyinfo));
	    gettyinfo(jn->ty);
	}
	if (jn->stat & STAT_STOPPED) {
	    if (jn->stat & STAT_SUBJOB) {
		/* If we have `cat foo|while read a; grep $a bar;done'
		 * and have hit ^Z, the sub-job is stopped, but the
		 * super-job may still be running, waiting to be stopped
		 * or to exit. So we have to send it a SIGTSTP. */
		int i;

		if ((i = super_job(job)))
		    killpg(jobtab[i].gleader, SIGTSTP);
	    }
	    return;
	}
    }
    {                   /* job is done or stopped, remember return value */
	lastval2 = val;
	/* If last process was run in the current shell, keep old status
	 * and let it handle its own traps, but always allow the test
	 * for the pgrp.
	 */
	if (jn->stat & STAT_CURSH)
	    inforeground = 1;
	else if (job == thisjob) {
	    lastval = val;
	    inforeground = 2;
	}
    }

    if (shout && !ttyfrozen && !jn->stty_in_env && !zleactive &&
	job == thisjob && !somestopped && !(jn->stat & STAT_NOSTTY))
	gettyinfo(&shttyinfo);

    if (isset(MONITOR)) {
	pid_t pgrp = gettygrp();           /* get process group of tty      */

	/* is this job in the foreground of an interactive shell? */
	if (mypgrp != pgrp && inforeground &&
	    (jn->gleader == pgrp || (pgrp > 1 && kill(-pgrp, 0) == -1))) {
	    if (list_pipe) {
		if (somestopped || (pgrp > 1 && kill(-pgrp, 0) == -1)) {
		    attachtty(mypgrp);
		    /* check window size and adjust if necessary */
		    adjustwinsize(0);
		} else {
		    /*
		     * Oh, dear, we're right in the middle of some confusion
		     * of shell jobs on the righthand side of a pipeline, so
		     * it's death to call attachtty() just yet.  Mark the
		     * fact in the job, so that the attachtty() will be called
		     * when the job is finally deleted.
		     */
		    jn->stat |= STAT_ATTACH;
		}
		/* If we have `foo|while true; (( x++ )); done', and hit
		 * ^C, we have to stop the loop, too. */
		if ((val & 0200) && inforeground == 1) {
		    if (!errbrk_saved) {
			errbrk_saved = 1;
			prev_breaks = breaks;
			prev_errflag = errflag;
		    }
		    breaks = loops;
		    errflag = 1;
		    inerrflush();
		}
	    } else {
		attachtty(mypgrp);
		/* check window size and adjust if necessary */
		adjustwinsize(0);
	    }
	}
    } else if (list_pipe && (val & 0200) && inforeground == 1) {
	if (!errbrk_saved) {
	    errbrk_saved = 1;
	    prev_breaks = breaks;
	    prev_errflag = errflag;
	}
	breaks = loops;
	errflag = 1;
	inerrflush();
    }
    if (somestopped && jn->stat & STAT_SUPERJOB)
	return;
    jn->stat |= (somestopped) ? STAT_CHANGED | STAT_STOPPED :
	STAT_CHANGED | STAT_DONE;
    if (!inforeground &&
	(jn->stat & (STAT_SUBJOB | STAT_DONE)) == (STAT_SUBJOB | STAT_DONE)) {
	int su;

	if ((su = super_job(jn - jobtab)))
	    handle_sub(su, 0);
    }
    if ((jn->stat & (STAT_DONE | STAT_STOPPED)) == STAT_STOPPED) {
	prevjob = curjob;
	curjob = job;
    }
    if ((isset(NOTIFY) || job == thisjob) && (jn->stat & STAT_LOCKED)) {
	printjob(jn, !!isset(LONGLISTJOBS), 0);
	if (zleactive)
	    refresh();
    }
    if (sigtrapped[SIGCHLD] && job != thisjob)
	dotrap(SIGCHLD);

    /* When MONITOR is set, the foreground process runs in a different *
     * process group from the shell, so the shell will not receive     *
     * terminal signals, therefore we we pretend that the shell got    *
     * the signal too.                                                 */
    if (inforeground == 2 && isset(MONITOR) && WIFSIGNALED(status)) {
	int sig = WTERMSIG(status);

	if (sig == SIGINT || sig == SIGQUIT) {
	    if (sigtrapped[sig]) {
		dotrap(sig);
		/* We keep the errflag as set or not by dotrap.
		 * This is to fulfil the promise to carry on
		 * with the jobs if trap returns zero.
		 * Setting breaks = loops ensures a consistent return
		 * status if inside a loop.  Maybe the code in loops
		 * should be changed.
		 */
		if (errflag)
		    breaks = loops;
	    } else {
		breaks = loops;
		errflag = 1;
	    }
	}
    }
}

/* lng = 0 means jobs    *
 * lng = 1 means jobs -l *
 * lng = 2 means jobs -p 
 *
 * synch = 0 means asynchronous
 * synch = 1 means synchronous
 * synch = 2 means called synchronously from jobs
*/

/**/
void
printjob(Job jn, int lng, int synch)
{
    Process pn;
    int job = jn - jobtab, len = 9, sig, sflag = 0, llen;
    int conted = 0, lineleng = columns, skip = 0, doputnl = 0;
    FILE *fout = (synch == 2) ? stdout : shout;

    if (jn->stat & STAT_NOPRINT)
	return;

    if (lng < 0) {
	conted = 1;
	lng = 0;
    }

/* find length of longest signame, check to see */
/* if we really need to print this job          */

    for (pn = jn->procs; pn; pn = pn->next) {
	if (jn->stat & STAT_SUPERJOB &&
	    jn->procs->status == SP_RUNNING && !pn->next)
	    pn->status = SP_RUNNING;
	if (pn->status != SP_RUNNING) {
	    if (WIFSIGNALED(pn->status)) {
		sig = WTERMSIG(pn->status);
		llen = strlen(sigmsg(sig));
		if (WCOREDUMP(pn->status))
		    llen += 14;
		if (llen > len)
		    len = llen;
		if (sig != SIGINT && sig != SIGPIPE)
		    sflag = 1;
		if (job == thisjob && sig == SIGINT)
		    doputnl = 1;
	    } else if (WIFSTOPPED(pn->status)) {
		sig = WSTOPSIG(pn->status);
		if ((int)strlen(sigmsg(sig)) > len)
		    len = strlen(sigmsg(sig));
		if (job == thisjob && sig == SIGTSTP)
		    doputnl = 1;
	    } else if (isset(PRINTEXITVALUE) && isset(SHINSTDIN) &&
		       WEXITSTATUS(pn->status))
		sflag = 1;
	}
    }

/* print if necessary */

    if (interact && jobbing && ((jn->stat & STAT_STOPPED) || sflag ||
				job != thisjob)) {
	int len2, fline = 1;
	Process qn;

	if (!synch)
	    trashzle();
	if (doputnl && !synch)
	    putc('\n', fout);
	for (pn = jn->procs; pn;) {
	    len2 = ((job == thisjob) ? 5 : 10) + len;	/* 2 spaces */
	    if (lng)
		qn = pn->next;
	    else {
		for (qn = pn->next; qn; qn = qn->next) {
		    if (qn->status != pn->status)
			break;
		    if ((int)strlen(qn->text) + len2 + ((qn->next) ? 3 : 0) > lineleng)
			break;
		    len2 += strlen(qn->text) + 2;
		}
	    }
	    if (job != thisjob) {
		if (fline) {
		    fprintf(fout, "[%ld]  %c ",
			    (long)(jn - jobtab),
			    (job == curjob) ? '+'
			    : (job == prevjob) ? '-' : ' ');
		}
		else
		    fprintf(fout, (job > 9) ? "        " : "       ");
	    } else
		fprintf(fout, "zsh: ");
	    if (lng) {
		if (lng == 1)
		    fprintf(fout, "%ld ", (long) pn->pid);
		else {
		    pid_t x = jn->gleader;

		    fprintf(fout, "%ld ", (long) x);
		    do
			skip++;
		    while ((x /= 10));
		    skip++;
		    lng = 0;
		}
	    } else
		fprintf(fout, "%*s", skip, "");
	    if (pn->status == SP_RUNNING) {
		if (!conted)
		    fprintf(fout, "running%*s", len - 7 + 2, "");
		else
		    fprintf(fout, "continued%*s", len - 9 + 2, "");
	    } else if (WIFEXITED(pn->status)) {
		if (WEXITSTATUS(pn->status))
		    fprintf(fout, "exit %-4d%*s", WEXITSTATUS(pn->status),
			    len - 9 + 2, "");
		else
		    fprintf(fout, "done%*s", len - 4 + 2, "");
	    } else if (WIFSTOPPED(pn->status))
		fprintf(fout, "%-*s", len + 2, sigmsg(WSTOPSIG(pn->status)));
	    else if (WCOREDUMP(pn->status)) {
		fprintf(fout, "%s (core dumped)%*s",
			sigmsg(WTERMSIG(pn->status)),
			(int)(len - 14 + 2 - strlen(sigmsg(WTERMSIG(pn->status)))), "");
	    } else
		fprintf(fout, "%-*s", len + 2, sigmsg(WTERMSIG(pn->status)));
	    for (; pn != qn; pn = pn->next)
		fprintf(fout, (pn->next) ? "%s | " : "%s", pn->text);
	    putc('\n', fout);
	    fline = 0;
	}
	fflush(fout);
    } else if (doputnl && interact && !synch) {
	putc('\n', fout);
	fflush(fout);
    }

/* print "(pwd now: foo)" messages */

    if (interact && job == thisjob && strcmp(jn->pwd, pwd)) {
	fprintf(shout, "(pwd now: ");
	fprintdir(pwd, shout);
	fprintf(shout, ")\n");
	fflush(shout);
    }
/* delete job if done */

    if (jn->stat & STAT_DONE) {
	if (should_report_time(jn))
	    dumptime(jn);
	deletejob(jn);
	if (job == curjob) {
	    curjob = prevjob;
	    prevjob = job;
	}
	if (job == prevjob)
	    setprevjob();
    } else
	jn->stat &= ~STAT_CHANGED;
}

/**/
void
deletefilelist(LinkList file_list)
{
    char *s;
    if (file_list) {
	while ((s = (char *)getlinknode(file_list))) {
	    unlink(s);
	    zsfree(s);
	}
	zfree(file_list, sizeof(struct linklist));
    }
}

/**/
void
deletejob(Job jn)
{
    struct process *pn, *nx;

    if (jn->stat & STAT_ATTACH) {
	attachtty(mypgrp);
	adjustwinsize(0);
    }

    for (pn = jn->procs; pn; pn = nx) {
	nx = pn->next;
	zfree(pn, sizeof(struct process));
    }
    zsfree(jn->pwd);

    deletefilelist(jn->filelist);

    if (jn->ty)
	zfree(jn->ty, sizeof(struct ttyinfo));

    if (jn->stat & STAT_WASSUPER)
	deletejob(jobtab + jn->other);
    *jn = zero;
}

/* set the previous job to something reasonable */

/**/
void
setprevjob(void)
{
    int i;

    /* #define for easier edit if bits change, especially STAT_is_SUBJOB */
#define STAT_is_INUSE (jobtab[i].stat & STAT_INUSE)
#define STAT_is_SUBJOB (jobtab[i].stat & (STAT_SUBJOB|STAT_NOPRINT))
#define STAT_is_STOPPED \
    ((jobtab[i].stat & (STAT_INUSE|STAT_STOPPED)) == (STAT_INUSE|STAT_STOPPED))

    for (i = MAXJOB - 1; i; i--)
	if (STAT_is_STOPPED && !STAT_is_SUBJOB &&
	    i != curjob && i != thisjob) {
	    prevjob = i;
	    return;
	}

    for (i = MAXJOB - 1; i; i--)
	if (STAT_is_INUSE && !STAT_is_SUBJOB &&
	    i != curjob && i != thisjob) {
	    prevjob = i;
	    return;
	}

    prevjob = -1;

#undef STAT_is_INUSE
#undef STAT_is_SUBJOB
#undef STAT_is_STOPPED
}

/* add a process to the current job */

/**/
void
addproc(pid_t pid, char *text)
{
    Process pn;
    struct timezone dummy_tz;

    pn = (Process) zcalloc(sizeof *pn);
    pn->pid = pid;
    if (text)
	strcpy(pn->text, text);
    else
	*pn->text = '\0';
    gettimeofday(&pn->bgtime, &dummy_tz);
    pn->status = SP_RUNNING;
    pn->next = NULL;

    /* if this is the first process we are adding to *
     * the job, then it's the group leader.          */
    if (!jobtab[thisjob].gleader)
	jobtab[thisjob].gleader = pid;

    /* attach this process to end of process list of current job */
    if (jobtab[thisjob].procs) {
	Process n;

	for (n = jobtab[thisjob].procs; n->next; n = n->next);
	pn->next = NULL;
	n->next = pn;
    } else {
	/* first process for this job */
	jobtab[thisjob].procs = pn;
    }
    /* If the first process in the job finished before any others were *
     * added, maybe STAT_DONE got set incorrectly.  This can happen if *
     * a $(...) was waited for and the last existing job in the        *
     * pipeline was already finished.  We need to be very careful that *
     * there was no call to printjob() between then and now, else      *
     * the job will already have been deleted from the table.          */
    jobtab[thisjob].stat &= ~STAT_DONE;
}

/* Check if we have files to delete.  We need to check this to see *
 * if it's all right to exec a command without forking in the last *
 * component of subshells or after the `-c' option.                */

/**/
int
havefiles(void)
{
    int i;

    for (i = 1; i < MAXJOB; i++)
	if (jobtab[i].stat && jobtab[i].filelist)
	    return 1;
    return 0;

}

/* wait for a particular process */

/**/
void
waitforpid(pid_t pid)
{
    int first = 1;

    /* child_block() around this loop in case #ifndef WNOHANG */
    child_block();		/* unblocked in child_suspend() */
    while (!errflag && (kill(pid, 0) >= 0 || errno != ESRCH)) {
	if (first)
	    first = 0;
	else
	    kill(pid, SIGCONT);

	child_suspend(SIGINT);
	child_block();
    }
    child_unblock();
}

/* wait for a job to finish */

/**/
void
waitjob(int job, int sig)
{
    Job jn = jobtab + job;

    child_block();		 /* unblocked during child_suspend() */
    if (jn->procs) {		 /* if any forks were done         */
	jn->stat |= STAT_LOCKED;
	if (jn->stat & STAT_CHANGED)
	    printjob(jn, !!isset(LONGLISTJOBS), 1);
	while (!errflag && jn->stat &&
	       !(jn->stat & STAT_DONE) &&
	       !(interact && (jn->stat & STAT_STOPPED))) {
	    child_suspend(sig);
	    /* Commenting this out makes ^C-ing a job started by a function
	       stop the whole function again.  But I guess it will stop
	       something else from working properly, we have to find out
	       what this might be.  --oberon

	    errflag = 0; */
	    if (subsh) {
		killjb(jn, SIGCONT);
		jn->stat &= ~STAT_STOPPED;
	    }
	    if (jn->stat & STAT_SUPERJOB)
		if (handle_sub(jn - jobtab, 1))
		    break;
	    child_block();
	}
    } else
	deletejob(jn);
    child_unblock();
}

/* wait for running job to finish */

/**/
void
waitjobs(void)
{
    waitjob(thisjob, 0);
    thisjob = -1;
}

/* clear job table when entering subshells */

/**/
void
clearjobtab(void)
{
    int i;

    for (i = 1; i < MAXJOB; i++) {
	if (jobtab[i].pwd)
	    zsfree(jobtab[i].pwd);
	if (jobtab[i].ty)
	    zfree(jobtab[i].ty, sizeof(struct ttyinfo));
    }

    memset(jobtab, 0, sizeof(jobtab)); /* zero out table */
}

/* Get a free entry in the job table and initialize it. */

/**/
int
initjob(void)
{
    int i;

    for (i = 1; i < MAXJOB; i++)
	if (!jobtab[i].stat) {
	    jobtab[i].stat = STAT_INUSE;
	    jobtab[i].pwd = ztrdup(pwd);
	    jobtab[i].gleader = 0;
	    return i;
	}

    zerr("job table full or recursion limit exceeded", NULL, 0);
    return -1;
}

/* print pids for & */

/**/
void
spawnjob(void)
{
    Process pn;

    /* if we are not in a subshell */
    if (!subsh) {
	if (curjob == -1 || !(jobtab[curjob].stat & STAT_STOPPED)) {
	    curjob = thisjob;
	    setprevjob();
	} else if (prevjob == -1 || !(jobtab[prevjob].stat & STAT_STOPPED))
	    prevjob = thisjob;
	if (interact && jobbing && jobtab[thisjob].procs) {
	    fprintf(stderr, "[%d]", thisjob);
	    for (pn = jobtab[thisjob].procs; pn; pn = pn->next)
		fprintf(stderr, " %ld", (long) pn->pid);
	    fprintf(stderr, "\n");
	    fflush(stderr);
	}
    }
    if (!jobtab[thisjob].procs)
	deletejob(jobtab + thisjob);
    else
	jobtab[thisjob].stat |= STAT_LOCKED;
    thisjob = -1;
}

static long clktck = 0;

static void
set_clktck(void)
{
#ifdef _SC_CLK_TCK
    if (!clktck)
	/* fetch clock ticks per second from *
	 * sysconf only the first time       */
	clktck = sysconf(_SC_CLK_TCK);
#else
# ifdef __NeXT__
    /* NeXTStep 3.3 defines CLK_TCK wrongly */
    clktck = 60;
# else
#  ifdef CLK_TCK
    clktck = CLK_TCK;
#  else
#   ifdef HZ
     clktck = HZ;
#   else
     clktck = 60;
#   endif
#  endif
# endif
#endif
}

/* Check whether shell should report the amount of time consumed   *
 * by job.  This will be the case if we have preceded the command  *
 * with the keyword time, or if REPORTTIME is non-negative and the *
 * amount of time consumed by the job is greater than REPORTTIME   */

/**/
int
should_report_time(Job j)
{
    Value v;
    char *s = "REPORTTIME";
    int reporttime;

    /* if the time keyword was used */
    if (j->stat & STAT_TIMED)
	return 1;

    if (!(v = getvalue(&s, 0)) || (reporttime = getintvalue(v)) < 0)
	return 0;

    /* can this ever happen? */
    if (!j->procs)
	return 0;

    set_clktck();
    return ((j->procs->ti.ut + j->procs->ti.st) / clktck >= reporttime);
}

/**/
void
printhhmmss(double secs)
{
    int mins = (int) secs / 60;
    int hours = mins / 60;

    secs -= 60 * mins;
    mins -= 60 * hours;
    if (hours)
	fprintf(stderr, "%d:%02d:%05.2f", hours, mins, secs);
    else if (mins)
	fprintf(stderr,      "%d:%05.2f",        mins, secs);
    else
	fprintf(stderr,           "%.3f",              secs);
}

/**/
void
printtime(struct timeval *real, struct timeinfo *ti, char *desc)
{
    char *s;
    double elapsed_time, user_time, system_time;
    int percent;

    if (!desc)
	desc = "";

    set_clktck();
    /* go ahead and compute these, since almost every TIMEFMT will have them */
    elapsed_time = real->tv_sec + real->tv_usec / 1000000.0;
    user_time    = ti->ut / (double) clktck;
    system_time  = ti->st / (double) clktck;
    percent      =  100.0 * (ti->ut + ti->st)
	/ (clktck * real->tv_sec + clktck * real->tv_usec / 1000000.0);

    if (!(s = getsparam("TIMEFMT")))
	s = DEFAULT_TIMEFMT;

    for (; *s; s++)
	if (*s == '%')
	    switch (*++s) {
	    case 'E':
		fprintf(stderr, "%4.2fs", elapsed_time);
		break;
	    case 'U':
		fprintf(stderr, "%4.2fs", user_time);
		break;
	    case 'S':
		fprintf(stderr, "%4.2fs", system_time);
		break;
	    case '*':
		switch (*++s) {
		case 'E':
		    printhhmmss(elapsed_time);
		    break;
		case 'U':
		    printhhmmss(user_time);
		    break;
		case 'S':
		    printhhmmss(system_time);
		    break;
		default:
		    fprintf(stderr, "%%*");
		    s--;
		    break;
		}
		break;
	    case 'P':
		fprintf(stderr, "%d%%", percent);
		break;
	    case 'J':
		fprintf(stderr, "%s", desc);
		break;
	    case '%':
		putc('%', stderr);
		break;
	    case '\0':
		s--;
		break;
	    default:
		fprintf(stderr, "%%%c", *s);
		break;
	} else
	    putc(*s, stderr);
    putc('\n', stderr);
    fflush(stderr);
}

/**/
void
dumptime(Job jn)
{
    Process pn;

    if (!jn->procs)
	return;
    for (pn = jn->procs; pn; pn = pn->next)
	printtime(dtime(&dtimeval, &pn->bgtime, &pn->endtime), &pn->ti, pn->text);
}

/**/
void
shelltime(void)
{
    struct timeinfo ti;
    struct timezone dummy_tz;
    struct tms buf;

    times(&buf);
    ti.ut = buf.tms_utime;
    ti.st = buf.tms_stime;
    gettimeofday(&now, &dummy_tz);
    printtime(dtime(&dtimeval, &shtimer, &now), &ti, "shell");
    ti.ut = buf.tms_cutime;
    ti.st = buf.tms_cstime;
    printtime(dtime(&dtimeval, &shtimer, &now), &ti, "children");
}

/* see if jobs need printing */
 
/**/
void
scanjobs(void)
{
    int i;
 
    for (i = 1; i < MAXJOB; i++)
        if (jobtab[i].stat & STAT_CHANGED)
            printjob(jobtab + i, 0, 1);
}

/* check to see if user has jobs running/stopped */

/**/
void
checkjobs(void)
{
    int i;

    for (i = 1; i < MAXJOB; i++)
	if (i != thisjob && (jobtab[i].stat & STAT_LOCKED) &&
	    !(jobtab[i].stat & STAT_NOPRINT))
	    break;
    if (i < MAXJOB) {
	if (jobtab[i].stat & STAT_STOPPED) {

#ifdef USE_SUSPENDED
	    zerr("you have suspended jobs.", NULL, 0);
#else
	    zerr("you have stopped jobs.", NULL, 0);
#endif

	} else
	    zerr("you have running jobs.", NULL, 0);
	stopmsg = 1;
    }
}

