/*
 * Copyright (c) 1999-2016 Apple Inc. All rights reserved.
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
 *		Changed some fields to 64-bit to alleviate overflows
 ************************************************************************
 */

#include <err.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/vm_page_size.h>
#include <mach/vm_statistics.h>
#include <sys/errno.h>
#include <sys/sysctl.h>

vm_statistics64_data_t	vm_stat, last;
bool has_mte = false;
char	*pgmname;
mach_port_t myHost;

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
	double delay = 0.0;
	int count = 0;

	pgmname = argv[0];

	setlinebuf (stdout);

	int c;
	while ((c = getopt (argc, argv, "c:")) != -1) {
		switch (c) {
			case 'c':
				count = (int)strtol(optarg, NULL, 10);
				if (count < 1) {
					warnx("count must be positive");
					usage();
				}
				break;
			default:
				usage();
				break;
		}
	}

	argc -= optind; argv += optind;

	if (argc == 1) {
		delay = strtod(argv[0], NULL);
		if (delay < 0.0)
			usage();
	} else if (argc > 1) {
		usage();
	}

	myHost = mach_host_self();

	boolean_t feat_mte2;
	size_t feat_mte2_size = sizeof(feat_mte2);
	int ret = sysctlbyname("hw.optional.arm.FEAT_MTE2", &feat_mte2, &feat_mte2_size, NULL, 0);
	if (!ret && feat_mte2 == TRUE) {
		has_mte = true;
	}

	if (delay == 0.0) {
		snapshot();
	} else {
		print_stats();
		for (int i = 1; i < count || count == 0; i++ ){
			usleep((int)(delay * USEC_PER_SEC));
			print_stats();
		}
	}
	exit(EXIT_SUCCESS);
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [[-c count] interval]\n", pgmname);
	exit(EXIT_FAILURE);
}

void
snapshot(void)
{
	get_stats(&vm_stat);
	printf("Mach Virtual Memory Statistics: (page size of %llu bytes)\n", (mach_vm_size_t)vm_kernel_page_size);

	sspstat("Pages free:", (uint64_t) (vm_stat.free_count - vm_stat.speculative_count));
	sspstat("Pages active:", (uint64_t) (vm_stat.active_count));
	sspstat("Pages inactive:", (uint64_t) (vm_stat.inactive_count));
	sspstat("Pages speculative:", (uint64_t) (vm_stat.speculative_count));
	sspstat("Pages throttled:", (uint64_t) (vm_stat.throttled_count));
	sspstat("Pages wired down:", (uint64_t) (vm_stat.wire_count));
	sspstat("Pages purgeable:", (uint64_t) (vm_stat.purgeable_count));
	sspstat("\"Translation faults\":", (uint64_t) (vm_stat.faults));
	sspstat("Pages copy-on-write:", (uint64_t) (vm_stat.cow_faults));
	sspstat("Pages zero filled:", (uint64_t) (vm_stat.zero_fill_count));
	sspstat("Pages reactivated:", (uint64_t) (vm_stat.reactivations));
	sspstat("Pages purged:", (uint64_t) (vm_stat.purges));
	sspstat("File-backed pages:", (uint64_t) (vm_stat.external_page_count));
	sspstat("Anonymous pages:", (uint64_t) (vm_stat.internal_page_count));
	sspstat("Pages stored in compressor:", (uint64_t) (vm_stat.total_uncompressed_pages_in_compressor));
	sspstat("Pages occupied by compressor:", (uint64_t) (vm_stat.compressor_page_count));
	sspstat("Decompressions:", (uint64_t) (vm_stat.decompressions));
	sspstat("Compressions:", (uint64_t) (vm_stat.compressions));
	sspstat("Pageins:", (uint64_t) (vm_stat.pageins));
	sspstat("Pageouts:", (uint64_t) (vm_stat.pageouts));
	sspstat("Swapins:", (uint64_t) (vm_stat.swapins));
	sspstat("Swapouts:", (uint64_t) (vm_stat.swapouts));
	if (has_mte) {
		sspstat("Pages tagged:", vm_stat.total_tagged_pages);
		sspstat("Pages tagged resident:", vm_stat.resident_tagged_pages);
		sspstat("Pages tagged compressed:", vm_stat.compressed_tagged_pages);

		sspstat("Pages tag-storage:", vm_stat.total_tag_storage_pages);
		sspstat("Pages tag-storage holding tags:", vm_stat.tag_storing_tag_storage_pages);
		sspstat("Pages tag-storage free:", vm_stat.free_tag_storage_pages);
		sspstat("Pages tag-storage non-tag pageable:", vm_stat.nontag_pageable_tag_storage_pages);
		sspstat("Pages tag-storage non-tag wired:", vm_stat.nontag_wired_tag_storage_pages);

		sspstat("Bytes of compressed tags:", vm_stat.compressed_tag_storage_bytes);
		sspstat("Tagged compressions:", vm_stat.tagged_compressions);
		sspstat("Tagged decompressions:", vm_stat.tagged_decompressions);
	}
}

