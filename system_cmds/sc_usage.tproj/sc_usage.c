/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
cc -I. -DPRIVATE -D__APPLE_PRIVATE -O -o sc_usage sc_usage.c -lncurses
*/

#define	Default_DELAY	1	/* default delay interval */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ptrace.h>

#include <libc.h>
#include <termios.h>
#include <curses.h>

#include <sys/ioctl.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <sys/sysctl.h>
#include <errno.h>
#include <mach/mach_time.h>
#include <err.h>


/* Number of lines of header information on the standard screen */
#define	HEADER_LINES	5

int    newLINES = 0;
int    Header_lines = HEADER_LINES;

int    how_to_sort = 0;
int    no_screen_refresh = 0;
int    execute_flag = 0;
int    topn = 0;
int    pid;
int    called = 0;
int    sort_now = 0;
int    waiting_index = 0;
FILE   *dfp = 0;  /*Debug output file */
long   start_time = 0;

#define SAMPLE_SIZE 20000

#define DBG_ZERO_FILL_FAULT   1
#define DBG_PAGEIN_FAULT      2
#define DBG_COW_FAULT         3
#define DBG_CACHE_HIT_FAULT   4

#define MAX_SC 1024
#define MAX_THREADS 16
#define MAX_NESTED  8
#define MAX_FAULTS  5


#define NUMPARMS 23


char *state_name[] = {
        "Dont Know",
	"Running S",
	"Running U",
	"Waiting",
	"Pre-empted",
};

#define DONT_KNOW   0
#define KERNEL_MODE 1
#define USER_MODE   2
#define WAITING     3
#define PREEMPTED   4

struct entry {
        int  sc_state;
        int  type;
        int  code;
        double otime;
        double stime;
        double ctime;
        double wtime;
};

struct th_info {
        int  thread;
        int  depth;
        int  vfslookup;
        int  curpri;
        long *pathptr;
        long pathname[NUMPARMS + 1];
        struct entry th_entry[MAX_NESTED];
};

struct sc_entry {
        char name[32];
        int  delta_count;
        int  total_count;
        int  waiting;
        unsigned int stime_secs;
        double       stime_usecs;
        unsigned int wtime_secs;
        double       wtime_usecs;
        unsigned int delta_wtime_secs;
        double       delta_wtime_usecs;
};

struct th_info th_state[MAX_THREADS];
struct sc_entry faults[MAX_FAULTS];

struct sc_entry *sc_tab;
int  *msgcode_tab;
int    msgcode_cnt;          /* number of MSG_ codes */

int    num_of_threads = 0;
int    now_collect_cpu_time = 0;

unsigned int utime_secs;
double       utime_usecs;

int          in_idle = 0;
unsigned int itime_secs;
double       itime_usecs;
unsigned int delta_itime_secs;
double       delta_itime_usecs;
double       idle_start;

int          in_other = 0;
unsigned int otime_secs;
double       otime_usecs;
unsigned int delta_otime_secs;
double       delta_otime_usecs;
double       other_start;

int    max_sc = 0;
int    bsc_base = 0;
int    msc_base = 0;
int	   mach_idle = 0;
int    mach_sched = 0;
int    mach_stkhandoff = 0;
int    vfs_lookup = 0;
int    mach_vmfault = 0;
int    bsc_exit = 0;
int    *sort_by_count;
int    *sort_by_wtime;

char   proc_name[32];

#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc

int    preempted;
int    csw;
int    total_faults;
int    scalls;

/* Default divisor */
#define DIVISOR 16.6666        /* Trace divisor converts to microseconds */
double divisor = DIVISOR;


int mib[6];
size_t needed;
char  *my_buffer;

kbufinfo_t bufinfo = {0, 0, 0, 0};

int trace_enabled = 0;
int set_remove_flag = 1;

struct kinfo_proc *kp_buffer = 0;
int kp_nentries = 0;

extern char **environ;

void set_enable();
void set_pidcheck();
void set_remove();
void set_numbufs();
void set_init();
void quit(char *);
int argtopid(char *);
int argtoi(int, char*, char*, int);


/*
 *  signal handlers
 */

void leave()			/* exit under normal conditions -- INT handler */
{

        if (no_screen_refresh == 0) {
	        move(LINES - 1, 0);
		refresh();
		endwin();
	}
	set_enable(0);
	set_pidcheck(pid, 0);
	set_remove();
	exit(0);
}

void err_leave(s)	/* exit under error conditions */
char *s;
{

        if (no_screen_refresh == 0) {
	        move(LINES - 1, 0);
		refresh();
		endwin();
	}

        printf("sc_usage: ");
	if (s)
		printf("%s ", s);

	set_enable(0);
	set_pidcheck(pid, 0);
	set_remove();

	exit(1);
}

void sigwinch()
{
        if (no_screen_refresh == 0)
	        newLINES = 1;
}

int
exit_usage(char *myname) {

        fprintf(stderr, "Usage: %s [-c codefile] [-e] [-l] [-sn] pid | cmd | -E execute path\n", myname);
	fprintf(stderr, "  -c         name of codefile containing mappings for syscalls\n");
	fprintf(stderr, "             Default is /usr/share/misc/trace.codes\n");
	fprintf(stderr, "  -e         enable sort by call count\n");
	fprintf(stderr, "  -l         turn off top style output\n");
	fprintf(stderr, "  -sn        change sample rate to every n seconds\n");
	fprintf(stderr, "  pid        selects process to sample\n");
	fprintf(stderr, "  cmd        selects command to sample\n");
	fprintf(stderr, "  -E         Execute the given path and optional arguments\n");

	exit(1);
}


#define usec_to_1000ths(t)       ((t) / 1000)

