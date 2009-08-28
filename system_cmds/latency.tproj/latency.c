/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
   cc -I. -DPRIVATE -D__APPLE_PRIVATE -O -o latency latency.c -lncurses
*/

#include <mach/mach.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

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

#include <libutil.h>

#include <sys/sysctl.h>
#include <errno.h>
#include <err.h>

#include <mach/host_info.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mach_syscalls.h>
#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <libkern/OSTypes.h>

extern mach_port_t clock_port;

#define KERN_KDPIDEX    14


int      s_usec_10_bins[10];
int      s_usec_100_bins[10];
int      s_msec_1_bins[10];
int      s_msec_10_bins[5];
int      s_too_slow;
int      s_max_latency;
int      s_min_latency = 0;
long long s_total_latency = 0;
int      s_total_samples;
long     s_thresh_hold;
int      s_exceeded_threshold = 0;

int      i_usec_10_bins[10];
int      i_usec_100_bins[10];
int      i_msec_1_bins[10];
int      i_msec_10_bins[5];
int      i_too_slow;
int      i_max_latency;
int      i_min_latency = 0;
long long i_total_latency = 0;
int      i_total_samples;
long     i_thresh_hold;
int      i_exceeded_threshold = 0;

long     start_time;
long     curr_time;
long     refresh_time;

char     *policy_name;
int      my_policy;
int      my_pri = -1;
int      num_of_usecs_to_sleep = 1000;

#define N_HIGH_RES_BINS 500
int      use_high_res_bins = false;
int      i_high_res_bins[N_HIGH_RES_BINS];
int      i_highest_latency = 0;

char *kernelpath = (char *)0;
char *code_file = (char *)0;

typedef struct {
  u_long  k_sym_addr;       /* kernel symbol address from nm */
  u_int   k_sym_len;        /* length of kernel symbol string */
  char   *k_sym_name;       /* kernel symbol string from nm */
} kern_sym_t;

kern_sym_t *kern_sym_tbl;      /* pointer to the nm table       */
int        kern_sym_count;    /* number of entries in nm table */
char       pcstring[128];

#define UNKNOWN "Can't find symbol name"


double   divisor;
int      gotSIGWINCH = 0;
int      trace_enabled = 0;
struct host_basic_info  hi;


#define SAMPLE_SIZE 300000

int mib[6];
size_t needed;
char  *my_buffer;

kbufinfo_t bufinfo = {0, 0, 0};

FILE *log_fp = (FILE *)0;
int num_of_codes = 0;
int need_new_map = 0;
int total_threads = 0;
kd_threadmap *mapptr = 0;

#define MAX_ENTRIES 4096
struct ct {
        int type;
        char name[32];
} codes_tab[MAX_ENTRIES];


#define NUMPARMS 23

struct th_info {
        uintptr_t  thread;
        int  type;
        uintptr_t  child_thread;
        int  arg1;
        double stime;
        long *pathptr;
        long pathname[NUMPARMS + 1];
};

#define MAX_THREADS 512
struct th_info th_state[MAX_THREADS];

int  cur_max = 0;

#define TRACE_DATA_NEWTHREAD   0x07000004
#define TRACE_STRING_NEWTHREAD 0x07010004
#define TRACE_STRING_EXEC      0x07010008

#define INTERRUPT         0x01050000
#define DECR_TRAP         0x01090000
#define DECR_SET          0x01090004
#define MACH_vmfault      0x01300008
#define MACH_sched        0x01400000
#define MACH_stkhandoff   0x01400008
#define VFS_LOOKUP        0x03010090
#define BSC_exit          0x040C0004
#define IES_action        0x050b0018
#define IES_filter        0x050b001c
#define TES_action        0x050c0010
#define CQ_action         0x050d0018


#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc

#define CPU_NUMBER(kp)	kdbg_get_cpu(kp)


char *fault_name[9] = {
        "",
	"ZeroFill",
	"PageIn",
	"COW",
	"CacheHit",
	"NoZeroFill",
	"Guard",
	"PageInFile",
	"PageInAnon"
};

char *pc_to_string();
static kern_return_t	set_time_constraint_policy(void);
static kern_return_t	set_standard_policy(void);

int decrementer_val = 0;     /* Value used to reset decrementer */
int set_remove_flag = 1;     /* By default, remove trace buffer */

kd_buf **last_decrementer_kd;   /* last DECR_TRAP per cpu */
#define MAX_LOG_COUNT  30       /* limits the number of entries dumped in log_decrementer */

