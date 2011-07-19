/*
 * Copyright (c) 1999-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
   cc -I/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders -DPRIVATE -D__APPLE_PRIVATE -arch x86_64 -arch i386 -O -o latency latency.c -lncurses -lutil
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
#include <libc.h>
#include <termios.h>
#include <curses.h>
#include <libutil.h>
#include <errno.h>
#include <err.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <mach/mach_error.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mach_syscalls.h>
#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <libkern/OSTypes.h>


int      s_usec_10_bins[10];
int      s_usec_100_bins[10];
int      s_msec_1_bins[10];
int      s_msec_10_bins[5];
int      s_too_slow;
int      s_max_latency;
int      s_min_latency = 0;
long long s_total_latency = 0;
int      s_total_samples = 0;
long     s_thresh_hold;
int      s_exceeded_threshold = 0;


#define N_HIGH_RES_BINS 500
int      use_high_res_bins = false;

struct i_latencies {
	int      i_usec_10_bins[10];
	int      i_usec_100_bins[10];
	int      i_msec_1_bins[10];
	int      i_msec_10_bins[5];
	int      i_too_slow;
	int      i_max_latency;
	int      i_min_latency;
	int      i_total_samples;
	int	 i_total;
	int      i_exceeded_threshold;
	uint64_t i_total_latency;
};

struct	i_latencies *i_lat;
boolean_t i_latency_per_cpu = FALSE;

int      i_high_res_bins[N_HIGH_RES_BINS];

long     i_thresh_hold;

int	 watch_priority = 97;

long     start_time;
long     curr_time;
long     refresh_time;


char *kernelpath = NULL;

typedef struct {
	void	 *k_sym_addr;	/* kernel symbol address from nm */
	u_int	  k_sym_len;	/* length of kernel symbol string */
	char     *k_sym_name;	/* kernel symbol string from nm */
} kern_sym_t;

kern_sym_t *kern_sym_tbl;	/* pointer to the nm table       */
int        kern_sym_count;	/* number of entries in nm table */



#define MAX_ENTRIES 4096
struct ct {
	int type;
	char name[32];
} codes_tab[MAX_ENTRIES];

char *code_file = NULL;
int	num_of_codes = 0;


double divisor;
sig_atomic_t gotSIGWINCH = 0;
int	trace_enabled = 0;
int	need_new_map = 1;
int	set_remove_flag = 1;	/* By default, remove trace buffer */

int	RAW_flag = 0;
int	RAW_fd	 = 0;

uint64_t first_now = 0;
uint64_t last_now = 0;
int	first_read = 1;


#define	SAMPLE_TIME_USECS 50000
#define SAMPLE_SIZE 300000
#define MAX_LOG_COUNT  30       /* limits the number of entries dumped in log_decrementer */

kbufinfo_t bufinfo = {0, 0, 0};

FILE *log_fp = NULL;

uint64_t sample_TOD_secs;
uint32_t sample_TOD_usecs;

int	sample_generation = 0;
int	num_i_latency_cpus = 1;
int	num_cpus;
char *my_buffer;
int	num_entries;

kd_buf **last_decrementer_kd;   /* last DECR_TRAP per cpu */


#define NUMPARMS 23

typedef struct event *event_t;

struct event {
	event_t	  ev_next;

	uintptr_t ev_thread;
	uint32_t  ev_type;
	uint64_t  ev_timestamp;
};


typedef struct lookup *lookup_t;

struct lookup {
	lookup_t  lk_next;
	
	uintptr_t lk_thread;
	uintptr_t lk_dvp;
	long	 *lk_pathptr;
	long	  lk_pathname[NUMPARMS + 1];
};


typedef struct threadmap *threadmap_t;

struct threadmap {
	threadmap_t	tm_next;
	
	uintptr_t	tm_thread;
	uintptr_t	tm_pthread;
	char		tm_command[MAXCOMLEN + 1];
	char		tm_orig_command[MAXCOMLEN + 1];
};


typedef struct threadrun *threadrun_t;

struct threadrun {
	threadrun_t	tr_next;
	
	uintptr_t	tr_thread;
	kd_buf		*tr_entry;
	uint64_t	tr_timestamp;
};


typedef struct thread_entry *thread_entry_t;

struct thread_entry {
	thread_entry_t	te_next;

	uintptr_t	te_thread;
};
	

#define HASH_SIZE       1024
#define HASH_MASK       1023

event_t         event_hash[HASH_SIZE];
lookup_t        lookup_hash[HASH_SIZE];
threadmap_t     threadmap_hash[HASH_SIZE];
threadrun_t	threadrun_hash[HASH_SIZE];

event_t         event_freelist;
lookup_t        lookup_freelist;
threadrun_t	threadrun_freelist;
threadmap_t     threadmap_freelist;
threadmap_t     threadmap_temp;

thread_entry_t	thread_entry_freelist;
thread_entry_t	thread_delete_list;
thread_entry_t	thread_reset_list;
thread_entry_t	thread_event_list;
thread_entry_t	thread_lookup_list;
thread_entry_t	thread_run_list;


#ifndef	RAW_VERSION1
typedef struct {
	int version_no;
	int thread_count;
	uint64_t TOD_secs;
	uint32_t TOD_usecs;
} RAW_header;

#define RAW_VERSION0    0x55aa0000
#define RAW_VERSION1    0x55aa0101
#endif


#define	USER_MODE	0
#define KERNEL_MODE	1


#define TRACE_DATA_NEWTHREAD	0x07000004
#define TRACE_STRING_NEWTHREAD	0x07010004
#define TRACE_STRING_EXEC	0x07010008

#define INTERRUPT         	0x01050000
#define DECR_TRAP         	0x01090000
#define DECR_SET          	0x01090004
#define MACH_vmfault     	0x01300008
#define MACH_sched        	0x01400000
#define MACH_stkhandoff   	0x01400008
#define MACH_makerunnable	0x01400018
#define MACH_idle		0x01400024
#define VFS_LOOKUP        	0x03010090
#define IES_action        	0x050b0018
#define IES_filter        	0x050b001c
#define TES_action        	0x050c0010
#define CQ_action         	0x050d0018
#define CPUPM_CPUSTER_RUNCOUNT	0x05310144

#define BSC_exit          	0x040C0004
#define BSC_thread_terminate	0x040c05a4

#define DBG_FUNC_MASK	~(DBG_FUNC_START | DBG_FUNC_END)

#define CPU_NUMBER(kp)	kdbg_get_cpu(kp)

#define EMPTYSTRING	""


const char *fault_name[] = {
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

const char *sched_reasons[] = {
	"N",
	"P",
	"Q",
	"?",
	"u",
	"U",
	"?",
	"?",
	"H",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"?",
	"Y"
};

#define ARRAYSIZE(x) ((int)(sizeof(x) / sizeof(*x)))
#define MAX_REASON ARRAYSIZE(sched_reasons)

static double handle_decrementer(kd_buf *, int);
static kd_buf *log_decrementer(kd_buf *kd_beg, kd_buf *kd_end, kd_buf *end_of_sample, double i_latency);
static void read_command_map(void);
static void enter_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, int print_info);
static void exit_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, int print_info);
static void print_entry(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, kd_buf *kd_note);
static void log_info(uint64_t now, uint64_t idelta, uint64_t start_bias, kd_buf *kd, kd_buf *kd_note);
static char *find_code(int);
static void pc_to_string(char *pcstring, uintptr_t pc, int max_len, int mode);
static void getdivisor(void);
static int sample_sc(void);
static void init_code_file(void);
static void do_kernel_nm(void);
static void open_logfile(const char*);
static int binary_search(kern_sym_t *list, int low, int high, uintptr_t addr);

static void create_map_entry(uintptr_t, char *);
static void check_for_thread_update(uintptr_t thread, int debugid_base, kd_buf *kbufp, char **command);
static void log_scheduler(kd_buf *kd_start, kd_buf *kd_stop, kd_buf *end_of_sample, double s_latency, uintptr_t thread);
static int check_for_scheduler_latency(int type, uintptr_t *thread, uint64_t now, kd_buf *kd, kd_buf **kd_start, double *latency);
static void open_rawfile(const char *path);

static void screen_update(FILE *);

static void set_enable(int);
static void set_remove(void);

static int
quit(char *s)
{
	if (!RAW_flag) {
		if (trace_enabled) {
			set_enable(0);
		}
		/* 
		 *  This flag is turned off when calling
		 * quit() due to a set_remove() failure.
		 */
		if (set_remove_flag) {
			set_remove();
		}
	}
	printf("latency: ");
	if (s) {
		printf("%s", s);
	}
	exit(1);
}

void
set_enable(int val) 
{
	int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDENABLE, val };
	size_t needed;

	if (sysctl(mib, ARRAYSIZE(mib), NULL, &needed, NULL, 0) < 0) {
		quit("trace facility failure, KERN_KDENABLE\n");
	}
}