void print_time(char *p, unsigned int useconds, unsigned int seconds)
{
	long	minutes, hours;

	minutes = seconds / 60;
	hours = minutes / 60;

	if (minutes < 100) { // up to 100 minutes
		sprintf(p, "%2ld:%02ld.%03ld", minutes, (unsigned long)(seconds % 60),
			(unsigned long)usec_to_1000ths(useconds));
	}
	else if (hours < 100) { // up to 100 hours
		sprintf(p, "%2ld:%02ld:%02ld ", hours, (minutes % 60),
				(unsigned long)(seconds % 60));
	}
	else {
		sprintf(p, "%4ld hrs ", hours);
	}
}

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	*myname   = "sc_usage";
	char    *codefile = "/usr/share/misc/trace.codes";
	char    ch;
	char    *ptr;
	int	delay = Default_DELAY;
        void screen_update();
	void sort_scalls();
	void sc_tab_init();
	void getdivisor();
	void reset_counters();

	if ( geteuid() != 0 ) {
	      printf("'sc_usage' must be run as root...\n");
	      exit(1);
	}

	/* get our name */
	if (argc > 0) {
		if ((myname = rindex(argv[0], '/')) == 0) {
			myname = argv[0];
		}
		else {
			myname++;
		}
	}

	while ((ch = getopt(argc, argv, "c:els:d:E")) != EOF) {
	       switch(ch) {
		case 's':
		        delay = argtoi('s', "decimal number", optarg, 10);
			break;
		case 'e':
			how_to_sort = 1;
			break;
		case 'l':
		        no_screen_refresh = 1;
			break;
	        case 'c':
		        codefile = optarg;
		        break;
	        case 'E':
		        execute_flag = 1;
			break;
		default:
		  /*		        exit_usage(myname);*/
		  exit_usage("default");
	       }
	}
	argc -= optind;
	//argv += optind;

	sc_tab_init(codefile);

	if (argc)
	  {
	    if (!execute_flag)
	      {
		/* parse a pid or a command */
		if((pid = argtopid(argv[optind])) < 0 )
		  exit_usage(myname);
	      }
	    else
	      { /* execute this command */

		uid_t uid, euid;
		gid_t gid, egid;
		
		ptr = strrchr(argv[optind], '/');
		if (ptr)
		  ptr++;
		else
		  ptr = argv[optind];

		strncpy(proc_name, ptr, sizeof(proc_name)-1);
		proc_name[sizeof(proc_name)-1] = '\0';

  		uid= getuid();
		gid= getgid();
		euid= geteuid();
		egid= getegid();

		seteuid(uid);
		setegid(gid);

		fprintf(stderr, "Starting program: %s\n", argv[optind]);
		fflush(stdout);
		fflush(stderr);
		switch ((pid = vfork()))
		  {
		  case -1:
		    perror("vfork: ");
		    exit(1);
		  case 0: /* child */
		    setsid();
		    ptrace(0,(pid_t)0,(caddr_t)0,0); /* PT_TRACE_ME */
		    execve(argv[optind], &argv[optind], environ);
		    perror("execve:");
		    exit(1);
		  }

		seteuid(euid);
		setegid(egid);
	      }
	  }
	else
	  {
	    exit_usage(myname);
	  }


	if (no_screen_refresh == 0) {

	        /* initializes curses and screen (last) */
		if (initscr() == (WINDOW *) 0)
		  {
		    printf("Unrecognized TERM type, try vt100\n");
		    exit(1);
		  }
		cbreak();
		timeout(100);
		noecho();

		clear();
		refresh();
	}


	/* set up signal handlers */
	signal(SIGINT, leave);
	signal(SIGQUIT, leave);
        signal(SIGHUP, leave);
        signal(SIGTERM, leave);
	signal(SIGWINCH, sigwinch);

        if (no_screen_refresh == 0)
	        topn = LINES - Header_lines;
	else {
	        topn = 1024;
		COLS = 80;
	}
	if ((my_buffer = malloc(SAMPLE_SIZE * sizeof(kd_buf))) == (char *)0)
	    quit("can't allocate memory for tracing info\n");

	set_remove();
	set_numbufs(SAMPLE_SIZE);
	set_init();
	set_pidcheck(pid, 1);
	set_enable(1);
	if (execute_flag)
	  ptrace(7, pid, (caddr_t)1, 0);  /* PT_CONTINUE */
	getdivisor();

	if (delay == 0)
	        delay = 1;
	if ((sort_now = 10 / delay) < 2)
	        sort_now = 2;

	(void)sort_scalls();
	(void)screen_update();

	/* main loop */

	while (1) {
	        int     i;
		char    c;
		void    sample_sc();
		
	        for (i = 0; i < (10 * delay) && newLINES == 0; i++) {

			if (no_screen_refresh == 0) {
			        if ((c = getch()) != ERR && (char)c == 'q') 
				        leave();
				if (c != ERR)
				        reset_counters();
			} else
			        usleep(100000);
			sample_sc();
		}
		(void)sort_scalls();

	        if (newLINES) {
		        /*
			  No need to check for initscr error return.
			  We won't get here if it fails on the first call.
			*/
		        endwin();
			clear();
			refresh();

		        topn = LINES - Header_lines;
		        newLINES = 0;
		}
		(void)screen_update();
	}
}