int
quit(s)
char *s;
{
        void set_enable();
	void set_rtcdec();
	void set_remove();

        if (trace_enabled)
	        set_enable(0);

	/* 
	   This flag is turned off when calling
	   quit() due to a set_remove() failure.
	*/
	if (set_remove_flag)
	  set_remove();

	if (decrementer_val)
	  set_rtcdec(0);

        printf("latency: ");
	if (s)
		printf("%s", s);

	exit(1);
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
set_pidexclude(int pid, int on_off) 
{
        kd_regtype kr;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDEX;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	sysctl(mib, 3, &kr, &needed, NULL, 0);
}

void set_rtcdec(decval)
int decval;
{kd_regtype kr;
 int ret;
 extern int errno;

	kr.type = KDBG_TYPENONE;
	kr.value1 = decval;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETRTCDEC;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	errno = 0;
	if ((ret=sysctl(mib, 3, &kr, &needed, NULL, 0)) < 0)
	  {
	      decrementer_val = 0;
	      /* ignore this sysctl error if it's not supported */
	      if (errno == ENOENT) 
		  return;
	      else
		  quit("trace facility failure, KERN_KDSETRTCDEC\n");
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

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	errno = 0;

	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	  {
  	      set_remove_flag = 0;
	      if(errno == EBUSY)
	          quit("the trace facility is currently in use...\n         fs_usage, sc_usage, and latency use this feature.\n\n");
	      else
	          quit("trace facility failure, KERN_KDREMOVE\n");
	  }
}

void
set_init_nologging()
{
        /* When we aren't logging, only collect the DECR_TRAP trace points */
        kd_regtype kr;
        kr.type = KDBG_VALCHECK;
        kr.value1 = DECR_TRAP;
	kr.value2 = 0;
        kr.value3 = 0;
        kr.value4 = 0;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;             /* no flags */
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
set_init_logging() 
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
write_high_res_latencies()
{
    int i;
    FILE *f;
    if(use_high_res_bins)
    {
        f = fopen("latencies.csv","w");
        for(i=0;i<N_HIGH_RES_BINS;i++)
        {
            fprintf(f,"%d,%d\n", i, i_high_res_bins[i]);
        }
        fclose(f);
    }
}

void sigwinch()
{
        gotSIGWINCH = 1;
}

void sigintr()
{
        void screen_update();

        write_high_res_latencies();

        set_enable(0);
	set_pidexclude(getpid(), 0);
        screen_update(log_fp);
	endwin();
	set_rtcdec(0);
	set_remove();
	
        exit(1);
}

void leave()    /* exit under normal conditions -- signal handler */
{
        write_high_res_latencies();

        set_enable(0);
	set_pidexclude(getpid(), 0);
	endwin();
	set_rtcdec(0);
	set_remove();
	
        exit(1);
}

void
screen_update(FILE *fp)
{
        int  i;
	int  itotal, stotal;
	int  elapsed_secs;
	int  elapsed_mins;
	int  elapsed_hours;
	unsigned int average_s_latency;
	unsigned int average_i_latency;
	char tbuf[256];

	if (fp == (FILE *)0) {
	        erase();
		move(0, 0);
	} else
	        fprintf(fp,"\n\n===================================================================================================\n");
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
	elapsed_secs = curr_time - start_time;
	elapsed_hours = elapsed_secs / 3600;
	elapsed_secs -= elapsed_hours * 3600;
	elapsed_mins = elapsed_secs / 60;
	elapsed_secs -= elapsed_mins * 60;

	sprintf(tbuf, "%-19.19s                            %2ld:%02ld:%02ld\n", &(ctime(&curr_time)[0]),
		(long)elapsed_hours, (long)elapsed_mins, (long)elapsed_secs);
	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	if (use_high_res_bins) {
		sprintf(tbuf, "INTERRUPTS(HIGH RESOLUTION)\n");

		if (fp)
			fprintf(fp, "%s", tbuf);
		else
			printw(tbuf);
	}
    if(use_high_res_bins)
    {
        for(i=0;i<N_HIGH_RES_BINS;i++) {
            if(i && !(i%10)) {
                sprintf(tbuf,"\n");
                if (fp)
                    fprintf(fp, "%s", tbuf);
                else
                    printw(tbuf);
            }
#define INDEX(i) ((i%10)*50 + (i/10))
            if(INDEX(i) <= i_highest_latency)
                sprintf(tbuf,"[%3d]: %6d ", INDEX(i), i_high_res_bins[INDEX(i)]);
            else
                tbuf[0] = '\0';

            if (fp)
                fprintf(fp, "%s", tbuf);
            else
                printw(tbuf);
        }
        sprintf(tbuf,"\n\n");
        if (fp)
            fprintf(fp, "%s", tbuf);
        else
            printw(tbuf);
    }

	sprintf(tbuf, "                     SCHEDULER     INTERRUPTS\n");

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	sprintf(tbuf, "---------------------------------------------\n");

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);



	sprintf(tbuf, "total_samples       %10d     %10d\n\n", s_total_samples, i_total_samples);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);



	for (itotal = 0, stotal = 0, i = 0; i < 10; i++) {
	        sprintf(tbuf, "delays < %3d usecs  %10d     %10d\n", (i + 1) * 10, s_usec_10_bins[i], i_usec_10_bins[i]);

		if (fp)
		        fprintf(fp, "%s", tbuf);
		else
		        printw(tbuf);

		stotal += s_usec_10_bins[i];
		itotal += i_usec_10_bins[i];
	}
	sprintf(tbuf, "total  < 100 usecs  %10d     %10d\n\n", stotal, itotal);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);



	for (itotal = 0, stotal = 0, i = 1; i < 10; i++) {
	        if (i < 9)
		        sprintf(tbuf, "delays < %3d usecs  %10d     %10d\n", (i + 1) * 100, s_usec_100_bins[i], i_usec_100_bins[i]);
		else
		        sprintf(tbuf, "delays <   1 msec   %10d     %10d\n", s_usec_100_bins[i], i_usec_100_bins[i]);

		if (fp)
		        fprintf(fp, "%s", tbuf);
		else
		        printw(tbuf);

		stotal += s_usec_100_bins[i];
		itotal += i_usec_100_bins[i];
	}
	sprintf(tbuf, "total  <   1 msec   %10d     %10d\n\n", stotal, itotal);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);



	for (itotal = 0, stotal = 0, i = 1; i < 10; i++) {
	        sprintf(tbuf, "delays < %3d msecs  %10d     %10d\n", (i + 1), s_msec_1_bins[i], i_msec_1_bins[i]);

		if (fp)
		        fprintf(fp, "%s", tbuf);
		else
		        printw(tbuf);

		stotal += s_msec_1_bins[i];
		itotal += i_msec_1_bins[i];
	}
	sprintf(tbuf, "total  <  10 msecs  %10d     %10d\n\n", stotal, itotal);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);




	for (itotal = 0, stotal = 0, i = 1; i < 5; i++) {
	        sprintf(tbuf, "delays < %3d msecs  %10d     %10d\n", (i + 1)*10, s_msec_10_bins[i], i_msec_10_bins[i]);

		if (fp)
		        fprintf(fp, "%s", tbuf);
		else
		        printw(tbuf);

		stotal += s_msec_10_bins[i];
		itotal += i_msec_10_bins[i];
	}
	sprintf(tbuf, "total  <  50 msecs  %10d     %10d\n\n", stotal, itotal);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	sprintf(tbuf, "delays >  50 msecs  %10d     %10d\n", s_too_slow, i_too_slow);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);



	sprintf(tbuf, "\nminimum latency(usecs) %7d        %7d\n", s_min_latency, i_min_latency);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	sprintf(tbuf, "maximum latency(usecs) %7d        %7d\n", s_max_latency, i_max_latency);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	if (s_total_samples)
	  average_s_latency = (unsigned int)(s_total_latency/s_total_samples);
	else
	  average_s_latency = 0;

	if (i_total_samples)
	  average_i_latency = (unsigned int)(i_total_latency/i_total_samples);
	else
	  average_i_latency = 0;

	sprintf(tbuf, "average latency(usecs) %7d        %7d\n", average_s_latency, average_i_latency);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	sprintf(tbuf, "exceeded threshold     %7d        %7d\n", s_exceeded_threshold, i_exceeded_threshold);

	if (fp)
	        fprintf(fp, "%s", tbuf);
	else
	        printw(tbuf);

	if (fp == (FILE *)0)
	        refresh();
	else
	        fflush(fp);
}

int
exit_usage()
{

        fprintf(stderr, "Usage: latency [-rt] [-h] [-c codefile] [-l logfile] [-st threshold]\n");

#if defined (__i386__)
	fprintf(stderr, "               [-it threshold] [-s sleep_in_usecs] [-n kernel]\n\n");	
#else
	fprintf(stderr, "               [-it threshold] [-s sleep_in_usecs]\n");	
	fprintf(stderr, "               [-d decrementer_in_usecs] [-n kernel]\n\n");
#endif


	fprintf(stderr, "  -rt   Set realtime scheduling policy.  Default is timeshare.\n");
	fprintf(stderr, "  -h    Display high resolution interrupt latencies and write them to latencies.csv (truncate existing file) upon exit.\n");
	fprintf(stderr, "  -c    specify name of codes file\n");
	fprintf(stderr, "  -l    specify name of file to log trace entries to when threshold is exceeded\n");
	fprintf(stderr, "  -st   set scheduler latency threshold in microseconds... if latency exceeds this, then log trace\n");
	fprintf(stderr, "  -it   set interrupt latency threshold in microseconds... if latency exceeds this, then log trace\n");
	fprintf(stderr, "  -s    set sleep time in microseconds\n");
#if !defined (__i386__)	
	fprintf(stderr, "  -d    set decrementer in microseconds.\n");
#endif
	fprintf(stderr, "  -n    specify kernel, default is /mach_kernel\n");	

	fprintf(stderr, "\nlatency must be run as root\n\n");

	exit(1);
}