void
sspstat(char *str, uint64_t n)
{
	printf("%-35s %16llu.\n", str, n);
}

void
banner(void)
{
	get_stats(&vm_stat);
	printf("Mach Virtual Memory Statistics: ");
	printf("(page size of %llu bytes)\n", (mach_vm_size_t)vm_kernel_page_size);
	printf("%8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %11s %9s %8s %8s %8s "
	       "%8s %8s %8s %8s %8s",
	       "free",
	       "active",
	       "specul",
	       "inactive",
	       "throttle",
	       "wired",
	       "prgable",
	       "faults",
	       "copy",
	       "0fill",
	       "reactive",
	       "purged",
	       "file-backed",
	       "anonymous",
	       "cmprssed",
	       "cmprssor",
	       "dcomprs",
	       "comprs",
	       "pageins",
	       "pageout",
	       "swapins",
	       "swapouts");
	if (has_mte) {
		printf(" %8s %9s %12s %11s %8s %8s %13s %14s %11s %10s %11s",
	       "tagged",
	       "tgd-rsdnt",
	       "tgd-cmprssed",
	       "tag-storage",
	       "tgs-tags",
	       "tgs-free",
	       "tgs-ntag-pgbl",
		   "tgs-ntag-wired",
		   "cmprsd-tags",
	       "tgd-comprs",
	       "tgd-dcomprs");
	}
	putchar('\n');
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
	pstat((uint64_t) (vm_stat.free_count - vm_stat.speculative_count), 8);
	pstat((uint64_t) (vm_stat.active_count), 8);
	pstat((uint64_t) (vm_stat.speculative_count), 8);
	pstat((uint64_t) (vm_stat.inactive_count), 8);
	pstat((uint64_t) (vm_stat.throttled_count), 8);
	pstat((uint64_t) (vm_stat.wire_count), 8);
	pstat((uint64_t) (vm_stat.purgeable_count), 8);
	pstat((uint64_t) (vm_stat.faults - last.faults), 8);
	pstat((uint64_t) (vm_stat.cow_faults - last.cow_faults), 8);
	pstat((uint64_t) (vm_stat.zero_fill_count - last.zero_fill_count), 8);
	pstat((uint64_t) (vm_stat.reactivations - last.reactivations), 8);
	pstat((uint64_t) (vm_stat.purges - last.purges), 8);
	pstat((uint64_t) (vm_stat.external_page_count), 11);
	pstat((uint64_t) (vm_stat.internal_page_count), 9);
	pstat((uint64_t) (vm_stat.total_uncompressed_pages_in_compressor), 8);
	pstat((uint64_t) (vm_stat.compressor_page_count), 8);
	pstat((uint64_t) (vm_stat.decompressions - last.decompressions), 8);
	pstat((uint64_t) (vm_stat.compressions - last.compressions), 8);
	pstat((uint64_t) (vm_stat.pageins - last.pageins), 8);
	pstat((uint64_t) (vm_stat.pageouts - last.pageouts), 8);
	pstat((uint64_t) (vm_stat.swapins - last.swapins), 8);
	pstat((uint64_t) (vm_stat.swapouts - last.swapouts), 8);
	if (has_mte) {
		pstat(vm_stat.total_tagged_pages, 8);
		pstat(vm_stat.resident_tagged_pages, 9);
		pstat(vm_stat.compressed_tagged_pages, 12);
		pstat(vm_stat.total_tag_storage_pages, 11);
		pstat(vm_stat.tag_storing_tag_storage_pages, 8);
		pstat(vm_stat.free_tag_storage_pages, 8);
		pstat(vm_stat.nontag_pageable_tag_storage_pages, 13);
		pstat(vm_stat.nontag_wired_tag_storage_pages, 14);
		pstat(vm_stat.compressed_tag_storage_bytes, 11);
		pstat(vm_stat.tagged_compressions - last.tagged_compressions, 10);
		pstat(vm_stat.tagged_decompressions - last.tagged_decompressions, 11);
	}
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
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	kern_return_t ret;
	if ((ret = host_statistics64(myHost, HOST_VM_INFO64, (host_info64_t)stat, &count) != KERN_SUCCESS)) {
		fprintf(stderr, "%s: failed to get statistics. error %s\n", pgmname, mach_error_string(ret));
		exit(EXIT_FAILURE);
	}
}