void
print_row(struct sc_entry *se, int no_wtime) {
	char    tbuf[256];
	int     clen;

	if (se->delta_count)
	        sprintf(tbuf, "%-23.23s    %8d(%d)", se->name, se->total_count, se->delta_count);
	else
	        sprintf(tbuf, "%-23.23s    %8d", se->name, se->total_count);
	clen = strlen(tbuf);

	memset(&tbuf[clen], ' ', 45 - clen);

	print_time(&tbuf[45], (unsigned long)(se->stime_usecs), se->stime_secs);
	clen = strlen(tbuf);

	if (no_wtime == 0 && (se->wtime_usecs || se->wtime_secs)) {
	        sprintf(&tbuf[clen], "  ");
		clen += strlen(&tbuf[clen]);

		print_time(&tbuf[clen], (unsigned long)(se->wtime_usecs), se->wtime_secs);
		clen += strlen(&tbuf[clen]);

		if (se->waiting || se->delta_wtime_usecs || se->delta_wtime_secs) {

		        sprintf(&tbuf[clen], "(");
			clen += strlen(&tbuf[clen]);

			print_time(&tbuf[clen], (unsigned long)(se->delta_wtime_usecs),
				                                se->delta_wtime_secs);
			clen += strlen(&tbuf[clen]);

			sprintf(&tbuf[clen], ")");
			clen += strlen(&tbuf[clen]);

			if (se->waiting) {
			        if (se->waiting == 1)
				        sprintf(&tbuf[clen], " W");
				else
				        sprintf(&tbuf[clen], " %d", se->waiting);
				clen += strlen(&tbuf[clen]);
			}
		}
	}
	sprintf(&tbuf[clen], "\n");

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
}