int
main(int argc, char *argv[])
{
	uint64_t start, stop;
	uint64_t timestamp1;
	uint64_t timestamp2;
	uint64_t adeadline, adelay;
	double fdelay;
	int      elapsed_usecs;
	double   nanosecs_to_sleep;
	int      loop_cnt, sample_sc_now;
	int      decrementer_usec = 0;
        kern_return_t           ret;
        unsigned int            size;
        host_name_port_t        host;
	void     getdivisor();
	void     sample_sc();
	void     init_code_file();
	void     do_kernel_nm();
	void     open_logfile();

	if (0 != reexec_to_match_kernel()) {
		fprintf(stderr, "Could not re-execute: %d\n", errno);
		exit(1);
	}

	my_policy = THREAD_STANDARD_POLICY;
	policy_name = "TIMESHARE";

	while (argc > 1) {
	        if (strcmp(argv[1], "-rt") == 0) {
		        my_policy = THREAD_TIME_CONSTRAINT_POLICY;   /* the real time band */
			policy_name = "REALTIME";

		} else if (strcmp(argv[1], "-st") == 0) {
			argc--;
			argv++;

			if (argc > 1)
			  s_thresh_hold = atoi(argv[1]);
			else
			  exit_usage();
			
		} else if (strcmp(argv[1], "-it") == 0) {
			argc--;
			argv++;
			
			if (argc > 1)
			  i_thresh_hold = atoi(argv[1]);
			else
			  exit_usage();
		} else if (strcmp(argv[1], "-c") == 0) {
			argc--;
			argv++;
			
			if (argc > 1)
			  code_file = argv[1];
			else
			  exit_usage();
		} else if (strcmp(argv[1], "-l") == 0) {
			argc--;
			argv++;
			
			if (argc > 1)
			  open_logfile(argv[1]);
			else
			  exit_usage();

		} else if (strcmp(argv[1], "-s") == 0) {
			argc--;
			argv++;

			if (argc > 1)
			  num_of_usecs_to_sleep = atoi(argv[1]);
			else
			  exit_usage();
		}
		else if (strcmp(argv[1], "-d") == 0) {
			argc--;
			argv++;

			if (argc > 1)
			  decrementer_usec = atoi(argv[1]);
			else
			  exit_usage();
#if defined(__i386__)
			/* ignore this option - setting the decrementer has no effect */
			decrementer_usec = 0;
#endif			
		}
		else if (strcmp(argv[1], "-n") == 0) {
			argc--;
			argv++;

			if (argc > 1)
			  kernelpath = argv[1];
			else
			  exit_usage();
		} else if (strcmp(argv[1], "-h") == 0) {
            use_high_res_bins = true;
		} else
		        exit_usage();

		argc--;
		argv++;
	}

        if ( geteuid() != 0 ) {
	  printf("'latency' must be run as root...\n");
	  exit(1);
        }

	if (kernelpath == (char *) 0)
	  kernelpath = "/mach_kernel";

	if (code_file == (char *) 0)
	   code_file = "/usr/share/misc/trace.codes";

	do_kernel_nm();

	sample_sc_now = 25000 / num_of_usecs_to_sleep;

	getdivisor();
	decrementer_val = decrementer_usec * divisor;

	/* get the cpu countfor the DECR_TRAP array */
        host = mach_host_self();
	size = sizeof(hi)/sizeof(int);
        ret = host_info(host, HOST_BASIC_INFO, (host_info_t)&hi, &size);
        if (ret != KERN_SUCCESS) {
            mach_error(argv[0], ret);
            exit(EXIT_FAILURE);
	}

	if ((last_decrementer_kd = (kd_buf **)malloc(hi.max_cpus * sizeof(kd_buf *))) == (kd_buf **)0)
	        quit("can't allocate memory for decrementer tracing info\n");

	nanosecs_to_sleep = (double)(num_of_usecs_to_sleep * 1000);
	fdelay = nanosecs_to_sleep * (divisor /1000);
	adelay = (uint64_t)fdelay;

	init_code_file();

	/* 
	   When the decrementer isn't set in the options,
	   decval will be zero and this call will reset
	   the system default ...
	*/
	set_rtcdec(decrementer_val);

	if (initscr() == (WINDOW *) 0)
	  {
	    printf("Unrecognized TERM type, try vt100\n");
	    exit(1);
	  }

	clear();
	refresh();
	signal(SIGWINCH, sigwinch);
	signal(SIGINT, sigintr);
	signal(SIGQUIT, leave);
	signal(SIGTERM, leave);
	signal(SIGHUP, leave);


	if ((my_buffer = malloc(SAMPLE_SIZE * sizeof(kd_buf))) == (char *)0)
	        quit("can't allocate memory for tracing info\n");
	set_remove();
	set_numbufs(SAMPLE_SIZE);
	set_enable(0);
	if(log_fp)
	  set_init_logging();
	else
	  set_init_nologging();
	set_pidexclude(getpid(), 1);
	set_enable(1);
	trace_enabled = 1;
	need_new_map = 1;

	loop_cnt = 0;
	start_time = time((long *)0);
	refresh_time = start_time;

	if (my_policy == THREAD_TIME_CONSTRAINT_POLICY)
	  {
	    /* the realtime band */
	    if(set_time_constraint_policy() != KERN_SUCCESS)
	      quit("Failed to set realtime policy.\n");
	  }

	for (;;) {
		curr_time = time((long *)0);

		if (curr_time >= refresh_time) {
		       if (my_policy == THREAD_TIME_CONSTRAINT_POLICY)
		       {
		          /* set standard timeshare policy during screen update */
		          if(set_standard_policy() != KERN_SUCCESS)
			    quit("Failed to set standard policy.\n");
		        }
		        screen_update((FILE *)0);
		        if (my_policy == THREAD_TIME_CONSTRAINT_POLICY)
		        {
			  /* set back to realtime band */
		          if(set_time_constraint_policy() != KERN_SUCCESS)
			    quit("Failed to set time_constraint policy.\n");
		        }
			refresh_time = curr_time + 1;
		}

		timestamp1 = mach_absolute_time();
		adeadline = timestamp1 + adelay;
		mach_wait_until(adeadline);
		timestamp2 = mach_absolute_time();

		start = timestamp1;

		stop = timestamp2;

		elapsed_usecs = (int)(((double)(stop - start)) / divisor);

		if ((elapsed_usecs -= num_of_usecs_to_sleep) <= 0)
		        continue;

		if (elapsed_usecs < 100)
		        s_usec_10_bins[elapsed_usecs/10]++;
		if (elapsed_usecs < 1000)
		        s_usec_100_bins[elapsed_usecs/100]++;
		else if (elapsed_usecs < 10000)
		        s_msec_1_bins[elapsed_usecs/1000]++;
		else if (elapsed_usecs < 50000)
		        s_msec_10_bins[elapsed_usecs/10000]++;
		else 
		        s_too_slow++;

		if (elapsed_usecs > s_max_latency)
		        s_max_latency = elapsed_usecs;
		if (elapsed_usecs < s_min_latency || s_total_samples == 0)
		        s_min_latency = elapsed_usecs;
		s_total_latency += elapsed_usecs;
		s_total_samples++;

		if (s_thresh_hold && elapsed_usecs > s_thresh_hold)
			s_exceeded_threshold++;
		loop_cnt++;

		if (log_fp && s_thresh_hold && elapsed_usecs > s_thresh_hold)
		        sample_sc(start, stop);
		else {
		        if (loop_cnt >= sample_sc_now) {
			        sample_sc((long long)0, (long long)0);
				loop_cnt = 0;
			}
		}
	        if (gotSIGWINCH) {
		        /*
			  No need to check for initscr error return.
			  We won't get here if it fails on the first call.
			*/
		        endwin();
			clear();
			refresh();

			gotSIGWINCH = 0;
		}
	}
}


void getdivisor()
{
  mach_timebase_info_data_t info;

  (void) mach_timebase_info (&info);

  divisor = ( (double)info.denom / (double)info.numer) * 1000;

}

/* This is the realtime band */
static kern_return_t
set_time_constraint_policy()
{
	kern_return_t				result;
	thread_time_constraint_policy_data_t	info;
	mach_msg_type_number_t			count;	
	boolean_t				get_default;

	get_default = TRUE;
	count = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
	result = thread_policy_get(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
				   (thread_policy_t)&info, &count, &get_default);
	if (result != KERN_SUCCESS)
		return (result);

	result = thread_policy_set(mach_thread_self(),	THREAD_TIME_CONSTRAINT_POLICY,
				   (thread_policy_t)&info, THREAD_TIME_CONSTRAINT_POLICY_COUNT);

	return (result);
}

