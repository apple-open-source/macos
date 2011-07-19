/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *	zprint.c
 *
 *	utility for printing out zone structures
 *
 *	With no arguments, prints information on all zone structures.
 *	With an argument, prints information only on those zones for
 *	which the given name is a substring of the zone's name.
 *	With a "-w" flag, calculates how much much space is allocated
 *	to zones but not currently in use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <mach_debug/mach_debug.h>
#include <mach/mach_error.h>
#include <libutil.h>
#include <errno.h>

#define streql(a, b)		(strcmp((a), (b)) == 0)
#define strneql(a, b, n)	(strncmp((a), (b), (n)) == 0)

static void usage(void);
static void printzone(mach_zone_name_t *, task_zone_info_t *);
static void colprintzone(mach_zone_name_t *, task_zone_info_t *);
static int  find_deltas(mach_zone_name_t *, task_zone_info_t *, task_zone_info_t *, char *, int, int);
static void colprintzoneheader(void);
static void printk(const char *, int);
static boolean_t substr(const char *a, int alen, const char *b, int blen);

static char *program;

static pid_t pid = 0;
static task_t task = TASK_NULL;
static boolean_t ShowPid = FALSE;

static boolean_t ShowDeltas = FALSE;
static boolean_t ShowWasted = FALSE;
static boolean_t ShowTotal = FALSE;
static boolean_t SortZones = FALSE;
static boolean_t ColFormat = TRUE;
static boolean_t PrintHeader = TRUE;

static unsigned long long totalsize = 0;
static unsigned long long totalused = 0;
static unsigned long long totalsum = 0;
static unsigned long long pidsum = 0;

static int last_time = 0;

static	char	*zname = NULL;
static	int	znamelen = 0;

static void
sigintr(__unused int signum)
{
	last_time = 1;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-w] [-s] [-c] [-h] [-t] [-d] [-p <pid>] [name]\n", program);
	exit(1);
}