void screen_update()
{
        char    *p1, *p2, *p3;
	char    tbuf[256];
	int     clen;
	int     plen;
	int     n, i, rows;
	long	curr_time;
	long    elapsed_secs;
	int     hours;
	int     minutes;
	struct  sc_entry *se;
	int     output_lf;
	int     max_rows;
	struct th_info *ti;
	
	if (no_screen_refresh == 0) {
	        /* clear for new display */
	        erase();
		move(0, 0);
	}
	rows = 0;

	sprintf(tbuf, "%-14.14s", proc_name);
	clen = strlen(tbuf);
	
	if (preempted == 1)
	        p1 = "preemption ";
	else
	        p1 = "preemptions";
	if (csw == 1)
	        p2 = "context switch  ";
	else
	        p2 = "context switches";
	if (num_of_threads == 1)
	        p3 = "thread ";
	else
	        p3 = "threads";

	sprintf(&tbuf[clen], " %4d %s %4d %s %4d %s",
		preempted, p1, csw, p2, num_of_threads, p3);
	clen += strlen(&tbuf[clen]);

	/*
	 *  Display the current time.
	 *  "ctime" always returns a string that looks like this:
	 *  
	 *	Sun Sep 16 01:03:52 1973
	 *      012345678901234567890123
	 *	          1         2
	 *
	 *  We want indices 11 thru 18 (length 8).
	 */
	curr_time = time((long *)0);

	if (start_time == 0)
	  start_time = curr_time;

	elapsed_secs = curr_time - start_time;
	minutes = elapsed_secs / 60;
	hours = minutes / 60;

	memset(&tbuf[clen], ' ', 78 - clen);

	clen = 78 - 8;

	sprintf(&tbuf[clen], "%-8.8s\n", &(ctime(&curr_time)[11]));

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);

	if (total_faults == 1)
	        p1 = "fault ";
	else
	        p1 = "faults";
	if (scalls == 1)
	        p2 = "system call ";
	else
	        p2 = "system calls";
	sprintf(tbuf, "               %4d %s      %4d %s",
		total_faults, p1, scalls, p2);

	clen = strlen(tbuf);
	sprintf(&tbuf[clen], "                    %3ld:%02ld:%02ld\n", 
		(long)hours, (long)(minutes % 60), (long)(elapsed_secs % 60));

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);



	sprintf(tbuf, "\nTYPE                            NUMBER        CPU_TIME   WAIT_TIME\n");

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);

	sprintf(tbuf, "------------------------------------------------------------------------------\n");
	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
	rows = 0;



	sprintf(tbuf, "System         Idle                                     ");
	clen = strlen(tbuf);

	print_time(&tbuf[clen], (unsigned long)(itime_usecs), itime_secs);
	clen += strlen(&tbuf[clen]);

	if (delta_itime_usecs || delta_itime_secs) {

	        sprintf(&tbuf[clen], "(");
		clen += strlen(&tbuf[clen]);

		print_time(&tbuf[clen], (unsigned long)(delta_itime_usecs), delta_itime_secs);
		clen += strlen(&tbuf[clen]);

		sprintf(&tbuf[clen], ")");
		clen += strlen(&tbuf[clen]);
	}
        sprintf(&tbuf[clen], "\n");

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
	rows++;



	sprintf(tbuf, "System         Busy                                     ");
	clen = strlen(tbuf);

	print_time(&tbuf[clen], (unsigned long)(otime_usecs), otime_secs);
	clen += strlen(&tbuf[clen]);

	if (delta_otime_usecs || delta_otime_secs) {

	        sprintf(&tbuf[clen], "(");	
	clen += strlen(&tbuf[clen]);

		print_time(&tbuf[clen], (unsigned long)(delta_otime_usecs), delta_otime_secs);
		clen += strlen(&tbuf[clen]);

		sprintf(&tbuf[clen], ")");
		clen += strlen(&tbuf[clen]);
	}
        sprintf(&tbuf[clen], "\n");

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
	rows++;


	sprintf(tbuf, "%-14.14s Usermode                      ", proc_name);
	clen = strlen(tbuf);

	print_time(&tbuf[clen], (unsigned long)(utime_usecs), utime_secs);
	clen += strlen(&tbuf[clen]);
        sprintf(&tbuf[clen], "\n");

	if (tbuf[COLS-2] != '\n') {
	        tbuf[COLS-1] = '\n';
		tbuf[COLS] = 0;
	}
	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
	rows++;

	if (num_of_threads)
	        max_rows = topn - (num_of_threads + 3);
	else
	        max_rows = topn;

	for (output_lf = 1, n = 1; rows < max_rows && n < MAX_FAULTS; n++) {
		se = &faults[n];

	        if (se->total_count == 0)
		        continue;
		if (output_lf == 1) {
		        sprintf(tbuf, "\n");
			if (no_screen_refresh)
			        printf("%s", tbuf);
			else
			        printw(tbuf);
			rows++;

			if (rows >= max_rows)
			        break;
			output_lf = 0;
		}
		print_row(se, 0);
		rows++;
	}
	sprintf(tbuf, "\n");

	if (no_screen_refresh)
	        printf("%s", tbuf);
	else
	        printw(tbuf);
	rows++;

	for (i = 0; rows < max_rows; i++) {
	        if (how_to_sort)
		       n = sort_by_count[i];
		else
		       n = sort_by_wtime[i];
	        if (n == -1)
		        break;
		print_row(&sc_tab[n], 0);
		rows++;
	}
	if (no_screen_refresh == 0) {
	        sprintf(tbuf, "\n");

	        while (rows++ < max_rows)
		        printw(tbuf);
	} else
	        printf("\n");

	if (num_of_threads) {
	        sprintf(tbuf, "\nCURRENT_TYPE              LAST_PATHNAME_WAITED_FOR     CUR_WAIT_TIME THRD# PRI\n");

		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		if (no_screen_refresh)
		        printf("%s", tbuf);
		else
		        printw(tbuf);

	        sprintf(tbuf, "------------------------------------------------------------------------------\n");
		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		if (no_screen_refresh)
		        printf("%s", tbuf);
		else
		        printw(tbuf);
	}
	ti = &th_state[0];
		
	for (i = 0; i < num_of_threads; i++, ti++) {
	        struct entry *te;
		char 	*p;
		uint64_t now;
		int      secs, time_secs, time_usecs;

		now = mach_absolute_time();

		while (ti->thread == 0 && ti < &th_state[MAX_THREADS])
		        ti++;
		if (ti == &th_state[MAX_THREADS])
		        break;

	        if (ti->depth) {
		        te = &ti->th_entry[ti->depth - 1];

		        if (te->sc_state == WAITING) {
			        if (te->code)
				        sprintf(tbuf, "%-23.23s", sc_tab[te->code].name);
				else
				        sprintf(tbuf, "%-23.23s", "vm_fault");
			} else 
			        sprintf(tbuf, "%-23.23s", state_name[te->sc_state]);
		} else {
		        te = &ti->th_entry[0];
		        sprintf(tbuf, "%-23.23s", state_name[te->sc_state]);
		}
		clen = strlen(tbuf);
		
		/* print the tail end of the pathname */
		p = (char *)ti->pathname;

		plen = strlen(p);
		if (plen > 34)
		  plen -= 34;
		else
		  plen = 0;
		sprintf(&tbuf[clen], "   %-34.34s ", &p[plen]);

		clen += strlen(&tbuf[clen]);

		time_usecs = (unsigned long)(((double)now - te->otime) / divisor);
		secs = time_usecs / 1000000;
		time_usecs -= secs * 1000000;
		time_secs = secs;

		print_time(&tbuf[clen], time_usecs, time_secs);
		clen += strlen(&tbuf[clen]);
		sprintf(&tbuf[clen], "  %2d %3d\n", i, ti->curpri);

		if (tbuf[COLS-2] != '\n') {
		        tbuf[COLS-1] = '\n';
			tbuf[COLS] = 0;
		}
		if (no_screen_refresh)
		        printf("%s", tbuf);
		else
		        printw(tbuf);
	}
	if (no_screen_refresh == 0) {
	        move(0, 0);
	        refresh();
	} else
	        printf("\n=================\n");



	for (i = 0; i < (MAX_SC + msgcode_cnt); i++) {
	        if ((n = sort_by_count[i]) == -1)
		        break;
		sc_tab[n].delta_count = 0;
		sc_tab[n].waiting = 0;
		sc_tab[n].delta_wtime_usecs = 0;
		sc_tab[n].delta_wtime_secs = 0;
	}
	for (i = 1; i < MAX_FAULTS; i++) {
	        faults[i].delta_count = 0;
		faults[i].waiting = 0;
		faults[i].delta_wtime_usecs = 0;
		faults[i].delta_wtime_secs = 0;
	}
	preempted = 0;
	csw = 0;
	total_faults = 0;
	scalls = 0;
	delta_itime_secs = 0;
	delta_itime_usecs = 0;
	delta_otime_secs = 0;
	delta_otime_usecs = 0;
}

void
reset_counters() {
        int   i;

	for (i = 0; i < (MAX_SC + msgcode_cnt) ; i++) {
		sc_tab[i].delta_count = 0;
		sc_tab[i].total_count = 0;
		sc_tab[i].waiting = 0;
		sc_tab[i].delta_wtime_usecs = 0;
		sc_tab[i].delta_wtime_secs = 0;
		sc_tab[i].wtime_usecs = 0;
		sc_tab[i].wtime_secs = 0;
		sc_tab[i].stime_usecs = 0;
		sc_tab[i].stime_secs = 0;
	}
	for (i = 1; i < MAX_FAULTS; i++) {
	        faults[i].delta_count = 0;
	        faults[i].total_count = 0;
		faults[i].waiting = 0;
		faults[i].delta_wtime_usecs = 0;
		faults[i].delta_wtime_secs = 0;
		faults[i].wtime_usecs = 0;
		faults[i].wtime_secs = 0;
		faults[i].stime_usecs = 0;
		faults[i].stime_secs = 0;
	}
	preempted = 0;
	csw = 0;
	total_faults = 0;
	scalls = 0;
	called = 0;
	
	utime_secs = 0;
	utime_usecs = 0;
	itime_secs = 0;
	itime_usecs = 0;
	delta_itime_secs = 0;
	delta_itime_usecs = 0;
	otime_secs = 0;
	otime_usecs = 0;
	delta_otime_secs = 0;
	delta_otime_usecs = 0;
}