/* This is the timeshare mode */
static kern_return_t
set_standard_policy()
{
	kern_return_t			result;
	thread_standard_policy_data_t	info;
	mach_msg_type_number_t		count;	
	boolean_t			get_default;

	get_default = TRUE;
	count = THREAD_STANDARD_POLICY_COUNT;
	result = thread_policy_get(mach_thread_self(), THREAD_STANDARD_POLICY,
				   (thread_policy_t)&info, &count, &get_default);
	if (result != KERN_SUCCESS)
		return (result);

	result = thread_policy_set(mach_thread_self(),	THREAD_STANDARD_POLICY,
				   (thread_policy_t)&info, THREAD_STANDARD_POLICY_COUNT);

	return (result);
}

												  
void read_command_map()
{
    size_t size;
    int mib[6];
  
    if (mapptr) {
	free(mapptr);
	mapptr = 0;
    }
    total_threads = bufinfo.nkdthreads;
    size = bufinfo.nkdthreads * sizeof(kd_threadmap);
    if (size)
    {
        if ((mapptr = (kd_threadmap *) malloc(size)))
	     bzero (mapptr, size);
	else
	{
	    printf("Thread map is not initialized -- this is not fatal\n");
	    return;
	}
    }
 
    /* Now read the threadmap */
    mib[0] = CTL_KERN;
    mib[1] = KERN_KDEBUG;
    mib[2] = KERN_KDTHRMAP;
    mib[3] = 0;
    mib[4] = 0;
    mib[5] = 0;		/* no flags */
    if (sysctl(mib, 3, mapptr, &size, NULL, 0) < 0)
    {
        /* This is not fatal -- just means I cant map command strings */

        printf("Can't read the thread map -- this is not fatal\n");
	free(mapptr);
	mapptr = 0;
	return;
    }
    return;
}


void create_map_entry(uintptr_t thread, char *command)
{
    int i, n;
    kd_threadmap *map;

    if (!mapptr)
        return;

    for (i = 0, map = 0; !map && i < total_threads; i++)
    {
        if (mapptr[i].thread == thread )	
	    map = &mapptr[i];   /* Reuse this entry, the thread has been reassigned */
    }

    if (!map)   /* look for invalid entries that I can reuse*/
    {
        for (i = 0, map = 0; !map && i < total_threads; i++)
	{
	    if (mapptr[i].valid == 0 )	
	        map = &mapptr[i];   /* Reuse this invalid entry */
	}
    }
  
    if (!map)
    {
        /* If reach here, then this is a new thread and 
	 * there are no invalid entries to reuse
	 * Double the size of the thread map table.
	 */

        n = total_threads * 2;
	mapptr = (kd_threadmap *) realloc(mapptr, n * sizeof(kd_threadmap));
	bzero(&mapptr[total_threads], total_threads*sizeof(kd_threadmap));
	map = &mapptr[total_threads];
	total_threads = n;
#if 0
	if (log_fp)
	  fprintf(log_fp, "MAP: increasing thread map to %d entries\n", total_threads);
#endif
    }
#if 0
    if (log_fp)
      fprintf(log_fp, "MAP: adding thread %x with name %s\n", thread, command);
#endif
    map->valid = 1;
    map->thread = thread;
    /*
      The trace entry that returns the command name will hold
      at most, MAXCOMLEN chars, and in that case, is not
      guaranteed to be null terminated.
    */
    (void)strncpy (map->command, command, MAXCOMLEN);
    map->command[MAXCOMLEN] = '\0';
}


kd_threadmap *find_thread_map(uintptr_t thread)
{
    int i;
    kd_threadmap *map;

    if (!mapptr)
        return((kd_threadmap *)0);

    for (i = 0; i < total_threads; i++)
    {
        map = &mapptr[i];
	if (map->valid && (map->thread == thread))
	{
	    return(map);
	}
    }
    return ((kd_threadmap *)0);
}

void
kill_thread_map(uintptr_t thread)
{
    kd_threadmap *map;

    if ((map = find_thread_map(thread))) {

#if 0
      if (log_fp)
        fprintf(log_fp, "MAP: deleting thread %x with name %s\n", thread, map->command);
#endif
        map->valid = 0;
	map->thread = 0;
	map->command[0] = '\0';
    }
}


struct th_info *find_thread(uintptr_t thread, int type1, int type2) {
       struct th_info *ti;

       for (ti = th_state; ti < &th_state[cur_max]; ti++) {
	       if (ti->thread == thread) {
		       if (type1 == 0)
			       return(ti);
		       if (type1 == ti->type)
			       return(ti);
		       if (type2 == ti->type)
			       return(ti);
	       }
       }
       return ((struct th_info *)0);
}


char *find_code(type)
{
        int i;

	for (i = 0; i < num_of_codes; i++) {
	        if (codes_tab[i].type == type)
		        return(codes_tab[i].name);
	}
	return ((char *)0);
}