int
main(int argc, char **argv)
{
	mach_zone_name_t *name = NULL;
	unsigned int nameCnt = 0;
	task_zone_info_t *info = NULL;
	unsigned int infoCnt = 0;

	task_zone_info_t *max_info = NULL;
	char		*deltas = NULL;

	kern_return_t	kr;
	int		i, j;
	int		first_time = 1;
	int             must_print = 1;
	int		interval = 1;

	signal(SIGINT, sigintr);

	program = strrchr(argv[0], '/');
	if (program == NULL)
		program = argv[0];
	else
		program++;

	for (i = 1; i < argc; i++) {
		if (streql(argv[i], "-d"))
			ShowDeltas = TRUE;
		else if (streql(argv[i], "-t"))
			ShowTotal = TRUE;
		else if (streql(argv[i], "-T"))
			ShowTotal = FALSE;
		else if (streql(argv[i], "-w"))
			ShowWasted = TRUE;
		else if (streql(argv[i], "-W"))
			ShowWasted = FALSE;
		else if (streql(argv[i], "-s"))
			SortZones = TRUE;
		else if (streql(argv[i], "-S"))
			SortZones = FALSE;
		else if (streql(argv[i], "-c"))
			ColFormat = TRUE;
		else if (streql(argv[i], "-C"))
			ColFormat = FALSE;
		else if (streql(argv[i], "-H"))
			PrintHeader = FALSE;
		else if (streql(argv[i], "-p")) {
			ShowPid = TRUE;
			if (i < argc - 1) {
				pid = atoi(argv[i+1]);
				i++;
			} else
				usage();
		} else if (streql(argv[i], "--")) {
			i++;
			break;
		} else if (argv[i][0] == '-')
			usage();
		else
			break;
	}

	switch (argc - i) {
	    case 0:
		zname = "";
		znamelen = 0;
		break;

	    case 1:
		zname = argv[i];
		znamelen = strlen(zname);
		break;

	    default:
		usage();
	}

	if (ShowDeltas) {
		SortZones = FALSE;
		ColFormat = TRUE;
		PrintHeader = TRUE;
	}

	if (ShowPid) {
		kr = task_for_pid(mach_task_self(), pid, &task);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: task_for_pid(%d) failed: %s (try running as root)\n", 
				program, pid, mach_error_string(kr));
			exit(1);
		}
	}

    for (;;) {
        if (ShowPid) {
	    kr = task_zone_info(task, &name, &nameCnt, &info, &infoCnt);
	    if (kr != KERN_SUCCESS) {
		fprintf(stderr, "%s: task_zone_info: %s\n",
			program, mach_error_string(kr));
		exit(1);
	    }
	} else {
	    mach_zone_info_t *zinfo = NULL;

	    kr = mach_zone_info(mach_host_self(),
				&name, &nameCnt, &zinfo, &infoCnt);
	    if (kr != KERN_SUCCESS) {
	        fprintf(stderr, "%s: mach_zone_info: %s\n",
			program, mach_error_string(kr));
		exit(1);
	    }
	    kr = vm_allocate(mach_task_self(), (vm_address_t *)&info,
			     infoCnt * sizeof *info, VM_FLAGS_ANYWHERE);
	    if (kr != KERN_SUCCESS) {
		    fprintf(stderr, "%s vm_allocate: %s\n",
			    program, mach_error_string(kr));
		    exit(1);
	    }
	    for (i = 0; i < infoCnt; i++) {
		    *(mach_zone_info_t *)(info + i) = zinfo[i];
		    info[i].tzi_caller_acct = 0;
		    info[i].tzi_task_alloc = 0;
		    info[i].tzi_task_free = 0;
	    }
	    kr = vm_deallocate(mach_task_self(), (vm_address_t) zinfo,
			       (vm_size_t) (infoCnt * sizeof *zinfo));
	    if (kr != KERN_SUCCESS) {
		    fprintf(stderr, "%s: vm_deallocate: %s\n",
			    program, mach_error_string(kr));
		    exit(1);
	    }
	}

	if (nameCnt != infoCnt) {
		fprintf(stderr, "%s: mach/task_zone_info: counts not equal?\n",
			program);
		exit(1);
	}

	if (first_time) {
		deltas = (char *)malloc(infoCnt);
		max_info = (task_zone_info_t *)malloc((infoCnt * sizeof *info));
	}

	if (SortZones) {
		for (i = 0; i < nameCnt-1; i++)
			for (j = i+1; j < nameCnt; j++) {
				int wastei, wastej;

				wastei = (info[i].tzi_cur_size -
					  (info[i].tzi_elem_size *
					   info[i].tzi_count));
				wastej = (info[j].tzi_cur_size -
					  (info[j].tzi_elem_size *
					   info[j].tzi_count));

				if (wastej > wastei) {
					task_zone_info_t tinfo;
					mach_zone_name_t tname;

					tinfo = info[i];
					info[i] = info[j];
					info[j] = tinfo;

					tname = name[i];
					name[i] = name[j];
					name[j] = tname;
				}
			}
	}

	must_print = find_deltas(name, info, max_info, deltas, infoCnt, first_time);
	if (must_print) {
		if (ColFormat) {
			if (!first_time)
				printf("\n");
			colprintzoneheader();
		}
		for (i = 0; i < nameCnt; i++) {
			if (deltas[i]) {
				if (ColFormat)
					colprintzone(&name[i], &info[i]);
				else
					printzone(&name[i], &info[i]);
			}
		}
	}

	first_time = 0;

	if ((name != NULL) && (nameCnt != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) name,
				   (vm_size_t) (nameCnt * sizeof *name));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: vm_deallocate: %s\n",
			     program, mach_error_string(kr));
			exit(1);
		}
	}

	if ((info != NULL) && (infoCnt != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) info,
				   (vm_size_t) (infoCnt * sizeof *info));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: vm_deallocate: %s\n",
			     program, mach_error_string(kr));
			exit(1);
		}
	}

	if ((ShowWasted||ShowTotal) && PrintHeader && !ShowDeltas) {
		printf("TOTAL SIZE   = %llu\n", totalsize);
		printf("TOTAL USED   = %llu\n", totalused);
		if (ShowWasted)
			printf("TOTAL WASTED = %llu\n", totalsize - totalused);
		if (ShowTotal)
			printf("TOTAL ALLOCS = %llu\n", totalsum);
	}

	if (ShowDeltas == FALSE || last_time)
	        break;

	sleep(interval);
    }
    exit(0);
}

static boolean_t
substr(const char *a, int alen, const char *b, int blen)
{
	int i;

	for (i = 0; i <= blen - alen; i++)
		if (strneql(a, b+i, alen))
			return TRUE;

	return FALSE;
}

static void
printzone(mach_zone_name_t *name, task_zone_info_t *info)
{
	unsigned long long used, size;

	printf("%.*s zone:\n", (int)sizeof name->mzn_name, name->mzn_name);
	printf("\tcur_size:    %lluK bytes (%llu elements)\n",
	       info->tzi_cur_size/1024,
	       (info->tzi_elem_size == 0) ? 0 :
	       info->tzi_cur_size/info->tzi_elem_size);
	printf("\tmax_size:    %lluK bytes (%llu elements)\n",
	       info->tzi_max_size/1024,
	       (info->tzi_elem_size == 0) ? 0 :
	       info->tzi_max_size/info->tzi_elem_size);
	printf("\telem_size:   %llu bytes\n",
	       info->tzi_elem_size);
	printf("\t# of elems:  %llu\n",
	       info->tzi_count);
	printf("\talloc_size:  %lluK bytes (%llu elements)\n",
	       info->tzi_alloc_size/1024,
	       (info->tzi_elem_size == 0) ? 0 :
	       info->tzi_alloc_size/info->tzi_elem_size);
	if (info->tzi_exhaustible)
		printf("\tEXHAUSTIBLE\n");
	if (info->tzi_collectable)
		printf("\tCOLLECTABLE\n");
	if (ShowPid && info->tzi_caller_acct)
		printf("\tCALLER ACCOUNTED\n");
	if (ShowPid) {
		pidsum += info->tzi_task_alloc - info->tzi_task_free;
		printf("\tproc_alloc_size: %8dK bytes (%llu elements)\n",
		       (int)((info->tzi_task_alloc - info->tzi_task_free)/1024),
		       (info->tzi_elem_size == 0) ? 0 :
		       (info->tzi_task_alloc - info->tzi_task_free)/info->tzi_elem_size);
	}
	if (ShowWasted) {
		totalused += used = info->tzi_elem_size * info->tzi_count;
		totalsize += size = info->tzi_cur_size;
		printf("\t\t\t\t\tWASTED: %llu\n", size - used);
	}
	if (ShowTotal) {
		totalsum += info->tzi_sum_size;
		printf("\t\t\t\t\tTOTAL: %llu\n", totalsum);
		if (ShowPid)
			printf("\t\t\t\t\tPID TOTAL: %llu\n", pidsum);
	}
}