void
set_numbufs(int nbufs) 
{
	int mib1[] = { CTL_KERN, KERN_KDEBUG, KERN_KDSETBUF, nbufs };
	int mib2[] = { CTL_KERN, KERN_KDEBUG, KERN_KDSETUP };
	size_t needed;

	if (sysctl(mib1, ARRAYSIZE(mib1), NULL, &needed, NULL, 0) < 0) {
		quit("trace facility failure, KERN_KDSETBUF\n");
	}
	if (sysctl(mib2, ARRAYSIZE(mib2), NULL, &needed, NULL, 0) < 0) {
		quit("trace facility failure, KERN_KDSETUP\n");
	}
}

void
set_pidexclude(int pid, int on_off) 
{
	int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDPIDEX };
	size_t needed = sizeof(kd_regtype);

	kd_regtype kr = {
		.type = KDBG_TYPENONE,
		.value1 = pid,
		.value2 = on_off
	};

	sysctl(mib, ARRAYSIZE(mib), &kr, &needed, NULL, 0);
}

void
get_bufinfo(kbufinfo_t *val)
{
	int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDGETBUF };
	size_t needed = sizeof (*val);
	
	if (sysctl(mib, ARRAYSIZE(mib), val, &needed, 0, 0) < 0) {
		quit("trace facility failure, KERN_KDGETBUF\n");
	}
}

void
set_remove(void)
{
	int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDREMOVE };
	size_t needed;

	errno = 0;

	if (sysctl(mib, ARRAYSIZE(mib), NULL, &needed, NULL, 0) < 0) {
		set_remove_flag = 0;
		if (errno == EBUSY) {
			quit("the trace facility is currently in use...\n         fs_usage, sc_usage, and latency use this feature.\n\n");
		} else {
			quit("trace facility failure, KERN_KDREMOVE\n");
		}
	}
}


void
write_high_res_latencies(void)
{
	int i;
	FILE *f;

	if (use_high_res_bins) {
		if ((f = fopen("latencies.csv","w"))) {
			for (i = 0; i < N_HIGH_RES_BINS; i++) {
				fprintf(f, "%d,%d\n", i, i_high_res_bins[i]);
			}
			fclose(f);
		}
	}
}

void
sigintr(int signo __attribute__((unused)))
{
	write_high_res_latencies();

	set_enable(0);
	set_pidexclude(getpid(), 0);
	screen_update(log_fp);
	endwin();
	set_remove();

	exit(1);
}

/* exit under normal conditions -- signal handler */
void
leave(int signo __attribute__((unused)))
{
	write_high_res_latencies();

	set_enable(0);
	set_pidexclude(getpid(), 0);
	endwin();
	set_remove();
	
	exit(1);
}

void
sigwinch(int signo __attribute__((unused)))
{
	gotSIGWINCH = 1;
}

void
print_total(FILE *fp, char *s, int total)
{
	int  cpu;
	int  clen;
	int  itotal;
	struct i_latencies *il;
	char tbuf[512];

	for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];
		itotal += il->i_total;
	}
	clen = sprintf(tbuf, "%s  %10d      %9d", s, total, itotal);

	for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];

		if (i_latency_per_cpu == TRUE) {
			clen += sprintf(&tbuf[clen], " %9d", il->i_total);
		}

		il->i_total = 0;
	}
	sprintf(&tbuf[clen], "\n");
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}
}



void
screen_update(FILE *fp)
{
	int i;
	int cpu;
	int clen;
	int itotal, stotal;
	int elapsed_secs;
	int elapsed_mins;
	int elapsed_hours;
	int min_lat, max_lat;
	uint64_t tot_lat;
	unsigned int average_s_latency;
	unsigned int average_i_latency;
	struct i_latencies *il;
	char tbuf[1024];

	if (fp == NULL) {
		erase();
		move(0, 0);
	} else {
		fprintf(fp,"\n\n===================================================================================================\n");
	}
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
	if (RAW_flag) {
		curr_time = sample_TOD_secs;
		elapsed_secs = ((last_now - first_now) / divisor) / 1000000;
	} else {
		elapsed_secs = curr_time - start_time;
	}

	elapsed_hours = elapsed_secs / 3600;
	elapsed_secs -= elapsed_hours * 3600;
	elapsed_mins = elapsed_secs / 60;
	elapsed_secs -= elapsed_mins * 60;

	sprintf(tbuf, "%-19.19s                            %2ld:%02ld:%02ld\n", &(ctime(&curr_time)[0]),
		(long)elapsed_hours, (long)elapsed_mins, (long)elapsed_secs);
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}

	sprintf(tbuf, "                     SCHEDULER     INTERRUPTS\n");
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}

	if (i_latency_per_cpu == TRUE) {
		clen = sprintf(tbuf, "                                        Total");

		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			if (cpu <= 9) {
				clen += sprintf(&tbuf[clen], "     CPU %d", cpu);
			} else {
				clen += sprintf(&tbuf[clen], "    CPU %d", cpu);
			}
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}

		clen = sprintf(tbuf, "\n-------------------------------------------------------");

		for (cpu = 1; cpu < num_i_latency_cpus; cpu++) {
			clen += sprintf(&tbuf[clen], "----------");
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	} else {
		sprintf(tbuf, "---------------------------------------------");
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	}
	for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];
		itotal += il->i_total_samples;
	}
	clen = sprintf(tbuf, "\ntotal_samples       %10d      %9d", s_total_samples, itotal);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];

			clen += sprintf(&tbuf[clen], " %9d", il->i_total_samples);
		}
	}
	sprintf(&tbuf[clen], "\n");
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}


	for (stotal = 0, i = 0; i < 10; i++) {
		for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];

			itotal += il->i_usec_10_bins[i];
			il->i_total += il->i_usec_10_bins[i];
		}
		clen = sprintf(tbuf, "\ndelays < %3d usecs  %10d      %9d", (i + 1) * 10, s_usec_10_bins[i], itotal);

		stotal += s_usec_10_bins[i];

		if (i_latency_per_cpu == TRUE) {
			for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
				il = &i_lat[cpu];
				
				clen += sprintf(&tbuf[clen], " %9d", il->i_usec_10_bins[i]);
			}
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	}
	print_total(fp, "\ntotal  < 100 usecs", stotal);

	for (stotal = 0, i = 1; i < 10; i++) {
		for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];

			itotal += il->i_usec_100_bins[i];
			il->i_total += il->i_usec_100_bins[i];
		}
		if (i < 9) {
			clen = sprintf(tbuf, "\ndelays < %3d usecs  %10d      %9d", (i + 1) * 100, s_usec_100_bins[i], itotal);
		} else {
			clen = sprintf(tbuf, "\ndelays <   1 msec   %10d      %9d", s_usec_100_bins[i], itotal);
		}

		stotal += s_usec_100_bins[i];

		if (i_latency_per_cpu == TRUE) {
			for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
				il = &i_lat[cpu];

				clen += sprintf(&tbuf[clen], " %9d", il->i_usec_100_bins[i]);
			}
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	}
	print_total(fp, "\ntotal  <   1 msec ", stotal);


	for (stotal = 0, i = 1; i < 10; i++) {
		for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];

			itotal += il->i_msec_1_bins[i];
			il->i_total += il->i_msec_1_bins[i];
		}
		clen = sprintf(tbuf, "\ndelays < %3d msecs  %10d      %9d", (i + 1), s_msec_1_bins[i], itotal);

		stotal += s_msec_1_bins[i];

		if (i_latency_per_cpu == TRUE) {
			for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
				il = &i_lat[cpu];

				clen += sprintf(&tbuf[clen], " %9d", il->i_msec_1_bins[i]);
			}
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	}
	print_total(fp, "\ntotal  <  10 msecs", stotal);

	for (stotal = 0, i = 1; i < 5; i++) {
		for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];

			itotal += il->i_msec_10_bins[i];
			il->i_total += il->i_msec_10_bins[i];
		}
		clen = sprintf(tbuf, "\ndelays < %3d msecs  %10d      %9d", (i + 1)*10, s_msec_10_bins[i], itotal);

		stotal += s_msec_10_bins[i];

		if (i_latency_per_cpu == TRUE) {
			for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
				il = &i_lat[cpu];

				clen += sprintf(&tbuf[clen], " %9d", il->i_msec_10_bins[i]);
			}
		}
		if (fp) {
			fprintf(fp, "%s", tbuf);
		} else {
			printw(tbuf);
		}
	}
	print_total(fp, "\ntotal  <  50 msecs", stotal);


	for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];
		itotal += il->i_too_slow;
	}
	clen = sprintf(tbuf, "\ndelays >  50 msecs  %10d      %9d", s_too_slow, itotal);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];
		
			clen += sprintf(&tbuf[clen], " %9d", il->i_too_slow);
		}
	}
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}

	for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];

		if (cpu == 0 || (il->i_min_latency < min_lat)) {
			min_lat = il->i_min_latency;
		}
	}
	clen = sprintf(tbuf, "\n\nminimum latency(usecs) %7d      %9d", s_min_latency, min_lat);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];
		
			clen += sprintf(&tbuf[clen], " %9d", il->i_min_latency);
		}
	}
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}


	for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];

		if (cpu == 0 || (il->i_max_latency > max_lat)) {
			max_lat = il->i_max_latency;
		}
	}
	clen = sprintf(tbuf, "\nmaximum latency(usecs) %7d      %9d", s_max_latency, max_lat);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];
		
			clen += sprintf(&tbuf[clen], " %9d", il->i_max_latency);
		}
	}
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}
	
	if (s_total_samples) {
		average_s_latency = (unsigned int)(s_total_latency/s_total_samples);
	} else {
		average_s_latency = 0;
	}

	for (itotal = 0, tot_lat = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];
		
		itotal += il->i_total_samples;
		tot_lat += il->i_total_latency;
	}
	if (itotal) {
		average_i_latency = (unsigned)(tot_lat/itotal);
	} else {
		average_i_latency = 0;
	}

	clen = sprintf(tbuf, "\naverage latency(usecs) %7d      %9d", average_s_latency, average_i_latency);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];
		
			if (il->i_total_samples) {
				average_i_latency = (unsigned int)(il->i_total_latency/il->i_total_samples);
			} else {
				average_i_latency = 0;
			}

			clen += sprintf(&tbuf[clen], " %9d", average_i_latency);
		}
	}
	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}
	
	for (itotal = 0, cpu = 0; cpu < num_i_latency_cpus; cpu++) {
		il = &i_lat[cpu];
		
		itotal += il->i_exceeded_threshold;
	}
	clen = sprintf(tbuf, "\nexceeded threshold     %7d      %9d", s_exceeded_threshold, itotal);

	if (i_latency_per_cpu == TRUE) {
		for (cpu = 0; cpu < num_i_latency_cpus; cpu++) {
			il = &i_lat[cpu];
		
			clen += sprintf(&tbuf[clen], " %9d", il->i_exceeded_threshold);
		}
	}
	sprintf(&tbuf[clen], "\n");

	if (fp) {
		fprintf(fp, "%s", tbuf);
	} else {
		printw(tbuf);
	}	

	if (fp == NULL) {
		refresh();
	} else {
		fflush(fp);
	}
}