void sample_sc(uint64_t start, uint64_t stop)
{
	kd_buf   *kd, *last_mach_sched, *start_kd, *end_of_sample;
	uint64_t now;
	int count, i;
	int first_entry = 1;
	double timestamp = 0.0;
	double last_timestamp = 0.0;
	double delta = 0.0;
	double start_bias = 0.0;	
	char   command[32];
	void read_command_map();

	if (log_fp && (my_policy == THREAD_TIME_CONSTRAINT_POLICY))
	  {
	    /* set standard timeshare policy when logging */
	    if(set_standard_policy() != KERN_SUCCESS)
	      quit("Failed to set standard policy.\n");
	  }

        /* Get kernel buffer information */
	get_bufinfo(&bufinfo);

	if (need_new_map) {
	        read_command_map();
		need_new_map = 0;
	}
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
	        for (i = 0; i < cur_max; i++) {
			th_state[i].thread = 0;
			th_state[i].type = -1;
			th_state[i].pathptr = (long *)NULL;
			th_state[i].pathname[0] = 0;
		}
		cur_max = 0;
		need_new_map = 1;
		
		set_enable(0);
		set_enable(1);

		if (log_fp) {
		        double latency;

		        latency = (double)(stop - start) / divisor;
			latency -= (double)num_of_usecs_to_sleep;

		        fprintf(log_fp, "\n\n%-19.19s   scheduling latency = %.1fus  num_of_traces = %d <<<<<<< trace buffer wrapped >>>>>>>\n\n",
				&(ctime(&curr_time)[0]), latency, count);
		}
	}
	end_of_sample = &((kd_buf *)my_buffer)[count];

	/* Always reinitialize the DECR_TRAP array */
	for (i=0; i < hi.max_cpus; i++)
	      last_decrementer_kd[i] = (kd_buf *)my_buffer;

	last_mach_sched = (kd_buf *)0;

	for (kd = (kd_buf *)my_buffer; kd < end_of_sample; kd++) {
	        int debugid, thread, cpunum;
		int type, clen, mode;
		int len;
		char *p;
		long *sargptr;
		kd_buf *cur_kd;
		double i_latency = 0.0;
		struct th_info *ti;
		char   command1[32];
		char   sched_info[64];
		kd_threadmap *map;
		kd_threadmap *find_thread_map();
		double handle_decrementer();
		kd_buf *log_decrementer();
		int check_for_thread_update();
		void enter_syscall();
		void exit_syscall();
		void print_entry();

		thread  = kd->arg5;
		cpunum	= CPU_NUMBER(kd);
		debugid = kd->debugid;
		type    = kd->debugid & DBG_FUNC_MASK;

		if (check_for_thread_update(thread, type, kd))
		        continue;

	        if (type == DECR_TRAP)
		        i_latency = handle_decrementer(kd);

		now = kd->timestamp & KDBG_TIMESTAMP_MASK;

		timestamp = ((double)now) / divisor;

		if (now < start || now > stop) {
		        if (debugid & DBG_FUNC_START)
			        enter_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 0);
			else if (debugid & DBG_FUNC_END)
			        exit_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 0);
			else if (type == DECR_TRAP) {
				cur_kd = kd;
			        if (log_fp && i_thresh_hold && (int)i_latency > i_thresh_hold) {
				        start_kd = last_decrementer_kd[cpunum];
				        kd = log_decrementer(start_kd, kd, end_of_sample, i_latency);
					if (kd >= end_of_sample)
					        break;
				}
				if ((kd->debugid & DBG_FUNC_MASK) == DECR_TRAP)
				  {
				    cpunum = CPU_NUMBER(kd);
				    last_decrementer_kd[cpunum] = kd;
				  }
				else
				  last_decrementer_kd[cpunum] = cur_kd;
			}
		        continue;
		}
		if (first_entry) {
		        double latency;
			char buf1[132];
			char buf2[132];

		        latency = (double)(stop - start) / divisor;
			latency -= (double)num_of_usecs_to_sleep;

			if (my_pri == -1)
			        sprintf(buf2, "default");
			else
			        sprintf(buf2, "%d", my_pri);
			sprintf(buf1, "%-19.19s     scheduling latency = %.1fus    sleep_request = %dus     policy = %s     priority = %s",
				&(ctime(&curr_time)[0]), latency, num_of_usecs_to_sleep, policy_name, buf2);
			clen = strlen(buf1);
			memset(buf2, '-', clen);
			buf2[clen] = 0;
			
			if (log_fp) {
			  fprintf(log_fp, "\n\n%s\n", buf2);
			  fprintf(log_fp, "%s\n\n", buf1);
			  fprintf(log_fp, "RelTime(Us)  Delta              debugid                      arg1       arg2       arg3      arg4       thread   cpu  command\n\n");
			}
		        start_bias = ((double)start) / divisor;
		        last_timestamp = timestamp;
			first_entry = 0;
		}
		delta = timestamp - last_timestamp;

		if ((map = find_thread_map(thread)))
		        strcpy(command, map->command);
		else
		        command[0] = 0;

		switch (type) {

		case CQ_action:
		    if (log_fp) {
		        fprintf(log_fp, "%9.1f %8.1f\t\tCQ_action @ %-59.59s %-8x  %d  %s\n",
			      timestamp - start_bias, delta, pc_to_string(kd->arg1, 59, 1) , thread, cpunum, command);
		    }
		    last_timestamp = timestamp;
		    break;

		case TES_action:
		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f\t\tTES_action @ %-58.58s %-8x  %d  %s\n",
			      timestamp - start_bias, delta, pc_to_string(kd->arg1, 58, 1) , thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		case IES_action:
		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f\t\tIES_action @ %-58.58s %-8x  %d  %s\n",
			      timestamp - start_bias, delta, pc_to_string(kd->arg1, 58, 1) , thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		case IES_filter:
		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f\t\tIES_filter @ %-58.58s %-8x  %d  %s\n",
			      timestamp - start_bias, delta, pc_to_string(kd->arg1, 58, 1) , thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		case DECR_TRAP:
		    last_decrementer_kd[cpunum] = kd;

		    if (i_thresh_hold && (int)i_latency > i_thresh_hold)
		            p = "*";
		    else
		            p = " ";

		    mode = 1;

		    if ((ti = find_thread(kd->arg5, 0, 0))) {
		            if (ti->type == -1 && strcmp(command, "kernel_task"))
			            mode = 0;
		    }

		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f[%.1f]%s\tDECR_TRAP @ %-59.59s %-8x  %d  %s\n",
			      timestamp - start_bias, delta, i_latency, p, pc_to_string(kd->arg2, 59, mode) , thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		case DECR_SET:
		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f[%.1f]  \t%-28.28s                                            %-8x  %d  %s\n",
			      timestamp - start_bias, delta, (double)kd->arg1/divisor, "DECR_SET", thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		case MACH_sched:
		case MACH_stkhandoff:
		    last_mach_sched = kd;

		    if ((map = find_thread_map(kd->arg2)))
		            strcpy(command1, map->command);
		    else
		            sprintf(command1, "%-8x", kd->arg2);

		    if ((ti = find_thread(kd->arg2, 0, 0))) {
		            if (ti->type == -1 && strcmp(command1, "kernel_task"))
			            p = "U";
			    else
			            p = "K";
		    } else
		            p = "*";
		    memset(sched_info, ' ', sizeof(sched_info));

		    sprintf(sched_info, "%14.14s", command);
		    clen = strlen(sched_info);
		    sched_info[clen] = ' ';

		    sprintf(&sched_info[14],  " @ pri %3d  -->  %14.14s", kd->arg3, command1);
		    clen = strlen(sched_info);
		    sched_info[clen] = ' ';

		    sprintf(&sched_info[45], " @ pri %3d%s", kd->arg4, p);

		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f\t\t%-10.10s  %s    %-8x  %d\n",
				    timestamp - start_bias, delta, "MACH_SCHED", sched_info, thread, cpunum);
		    }

		    last_timestamp = timestamp;
		    break;

		case VFS_LOOKUP:
		    if ((ti = find_thread(thread, 0, 0)) == (struct th_info *)0) {
		            if (cur_max >= MAX_THREADS)
			            continue;
			    ti = &th_state[cur_max++];

			    ti->thread = thread;
			    ti->type   = -1;
			    ti->pathptr = (long *)0;
			    ti->child_thread = 0;
		    }
		    while ( (kd < end_of_sample) && ((kd->debugid & DBG_FUNC_MASK) == VFS_LOOKUP))
		      {
			if (ti->pathptr == NULL) {
			    ti->arg1 = kd->arg1;
			    sargptr = ti->pathname;
				
			    *sargptr++ = kd->arg2;
			    *sargptr++ = kd->arg3;
			    *sargptr++ = kd->arg4;
			    /*
			     * NULL terminate the 'string'
			     */
			    *sargptr = 0;
			    ti->pathptr = sargptr;

			} else {
		            sargptr = ti->pathptr;

			    /*
                               We don't want to overrun our pathname buffer if the
                               kernel sends us more VFS_LOOKUP entries than we can
                               handle.
			    */

                            if (sargptr >= &ti->pathname[NUMPARMS])
			      {
				kd++;
				continue;
			      }

                            /*
                              We need to detect consecutive vfslookup entries.
                              So, if we get here and find a START entry,
                              fake the pathptr so we can bypass all further
                              vfslookup entries.
			    */

                            if (kd->debugid & DBG_FUNC_START)
                              {
                                ti->pathptr = &ti->pathname[NUMPARMS];
                              }
			    else
			      {
				*sargptr++ = kd->arg1;
				*sargptr++ = kd->arg2;
				*sargptr++ = kd->arg3;
				*sargptr++ = kd->arg4;
				/*
				 * NULL terminate the 'string'
				 */
				*sargptr = 0;

				ti->pathptr = sargptr;
			      }
			}
			kd++;
		    }
		    p = (char *)ti->pathname;

		    kd--;

		    /* print the tail end of the pathname */
		    len = strlen(p);
		    if (len > 42)
		      len -= 42;
		    else
		      len = 0;
			    
		    if (log_fp) {
		      fprintf(log_fp, "%9.1f %8.1f\t\t%-14.14s %-42s    %-8x   %-8x  %d  %s\n",
			      timestamp - start_bias, delta, "VFS_LOOKUP", 
			      &p[len], ti->arg1, thread, cpunum, command);
		    }

		    last_timestamp = timestamp;
		    break;

		default:
		    if (debugid & DBG_FUNC_START)
		            enter_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 1);
		    else if (debugid & DBG_FUNC_END)
		            exit_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 1);
		    else
		            print_entry(log_fp, kd, thread, type, command, timestamp, delta, start_bias);

		    last_timestamp = timestamp;
		    break;
		}
	}
	if (last_mach_sched && log_fp)
	        fprintf(log_fp, "\nblocked by %s @ priority %d\n", command, last_mach_sched->arg3);
#if 0
	if (first_entry == 0 && log_fp)
	        fprintf(log_fp, "\n   start = %qd   stop = %qd    count = %d    now = %qd\n", start, stop, count, now);
#endif
	if (log_fp)
	        fflush(log_fp);

	if (log_fp && (my_policy == THREAD_TIME_CONSTRAINT_POLICY))
	  {
	    /* set back to realtime band */
	    if(set_time_constraint_policy() != KERN_SUCCESS)
	      quit("Failed to set time_constraint policy.\n");
	  }
}