static void
printk(const char *fmt, int i)
{
	printf(fmt, i / 1024);
	putchar('K');
}

static void
colprintzone(mach_zone_name_t *zone_name, task_zone_info_t *info)
{
	char *name = zone_name->mzn_name;
	int j, namewidth;
	unsigned long long used, size;

	namewidth = 25;
	if (ShowWasted || ShowTotal) {
		namewidth -= 7;
	}
	for (j = 0; j < namewidth - 1 && name[j]; j++) {
		if (name[j] == ' ') {
			putchar('.');
		} else {
			putchar(name[j]);
		}
	}
	if (j == namewidth - 1) {
		if (name[j]) {
			putchar('$');
		} else {
			putchar(' ');
		}
	} else {
		for (; j < namewidth; j++) {
			putchar(' ');
		}
	}
	printf("%5llu", info->tzi_elem_size);
	printk("%8llu", info->tzi_cur_size);
	if (info->tzi_max_size / 1024 > 9999999) {
		printf("   ------");
	} else {
		printk("%8llu", info->tzi_max_size);
	}
	printf("%10llu", info->tzi_cur_size / info->tzi_elem_size);
	if (info->tzi_max_size / 1024 >= 999999999) {
		printf(" ---------");
	} else {
		printf("%10llu", info->tzi_max_size / info->tzi_elem_size);
	}
	printf("%10llu", info->tzi_count);
	printk("%5llu", info->tzi_alloc_size);
	printf("%6llu", info->tzi_alloc_size / info->tzi_elem_size);

	totalused += used = info->tzi_elem_size * info->tzi_count;
	totalsize += size = info->tzi_cur_size;
	totalsum += info->tzi_sum_size;

	printf(" %c%c%c",
	       (info->tzi_exhaustible ? 'X' : ' '),
	       (info->tzi_caller_acct ? 'A' : ' '),
	       (info->tzi_collectable ? 'C' : ' '));
	if (ShowWasted) {
		printk("%8llu", size - used);
	}
	if (ShowPid) {
		printf("%8dK", (int)((info->tzi_task_alloc - info->tzi_task_free)/1024));
	}
	if (ShowTotal) {
		if (info->tzi_sum_size < 1024)
			printf("%16lluB", info->tzi_sum_size);
		else
			printf("%16lluK", info->tzi_sum_size/1024);
	}
	printf("\n");
}

static void
colprintzoneheader(void)
{
	if (! PrintHeader) {
		return;
	}
	printf("%s                   elem      cur      max       cur       max"
	       "       cur alloc alloc          %s\n", 
	       (ShowWasted||ShowTotal)? "" : "       ", (ShowPid) ? "PID" : "" );
	printf("zone name%s          size     size     size     #elts     #elts"
	       "     inuse  size count   ", (ShowWasted||ShowTotal)? "" : "       " );
	if (ShowWasted)
		printf("   wasted");
	if (ShowPid)
		printf("    Allocs");
	if (ShowTotal)
		printf("     Total Allocs");
	printf("\n%s------------------------------------------"
	       "--------------------------------------------",
	       (ShowWasted||ShowTotal)? "" : "-------");
	if (ShowWasted)
		printf("---------");
	if (ShowPid)
		printf("----------");
	if (ShowTotal)
		printf("-----------------");
	printf("\n");
}

int
find_deltas(mach_zone_name_t *name, task_zone_info_t *info, task_zone_info_t *max_info,
	    char *deltas, int cnt, int first_time)
{
       int i;
       int  found_one = 0;

       for (i = 0; i < cnt; i++) {
	       deltas[i] = 0;
	       if (substr(zname, znamelen, name[i].mzn_name,
			  strnlen(name[i].mzn_name, sizeof name[i].mzn_name))) {
		       if (first_time || info->tzi_cur_size > max_info->tzi_cur_size ||
			   (ShowTotal && ((info->tzi_sum_size >> 1) > max_info->tzi_sum_size))) {
			       max_info->tzi_cur_size = info->tzi_cur_size;
			       max_info->tzi_sum_size = info->tzi_sum_size;
			       deltas[i] = 1;
			       found_one = 1;
		       }
	       }
	       info++;
	       max_info++;
       }
       return(found_one);
}