void
sc_tab_init(char *codefile) {
        int  code;
	int  n, cnt;
	int msgcode_indx=0;
	char name[56];
        FILE *fp;

	if ((fp = fopen(codefile,"r")) == (FILE *)0) {
		printf("Failed to open code description file %s\n", codefile);
		exit(1);
	}

	n = fscanf(fp, "%d\n", &cnt);
	if (n != 1)
	        return;

	/* Count Mach message MSG_ codes */
	for (msgcode_cnt=0;;) {
	        n = fscanf(fp, "%x%55s\n", &code, &name[0]);
		if (n != 2)
		  break;
		if (strncmp ("MSG_", &name[0], 4) == 0)
		  msgcode_cnt++;
		if (strcmp("USER_TEST", &name[0]) == 0)
		        break;
	}

	sc_tab = (struct sc_entry *)malloc((MAX_SC+msgcode_cnt) * sizeof (struct sc_entry));
        if(!sc_tab)
	    quit("can't allocate memory for system call table\n");
	bzero(sc_tab,(MAX_SC+msgcode_cnt) * sizeof (struct sc_entry));

	msgcode_tab = (int *)malloc(msgcode_cnt * sizeof(int));
        if (!msgcode_tab)
	    quit("can't allocate memory for msgcode table\n");
	bzero(msgcode_tab,(msgcode_cnt * sizeof(int)));

	sort_by_count = (int *)malloc((MAX_SC + msgcode_cnt + 1) * sizeof(int));
        if (!sort_by_count)
	    quit("can't allocate memory for sort_by_count table\n");
	bzero(sort_by_count,(MAX_SC + msgcode_cnt + 1) * sizeof(int));

	sort_by_wtime = (int *)malloc((MAX_SC + msgcode_cnt + 1) * sizeof(int));
        if (!sort_by_wtime)
	    quit("can't allocate memory for sort_by_wtime table\n");
	bzero(sort_by_wtime, (MAX_SC + msgcode_cnt + 1) * sizeof(int));


	rewind(fp);

	n = fscanf(fp, "%d\n", &cnt);

	if (n != 1)
	        return;

	for (;;) {
	        n = fscanf(fp, "%x%55s\n", &code, &name[0]);

		if (n != 2)
		        break;

		if (strcmp("MACH_vmfault", &name[0]) == 0) {
		        mach_vmfault = code;
			continue;
		}
		if (strcmp("MACH_SCHED", &name[0]) == 0) {
		        mach_sched = code;
			continue;
		}
		if (strcmp("MACH_STKHANDOFF", &name[0]) == 0) {
		        mach_stkhandoff = code;
			continue;
		}
		if (strcmp("MACH_IDLE", &name[0]) == 0) {
		        mach_idle = code;
			continue;
		}
		if (strcmp("VFS_LOOKUP", &name[0]) == 0) {
		        vfs_lookup = code;
			continue;
		}
		if (strcmp("BSC_SysCall", &name[0]) == 0) {
		        bsc_base = code;
			continue;
		}
		if (strcmp("MACH_SysCall", &name[0]) == 0) {
		        msc_base = code;
			continue;
		}
		if (strcmp("BSC_exit", &name[0]) == 0) {
		        bsc_exit = code;
			continue;
		}
		if (strncmp("MSG_", &name[0], 4) == 0) {
		        msgcode_tab[msgcode_indx] = ((code & 0x00ffffff) >>2);
		        n = MAX_SC + msgcode_indx;
			strncpy(&sc_tab[n].name[0], &name[4], 31 );
			msgcode_indx++;
			continue;
		}
		if (strncmp("MSC_", &name[0], 4) == 0) {
		        n = 512 + ((code>>2) & 0x1ff);
			strcpy(&sc_tab[n].name[0], &name[4]);
			continue;
		}
		if (strncmp("BSC_", &name[0], 4) == 0) {
		        n = (code>>2) & 0x1ff;
			strcpy(&sc_tab[n].name[0], &name[4]);
			continue;
		}
		if (strcmp("USER_TEST", &name[0]) == 0)
		        break;
	}
	strcpy(&faults[1].name[0], "zero_fill");
	strcpy(&faults[2].name[0], "pagein");
	strcpy(&faults[3].name[0], "copy_on_write");
	strcpy(&faults[4].name[0], "cache_hit");
}

void
find_proc_names()
{
	size_t			bufSize = 0;
	struct kinfo_proc       *kp;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0)
	        quit("trace facility failure, KERN_PROC_ALL\n");

	if((kp = (struct kinfo_proc *)malloc(bufSize)) == (struct kinfo_proc *)0)
	    quit("can't allocate memory for proc buffer\n");
	
	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0)
	        quit("trace facility failure, KERN_PROC_ALL\n");

        kp_nentries = bufSize/ sizeof(struct kinfo_proc);
	kp_buffer = kp;
}

struct th_info *find_thread(int thread) {
       struct th_info *ti;

       for (ti = th_state; ti < &th_state[MAX_THREADS]; ti++) {
	       if (ti->thread == thread)
		       return(ti);
       }
       return ((struct th_info *)0);
}