void
enter_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, double timestamp, double delta, double bias, int print_info)
{
       struct th_info *ti;
       int    i;
       int    cpunum;
       char  *p;

       cpunum = CPU_NUMBER(kd);

       if (print_info && fp) {
	       if ((p = find_code(type))) {
		       if (type == INTERRUPT) {
			       int mode = 1;

			       if ((ti = find_thread(kd->arg5, 0, 0))) {
				       if (ti->type == -1 && strcmp(command, "kernel_task"))
					       mode = 0;
			       }

			       fprintf(fp, "%9.1f %8.1f\t\tINTERRUPT @ %-59.59s %-8x  %d  %s\n",
				       timestamp - bias, delta, pc_to_string(kd->arg2, 59, mode), thread, cpunum, command);
		       } else if (type == MACH_vmfault) {
			       fprintf(fp, "%9.1f %8.1f\t\t%-28.28s                                            %-8x  %d  %s\n",
				       timestamp - bias, delta, p, thread, cpunum, command);
		       } else {
			       fprintf(fp, "%9.1f %8.1f\t\t%-28.28s %-8x   %-8x   %-8x  %-8x   %-8x  %d  %s\n",
				       timestamp - bias, delta, p, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
				       thread, cpunum, command);
		       }
	       } else {
		       fprintf(fp, "%9.1f %8.1f\t\t%-8x                     %-8x   %-8x   %-8x  %-8x   %-8x  %d  %s\n",
			       timestamp - bias, delta, type, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
			       thread, cpunum, command);
	       }
       }
       if ((ti = find_thread(thread, -1, type)) == (struct th_info *)0) {
	       if (cur_max >= MAX_THREADS) {
		       static int do_this_once = 1;

		       if (do_this_once) {
			       for (i = 0; i < cur_max; i++) {
				       if (!fp)
					     break;
				       fprintf(fp, "thread = %x, type = %x\n", 
					       th_state[i].thread, th_state[i].type);
			       }
			       do_this_once = 0;
		       }
		       return;

	       }
	       ti = &th_state[cur_max++];

	       ti->thread = thread;
	       ti->child_thread = 0;
       }
       if (type != BSC_exit)
	       ti->type = type;
       else
	       ti->type = -1;
       ti->stime  = timestamp;
       ti->pathptr = (long *)NULL;

#if 0
       if (print_info && fp)
	       fprintf(fp, "cur_max = %d,  ti = %x,  type = %x,  thread = %x\n", cur_max, ti, ti->type, ti->thread);
#endif
}


void
exit_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, double timestamp, double delta, double bias, int print_info)
{
       struct th_info *ti;
       int    cpunum;
       char   *p;
       uint64_t user_addr;

       cpunum = CPU_NUMBER(kd);

       ti = find_thread(thread, type, type);
#if 0
       if (print_info && fp)
	       fprintf(fp, "cur_max = %d,  ti = %x,  type = %x,  thread = %x\n", cur_max, ti, type, thread);
#endif
       if (print_info && fp) {
	       if (ti)
		       fprintf(fp, "%9.1f %8.1f(%.1f) \t", timestamp - bias, delta, timestamp - ti->stime);
	       else
		       fprintf(fp, "%9.1f %8.1f()      \t", timestamp - bias, delta);

	       if ((p = find_code(type))) {
		       if (type == INTERRUPT) {
			       fprintf(fp, "INTERRUPT                                                               %-8x  %d  %s\n", thread, cpunum, command);
		       } else if (type == MACH_vmfault && kd->arg4 <= DBG_PAGEIND_FAULT) {
			       user_addr = ((uint64_t)kd->arg1 << 32) | (uint32_t)kd->arg2;

			       fprintf(fp, "%-28.28s %-10.10s   %-16qx              %-8x  %d  %s\n",
				       p, fault_name[kd->arg4], user_addr,
				       thread, cpunum, command);
		       } else {
			       fprintf(fp, "%-28.28s %-8x   %-8x                        %-8x  %d  %s\n",
				       p, kd->arg1, kd->arg2,
				       thread, cpunum, command);
		       }
	       } else {
		       fprintf(fp, "%-8x                     %-8x   %-8x                        %-8x  %d  %s\n",
			       type, kd->arg1, kd->arg2,
			       thread, cpunum, command);
	       }
       }
       if (ti == (struct th_info *)0) {
	       if ((ti = find_thread(thread, -1, -1)) == (struct th_info *)0) {
		       if (cur_max >= MAX_THREADS)
			       return;
		       ti = &th_state[cur_max++];

		       ti->thread = thread;
		       ti->child_thread = 0;
		       ti->pathptr = (long *)NULL;
	       }
       }
       ti->type = -1;
}

void
print_entry(FILE *fp, kd_buf *kd, int thread, int type, char *command, double timestamp, double delta, double bias)
{
       char  *p;
       int cpunum;

       if (!fp)
	 return;

       cpunum = CPU_NUMBER(kd);
#if 0
       fprintf(fp, "cur_max = %d, type = %x,  thread = %x, cpunum = %d\n", cur_max, type, thread, cpunum);
#endif
       if ((p = find_code(type))) {
	       fprintf(fp, "%9.1f %8.1f\t\t%-28.28s %-8x   %-8x   %-8x  %-8x   %-8x  %d  %s\n",
		       timestamp - bias, delta, p, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
		       thread, cpunum, command);
       } else {
	       fprintf(fp, "%9.1f %8.1f\t\t%-8x                     %-8x   %-8x   %-8x  %-8x   %-8x  %d  %s\n",
		       timestamp - bias, delta, type, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
		       thread, cpunum, command);
       }
}

int
check_for_thread_update(int thread, int type, kd_buf *kd)
{
        struct th_info *ti;
	void create_map_entry();

        switch (type) {

	case TRACE_DATA_NEWTHREAD:
	    if ((ti = find_thread(thread, 0, 0)) == (struct th_info *)0) {
	            if (cur_max >= MAX_THREADS)
		            return (1);
		    ti = &th_state[cur_max++];

		    ti->thread = thread;
		    ti->type   = -1;
		    ti->pathptr = (long *)NULL;
	    }
	    ti->child_thread = kd->arg1;
	    return (1);

	case TRACE_STRING_NEWTHREAD:
	    if ((ti = find_thread(thread, 0, 0)) == (struct th_info *)0)
	            return (1);
	    if (ti->child_thread == 0)
	            return (1);
	    create_map_entry(ti->child_thread, (char *)&kd->arg1);

	    ti->child_thread = 0;
	    return (1);

	case TRACE_STRING_EXEC:
	    create_map_entry(thread, (char *)&kd->arg1);
	    return (1);

	}
	return (0);
}