int
exit_usage(void)
{
	fprintf(stderr, "Usage: latency [-p priority] [-h] [-m] [-st threshold] [-it threshold]\n");
	fprintf(stderr, "               [-c codefile] [-l logfile] [-R rawfile] [-n kernel]\n\n");
 
	fprintf(stderr, "  -p    specify scheduling priority to watch... default is realtime\n");
	fprintf(stderr, "  -h    Display high resolution interrupt latencies and write them to latencies.csv (truncate existing file) upon exit.\n");
	fprintf(stderr, "  -st   set scheduler latency threshold in microseconds... if latency exceeds this, then log trace\n");
	fprintf(stderr, "  -m    specify per-CPU interrupt latency reporting\n");	
	fprintf(stderr, "  -it   set interrupt latency threshold in microseconds... if latency exceeds this, then log trace\n");
	fprintf(stderr, "  -c    specify name of codes file... default is /usr/share/misc/trace.codes\n");
	fprintf(stderr, "  -l    specify name of file to log trace entries to when the specified threshold is exceeded\n");
	fprintf(stderr, "  -R    specify name of raw trace file to process\n");
	fprintf(stderr, "  -n    specify kernel... default is /mach_kernel\n");	

	fprintf(stderr, "\nlatency must be run as root\n\n");

	exit(1);
}