int
cmp_wtime(struct sc_entry *s1, struct sc_entry *s2) {

        if (s1->wtime_secs < s2->wtime_secs)
	        return 0;
        if (s1->wtime_secs > s2->wtime_secs)
	        return 1;
        if (s1->wtime_usecs <= s2->wtime_usecs)
	        return 0;
	return 1;
}


void
sort_scalls() {
        int  i, n, k, cnt, secs;
	struct th_info *ti;
	struct sc_entry *se;
	struct entry *te;
	uint64_t now;

	now = mach_absolute_time();

	for (ti = th_state; ti < &th_state[MAX_THREADS]; ti++) {
	        if (ti->thread == 0)
		        continue;

	        if (ti->depth) {
		        te = &ti->th_entry[ti->depth-1];

		        if (te->sc_state == WAITING) {
			        if (te->code)
				        se = &sc_tab[te->code];
				else
				        se = &faults[DBG_PAGEIN_FAULT];
				se->waiting++;
			        se->wtime_usecs += ((double)now - te->stime) / divisor;
			        se->delta_wtime_usecs += ((double)now - te->stime) / divisor;
				te->stime = (double)now;

				secs = se->wtime_usecs / 1000000;
				se->wtime_usecs -= secs * 1000000;
				se->wtime_secs += secs;

				secs = se->delta_wtime_usecs / 1000000;
				se->delta_wtime_usecs -= secs * 1000000;
				se->delta_wtime_secs += secs;
			}
		} else {
		        te = &ti->th_entry[0];

			if (te->sc_state == PREEMPTED) {
			        if ((unsigned long)(((double)now - te->otime) / divisor) > 5000000) {
				        ti->thread = 0;
					ti->vfslookup = 0;
					ti->pathptr = (long *)NULL;
					ti->pathname[0] = 0;
					num_of_threads--;
				}
			}
		}
	}
        if ((called % sort_now) == 0) {
	        sort_by_count[0] = -1;
	        sort_by_wtime[0] = -1;
	        for (cnt = 1, n = 1; n < (MAX_SC + msgcode_cnt); n++) {
		        if (sc_tab[n].total_count) {
			        for (i = 0; i < cnt; i++) {
				        if ((k = sort_by_count[i]) == -1 ||
					        sc_tab[n].total_count > sc_tab[k].total_count) {

					        for (k = cnt - 1; k >= i; k--)
						        sort_by_count[k+1] = sort_by_count[k];
						sort_by_count[i] = n;
						break;
					}
				}
				if (how_to_sort == 0) {
				        for (i = 0; i < cnt; i++) {
					        if ((k = sort_by_wtime[i]) == -1 ||
						        cmp_wtime(&sc_tab[n], &sc_tab[k])) {

						        for (k = cnt - 1; k >= i; k--)
							        sort_by_wtime[k+1] = sort_by_wtime[k];
							sort_by_wtime[i] = n;
							break;
						}
					}
				}
				cnt++;
			}
		}
	}
	called++;
}

void
set_enable(int val) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;		/* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDENABLE\n");

	if (val)
	  trace_enabled = 1;
	else
	  trace_enabled = 0;
}

void
set_numbufs(int nbufs) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDSETBUF\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDSETUP\n");

}

void
set_pidcheck(int pid, int on_off) 
{
        kd_regtype kr;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDTR;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;
	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
	        if (on_off == 1) { 
		        printf("pid %d does not exist\n", pid);
			set_remove();
			exit(2);
                }
	}
}

void
get_bufinfo(kbufinfo_t *val)
{
        needed = sizeof (*val);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, val, &needed, 0, 0) < 0)
	        quit("trace facility failure, KERN_KDGETBUF\n");

}

void
set_remove() 
{
        extern int errno;

        errno = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	  {
	    set_remove_flag = 0;

	    if (errno == EBUSY)
		quit("the trace facility is currently in use...\n          fs_usage, sc_usage, and latency use this feature.\n\n");
	    else
	        quit("trace facility failure, KERN_KDREMOVE\n");
	  }
}

void
set_init() 
{       kd_regtype kr;

	kr.type = KDBG_RANGETYPE;
	kr.value1 = 0;
	kr.value2 = -1;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDSETREG\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDSETUP\n");

}

void
sample_sc()
{
	kd_buf *kd;
	int i, count;
	int secs;
	int find_msgcode();
	
        /* Get kernel buffer information */
	get_bufinfo(&bufinfo);
#ifdef OLD_KDEBUG
	set_enable(0);
#endif
	needed = bufinfo.nkdbufs * sizeof(kd_buf);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, my_buffer, &needed, NULL, 0) < 0)
	        quit("trace facility failure, KERN_KDREADTR\n");
	count = needed;

	if (bufinfo.flags & KDBG_WRAPPED) {
	        for (i = 0; i < MAX_THREADS; i++) {
		        th_state[i].depth = 0;
			th_state[i].thread = 0;
			th_state[i].vfslookup = 0;
			th_state[i].pathptr = (long *)NULL;
			th_state[i].pathname[0] = 0;
		}
		num_of_threads = 0;
	}
#ifdef OLD_KDEBUG
	set_remove();
	set_init();
	set_pidcheck(pid, 1);
	set_enable(1);          /* re-enable kernel logging */