kd_buf *log_decrementer(kd_buf *kd_beg, kd_buf *kd_end, kd_buf *end_of_sample, double i_latency)
{
        kd_buf *kd, *kd_start, *kd_stop;
	int kd_count;     /* Limit the boundary of kd_start */
	double timestamp = 0.0;
	double last_timestamp = 0.0;
	double delta = 0.0;
	double start_bias = 0.0;
	int thread, cpunum;
	int debugid, type, clen;
	int len;
	uint64_t now;
	struct th_info *ti;
	long  *sargptr;
	char  *p;
	char   command[32];
	char   command1[32];
	char   sched_info[64];
	char   buf1[128];
	char   buf2[128];
	kd_threadmap *map;
	kd_threadmap *find_thread_map();

	sprintf(buf1, "%-19.19s     interrupt latency = %.1fus", &(ctime(&curr_time)[0]), i_latency);
	clen = strlen(buf1);
	memset(buf2, '-', clen);
	buf2[clen] = 0;
	fprintf(log_fp, "\n\n%s\n", buf2);
	fprintf(log_fp, "%s\n\n", buf1);

	fprintf(log_fp, "RelTime(Us)  Delta              debugid                      arg1       arg2       arg3      arg4       thread   cpu   command\n\n");

	thread = kd_beg->arg5;
	cpunum = CPU_NUMBER(kd_end);

	for (kd_count = 0, kd_start = kd_beg - 1; (kd_start >= (kd_buf *)my_buffer); kd_start--, kd_count++) {
	        if (kd_count == MAX_LOG_COUNT)
		        break;

		if (CPU_NUMBER(kd_start) != cpunum)
		        continue;
										     
		if ((kd_start->debugid & DBG_FUNC_MASK) == DECR_TRAP)
		        break;

		if (kd_start->arg5 != thread)
		        break;
	}

	if (kd_start < (kd_buf *)my_buffer)
	        kd_start = (kd_buf *)my_buffer;

	thread = kd_end->arg5;

	for (kd_stop = kd_end + 1; kd_stop < end_of_sample; kd_stop++) {
										     
		if ((kd_stop->debugid & DBG_FUNC_MASK) == DECR_TRAP)
		        break;

		if (CPU_NUMBER(kd_stop) != cpunum)
		        continue;

		if (kd_stop->arg5 != thread)
		        break;
	}

	if (kd_stop >= end_of_sample)
	        kd_stop = end_of_sample - 1;

	now = kd_start->timestamp & KDBG_TIMESTAMP_MASK;
	timestamp = ((double)now) / divisor;

	for (kd = kd_start; kd <= kd_stop; kd++) {
		type = kd->debugid & DBG_FUNC_MASK;

	        if ((ti = find_thread(kd->arg5, type, type))) {
		        if (ti->stime >= timestamp)
			        ti->type = -1;
		}
	}
	for (kd = kd_start; kd <= kd_stop; kd++) {
	        int    mode;

	        thread  = kd->arg5;
		cpunum	= CPU_NUMBER(kd);
		debugid = kd->debugid;
		type    = kd->debugid & DBG_FUNC_MASK;

		now = kd->timestamp & KDBG_TIMESTAMP_MASK;

		timestamp = ((double)now) / divisor;

		if (kd == kd_start) {
		        start_bias = timestamp;
		        last_timestamp = timestamp;
		}
		delta = timestamp - last_timestamp;

		if ((map = find_thread_map(thread)))
		        strcpy(command, map->command);
		else
		        command[0] = 0;


		switch (type) {

		case CQ_action:
		    fprintf(log_fp, "%9.1f %8.1f\t\tCQ_action @ %-59.59s %-8x  %d  %s\n",
			    timestamp - start_bias, delta, pc_to_string(kd->arg1, 59, 1) , thread, cpunum, command);

		    last_timestamp = timestamp;
		    break;

		case DECR_TRAP:
		    if ((int)(kd->arg1) >= 0)
		            i_latency = 0;
		    else
		            i_latency = (((double)(-1 - kd->arg1)) / divisor);

		    if (i_thresh_hold && (int)i_latency > i_thresh_hold)
		            p = "*";
		    else
		            p = " ";

		    mode = 1;

		    if ((ti = find_thread(kd->arg5, 0, 0))) {
		            if (ti->type == -1 && strcmp(command, "kernel_task"))
			            mode = 0;
		    }
		    fprintf(log_fp, "%9.1f %8.1f[%.1f]%s\tDECR_TRAP @ %-59.59s %-8x  %d  %s\n",
			    timestamp - start_bias, delta, i_latency, p, pc_to_string(kd->arg2, 59, mode) , thread, cpunum, command);

		    last_timestamp = timestamp;
		    break;

	        case DECR_SET:
		    fprintf(log_fp, "%9.1f %8.1f[%.1f]  \t%-28.28s                                            %-8x  %d  %s\n",
			    timestamp - start_bias, delta, (double)kd->arg1/divisor,
			    "DECR_SET", thread, cpunum, command);

		    last_timestamp = timestamp;
		    break;

		case MACH_sched:
		case MACH_stkhandoff:
		    if ((map = find_thread_map(kd->arg2)))
		            strcpy(command1, map->command);
		    else
		            sprintf(command1, "%-8x", kd->arg2);

		    if ((ti = find_thread(kd->arg2, 0, 0))) {
		            if (ti->type == -1 && strcmp(command1, "kernel_task"))
			            p = "U";
			    else
			            p = "K";
		    } else
		            p = "*";
		    memset(sched_info, ' ', sizeof(sched_info));

		    sprintf(sched_info, "%14.14s", command);
		    clen = strlen(sched_info);
		    sched_info[clen] = ' ';

		    sprintf(&sched_info[14],  " @ pri %3d  -->  %14.14s", kd->arg3, command1);
		    clen = strlen(sched_info);
		    sched_info[clen] = ' ';

		    sprintf(&sched_info[45], " @ pri %3d%s", kd->arg4, p);

		    fprintf(log_fp, "%9.1f %8.1f\t\t%-10.10s  %s    %-8x  %d\n",
				    timestamp - start_bias, delta, "MACH_SCHED", sched_info, thread, cpunum);

		    last_timestamp = timestamp;
		    break;

		case VFS_LOOKUP:
		    if ((ti = find_thread(thread, 0, 0)) == (struct th_info *)0) {
		            if (cur_max >= MAX_THREADS)
			            continue;
			    ti = &th_state[cur_max++];

			    ti->thread = thread;
			    ti->type   = -1;
			    ti->pathptr = (long *)NULL;
			    ti->child_thread = 0;
		    }

		    while ( (kd <= kd_stop) && (kd->debugid & DBG_FUNC_MASK) == VFS_LOOKUP)
		      {
			if (ti->pathptr == NULL) {
			    ti->arg1 = kd->arg1;
			    sargptr = ti->pathname;
				
			    *sargptr++ = kd->arg2;
			    *sargptr++ = kd->arg3;
			    *sargptr++ = kd->arg4;
			    /*
			     * NULL terminate the 'string'
			     */
			    *sargptr = 0;

			    ti->pathptr = sargptr;

			} else {
  		            sargptr = ti->pathptr;

			    /*
                               We don't want to overrun our pathname buffer if the
                               kernel sends us more VFS_LOOKUP entries than we can
                               handle.
			    */

                            if (sargptr >= &ti->pathname[NUMPARMS])
			      {
				kd++;
				continue;
			      }

                            /*
                              We need to detect consecutive vfslookup entries.
                              So, if we get here and find a START entry,
                              fake the pathptr so we can bypass all further
                              vfslookup entries.
			    */

                            if (kd->debugid & DBG_FUNC_START)
                              {
                                ti->pathptr = &ti->pathname[NUMPARMS];
                              }
			    else
			      {
				*sargptr++ = kd->arg1;
				*sargptr++ = kd->arg2;
				*sargptr++ = kd->arg3;
				*sargptr++ = kd->arg4;
				/*
				 * NULL terminate the 'string'
				 */
				*sargptr = 0;

				ti->pathptr = sargptr;
			      }
			}
			kd++;
		    }
		    p = (char *)ti->pathname;

		    kd--;
		    /* print the tail end of the pathname */
		    len = strlen(p);
		    if (len > 42)
		      len -= 42;
		    else
		      len = 0;
		    
		    fprintf(log_fp, "%9.1f %8.1f\t\t%-14.14s %-42s    %-8x   %-8x  %d  %s\n",
			    timestamp - start_bias, delta, "VFS_LOOKUP", 
			    &p[len], ti->arg1, thread, cpunum, command);

		    last_timestamp = timestamp;
		    break;

		default:
		    if (debugid & DBG_FUNC_START)
		            enter_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 1);
		    else if (debugid & DBG_FUNC_END)
		            exit_syscall(log_fp, kd, thread, type, command, timestamp, delta, start_bias, 1);
		    else
		            print_entry(log_fp, kd, thread, type, command, timestamp, delta, start_bias);

		    last_timestamp = timestamp;
		    break;
		}
	}
	return(kd_stop);
}


