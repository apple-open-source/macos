/*
 * Copyright (c) 1999-2009 Apple Computer, Inc. All rights reserved.
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
 *	File:	vm_stat.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr.
 *
 *
 *	Display Mach VM statistics.
 *
 ************************************************************************
 * HISTORY
 *  6-Jun-86  Avadis Tevanian, Jr. (avie) at Carnegie-Mellon University
 *	Use official Mach interface.
 *
 *  25-mar-99	A.Ramesh at Apple
 *		Ported to MacOS X
 *  
 *  22-Jan-09	R.Branche at Apple
 *  		Changed some fields to 64-bit to alleviate overflows
 ************************************************************************
 */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <mach/mach.h>

vm_statistics64_data_t	vm_stat, last;
natural_t percent;
int	delay;
char	*pgmname;
mach_port_t myHost;
vm_size_t pageSize = 4096; 	/* set to 4k default */

void usage(void);
void snapshot(void);
void sspstat(char *str, uint64_t n);
void banner(void);
void print_stats(void);
void get_stats(vm_statistics64_t stat);

void pstat(uint64_t n, int width);

int
main(int argc, char *argv[])
{

	pgmname = argv[0];
	delay = 0;


	setlinebuf (stdout);

	if (argc == 2) {
		if (sscanf(argv[1], "%d", &delay) != 1)
			usage();
		if (delay < 0)
			usage();
	}

	myHost = mach_host_self();

	if(host_page_size(mach_host_self(), &pageSize) != KERN_SUCCESS) {
		fprintf(stderr, "%s: failed to get pagesize; defaulting to 4K.\n", pgmname);
		pageSize = 4096;
	}	

	if (delay == 0) {
		snapshot();
	}
	else {
		while (1) {
			print_stats();
			sleep(delay);
		}
	}
	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [ repeat-interval ]\n", pgmname);
	exit(EXIT_FAILURE);
}

void
snapshot(void)
{

	get_stats(&vm_stat);
	printf("Mach Virtual Memory Statistics: (page size of %d bytes)\n",
				(int) (pageSize));

	sspstat("Pages free:", (uint64_t) (vm_stat.free_count - vm_stat.speculative_count));
	sspstat("Pages active:", (uint64_t) (vm_stat.active_count));
	sspstat("Pages inactive:", (uint64_t) (vm_stat.inactive_count));
	sspstat("Pages speculative:", (uint64_t) (vm_stat.speculative_count));
	sspstat("Pages wired down:", (uint64_t) (vm_stat.wire_count));
	sspstat("\"Translation faults\":", (uint64_t) (vm_stat.faults));
	sspstat("Pages copy-on-write:", (uint64_t) (vm_stat.cow_faults));
	sspstat("Pages zero filled:", (uint64_t) (vm_stat.zero_fill_count));
	sspstat("Pages reactivated:", (uint64_t) (vm_stat.reactivations));
	sspstat("Pageins:", (uint64_t) (vm_stat.pageins));
	sspstat("Pageouts:", (uint64_t) (vm_stat.pageouts));
#if defined(__ppc__) /* vm_statistics are still 32-bit on ppc */
	printf("Object cache: %u hits of %u lookups (%u%% hit rate)\n",
#else
	printf("Object cache: %llu hits of %llu lookups (%u%% hit rate)\n",
#endif
		vm_stat.hits, vm_stat.lookups, percent);

}

void
sspstat(char *str, uint64_t n)
{
	printf("%-25s %16llu.\n", str, n);
}

void
banner(void)
{
	get_stats(&vm_stat);
	printf("Mach Virtual Memory Statistics: ");
	printf("(page size of %d bytes, cache hits %u%%)\n",
				(int) (pageSize), percent);
	printf("%6s %6s %6s %8s %6s %8s %8s %8s %8s %8s %8s\n",
		"free",
		"active",
		"spec",
		"inactive",
		"wire",
		"faults",
		"copy",
		"0fill",
		"reactive",
		"pageins",
		"pageout");
	bzero(&last, sizeof(last));
}

void
print_stats(void)
{
	static int count = 0;

	if (count++ == 0)
		banner();

	if (count > 20)
		count = 0;

	get_stats(&vm_stat);
	pstat((uint64_t) (vm_stat.free_count - vm_stat.speculative_count), 6);
	pstat((uint64_t) (vm_stat.active_count), 6);
	pstat((uint64_t) (vm_stat.speculative_count), 6);
	pstat((uint64_t) (vm_stat.inactive_count), 8);
	pstat((uint64_t) (vm_stat.wire_count), 6);
	pstat((uint64_t) (vm_stat.faults - last.faults), 8);
	pstat((uint64_t) (vm_stat.cow_faults - last.cow_faults), 8);
	pstat((uint64_t) (vm_stat.zero_fill_count - last.zero_fill_count), 8);
	pstat((uint64_t) (vm_stat.reactivations - last.reactivations), 8);
	pstat((uint64_t) (vm_stat.pageins - last.pageins), 8);
	pstat((uint64_t) (vm_stat.pageouts - last.pageouts), 8);
	putchar('\n');
	last = vm_stat;
}

void
pstat(uint64_t n, int width)
{
	char buf[80];
	if (width >= sizeof(buf)) {
		width = sizeof(buf) -1;
	}

	unsigned long long nb = n * (unsigned long long)pageSize;

	/* Now that we have the speculative field, there is really not enough
	 space, but we were actually overflowing three or four fields before
	 anyway.  So any field that overflows we drop some insignifigant
	 digets and slap on the appropriate suffix
	*/
	int w = snprintf(buf, sizeof(buf), "%*llu", width, n);
	if (w > width) {
		w = snprintf(buf, sizeof(buf), "%*lluK", width -1, n / 1000);
		if (w > width) {
			w = snprintf(buf, sizeof(buf), "%*lluM", width -1, n / 1000000);
			if (w > width) {
				w = snprintf(buf, sizeof(buf), "%*lluG", width -1, n / 1000000000);
			}
		}
	}
	fputs(buf, stdout);
	putchar(' ');
}

void
get_stats(vm_statistics64_t stat)
{
	unsigned int count = HOST_VM_INFO64_COUNT;
	if (host_statistics64(myHost, HOST_VM_INFO64, (host_info64_t)stat, &count) != KERN_SUCCESS) {
		fprintf(stderr, "%s: failed to get statistics.\n", pgmname);
		exit(EXIT_FAILURE);
	}
	if (stat->lookups == 0)
		percent = 0;
	else {
		/*
		 * We have limited precision with the 32-bit natural_t fields
		 * in the vm_statistics structure.  There's nothing we can do
		 * about counter overflows, but we can avoid percentage
		 * calculation overflows by doing the computation in floating
		 * point arithmetic ...
		 */
		percent = (natural_t)(((double)stat->hits*100)/stat->lookups);
	}
}