#endif
	kd = (kd_buf *)my_buffer;

	for (i = 0; i < count; i++) {
	        int debugid, baseid, thread;
		int type, code;
		uint64_t now;
		struct th_info *ti, *switched_out, *switched_in;
		struct sc_entry *se;
		struct entry *te;

		thread  = kd[i].arg5;
		debugid = kd[i].debugid;
		type    = kd[i].debugid & DBG_FUNC_MASK;

		code = 0;
		switched_out = (struct th_info *)0;
		switched_in  = (struct th_info *)0;

		now = kd[i].timestamp & KDBG_TIMESTAMP_MASK;
		
		baseid = debugid & 0xffff0000;

		if (type == vfs_lookup) {
		        long *sargptr;

		        if ((ti = find_thread(thread)) == (struct th_info *)0)
			        continue;

		        if (ti->vfslookup == 1) {
			        ti->vfslookup++;
			        sargptr = ti->pathname;

				*sargptr++ = kd[i].arg2;
				*sargptr++ = kd[i].arg3;
				*sargptr++ = kd[i].arg4;
				/*
				 * NULL terminate the 'string'
				 */
				*sargptr = 0;

				ti->pathptr = sargptr;

			} else if (ti->vfslookup > 1) {
			        ti->vfslookup++;
			        sargptr = ti->pathptr;

				/*
				  We don't want to overrun our pathname buffer if the
				  kernel sends us more VFS_LOOKUP entries than we can
				  handle.
				*/

				if (sargptr >= &ti->pathname[NUMPARMS])
					continue;

				/*
				  We need to detect consecutive vfslookup entries.
				  So, if we get here and find a START entry,
				  fake the pathptr so we can bypass all further
				  vfslookup entries.
				*/

				if (debugid & DBG_FUNC_START)
				  {
				    ti->pathptr = &ti->pathname[NUMPARMS];
				    continue;
				  }

				*sargptr++ = kd[i].arg1;
				*sargptr++ = kd[i].arg2;
				*sargptr++ = kd[i].arg3;
				*sargptr++ = kd[i].arg4;
				/*
				 * NULL terminate the 'string'
				 */
				*sargptr = 0;

				ti->pathptr = sargptr;
			}
			continue;

		} else if (baseid == bsc_base)
		        code = (debugid >> 2) & 0x1ff;
		else if (baseid == msc_base)
		        code = 512 + ((debugid >> 2) & 0x1ff);
		else if (type == mach_idle) {
			if (debugid & DBG_FUNC_START) {
				switched_out = find_thread(kd[i].arg5);
				switched_in = 0;
			}
			else
			if (debugid & DBG_FUNC_END) {
				switched_in = find_thread(kd[i].arg5);
				switched_out = 0;
			}

			if (in_idle) {
			        itime_usecs += ((double)now - idle_start) / divisor;
			        delta_itime_usecs += ((double)now - idle_start) / divisor;
				in_idle = 0;
			} else if (in_other) {
			        otime_usecs += ((double)now - other_start) / divisor;
			        delta_otime_usecs += ((double)now - other_start) / divisor;
				in_other = 0;
			}
			if ( !switched_in) {
			        /*
					 * not one of the target proc's threads
					 */
			        if (now_collect_cpu_time) {
					        in_idle = 1;
							idle_start = (double)now;
					}
			}
			else {
			        if (now_collect_cpu_time) {
							in_idle = 0;
							in_other = 1;
							other_start = (double)now;
					}
			}
			if ( !switched_in && !switched_out)
			        continue;

		}
		else if (type == mach_sched || type == mach_stkhandoff) {
			switched_out = find_thread(kd[i].arg5);
			switched_in  = find_thread(kd[i].arg2);

			if (in_idle) {
			        itime_usecs += ((double)now - idle_start) / divisor;
			        delta_itime_usecs += ((double)now - idle_start) / divisor;
				in_idle = 0;
			} else if (in_other) {
			        otime_usecs += ((double)now - other_start) / divisor;
			        delta_otime_usecs += ((double)now - other_start) / divisor;
				in_other = 0;
			}
			if ( !switched_in) {
			        /*
					 * not one of the target proc's threads
					 */
			        if (now_collect_cpu_time) {
					        in_other = 1;
							other_start = (double)now;
					}
			}
			if ( !switched_in && !switched_out)
			        continue;

		}
	        else if ((baseid & 0xff000000) == 0xff000000) {
		        code = find_msgcode (debugid);
			if (!code)
			  continue;
		} else if (baseid != mach_vmfault)
		        continue;

		if (switched_out || switched_in) {
		        if (switched_out) {
			        ti = switched_out;
				ti->curpri = kd[i].arg3;

				if (ti->depth) {
				        te = &ti->th_entry[ti->depth-1];

					if (te->sc_state == KERNEL_MODE)
					        te->ctime += (double)now - te->stime;
					te->sc_state = WAITING; 

					ti->vfslookup = 1;

				} else {
				        te = &ti->th_entry[0];

					if (te->sc_state == USER_MODE)
					        utime_usecs += ((double)now - te->stime) / divisor;
					te->sc_state = PREEMPTED;
					preempted++;
				}
				te->stime = (double)now;
				te->otime = (double)now;
				now_collect_cpu_time = 1;
				csw++;
			}
			if (switched_in) {
			        ti = switched_in;
				ti->curpri = kd[i].arg4;

				if (ti->depth) {
				        te = &ti->th_entry[ti->depth-1];

					if (te->sc_state == WAITING)
					        te->wtime += (double)now - te->stime;
					te->sc_state = KERNEL_MODE; 
				} else {
				        te = &ti->th_entry[0];

					te->sc_state = USER_MODE;
				}
				te->stime = (double)now;
				te->otime = (double)now;
			}
			continue;
		} 
		if ((ti = find_thread(thread)) == (struct th_info *)0) {
		        for (ti = &th_state[0]; ti < &th_state[MAX_THREADS]; ti++) {
			        if (ti->thread == 0) {
				        ti->thread = thread;
					num_of_threads++;
					break;
				}
			}
			if (ti == &th_state[MAX_THREADS])
			        continue;
		}
		if (debugid & DBG_FUNC_START) {
		        ti->vfslookup = 0;

		        if (ti->depth) {
			        te = &ti->th_entry[ti->depth-1];

				if (te->sc_state == KERNEL_MODE)
				        te->ctime += (double)now - te->stime;
			} else {
			        te = &ti->th_entry[0];

				if (te->sc_state == USER_MODE)
				        utime_usecs += ((double)now - te->stime) / divisor;
			}
			te->stime = (double)now;
			te->otime = (double)now;

			if (ti->depth < MAX_NESTED) {
			        te = &ti->th_entry[ti->depth];

			        te->sc_state = KERNEL_MODE;
				te->type = type;
				te->code = code;
				te->stime = (double)now;
				te->otime = (double)now;
				te->ctime = (double)0;
				te->wtime = (double)0;
				ti->depth++;
			}

		} else if (debugid & DBG_FUNC_END) {
		        if (code) {
			        se = &sc_tab[code];
				scalls++;
			} else {
			        se = &faults[kd[i].arg4];
				total_faults++;
			}
			if (se->total_count == 0)
			        called = 0;
			se->delta_count++;
			se->total_count++;

		        while (ti->depth) {
			        te = &ti->th_entry[ti->depth-1];

			        if (te->type == type) {
				        se->stime_usecs += te->ctime / divisor;
					se->stime_usecs += ((double)now - te->stime) / divisor;

					se->wtime_usecs += te->wtime / divisor;
					se->delta_wtime_usecs += te->wtime / divisor;

					secs = se->stime_usecs / 1000000;
					se->stime_usecs -= secs * 1000000;
					se->stime_secs += secs;

					secs = se->wtime_usecs / 1000000;
					se->wtime_usecs -= secs * 1000000;
					se->wtime_secs += secs;

					secs = se->delta_wtime_usecs / 1000000;
					se->delta_wtime_usecs -= secs * 1000000;
					se->delta_wtime_secs += secs;

					ti->depth--;

					if (ti->depth == 0) {
					        /* 
						 * headed back to user mode
						 * start the time accumulation
						 */
					        te = &ti->th_entry[0];
					        te->sc_state = USER_MODE;
					} else
					        te = &ti->th_entry[ti->depth-1];

					te->stime = (double)now;
					te->otime = (double)now;

					break;
				}
				ti->depth--;

				if (ti->depth == 0) {
				        /* 
					 * headed back to user mode
					 * start the time accumulation
					 */
				        te = &ti->th_entry[0];
					te->sc_state = USER_MODE;
					te->stime = (double)now;
					te->otime = (double)now;
				}
			}
		}
	}
	secs = utime_usecs / 1000000;
	utime_usecs -= secs * 1000000;
	utime_secs += secs;

	secs = itime_usecs / 1000000;
	itime_usecs -= secs * 1000000;
	itime_secs += secs;

	secs = delta_itime_usecs / 1000000;
	delta_itime_usecs -= secs * 1000000;
	delta_itime_secs += secs;

	secs = otime_usecs / 1000000;
	otime_usecs -= secs * 1000000;
	otime_secs += secs;

	secs = delta_otime_usecs / 1000000;
	delta_otime_usecs -= secs * 1000000;
	delta_otime_secs += secs;
}