double handle_decrementer(kd_buf *kd)
{
        double latency;
	long   elapsed_usecs;

	if ((long)(kd->arg1) >= 0)
	       latency = 1;
	else
	       latency = (((double)(-1 - kd->arg1)) / divisor);
	elapsed_usecs = (long)latency;

	if (elapsed_usecs < 100)
	        i_usec_10_bins[elapsed_usecs/10]++;
	if (elapsed_usecs < 1000)
	        i_usec_100_bins[elapsed_usecs/100]++;
	else if (elapsed_usecs < 10000)
	        i_msec_1_bins[elapsed_usecs/1000]++;
	else if (elapsed_usecs < 50000)
	        i_msec_10_bins[elapsed_usecs/10000]++;
	else 
	        i_too_slow++;

    if(use_high_res_bins && elapsed_usecs < N_HIGH_RES_BINS) {
        if(elapsed_usecs > i_highest_latency)
            i_highest_latency = elapsed_usecs;
        i_high_res_bins[elapsed_usecs]++;
    }

	if (i_thresh_hold && elapsed_usecs > i_thresh_hold)
	        i_exceeded_threshold++;
	if (elapsed_usecs > i_max_latency)
	        i_max_latency = elapsed_usecs;
	if (elapsed_usecs < i_min_latency || i_total_samples == 0)
	        i_min_latency = elapsed_usecs;
	i_total_latency += elapsed_usecs;
	i_total_samples++;

	return (latency);
}


void init_code_file()
{
        FILE *fp;
	int   i, n, code;
	char name[128];

	if ((fp = fopen(code_file, "r")) == (FILE *)0) {
	        if (log_fp)
		        fprintf(log_fp, "open of %s failed\n", code_file);
	        return;
	}
	for (i = 0; i < MAX_ENTRIES; i++) {
	        n = fscanf(fp, "%x%127s\n", &code, name);

		if (n == 1 && i == 0) {
			/*
			 * old code file format, just skip
			 */
			continue;
		}
		if (n != 2)
		        break;

		strncpy(codes_tab[i].name, name, 32);
		codes_tab[i].type = code;
	}
	num_of_codes = i;

	fclose(fp);
}


void
do_kernel_nm()
{
  int i, len;
  FILE *fp = (FILE *)0;
  char tmp_nm_file[128];
  char tmpstr[1024];
  char inchr;

  bzero(tmp_nm_file, 128);
  bzero(tmpstr, 1024);

  /* Build the temporary nm file path */
  strcpy(tmp_nm_file,"/tmp/knm.out.XXXXXX");
  if (!mktemp(tmp_nm_file)) {
    fprintf(stderr, "Error in mktemp call\n");
    return;
  }

  /* Build the nm command and create a tmp file with the output*/
  sprintf (tmpstr, "/usr/bin/nm -f -n -s __TEXT __text %s > %s",
	   kernelpath, tmp_nm_file);
  system(tmpstr);
  
  /* Parse the output from the nm command */
  if ((fp=fopen(tmp_nm_file, "r")) == (FILE *)0)
    {
      /* Hmmm, let's not treat this as fatal */
      fprintf(stderr, "Failed to open nm symbol file [%s]\n", tmp_nm_file);
      return;
    }

  /* Count the number of symbols in the nm symbol table */
  kern_sym_count=0;
  while ( (inchr = getc(fp)) != -1)
    {
      if (inchr == '\n')
	kern_sym_count++;
    }

  rewind(fp);

  /* Malloc the space for symbol table */
  if (kern_sym_count > 0)
    {
       kern_sym_tbl = (kern_sym_t *)malloc(kern_sym_count * sizeof (kern_sym_t));
       if (!kern_sym_tbl)
	 {
	   /* Hmmm, lets not treat this as fatal */
	   fprintf(stderr, "Can't allocate memory for kernel symbol table\n");
	 }
       else
	 bzero(kern_sym_tbl, (kern_sym_count * sizeof(kern_sym_t)));
    }
  else
    {
      /* Hmmm, lets not treat this as fatal */
      fprintf(stderr, "No kernel symbol table \n");
    }

  for (i=0; i<kern_sym_count; i++)
    {
      bzero(tmpstr, 1024);
      if (fscanf(fp, "%lx %c %s", &kern_sym_tbl[i].k_sym_addr, &inchr, tmpstr) != 3)
	break;
      else
	{
	  len = strlen(tmpstr);
	  kern_sym_tbl[i].k_sym_name = malloc(len + 1);

	  if (kern_sym_tbl[i].k_sym_name == (char *)0)
	    {
	      fprintf(stderr, "Can't allocate memory for symbol name [%s]\n", tmpstr);
	      kern_sym_tbl[i].k_sym_name = (char *)0;
	      len = 0;
	    }
	  else
	    strcpy(kern_sym_tbl[i].k_sym_name, tmpstr);

	  kern_sym_tbl[i].k_sym_len = len;
	}
    } /* end for */

  if (i != kern_sym_count)
    {
      /* Hmmm, didn't build up entire table from nm */
      /* scrap the entire thing */
      if (kern_sym_tbl)
	free (kern_sym_tbl);
      kern_sym_tbl = (kern_sym_t *)0;
      kern_sym_count = 0;
    }

  fclose(fp);

  /* Remove the temporary nm file */
  unlink(tmp_nm_file);

#if 0
  /* Dump the kernel symbol table */
  for (i=0; i < kern_sym_count; i++)
    {
      if (kern_sym_tbl[i].k_sym_name)
	  printf ("[%d] 0x%x    %s\n", i, 
		  kern_sym_tbl[i].k_sym_addr, kern_sym_tbl[i].k_sym_name);
      else
	  printf ("[%d] 0x%x    %s\n", i, 
		  kern_sym_tbl[i].k_sym_addr, "No symbol name");
    }
#endif
}

char *
pc_to_string(unsigned int pc, int max_len, int mode)
{
  int ret;
  int len;

  int binary_search();

  if (mode == 0)
    {
      sprintf(pcstring, "0x%-8x [usermode addr]", pc);
      return(pcstring);
    }

  ret=0;
  ret = binary_search(kern_sym_tbl, 0, kern_sym_count-1, pc);

  if (ret == -1)
    {
      sprintf(pcstring, "0x%x", pc);
      return(pcstring);
    }
  else if (kern_sym_tbl[ret].k_sym_name == (char *)0)
    {
      sprintf(pcstring, "0x%x", pc);
      return(pcstring);
    }
  else
    {
      if ((len = kern_sym_tbl[ret].k_sym_len) > (max_len - 8))
	    len = max_len - 8;

      memcpy(pcstring, kern_sym_tbl[ret].k_sym_name, len);
      sprintf(&pcstring[len], "+0x%-5lx", pc - kern_sym_tbl[ret].k_sym_addr);

      return (pcstring);
    }
}


/* Return -1 if not found, else return index */
int binary_search(list, low, high, addr)
kern_sym_t *list;
int low, high;
unsigned int  addr;
{
  int mid;
  
  mid = (low + high) / 2;
  
  if (low > high)
    return (-1);   /* failed */
  else if (low + 1 == high)
    { 
      if (list[low].k_sym_addr <= addr &&
	   addr < list[high].k_sym_addr)
	{
	  /* We have a range match */
	  return(low);
	}
      else if (list[high].k_sym_addr <= addr)
	{
	  return(high);
	}
      else
	return(-1);   /* Failed */
    }      
  else if (addr < list[mid].k_sym_addr)
    {
      return(binary_search (list, low, mid, addr));
    }
  else
    {
      return(binary_search (list, mid, high, addr));
    }
}

void
open_logfile(char *path)
{
    log_fp = fopen(path, "a");

    if (!log_fp)
      {
	/* failed to open path */
	fprintf(stderr, "latency: failed to open logfile [%s]\n", path);
	exit_usage();
      }
}