int
main(int argc, char *argv[])
{
	if (0 != reexec_to_match_kernel()) {
		fprintf(stderr, "Could not re-execute: %d\n", errno);
		exit(1);
	}
	while (argc > 1) {

		if (strcmp(argv[1], "-R") == 0) {
			argc--;
			argv++;

			if (argc > 1) {
				open_rawfile(argv[1]);
			} else {
				exit_usage();
			}

			RAW_flag = 1;

		} else if (strcmp(argv[1], "-p") == 0) {
			argc--;
			argv++;

			if (argc > 1) {
				watch_priority = atoi(argv[1]);
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-st") == 0) {
			argc--;
			argv++;

			if (argc > 1) {
				s_thresh_hold = atoi(argv[1]);
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-it") == 0) {
			argc--;
			argv++;
			
			if (argc > 1) {
				i_thresh_hold = atoi(argv[1]);
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-c") == 0) {
			argc--;
			argv++;
			
			if (argc > 1) {
				code_file = argv[1];
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-l") == 0) {
			argc--;
			argv++;
			
			if (argc > 1) {
				open_logfile(argv[1]);
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-n") == 0) {
			argc--;
			argv++;

			if (argc > 1) {
				kernelpath = argv[1];
			} else {
				exit_usage();
			}
		} else if (strcmp(argv[1], "-h") == 0) {
			use_high_res_bins = TRUE;

		} else if (strcmp(argv[1], "-m") == 0) {
			i_latency_per_cpu = TRUE;

		} else {
		        exit_usage();
		}

		argc--;
		argv++;
	}
	if (!RAW_flag) {
		if (geteuid() != 0) {
			printf("'latency' must be run as root...\n");
			exit(1);
		}
	}
	if (kernelpath == NULL) {
		kernelpath = "/mach_kernel";
	}

	if (code_file == NULL) {
		code_file = "/usr/share/misc/trace.codes";
	}

	do_kernel_nm();

	getdivisor();

	init_code_file();

	if (!RAW_flag) {
		if (initscr() == NULL) {
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

		/*
		 * grab the number of cpus and scale the buffer size
		 */
		int mib[] = { CTL_HW, HW_NCPU };
		size_t len = sizeof(num_cpus);

		sysctl(mib, ARRAYSIZE(mib), &num_cpus, &len, NULL, 0);

		set_remove();
		set_numbufs(SAMPLE_SIZE * num_cpus);

		get_bufinfo(&bufinfo);

		set_enable(0);

		set_pidexclude(getpid(), 1);
		set_enable(1);

		num_entries = bufinfo.nkdbufs;
	} else {
		num_entries = 50000;
		num_cpus    = 128;
	}

	if ((my_buffer = malloc(num_entries * sizeof(kd_buf))) == NULL) {
		quit("can't allocate memory for tracing info\n");
	}

	if ((last_decrementer_kd = (kd_buf **)malloc(num_cpus * sizeof(kd_buf *))) == NULL) {
		quit("can't allocate memory for decrementer tracing info\n");
	}

	if (i_latency_per_cpu == FALSE) {
		num_i_latency_cpus = 1;
	} else {
		num_i_latency_cpus = num_cpus;
	}

	if ((i_lat = (struct i_latencies *)malloc(num_i_latency_cpus * sizeof(struct i_latencies))) == NULL) {
		quit("can't allocate memory for interrupt latency info\n");
	}

	bzero((char *)i_lat, num_i_latency_cpus * sizeof(struct i_latencies));

	if (RAW_flag) {
		while (sample_sc()) {
			continue;
		}

		if (log_fp) {
			screen_update(log_fp);
		}

		screen_update(stdout);

	} else {
		uint64_t adelay;
		double	fdelay;
		double	nanosecs_to_sleep;

		nanosecs_to_sleep = (double)(SAMPLE_TIME_USECS * 1000);
		fdelay = nanosecs_to_sleep * (divisor /1000);
		adelay = (uint64_t)fdelay;

		trace_enabled = 1;

		start_time = time(NULL);
		refresh_time = start_time;

		for (;;) {
			curr_time = time(NULL);

			if (curr_time >= refresh_time) {
				screen_update(NULL);
				refresh_time = curr_time + 1;
			}
			mach_wait_until(mach_absolute_time() + adelay);

			sample_sc();

			if (gotSIGWINCH) {
				/*
				 * No need to check for initscr error return.
				 * We won't get here if it fails on the first call.
				 */
				endwin();
				clear();
				refresh();

				gotSIGWINCH = 0;
			}
		}
	}
}


												  
void
read_command_map(void)
{
	kd_threadmap *mapptr = 0;
	int	total_threads = 0;
	size_t	size;
	off_t	offset;
	int	i;
	RAW_header header = {0};

	if (RAW_flag) {
                if (read(RAW_fd, &header, sizeof(RAW_header)) != sizeof(RAW_header)) {
                        perror("read failed");
			exit(2);
                }
		if (header.version_no != RAW_VERSION1) {
			header.version_no = RAW_VERSION0;
			header.TOD_secs = time(NULL);
			header.TOD_usecs = 0;

			lseek(RAW_fd, (off_t)0, SEEK_SET);

			if (read(RAW_fd, &header.thread_count, sizeof(int)) != sizeof(int)) {
				perror("read failed");
				exit(2);
			}
		}
                total_threads = header.thread_count;
		
		sample_TOD_secs = header.TOD_secs;
		sample_TOD_usecs = header.TOD_usecs;

		if (total_threads == 0 && header.version_no != RAW_VERSION0) {
			offset = lseek(RAW_fd, (off_t)0, SEEK_CUR);
			offset = (offset + (4095)) & ~4095;

			lseek(RAW_fd, offset, SEEK_SET);
		}
	} else {
		total_threads = bufinfo.nkdthreads;
	}
		
	size = total_threads * sizeof(kd_threadmap);

	if (size == 0 || ((mapptr = (kd_threadmap *) malloc(size)) == 0)) {
		return;
	}
	bzero (mapptr, size);
 
	/*
	 * Now read the threadmap
	 */
	if (RAW_flag) {
		if (read(RAW_fd, mapptr, size) != size) {
			printf("Can't read the thread map -- this is not fatal\n");
		}
		if (header.version_no != RAW_VERSION0) {
			offset = lseek(RAW_fd, (off_t)0, SEEK_CUR);
			offset = (offset + (4095)) & ~4095;

			lseek(RAW_fd, offset, SEEK_SET);
		}
	} else {
		int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDTHRMAP};
		if (sysctl(mib, ARRAYSIZE(mib), mapptr, &size, NULL, 0) < 0) {
			/*
			 * This is not fatal -- just means I cant map command strings
			 */
			printf("Can't read the thread map -- this is not fatal\n");

			total_threads = 0;
		}
	}
	for (i = 0; i < total_threads; i++) {
		create_map_entry(mapptr[i].thread, &mapptr[i].command[0]);
	}
	free(mapptr);
}

void
create_map_entry(uintptr_t thread, char *command)
{
	threadmap_t tme;

	if ((tme = threadmap_freelist)) {
		threadmap_freelist = tme->tm_next;
	} else {
		tme = (threadmap_t)malloc(sizeof(struct threadmap));
	}

	tme->tm_thread = thread;

	(void)strncpy (tme->tm_command, command, MAXCOMLEN);
	tme->tm_command[MAXCOMLEN] = '\0';
	tme->tm_orig_command[0] = '\0';

	int hashid = thread & HASH_MASK;

	tme->tm_next = threadmap_hash[hashid];
	threadmap_hash[hashid] = tme;
}

void
delete_thread_entry(uintptr_t thread)
{
	threadmap_t tme;

	int hashid = thread & HASH_MASK;

	if ((tme = threadmap_hash[hashid])) {
		if (tme->tm_thread == thread) {
			threadmap_hash[hashid] = tme->tm_next;
		} else {
			threadmap_t tme_prev = tme;

			for (tme = tme->tm_next; tme; tme = tme->tm_next) {
				if (tme->tm_thread == thread) {
					tme_prev->tm_next = tme->tm_next;
					break;
				}
				tme_prev = tme;
			}
		}
		if (tme) {
			tme->tm_next = threadmap_freelist;
			threadmap_freelist = tme;
		}
	}
}

void
find_and_insert_tmp_map_entry(uintptr_t pthread, char *command)
{
	threadmap_t tme;

	if ((tme = threadmap_temp)) {
		if (tme->tm_pthread == pthread) {
			threadmap_temp = tme->tm_next;
		} else {
			threadmap_t tme_prev = tme;

			for (tme = tme->tm_next; tme; tme = tme->tm_next) {
				if (tme->tm_pthread == pthread) {
					tme_prev->tm_next = tme->tm_next;
					break;
				}
				tme_prev = tme;
			}
		}
		if (tme) {
			(void)strncpy (tme->tm_command, command, MAXCOMLEN);
			tme->tm_command[MAXCOMLEN] = '\0';
			tme->tm_orig_command[0] = '\0';

			int hashid = tme->tm_thread & HASH_MASK;
			tme->tm_next = threadmap_hash[hashid];
			threadmap_hash[hashid] = tme;
		}
	}
}

void
create_tmp_map_entry(uintptr_t thread, uintptr_t pthread)
{
	threadmap_t tme;

	if ((tme = threadmap_freelist)) {
		threadmap_freelist = tme->tm_next;
	} else {
		tme = malloc(sizeof(struct threadmap));
	}

	tme->tm_thread = thread;
	tme->tm_pthread = pthread;
	tme->tm_command[0] = '\0';
	tme->tm_orig_command[0] = '\0';

	tme->tm_next = threadmap_temp;
	threadmap_temp = tme;
}

threadmap_t
find_thread_entry(uintptr_t thread)
{
	threadmap_t tme;

	int hashid = thread & HASH_MASK;

	for (tme = threadmap_hash[hashid]; tme; tme = tme->tm_next) {
		if (tme->tm_thread == thread) {
			return tme;
		}
	}
	return 0;
}

void
find_thread_name(uintptr_t thread, char **command)
{
	threadmap_t     tme;

	if ((tme = find_thread_entry(thread))) {
		*command = tme->tm_command;
	} else {
		*command = EMPTYSTRING;
	}
}

void
add_thread_entry_to_list(thread_entry_t *list, uintptr_t thread)
{
	thread_entry_t	te;

	if ((te = thread_entry_freelist)) {
		thread_entry_freelist = te->te_next;
	} else {
		te = (thread_entry_t)malloc(sizeof(struct thread_entry));
	}

	te->te_thread = thread;
	te->te_next = *list;
	*list = te;
}

void
exec_thread_entry(uintptr_t thread, char *command)
{
	threadmap_t	tme;

	if ((tme = find_thread_entry(thread))) {
		if (tme->tm_orig_command[0] == '\0') {
			(void)strncpy (tme->tm_orig_command, tme->tm_command, MAXCOMLEN);
			tme->tm_orig_command[MAXCOMLEN] = '\0';
		}
		(void)strncpy (tme->tm_command, command, MAXCOMLEN);
		tme->tm_command[MAXCOMLEN] = '\0';

		add_thread_entry_to_list(&thread_reset_list, thread);
	} else {
		create_map_entry(thread, command);
	}
}

void
record_thread_entry_for_gc(uintptr_t thread)
{
	add_thread_entry_to_list(&thread_delete_list, thread);
}

void
gc_thread_entries(void)
{
	thread_entry_t te;
	thread_entry_t te_next;
	int count = 0;

	for (te = thread_delete_list; te; te = te_next) {
		delete_thread_entry(te->te_thread);

		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;

		count++;
	}
	thread_delete_list = 0;
}

void
gc_reset_entries(void)
{
	thread_entry_t te;
	thread_entry_t te_next;
	int count = 0;

	for (te = thread_reset_list; te; te = te_next) {
		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;

		count++;
	}
	thread_reset_list = 0;
}

void
reset_thread_names(void)
{
	thread_entry_t te;
	thread_entry_t te_next;
	int count = 0;

	for (te = thread_reset_list; te; te = te_next) {
		threadmap_t     tme;

		if ((tme = find_thread_entry(te->te_thread))) {
			if (tme->tm_orig_command[0]) {
				(void)strncpy (tme->tm_command, tme->tm_orig_command, MAXCOMLEN);
				tme->tm_command[MAXCOMLEN] = '\0';
				tme->tm_orig_command[0] = '\0';
			}
		}
		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;

		count++;
	}
	thread_reset_list = 0;
}

void
delete_all_thread_entries(void)
{
	threadmap_t tme = 0;
	threadmap_t tme_next = 0;
	int i;

	for (i = 0; i < HASH_SIZE; i++) {
		for (tme = threadmap_hash[i]; tme; tme = tme_next) {
			tme_next = tme->tm_next;
                        tme->tm_next = threadmap_freelist;
			threadmap_freelist = tme;
		}
		threadmap_hash[i] = 0;
	}
}




static void
insert_run_event(uintptr_t thread, kd_buf *kd, uint64_t now)
{
	threadrun_t	trp;

	int hashid = thread & HASH_MASK;

	for (trp = threadrun_hash[hashid]; trp; trp = trp->tr_next) {
		if (trp->tr_thread == thread) {
			break;
		}
	}
	if (trp == NULL) {
		if ((trp = threadrun_freelist)) {
			threadrun_freelist = trp->tr_next;
		} else {
			trp = (threadrun_t)malloc(sizeof(struct threadrun));
		}

		trp->tr_thread = thread;

		trp->tr_next = threadrun_hash[hashid];
		threadrun_hash[hashid] = trp;

		add_thread_entry_to_list(&thread_run_list, thread);
	}
	trp->tr_entry = kd;
	trp->tr_timestamp = now;
}

static threadrun_t
find_run_event(uintptr_t thread)
{
	threadrun_t trp;
	int hashid = thread & HASH_MASK;

	for (trp = threadrun_hash[hashid]; trp; trp = trp->tr_next) {
		if (trp->tr_thread == thread) {
			return trp;
		}
	}
	return 0;
}

static void
delete_run_event(uintptr_t thread)
{
	threadrun_t	trp = 0;
	threadrun_t trp_prev;

	int hashid = thread & HASH_MASK;

	if ((trp = threadrun_hash[hashid])) {
		if (trp->tr_thread == thread) {
			threadrun_hash[hashid] = trp->tr_next;
		} else {
			trp_prev = trp;

			for (trp = trp->tr_next; trp; trp = trp->tr_next) {
				if (trp->tr_thread == thread) {
					trp_prev->tr_next = trp->tr_next;
					break;
				}
				trp_prev = trp;
			}
		}
		if (trp) {
			trp->tr_next = threadrun_freelist;
			threadrun_freelist = trp;
		}
	}
}

static void
gc_run_events(void) {
	thread_entry_t te;
	thread_entry_t te_next;
	threadrun_t	trp;
	threadrun_t	trp_next;
	int count = 0;

	for (te = thread_run_list; te; te = te_next) {
		int hashid = te->te_thread & HASH_MASK;

		for (trp = threadrun_hash[hashid]; trp; trp = trp_next) {
			trp_next = trp->tr_next;
			trp->tr_next = threadrun_freelist;
			threadrun_freelist = trp;
			count++;
		}
		threadrun_hash[hashid] = 0;

		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;
	}
	thread_run_list = 0;
}



static void
insert_start_event(uintptr_t thread, int type, uint64_t now)
{
	event_t evp;

	int hashid = thread & HASH_MASK;

	for (evp = event_hash[hashid]; evp; evp = evp->ev_next) {
		if (evp->ev_thread == thread && evp->ev_type == type) {
			break;
		}
	}
	if (evp == NULL) {
		if ((evp = event_freelist)) {
			event_freelist = evp->ev_next;
		} else {
			evp = (event_t)malloc(sizeof(struct event));
		}

		evp->ev_thread = thread;
		evp->ev_type = type;

		evp->ev_next = event_hash[hashid];
		event_hash[hashid] = evp;

		add_thread_entry_to_list(&thread_event_list, thread);
	}
	evp->ev_timestamp = now;
}


static uint64_t
consume_start_event(uintptr_t thread, int type, uint64_t now)
{
	event_t evp;
	event_t evp_prev;
	uint64_t elapsed = 0;

	int hashid = thread & HASH_MASK;

	if ((evp = event_hash[hashid])) {
		if (evp->ev_thread == thread && evp->ev_type == type) {
			event_hash[hashid] = evp->ev_next;
		} else {
			evp_prev = evp;

			for (evp = evp->ev_next; evp; evp = evp->ev_next) {
				if (evp->ev_thread == thread && evp->ev_type == type) {
					evp_prev->ev_next = evp->ev_next;
					break;
				}
				evp_prev = evp;
			}
		}
		if (evp) {
			elapsed = now - evp->ev_timestamp;

			if (now < evp->ev_timestamp) {
				printf("consume: now = %qd,  timestamp = %qd\n", now, evp->ev_timestamp);
				elapsed = 0;
			}
			evp->ev_next = event_freelist;
			event_freelist = evp;
		}
	}
	return elapsed;
}

static void
gc_start_events(void)
{
	thread_entry_t	te;
	thread_entry_t	te_next;
	event_t         evp;
	event_t         evp_next;
	int		count = 0;
        int		hashid;

	for (te = thread_event_list; te; te = te_next) {

		hashid = te->te_thread & HASH_MASK;

		for (evp = event_hash[hashid]; evp; evp = evp_next) {
			evp_next = evp->ev_next;
                        evp->ev_next = event_freelist;
                        event_freelist = evp;
			count++;
		}
		event_hash[hashid] = 0;

		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;
	}
	thread_event_list = 0;
}

int
thread_in_user_mode(uintptr_t thread, char *command)
{
	event_t evp;

	if (strcmp(command, "kernel_task") == 0) {
		return 0;
	}

	int hashid = thread & HASH_MASK;

	for (evp = event_hash[hashid]; evp; evp = evp->ev_next) {
		if (evp->ev_thread == thread) {
			return 0;
		}
	}
	return 1;
}



static lookup_t
handle_lookup_event(uintptr_t thread, int debugid, kd_buf *kdp)
{
	lookup_t lkp;
	boolean_t first_record = FALSE;

	int hashid = thread & HASH_MASK;

	if (debugid & DBG_FUNC_START) {
		first_record = TRUE;
	}

	for (lkp = lookup_hash[hashid]; lkp; lkp = lkp->lk_next) {
		if (lkp->lk_thread == thread) {
			break;
		}
	}
	if (lkp == NULL) {
		if (first_record == FALSE) {
			return 0;
		}

		if ((lkp = lookup_freelist)) {
			lookup_freelist = lkp->lk_next;
		} else {
			lkp = (lookup_t)malloc(sizeof(struct lookup));
		}

		lkp->lk_thread = thread;

		lkp->lk_next = lookup_hash[hashid];
		lookup_hash[hashid] = lkp;

		add_thread_entry_to_list(&thread_lookup_list, thread);
	}

	if (first_record == TRUE) {
		lkp->lk_pathptr = lkp->lk_pathname;
		lkp->lk_dvp = kdp->arg1;
	} else {
		if (lkp->lk_pathptr > &lkp->lk_pathname[NUMPARMS-4]) {
			return lkp;
		}
		*lkp->lk_pathptr++ = kdp->arg1;
	}
	*lkp->lk_pathptr++ = kdp->arg2;
	*lkp->lk_pathptr++ = kdp->arg3;
	*lkp->lk_pathptr++ = kdp->arg4;
	*lkp->lk_pathptr = 0;

	if (debugid & DBG_FUNC_END) {
		return lkp;
	}

	return 0;
}

static void
delete_lookup_event(uintptr_t thread, lookup_t lkp_to_delete)
{
	lookup_t	lkp;
	lookup_t	lkp_prev;
	int		hashid;

	hashid = thread & HASH_MASK;

	if ((lkp = lookup_hash[hashid])) {
		if (lkp == lkp_to_delete) {
			lookup_hash[hashid] = lkp->lk_next;
		} else {
			lkp_prev = lkp;

			for (lkp = lkp->lk_next; lkp; lkp = lkp->lk_next) {
				if (lkp == lkp_to_delete) {
					lkp_prev->lk_next = lkp->lk_next;
					break;
				}
				lkp_prev = lkp;
			}
		}
		if (lkp) {
			lkp->lk_next = lookup_freelist;
			lookup_freelist = lkp;
		}
	}
}

static void
gc_lookup_events(void) {
	thread_entry_t	te;
	thread_entry_t	te_next;
	lookup_t	lkp;
	lookup_t	lkp_next;
	int		count = 0;
        int		hashid;

	for (te = thread_lookup_list; te; te = te_next) {
		hashid = te->te_thread & HASH_MASK;

		for (lkp = lookup_hash[hashid]; lkp; lkp = lkp_next) {
			lkp_next = lkp->lk_next;
                        lkp->lk_next = lookup_freelist;
                        lookup_freelist = lkp;
			count++;
		}
		lookup_hash[hashid] = 0;

		te_next = te->te_next;
		te->te_next = thread_entry_freelist;
		thread_entry_freelist = te;
	}
	thread_lookup_list = 0;
}

int
sample_sc(void)
{
	kd_buf	*kd, *end_of_sample;
	int	keep_going = 1;
	int	count, i;

	if (!RAW_flag) {
		/*
		 * Get kernel buffer information
		 */
		get_bufinfo(&bufinfo);
	}
	if (need_new_map) {
		delete_all_thread_entries();
	        read_command_map();
		need_new_map = 0;
	}
	if (RAW_flag) {
		uint32_t bytes_read;

		bytes_read = read(RAW_fd, my_buffer, num_entries * sizeof(kd_buf));

		if (bytes_read == -1) {
			perror("read failed");
			exit(2);
		}
		count = bytes_read / sizeof(kd_buf);

		if (count != num_entries) {
			keep_going = 0;
		}

		if (first_read) {
			kd = (kd_buf *)my_buffer;
			first_now = kd->timestamp & KDBG_TIMESTAMP_MASK;
			first_read = 0;
		}
		
	} else {
		int mib[] = { CTL_KERN, KERN_KDEBUG, KERN_KDREADTR };
		size_t needed = bufinfo.nkdbufs * sizeof(kd_buf);

		if (sysctl(mib, ARRAYSIZE(mib), my_buffer, &needed, NULL, 0) < 0) {
			quit("trace facility failure, KERN_KDREADTR\n");
		}

		count = needed;
		sample_generation++;

		if (bufinfo.flags & KDBG_WRAPPED) {
			need_new_map = 1;
			
			if (log_fp) {
				fprintf(log_fp, "\n\n%-19.19s   sample = %d   <<<<<<< trace buffer wrapped >>>>>>>\n\n",
					&(ctime(&curr_time)[0]), sample_generation);
			}
			set_enable(0);
			set_enable(1);
		}
	}
	end_of_sample = &((kd_buf *)my_buffer)[count];

	/*
	 * Always reinitialize the DECR_TRAP array
	 */
	for (i = 0; i < num_cpus; i++) {
	      last_decrementer_kd[i] = (kd_buf *)my_buffer;
	}

	for (kd = (kd_buf *)my_buffer; kd < end_of_sample; kd++) {
		kd_buf *kd_start;
		uintptr_t thread = kd->arg5;
		int	type = kd->debugid & DBG_FUNC_MASK;

		(void)check_for_thread_update(thread, type, kd, NULL);

		uint64_t now = kd->timestamp & KDBG_TIMESTAMP_MASK;
		last_now = now;

		if (type == DECR_TRAP) {
			int cpunum = CPU_NUMBER(kd);
			double i_latency = handle_decrementer(kd, cpunum);

			if (log_fp) {
				if (i_thresh_hold && (int)i_latency > i_thresh_hold) {
					kd_start = last_decrementer_kd[cpunum];

					log_decrementer(kd_start, kd, end_of_sample, i_latency);
				}
				last_decrementer_kd[cpunum] = kd;
			}
		} else {
			double s_latency;
			if (check_for_scheduler_latency(type, &thread, now, kd, &kd_start, &s_latency)) {
				log_scheduler(kd_start, kd, end_of_sample, s_latency, thread);
			}
		}
	}
	if (log_fp) {
		fflush(log_fp);
	}

	gc_thread_entries();
	gc_reset_entries();
	gc_run_events();

	return keep_going;
}



void
enter_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, int print_info)
{
	char	*p;
	double	timestamp;
	double	delta;
	char	pcstring[128];

	int cpunum = CPU_NUMBER(kd);

	if (print_info && fp) {
		timestamp = (double)(now - start_bias) / divisor;
		delta = (double)idelta / divisor;

		if ((p = find_code(type))) {
			if (type == INTERRUPT) {
				int mode;

				if (kd->arg3) {
					mode = USER_MODE;
				} else {
					mode = KERNEL_MODE;
				}

				pc_to_string(&pcstring[0], kd->arg2, 58, mode);

				fprintf(fp, "%9.1f %8.1f\t\tINTERRUPT[%2lx] @ %-58.58s %-8x  %d  %s\n",
					timestamp, delta, kd->arg1, &pcstring[0], thread, cpunum, command);
			} else if (type == MACH_vmfault) {
				fprintf(fp, "%9.1f %8.1f\t\t%-28.28s                                               %-8x  %d  %s\n",
					timestamp, delta, p, thread, cpunum, command);
			} else {
				fprintf(fp, "%9.1f %8.1f\t\t%-28.28s %-8lx   %-8lx   %-8lx  %-8lx      %-8x  %d  %s\n",
					timestamp, delta, p, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
					thread, cpunum, command);
			}
		} else {
			fprintf(fp, "%9.1f %8.1f\t\t%-8x                     %-8lx   %-8lx   %-8lx  %-8lx      %-8x  %d  %s\n",
				timestamp, delta, type, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
				thread, cpunum, command);
	       }
	}
	if (type != BSC_thread_terminate && type != BSC_exit) {
		insert_start_event(thread, type, now);
	}
}


void
exit_syscall(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, int print_info)
{
	char   *p;
	uint64_t user_addr;
	double	timestamp;
	double	delta;
	double  elapsed_timestamp;

	elapsed_timestamp = (double)consume_start_event(thread, type, now) / divisor;

	if (print_info && fp) {
		int cpunum = CPU_NUMBER(kd);

		timestamp = (double)(now - start_bias) / divisor;
		delta = (double)idelta / divisor;

		fprintf(fp, "%9.1f %8.1f(%.1f) \t", timestamp, delta, elapsed_timestamp);

		if ((p = find_code(type))) {
			if (type == INTERRUPT) {
				fprintf(fp, "INTERRUPT                                                                  %-8x  %d  %s\n", thread, cpunum, command);
			} else if (type == MACH_vmfault && kd->arg4 <= DBG_PAGEIND_FAULT) {
				user_addr = ((uint64_t)kd->arg1 << 32) | (uint32_t)kd->arg2;

				fprintf(fp, "%-28.28s %-10.10s   %-16qx                 %-8x  %d  %s\n",
					p, fault_name[kd->arg4], user_addr,
					thread, cpunum, command);
		       } else {
				fprintf(fp, "%-28.28s %-8lx   %-8lx                           %-8x  %d  %s\n",
					p, kd->arg1, kd->arg2,
					thread, cpunum, command);
		       }
		} else {
			fprintf(fp, "%-8x                     %-8lx   %-8lx                           %-8x  %d  %s\n",
				type, kd->arg1, kd->arg2,
				thread, cpunum, command);
		}
	}
}


void
print_entry(FILE *fp, kd_buf *kd, int thread, int type, char *command, uint64_t now, uint64_t idelta, uint64_t start_bias, kd_buf *kd_note)
{
	char	*p;

	if (!fp) {
		return;
	}

	int cpunum = CPU_NUMBER(kd);

	double timestamp = (double)(now - start_bias) / divisor;
	double delta = (double)idelta / divisor;

	if ((p = find_code(type))) {
		if (kd == kd_note) {
			fprintf(fp, "%9.1f %8.1f\t**\t", timestamp, delta);
		} else {
			fprintf(fp, "%9.1f %8.1f\t\t", timestamp, delta);
		}
		fprintf(fp, "%-28.28s %-8lx   %-8lx   %-8lx  %-8lx      %-8x  %d  %s\n",
			p, kd->arg1, kd->arg2, kd->arg3, kd->arg4, thread, cpunum, command);
	} else {
		fprintf(fp, "%9.1f %8.1f\t\t%-8x                     %-8lx   %-8lx   %-8lx  %-8lx   %-8x  %d  %s\n",
			timestamp, delta, type, kd->arg1, kd->arg2, kd->arg3, kd->arg4, 
			thread, cpunum, command);
	}
}


void
check_for_thread_update(uintptr_t thread, int debugid_base, kd_buf *kbufp, char **command)
{
	if (debugid_base == TRACE_DATA_NEWTHREAD) {
		/*
		 * Save the create thread data
		 */
		create_tmp_map_entry(kbufp->arg1, thread);
	} else if (debugid_base == TRACE_STRING_NEWTHREAD) {
		/*
		 * process new map entry
		 */
		find_and_insert_tmp_map_entry(thread, (char *)&kbufp->arg1);
	} else if (debugid_base == TRACE_STRING_EXEC) {
		exec_thread_entry(thread, (char *)&kbufp->arg1);
	} else {
		if (debugid_base == BSC_exit || debugid_base == BSC_thread_terminate) {
			record_thread_entry_for_gc(thread);
		}
		if (command) {
			find_thread_name(thread, command);
		}
	}
}


void
log_info(uint64_t now, uint64_t idelta, uint64_t start_bias, kd_buf *kd, kd_buf *kd_note)
{
	lookup_t	lkp;
	int		mode;
	int		reason;
	char		*p;
	char		*command;
	char		*command1;
	char		command_buf[32];
	char		sched_info[64];
	char		pcstring[128];
	const char *sched_reason;
	double		i_latency;
	double		timestamp;
	double		delta;
	char joe[32];

	int thread  = kd->arg5;
	int cpunum	= CPU_NUMBER(kd);
	int debugid = kd->debugid;
	int type    = kd->debugid & DBG_FUNC_MASK;

	(void)check_for_thread_update(thread, type, kd, &command);

	if ((type >> 24) == DBG_TRACE) {
		if (((type >> 16) & 0xff) != DBG_TRACE_INFO) {
			return;
		}
	}
	timestamp = (double)(now - start_bias) / divisor;
	delta = (double)idelta / divisor;

	switch (type) {

		case CQ_action:
			pc_to_string(&pcstring[0], kd->arg1, 62, KERNEL_MODE);

			fprintf(log_fp, "%9.1f %8.1f\t\tCQ_action @ %-62.62s %-8x  %d  %s\n",
				timestamp, delta, &pcstring[0], thread, cpunum, command);
			break;

		case TES_action:
			pc_to_string(&pcstring[0], kd->arg1, 61, KERNEL_MODE);

			fprintf(log_fp, "%9.1f %8.1f\t\tTES_action @ %-61.61s %-8x  %d  %s\n",
				timestamp, delta, &pcstring[0], thread, cpunum, command);
			break;

		case IES_action:
			pc_to_string(&pcstring[0], kd->arg1, 61, KERNEL_MODE);

			fprintf(log_fp, "%9.1f %8.1f\t\tIES_action @ %-61.61s %-8x  %d  %s\n",
				timestamp, delta, &pcstring[0], thread, cpunum, command);
			break;

		case IES_filter:
			pc_to_string(&pcstring[0], kd->arg1, 61, KERNEL_MODE);

			fprintf(log_fp, "%9.1f %8.1f\t\tIES_filter @ %-61.61s %-8x  %d  %s\n",
				timestamp, delta, &pcstring[0], thread, cpunum, command);
			break;

		case DECR_TRAP:
			if ((int)kd->arg1 >= 0) {
				i_latency = 0;
			} else {
				i_latency = (((double)(-1 - kd->arg1)) / divisor);
			}

			if (i_thresh_hold && (int)i_latency > i_thresh_hold) {
				p = "*";
			} else {
				p = " ";
			}

			if (kd->arg3) {
				mode = USER_MODE;
			} else {
				mode = KERNEL_MODE;
			}

			pc_to_string(&pcstring[0], kd->arg2, 62, mode);

			fprintf(log_fp, "%9.1f %8.1f[%.1f]%s\tDECR_TRAP @ %-62.62s %-8x  %d  %s\n",
					timestamp, delta, i_latency, p, &pcstring[0], thread, cpunum, command);
			break;

		case DECR_SET:
			fprintf(log_fp, "%9.1f %8.1f[%.1f]  \t%-28.28s                                               %-8x  %d  %s\n",
					timestamp, delta, (double)kd->arg1/divisor, "DECR_SET", thread, cpunum, command);
			break;

		case MACH_sched:
		case MACH_stkhandoff:

			find_thread_name(kd->arg2, &command1);
			
			if (command1 == EMPTYSTRING) {
				command1 = command_buf;
				sprintf(command1, "%-8lx", kd->arg2);
			}
			if (thread_in_user_mode(kd->arg2, command1)) {
				p = "U";
			} else {
				p = "K";
			}

			reason = kd->arg1;

			if (reason > MAX_REASON) {
				sched_reason = "?";
			} else {
				sched_reason = sched_reasons[reason];
			}

			if (sched_reason[0] == '?') {
				sprintf(joe, "%x", reason);
				sched_reason = joe;
			}
			sprintf(sched_info, "%14.14s @ pri %3lu  -->  %14.14s @ pri %3lu%s", command, kd->arg3, command1, kd->arg4, p);

			fprintf(log_fp, "%9.1f %8.1f\t\t%-10.10s[%s]  %s    %-8x  %d\n",
				timestamp, delta, "MACH_SCHED", sched_reason, sched_info, thread, cpunum);
			break;

		case VFS_LOOKUP:
			if ((lkp = handle_lookup_event(thread, debugid, kd))) {
				/*
				 * print the tail end of the pathname
				 */
				p = (char *)lkp->lk_pathname;
				int clen = strlen(p);

				if (clen > 45) {
					clen -= 45;
				} else {
					clen = 0;
				}
				
				fprintf(log_fp, "%9.1f %8.1f\t\t%-14.14s %-45s    %-8lx   %-8x  %d  %s\n",
					timestamp, delta, "VFS_LOOKUP", 
					&p[clen], lkp->lk_dvp, thread, cpunum, command);

				delete_lookup_event(thread, lkp);
			}
			break;

		default:
			if (debugid & DBG_FUNC_START) {
				enter_syscall(log_fp, kd, thread, type, command, now, idelta, start_bias, 1);
			} else if (debugid & DBG_FUNC_END) {
				exit_syscall(log_fp, kd, thread, type, command, now, idelta, start_bias, 1);
			} else {
				print_entry(log_fp, kd, thread, type, command, now, idelta, start_bias, kd_note);
			}
			break;
	}
}



void
log_range(kd_buf *kd_buffer, kd_buf *kd_start, kd_buf *kd_stop, kd_buf *kd_note, char *buf1)
{
	uint64_t last_timestamp = 0;
	uint64_t delta = 0;
	uint64_t start_bias = 0;
	uint64_t now;
	kd_buf	*kd;
	int	clen;
	char buf2[128];

	clen = strlen(buf1);
	memset(buf2, '-', clen);
	buf2[clen] = 0;
	fprintf(log_fp, "\n\n%s\n", buf2);
	fprintf(log_fp, "%s\n\n", buf1);

	fprintf(log_fp, "RelTime(Us)  Delta              debugid                      arg1       arg2       arg3      arg4       thread   cpu   command\n\n");

	reset_thread_names();

	last_timestamp = kd_start->timestamp & KDBG_TIMESTAMP_MASK;
	start_bias = last_timestamp;

	for (kd = kd_buffer; kd <= kd_stop; kd++) {
		now = kd->timestamp & KDBG_TIMESTAMP_MASK;

		if (kd >= kd_start) {
			delta = now - last_timestamp;

			log_info(now, delta, start_bias, kd, kd_note);

			last_timestamp = now;
		} else {
			int	debugid = kd->debugid;
			int	thread = kd->arg5;
			int	type = kd->debugid & DBG_FUNC_MASK;

			if ((type >> 24) == DBG_TRACE) {
				if (((type >> 16) & 0xff) != DBG_TRACE_INFO) {
					continue;
				}
			}
			if (type == BSC_thread_terminate || type == BSC_exit) {
				continue;
			}

			if (debugid & DBG_FUNC_START) {
				insert_start_event(thread, type, now);
			} else if (debugid & DBG_FUNC_END) {
				(void)consume_start_event(thread, type, now);
			}
		}
	}
	gc_start_events();
	gc_lookup_events();
}


kd_buf *
log_decrementer(kd_buf *kd_beg, kd_buf *kd_end, kd_buf *end_of_sample, double i_latency)
{
	kd_buf *kd_start, *kd_stop;
	int kd_count; /* Limit the boundary of kd_start */
	uint64_t now;
	double sample_timestamp;
	char buf1[128];

	int thread = kd_beg->arg5;
	int cpunum = CPU_NUMBER(kd_end);

	for (kd_count = 0, kd_start = kd_beg - 1; (kd_start >= (kd_buf *)my_buffer); kd_start--, kd_count++) {
		if (kd_count == MAX_LOG_COUNT) {
		        break;
		}

		if (CPU_NUMBER(kd_start) != cpunum) {
		        continue;
		}
										     
		if ((kd_start->debugid & DBG_FUNC_MASK) == DECR_TRAP) {
			break;
		}

		if (kd_start->arg5 != thread) {
			break;
		}
	}
	if (kd_start < (kd_buf *)my_buffer) {
		kd_start = (kd_buf *)my_buffer;
	}

	thread = kd_end->arg5;

	for (kd_stop = kd_end + 1; kd_stop < end_of_sample; kd_stop++) {
		if (CPU_NUMBER(kd_stop) != cpunum) {
			continue;
		}

		if ((kd_stop->debugid & DBG_FUNC_MASK) == INTERRUPT) {
			break;
		}

		if (kd_stop->arg5 != thread) {
			break;
		}
	}
	if (kd_stop >= end_of_sample) {
		kd_stop = end_of_sample - 1;
	}

	if (RAW_flag) {
		time_t TOD_secs;
		uint64_t TOD_usecs;

		now = kd_start->timestamp & KDBG_TIMESTAMP_MASK;
		sample_timestamp = (double)(now - first_now) / divisor;

		TOD_usecs = (uint64_t)sample_timestamp;
		TOD_secs = sample_TOD_secs + ((sample_TOD_usecs + TOD_usecs) / 1000000);

		sprintf(buf1, "%-19.19s     interrupt latency = %.1fus [timestamp %.1f]", ctime(&TOD_secs), i_latency, sample_timestamp);
	} else {
		sprintf(buf1, "%-19.19s     interrupt latency = %.1fus [sample %d]", &(ctime(&curr_time)[0]), i_latency, sample_generation);
	}

	log_range((kd_buf *)my_buffer, kd_start, kd_stop, 0, buf1);

	return kd_stop;
}


void
log_scheduler(kd_buf *kd_beg, kd_buf *kd_end, kd_buf *end_of_sample, double s_latency, uintptr_t thread)
{
	kd_buf *kd_start, *kd_stop;
	uint64_t now;
	double sample_timestamp;
	char buf1[128];

	int cpunum = CPU_NUMBER(kd_end);

	for (kd_start = kd_beg; (kd_start >= (kd_buf *)my_buffer); kd_start--) {
		if (CPU_NUMBER(kd_start) == cpunum) {
			break;
		}
	}
	if (kd_start < (kd_buf *)my_buffer) {
		kd_start = (kd_buf *)my_buffer;
	}

	for (kd_stop = kd_end + 1; kd_stop < end_of_sample; kd_stop++) {
		if (kd_stop->arg5 == thread) {
			break;
		}
	}
	if (kd_stop >= end_of_sample) {
	        kd_stop = end_of_sample - 1;
	}

	if (RAW_flag) {
		time_t	TOD_secs;
		uint64_t TOD_usecs;

		now = kd_start->timestamp & KDBG_TIMESTAMP_MASK;
		sample_timestamp = (double)(now - first_now) / divisor;

		TOD_usecs = (uint64_t)sample_timestamp;
		TOD_secs = sample_TOD_secs + ((sample_TOD_usecs + TOD_usecs) / 1000000);

		sprintf(buf1, "%-19.19s     priority = %d,  scheduling latency = %.1fus [timestamp %.1f]", ctime(&TOD_secs), watch_priority, s_latency, sample_timestamp);
	} else {
		sprintf(buf1, "%-19.19s     priority = %d,  scheduling latency = %.1fus [sample %d]", &(ctime(&curr_time)[0]), watch_priority, s_latency, sample_generation);
	}

	log_range((kd_buf *)my_buffer, kd_start, kd_stop, kd_beg, buf1);
}



int
check_for_scheduler_latency(int type, uintptr_t *thread, uint64_t now, kd_buf *kd, kd_buf **kd_start, double *latency)
{
	int found_latency = 0;

	if (type == MACH_makerunnable) {
		if (watch_priority == kd->arg2) {
			insert_run_event(kd->arg1, kd, now);
		}
	} else if (type == MACH_sched || type == MACH_stkhandoff) {
		threadrun_t	trp;

		if (type == MACH_sched || type == MACH_stkhandoff) {
			*thread = kd->arg2;
		}

		if ((trp = find_run_event(*thread))) {
			double d_s_latency = (((double)(now - trp->tr_timestamp)) / divisor);
			int s_latency = (int)d_s_latency;

			if (s_latency) {
				if (s_latency < 100) {
					s_usec_10_bins[s_latency/10]++;
				}
				if (s_latency < 1000) {
					s_usec_100_bins[s_latency/100]++;
				} else if (s_latency < 10000) {
					s_msec_1_bins[s_latency/1000]++;
				} else if (s_latency < 50000) {
					s_msec_10_bins[s_latency/10000]++;
				} else {
					s_too_slow++;
				}

				if (s_latency > s_max_latency) {
					s_max_latency = s_latency;
				}
				if (s_latency < s_min_latency || s_total_samples == 0) {
					s_min_latency = s_latency;
				}
				s_total_latency += s_latency;
				s_total_samples++;

				if (s_thresh_hold && s_latency > s_thresh_hold) {
					s_exceeded_threshold++;
					
					if (log_fp) {
						*kd_start = trp->tr_entry;
						*latency = d_s_latency;
						found_latency = 1;
					}
				}
			}
			delete_run_event(*thread);
		}
	}
	return found_latency;
}


double
handle_decrementer(kd_buf *kd, int cpunum)
{
	struct i_latencies *il;
	double latency;
	long   elapsed_usecs;

	if (i_latency_per_cpu == FALSE) {
		cpunum = 0;
	}

	il = &i_lat[cpunum];

	if ((long)(kd->arg1) >= 0) {
		latency = 1;
	} else {
		latency = (((double)(-1 - kd->arg1)) / divisor);
	}
	elapsed_usecs = (long)latency;

	if (elapsed_usecs < 100) {
		il->i_usec_10_bins[elapsed_usecs/10]++;
	}
	
	if (elapsed_usecs < 1000) {
		il->i_usec_100_bins[elapsed_usecs/100]++;
	} else if (elapsed_usecs < 10000) {
		il->i_msec_1_bins[elapsed_usecs/1000]++;
	} else if (elapsed_usecs < 50000) {
		il->i_msec_10_bins[elapsed_usecs/10000]++;
	} else {
		il->i_too_slow++;
	}

	if (use_high_res_bins && elapsed_usecs < N_HIGH_RES_BINS) {
		i_high_res_bins[elapsed_usecs]++;
	}
	if (i_thresh_hold && elapsed_usecs > i_thresh_hold) {
	        il->i_exceeded_threshold++;
	}
	if (elapsed_usecs > il->i_max_latency) {
	        il->i_max_latency = elapsed_usecs;
	}
	if (elapsed_usecs < il->i_min_latency || il->i_total_samples == 0) {
	        il->i_min_latency = elapsed_usecs;
	}
	il->i_total_latency += elapsed_usecs;
	il->i_total_samples++;

	return latency;
}



char *
find_code(int type)
{
	int i;
	for (i = 0; i < num_of_codes; i++) {
		if (codes_tab[i].type == type) {
			return codes_tab[i].name;
		}
	}
	return NULL;
}


void
init_code_file(void)
{
	FILE *fp;
	int i;

	if ((fp = fopen(code_file, "r")) == NULL) {
		if (log_fp) {
			fprintf(log_fp, "open of %s failed\n", code_file);
		}
		return;
	}
	for (i = 0; i < MAX_ENTRIES; i++) {
		int code;
		char name[128];
		int n = fscanf(fp, "%x%127s\n", &code, name);

		if (n == 1 && i == 0) {
			/*
			 * old code file format, just skip
			 */
			continue;
		}
		if (n != 2) {
			break;
		}

		strncpy(codes_tab[i].name, name, 32);
		codes_tab[i].type = code;
	}
	num_of_codes = i;

	fclose(fp);
}


void
do_kernel_nm(void)
{
	int i, len;
	FILE *fp = NULL;
	char tmp_nm_file[128];
	char tmpstr[1024];
	char inchr;

	bzero(tmp_nm_file, 128);
	bzero(tmpstr, 1024);

	/*
	 * Build the temporary nm file path
	 */
	strcpy(tmp_nm_file,"/tmp/knm.out.XXXXXX");

	if (!mktemp(tmp_nm_file)) {
		fprintf(stderr, "Error in mktemp call\n");
		return;
	}

	/*
	 * Build the nm command and create a tmp file with the output
	 */
	sprintf (tmpstr, "/usr/bin/nm -f -n -s __TEXT __text %s > %s",
		 kernelpath, tmp_nm_file);
	system(tmpstr);
  
	/*
	 * Parse the output from the nm command
	 */
	if ((fp = fopen(tmp_nm_file, "r")) == NULL) {
		/* Hmmm, let's not treat this as fatal */
		fprintf(stderr, "Failed to open nm symbol file [%s]\n", tmp_nm_file);
		return;
	}
	/*
	 * Count the number of symbols in the nm symbol table
	 */
	kern_sym_count = 0;

	while ((inchr = getc(fp)) != -1) {
		if (inchr == '\n') {
			kern_sym_count++;
		}
	}
	rewind(fp);

	/*
	 * Malloc the space for symbol table
	 */
	if (kern_sym_count > 0) {
		kern_sym_tbl = malloc(kern_sym_count * sizeof(kern_sym_t));

		if (!kern_sym_tbl) {
			/*
			 * Hmmm, lets not treat this as fatal
			 */
			fprintf(stderr, "Can't allocate memory for kernel symbol table\n");
		} else {
			bzero(kern_sym_tbl, kern_sym_count * sizeof(kern_sym_t));
		}
	} else {
		/*
		 * Hmmm, lets not treat this as fatal
		 */
		fprintf(stderr, "No kernel symbol table \n");
	}
	for (i = 0; i < kern_sym_count; i++) {
		bzero(tmpstr, 1024);

		if (fscanf(fp, "%p %c %s", &kern_sym_tbl[i].k_sym_addr, &inchr, tmpstr) != 3) {
			break;
		} else {
			len = strlen(tmpstr);
			kern_sym_tbl[i].k_sym_name = malloc(len + 1);

			if (kern_sym_tbl[i].k_sym_name == NULL) {
				fprintf(stderr, "Can't allocate memory for symbol name [%s]\n", tmpstr);
				kern_sym_tbl[i].k_sym_name = NULL;
				len = 0;
			} else {
				strcpy(kern_sym_tbl[i].k_sym_name, tmpstr);
			}

			kern_sym_tbl[i].k_sym_len = len;
		}
	}
	if (i != kern_sym_count) {
		/*
		 * Hmmm, didn't build up entire table from nm
		 * scrap the entire thing
		 */
		free(kern_sym_tbl);
		kern_sym_tbl = NULL;
		kern_sym_count = 0;
	}
	fclose(fp);

	/*
	 * Remove the temporary nm file
	 */
	unlink(tmp_nm_file);
#if 0
	/*
	 * Dump the kernel symbol table
	 */
	for (i = 0; i < kern_sym_count; i++) {
		if (kern_sym_tbl[i].k_sym_name) {
			printf ("[%d] %-16p    %s\n", i, 
				kern_sym_tbl[i].k_sym_addr, kern_sym_tbl[i].k_sym_name);
		} else {
			printf ("[%d] %-16p    %s\n", i, 
				kern_sym_tbl[i].k_sym_addr, "No symbol name");
		}
	}
#endif
}

void
pc_to_string(char *pcstring, uintptr_t pc, int max_len, int mode)
{
	int ret;
	int len;

	if (mode == USER_MODE) {
		sprintf(pcstring, "%-16lx [usermode addr]", pc);
		return;
	}
	ret = binary_search(kern_sym_tbl, 0, kern_sym_count-1, pc);

	if (ret == -1 || kern_sym_tbl[ret].k_sym_name == NULL) {
		sprintf(pcstring, "%-16lx", pc);
		return;
	}
	if ((len = kern_sym_tbl[ret].k_sym_len) > (max_len - 8)) {
		len = max_len - 8;
	}

	memcpy(pcstring, kern_sym_tbl[ret].k_sym_name, len);

	sprintf(&pcstring[len], "+0x%-5lx", pc - (uintptr_t)kern_sym_tbl[ret].k_sym_addr);
}


/*
 * Return -1 if not found, else return index
 */
int
binary_search(kern_sym_t *list, int low, int high, uintptr_t addr)
{
	int mid;
  
	if (kern_sym_count == 0) {
		return -1;
	}

	if (low > high) {
		return -1;   /* failed */
	}

	if (low + 1 == high) { 
		if ((uintptr_t)list[low].k_sym_addr <= addr && addr < (uintptr_t)list[high].k_sym_addr) {
			/*
			 * We have a range match
			 */
			return low;
		}
		if ((uintptr_t)list[high].k_sym_addr <= addr) {
			return high;
		}
		/*
		 * Failed
		 */
		return -1;
	}      
	mid = (low + high) / 2;

	if (addr < (uintptr_t)list[mid].k_sym_addr) {
		return binary_search(list, low, mid, addr);
	}

	return binary_search(list, mid, high, addr);
}


void
open_logfile(const char *path)
{
	log_fp = fopen(path, "a");

	if (!log_fp) {
		/*
		 * failed to open path
		 */
		fprintf(stderr, "latency: failed to open logfile [%s]\n", path);
		exit_usage();
	}
}


void
open_rawfile(const char *path)
{
	RAW_fd = open(path, O_RDONLY);

	if (RAW_fd == -1) {
		/*
		 * failed to open path
		 */
		fprintf(stderr, "latency: failed to open RAWfile [%s]\n", path);
		exit_usage();
	}
}


void
getdivisor(void)
{
	mach_timebase_info_data_t info;
	
	(void)mach_timebase_info(&info);

	divisor = ((double)info.denom / (double)info.numer) * 1000;
}