void
quit(char *s)
{
        if (trace_enabled)
	        set_enable(0);

	/* 
	   This flag is turned off when calling
	   quit() due to a set_remove() failure.
	*/
	if (set_remove_flag)
	        set_remove();

        printf("sc_usage: ");
	if (s)
		printf("%s ", s);

	exit(1);
}

void getdivisor()
{
  mach_timebase_info_data_t info;

  (void) mach_timebase_info (&info);

  divisor = ( (double)info.denom / (double)info.numer) * 1000;

}


int
argtopid(str)
        char *str;
{
        char *cp;
        int ret;
	int i;

	if (!kp_buffer)
	        find_proc_names();

        ret = (int)strtol(str, &cp, 10);
        if (cp == str || *cp) {
	  /* Assume this is a command string and find first matching pid */
	        for (i=0; i < kp_nentries; i++) {
		      if(kp_buffer[i].kp_proc.p_stat == 0)
		          continue;
		      else {
			  if(!strcmp(str, kp_buffer[i].kp_proc.p_comm))
			    {
			      strncpy(proc_name, kp_buffer[i].kp_proc.p_comm, sizeof(proc_name)-1);
			      proc_name[sizeof(proc_name)-1] = '\0';
			      return(kp_buffer[i].kp_proc.p_pid);
			    }
		      }
		}
	}
	else
	  {
	    for (i=0; i < kp_nentries; i++)
	      {
		if(kp_buffer[i].kp_proc.p_stat == 0)
		  continue;
		else if (kp_buffer[i].kp_proc.p_pid == ret) {
		    strncpy(proc_name, kp_buffer[i].kp_proc.p_comm, sizeof(proc_name)-1);
		    proc_name[sizeof(proc_name)-1] = '\0';
		    return(kp_buffer[i].kp_proc.p_pid);		      
		}
	      }
	  }
        return(-1);
}


/* Returns index into sc_tab for a mach msg entry */
int
find_msgcode(int debugid)
{

  int indx;

  for (indx=0; indx< msgcode_cnt; indx++)
    {
      if (msgcode_tab[indx] == ((debugid & 0x00ffffff) >>2))
	  return (MAX_SC+indx);
    }
  return (0);
}

int
argtoi(flag, req, str, base)
	int flag;
	char *req, *str;
	int base;
{
	char *cp;
	int ret;

	ret = (int)strtol(str, &cp, base);
	if (cp == str || *cp)
		errx(EINVAL, "-%c flag requires a %s", flag, req);
	return (ret);
}

