/*
 * Copyright (c) 2009-2016 Apple Inc. All rights reserved.
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

#include <vm_statistics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include <mach_debug/mach_debug.h>
#include <mach/mach_error.h>
#include <libutil.h>
#include <errno.h>
#include <sysexits.h>
#include <getopt.h>
#include <malloc/malloc.h>
#include <Kernel/IOKit/IOKitDebug.h>
#include <Kernel/libkern/OSKextLibPrivate.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/kext/OSKext.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreSymbolication/CoreSymbolication.h>

#ifndef VM_KERN_SITE_ZONE_VIEW
#define VM_KERN_SITE_ZONE_VIEW 0x00001000
#endif

#define streql(a, b)            (strcmp((a), (b)) == 0)
#define strneql(a, b, n)        (strncmp((a), (b), (n)) == 0)
#define PRINTK(fmt, value)      \
	printf(fmt "K", (value) / 1024 )        /* ick */

static void usage(FILE *stream);
static void printzone(mach_zone_name_t *, mach_zone_info_t *);
static void colprintzone(mach_zone_name_t *, mach_zone_info_t *);
static int  find_deltas(mach_zone_name_t *, mach_zone_info_t *, mach_zone_info_t *, char *, int, int);
static void colprintzoneheader(void);
static boolean_t substr(const char *a, size_t alen, const char *b, size_t blen);

static int  SortName(void * thunk, const void * left, const void * right);
static int  SortSize(void * thunk, const void * left, const void * right);
static void PrintLarge(mach_memory_info_t *wiredInfo, unsigned int wiredInfoCnt,
    mach_zone_info_t *zoneInfo, mach_zone_name_t *zoneNames,
    unsigned int zoneCnt, uint64_t zoneElements,
    int (*func)(void *, const void *, const void *), boolean_t column);

static char *program;

static boolean_t ShowDeltas = FALSE;
static boolean_t ShowWasted = FALSE;
static boolean_t ShowTotal = FALSE;
static boolean_t ShowLarge = TRUE;
static boolean_t SortZones = FALSE;
static boolean_t ColFormat = TRUE;
static boolean_t PrintHeader = TRUE;

static unsigned long long totalsize = 0;
static unsigned long long totalused = 0;
static unsigned long long totalsum = 0;
static unsigned long long totalfragmented = 0;
static unsigned long long totalcollectable = 0;

static int last_time = 0;

static  char    *zname = NULL;
static  size_t  znamelen = 0;

#define LEFTALIGN -1
#define RIGHTALIGN 1

typedef struct {
	char *line1;
	char *line2;
	int colwidth;
	int alignment;
	bool visible;
} column_format;

enum {
	COL_ZONE_NAME,
	COL_ELEM_SIZE,
	COL_CUR_SIZE,
	COL_MAX_SIZE,
	COL_CUR_ELTS,
	COL_MAX_ELTS,
	COL_CUR_INUSE,
	COL_ALLOC_SIZE,
	COL_ALLOC_COUNT,
	COL_ZONE_FLAGS,
	COL_FRAG_SIZE,
	COL_FREE_SIZE,
	COL_TOTAL_ALLOCS,
	COL_MAX
};

/*
 * The order in which the columns appear below should match
 * the order in which the values are printed in colprintzone().
 */
static column_format columns[] = {
	[COL_ZONE_NAME]         = { "", "zone name", 25, LEFTALIGN, true },
	[COL_ELEM_SIZE]         = { "elem", "size", 6, RIGHTALIGN, true },
	[COL_CUR_SIZE]          = { "cur", "size", 11, RIGHTALIGN, true },
	[COL_MAX_SIZE]          = { "max", "size", 11, RIGHTALIGN, true },
	[COL_CUR_ELTS]          = { "cur", "#elts", 10, RIGHTALIGN, true },
	[COL_MAX_ELTS]          = { "max", "#elts", 11, RIGHTALIGN, true },
	[COL_CUR_INUSE]         = { "cur", "inuse", 11, RIGHTALIGN, true },
	[COL_ALLOC_SIZE]        = { "alloc", "size", 6, RIGHTALIGN, true },
	[COL_ALLOC_COUNT]       = { "alloc", "count", 6, RIGHTALIGN, true },
	[COL_ZONE_FLAGS]        = { "", "", 2, RIGHTALIGN, true },
	/* additional columns for special flags, not visible by default */
	[COL_FRAG_SIZE]         = { "frag", "size", 9, RIGHTALIGN, false },
	[COL_FREE_SIZE]         = { "free", "size", 9, RIGHTALIGN, false },
	[COL_TOTAL_ALLOCS]      = { "total", "allocs", 17, RIGHTALIGN, false }
};

static void
sigintr(__unused int signum)
{
	last_time = 1;
}

static void
usage(FILE *stream)
{
	fprintf(stream, "usage: %s [-w] [-s] [-c] [-h] [-H] [-t] [-d] [-l] [-L] [name]\n\n", program);
	fprintf(stream, "\t-w\tshow wasted memory for each zone\n");
	fprintf(stream, "\t-s\tsort zones by wasted memory\n");
	fprintf(stream, "\t-c\t(default) display output formatted in columns\n");
	fprintf(stream, "\t-h\tdisplay this help message\n");
	fprintf(stream, "\t-H\thide column names\n");
	fprintf(stream, "\t-t\tdisplay the total size of allocations over the life of the zone\n");
	fprintf(stream, "\t-d\tdisplay deltas over time\n");
	fprintf(stream, "\t-l\t(default) display wired memory info after zone info\n");
	fprintf(stream, "\t-L\tdo not show wired memory info, only show zone info\n");
	fprintf(stream, "\nAny option (including default options) can be overridden by specifying the option in upper-case.\n\n");
	exit(stream != stdout);
}

int
main(int argc, char **argv)
{
	mach_zone_name_t *name = NULL;
	unsigned int nameCnt = 0;
	mach_zone_info_t *info = NULL;
	unsigned int infoCnt = 0;
	mach_memory_info_t *wiredInfo = NULL;
	unsigned int wiredInfoCnt = 0;
	mach_zone_info_t *max_info = NULL;
	char            *deltas = NULL;
	uint64_t        zoneElements;

	kern_return_t   kr;
	int             i, j;
	int             first_time = 1;
	int     must_print = 1;
	int             interval = 1;

	signal(SIGINT, sigintr);

	program = strrchr(argv[0], '/');
	if (program == NULL) {
		program = argv[0];
	} else {
		program++;
	}

	for (i = 1; i < argc; i++) {
		if (streql(argv[i], "-d")) {
			ShowDeltas = TRUE;
		} else if (streql(argv[i], "-t")) {
			ShowTotal = TRUE;
		} else if (streql(argv[i], "-T")) {
			ShowTotal = FALSE;
		} else if (streql(argv[i], "-w")) {
			ShowWasted = TRUE;
		} else if (streql(argv[i], "-W")) {
			ShowWasted = FALSE;
		} else if (streql(argv[i], "-l")) {
			ShowLarge = TRUE;
		} else if (streql(argv[i], "-L")) {
			ShowLarge = FALSE;
		} else if (streql(argv[i], "-s")) {
			SortZones = TRUE;
		} else if (streql(argv[i], "-S")) {
			SortZones = FALSE;
		} else if (streql(argv[i], "-c")) {
			ColFormat = TRUE;
		} else if (streql(argv[i], "-C")) {
			ColFormat = FALSE;
		} else if (streql(argv[i], "-h")) {
			usage(stdout);
		} else if (streql(argv[i], "-H")) {
			PrintHeader = FALSE;
		} else if (streql(argv[i], "--")) {
			i++;
			break;
		} else if (argv[i][0] == '-') {
			usage(stderr);
		} else {
			break;
		}
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
		usage(stderr);
	}

	if (ShowDeltas) {
		SortZones = FALSE;
		ColFormat = TRUE;
		PrintHeader = TRUE;
	}

	if (ShowWasted) {
		columns[COL_FRAG_SIZE].visible = true;
		columns[COL_FREE_SIZE].visible = true;
	}
	if (ShowTotal) {
		columns[COL_TOTAL_ALLOCS].visible = true;
	}

	for (;;) {
		kr = mach_memory_info(mach_host_self(),
		    &name, &nameCnt, &info, &infoCnt,
		    &wiredInfo, &wiredInfoCnt);
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: mach_memory_info: %s (try running as root)\n",
			    program, mach_error_string(kr));
			exit(1);
		}

		if (nameCnt != infoCnt) {
			fprintf(stderr, "%s: mach_zone_name/ mach_zone_info: counts not equal?\n",
			    program);
			exit(1);
		}

		if (first_time) {
			deltas = (char *)malloc(infoCnt);
			max_info = (mach_zone_info_t *)malloc((infoCnt * sizeof *info));
		}

		if (SortZones) {
			for (i = 0; i < nameCnt - 1; i++) {
				for (j = i + 1; j < nameCnt; j++) {
					unsigned long long wastei, wastej;

					wastei = (info[i].mzi_cur_size -
					    (info[i].mzi_elem_size *
					    info[i].mzi_count));
					wastej = (info[j].mzi_cur_size -
					    (info[j].mzi_elem_size *
					    info[j].mzi_count));

					if (wastej > wastei) {
						mach_zone_info_t tinfo;
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
		}

		must_print = find_deltas(name, info, max_info, deltas, infoCnt, first_time);
		zoneElements = 0;
		if (must_print) {
			if (ColFormat) {
				if (!first_time) {
					printf("\n");
				}
				colprintzoneheader();
			}
			for (i = 0; i < nameCnt; i++) {
				if (deltas[i]) {
					if (ColFormat) {
						colprintzone(&name[i], &info[i]);
					} else {
						printzone(&name[i], &info[i]);
					}
					zoneElements += info[i].mzi_count;
				}
			}
		}

		if (ShowLarge && first_time) {
			PrintLarge(wiredInfo, wiredInfoCnt, &info[0], &name[0],
			    nameCnt, zoneElements,
			    SortZones ? &SortSize : &SortName, ColFormat);
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

		if ((wiredInfo != NULL) && (wiredInfoCnt != 0)) {
			kr = vm_deallocate(mach_task_self(), (vm_address_t) wiredInfo,
			    (vm_size_t) (wiredInfoCnt * sizeof *wiredInfo));
			if (kr != KERN_SUCCESS) {
				fprintf(stderr, "%s: vm_deallocate: %s\n",
				    program, mach_error_string(kr));
				exit(1);
			}
		}

		if ((ShowWasted || ShowTotal) && PrintHeader && !ShowDeltas) {
			printf("\nZONE TOTALS\n");
			printf("---------------------------------------------\n");
			printf("TOTAL SIZE        = %llu\n", totalsize);
			printf("TOTAL USED        = %llu\n", totalused);
			if (ShowWasted) {
				printf("TOTAL WASTED      = %llu\n", totalsize - totalused);
				printf("TOTAL FRAGMENTED  = %llu\n", totalfragmented);
				printf("TOTAL COLLECTABLE = %llu\n", totalcollectable);
			}
			if (ShowTotal) {
				printf("TOTAL ALLOCS      = %llu\n", totalsum);
			}
		}

		if (ShowDeltas == FALSE || last_time) {
			break;
		}

		sleep(interval);
	}
	exit(0);
}

static boolean_t
substr(const char *a, size_t alen, const char *b, size_t blen)
{
	int i;

	if (alen > blen) {
		return FALSE;
	}

	for (i = 0; i <= blen - alen; i++) {
		if (strneql(a, b + i, alen)) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
printzone(mach_zone_name_t *name, mach_zone_info_t *info)
{
	unsigned long long used, size, fragmented, collectable;

	printf("%.*s zone:\n", (int)sizeof name->mzn_name, name->mzn_name);
	printf("\tcur_size:    %lluK bytes (%llu elements)\n",
	    info->mzi_cur_size / 1024,
	    (info->mzi_elem_size == 0) ? 0 :
	    info->mzi_cur_size / info->mzi_elem_size);
	printf("\tmax_size:    %lluK bytes (%llu elements)\n",
	    info->mzi_max_size / 1024,
	    (info->mzi_elem_size == 0) ? 0 :
	    info->mzi_max_size / info->mzi_elem_size);
	printf("\telem_size:   %llu bytes\n",
	    info->mzi_elem_size);
	printf("\t# of elems:  %llu\n",
	    info->mzi_count);
	printf("\talloc_size:  %lluK bytes (%llu elements)\n",
	    info->mzi_alloc_size / 1024,
	    (info->mzi_elem_size == 0) ? 0 :
	    info->mzi_alloc_size / info->mzi_elem_size);
	if (info->mzi_exhaustible) {
		printf("\tEXHAUSTIBLE\n");
	}
	if (GET_MZI_COLLECTABLE_FLAG(info->mzi_collectable)) {
		printf("\tCOLLECTABLE\n");
	}
	if (ShowWasted) {
		totalused += used = info->mzi_elem_size * info->mzi_count;
		totalsize += size = info->mzi_cur_size;
		totalcollectable += collectable = GET_MZI_COLLECTABLE_BYTES(info->mzi_collectable);
		totalfragmented += fragmented = size - used - collectable;
		printf("\t\t\t\t\tWASTED: %llu\n", size - used);
		printf("\t\t\t\t\tFRAGMENTED: %llu\n", fragmented);
		printf("\t\t\t\t\tCOLLECTABLE: %llu\n", collectable);
	}
	if (ShowTotal) {
		totalsum += info->mzi_sum_size;
		printf("\t\t\t\t\tTOTAL: %llu\n", totalsum);
	}
}

static void
colprintzone(mach_zone_name_t *zone_name, mach_zone_info_t *info)
{
	char *name = zone_name->mzn_name;
	int j, namewidth;
	unsigned long long used, size, fragmented, collectable;

	namewidth = columns[COL_ZONE_NAME].colwidth;

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


#define PRINTCOL(value, index)                                                                                    \
	if (columns[(index)].visible) {                                                                           \
	        printf(" %*llu", columns[(index)].colwidth * columns[(index)].alignment, (value));                \
	}
#define PRINTCOLSTR(value, index)                                                                                 \
	if (columns[(index)].visible) {                                                                           \
	        printf(" %*s", columns[(index)].colwidth * columns[(index)].alignment, (value));                  \
	}
#define PRINTCOLK(value, index)                                                                                   \
	if (columns[(index)].visible) {                                                                           \
	        printf(" %*lluK", (columns[(index)].colwidth - 1) * columns[(index)].alignment, (value) / 1024 ); \
	}
#define PRINTCOLSZ(value, index)                                                                                  \
	if (columns[(index)].visible) {                                                                           \
	        if ((value) < 1024) {                                                                             \
	                printf(" %*lluB", (columns[(index)].colwidth - 1) * columns[(index)].alignment, (value)); \
	        } else {                                                                                          \
	                PRINTCOLK(value, index)                                                                   \
	        }                                                                                                 \
	}


	PRINTCOL(info->mzi_elem_size, COL_ELEM_SIZE);
	PRINTCOLK(info->mzi_cur_size, COL_CUR_SIZE);
	if (info->mzi_max_size / 1024 > 9999999) {
		/*
		 * Zones with preposterously large maximum sizes are shown with `-------'
		 * in the max size and max num elts fields.
		 */
		PRINTCOLSTR("-------", COL_MAX_SIZE);
	} else {
		PRINTCOLK(info->mzi_max_size, COL_MAX_SIZE);
	}
	PRINTCOL(info->mzi_cur_size / info->mzi_elem_size, COL_CUR_ELTS);
	if (info->mzi_max_size / 1024 > 9999999) {
		PRINTCOLSTR("-------", COL_MAX_ELTS);
	} else {
		PRINTCOL(info->mzi_max_size / info->mzi_elem_size, COL_MAX_ELTS);
	}
	PRINTCOL(info->mzi_count, COL_CUR_INUSE);
	PRINTCOLK(info->mzi_alloc_size, COL_ALLOC_SIZE);
	PRINTCOL(info->mzi_alloc_size / info->mzi_elem_size, COL_ALLOC_COUNT);

	totalused += used = info->mzi_elem_size * info->mzi_count;
	totalsize += size = info->mzi_cur_size;
	totalsum += info->mzi_sum_size;
	totalcollectable += collectable = GET_MZI_COLLECTABLE_BYTES(info->mzi_collectable);
	totalfragmented += fragmented = size - used - collectable;

	printf(" %c%c",
	    (info->mzi_exhaustible ? 'X' : ' '),
	    (GET_MZI_COLLECTABLE_FLAG(info->mzi_collectable) ? 'C' : ' '));

	PRINTCOLSZ(fragmented, COL_FRAG_SIZE);
	PRINTCOLSZ(collectable, COL_FREE_SIZE);
	PRINTCOLSZ(info->mzi_sum_size, COL_TOTAL_ALLOCS);

	printf("\n");
}


static void
colprintzoneheader(void)
{
	int i, totalwidth = 0;

	if (!PrintHeader) {
		return;
	}

	for (i = 0; i < COL_MAX; i++) {
		if (columns[i].visible) {
			printf("%*s ", columns[i].colwidth * columns[i].alignment, columns[i].line1);
		}
	}
	printf("\n");

	for (i = 0; i < COL_MAX; i++) {
		if (columns[i].visible) {
			printf("%*s ", columns[i].colwidth * columns[i].alignment, columns[i].line2);
			totalwidth += (columns[i].colwidth + 1);
		}
	}

	printf("\n");
	for (i = 0; i < totalwidth; i++) {
		printf("-");
	}
	printf("\n");
}

int
find_deltas(mach_zone_name_t *name, mach_zone_info_t *info, mach_zone_info_t *max_info,
    char *deltas, int cnt, int first_time)
{
	int i;
	int  found_one = 0;

	for (i = 0; i < cnt; i++) {
		deltas[i] = 0;
		if (substr(zname, znamelen, name[i].mzn_name,
		    strnlen(name[i].mzn_name, sizeof name[i].mzn_name))) {
			if (first_time || info->mzi_cur_size > max_info->mzi_cur_size ||
			    (ShowTotal && ((info->mzi_sum_size >> 1) > max_info->mzi_sum_size))) {
				max_info->mzi_cur_size = info->mzi_cur_size;
				max_info->mzi_sum_size = info->mzi_sum_size;
				deltas[i] = 1;
				found_one = 1;
			}
		}
		info++;
		max_info++;
	}
	return found_one;
}

/*********************************************************************
*********************************************************************/

static char *
kern_vm_tag_name(uint64_t tag)
{
	char * result;
	const char * name;
	switch (tag) {
	case (VM_KERN_MEMORY_NONE):             name = "VM_KERN_MEMORY_NONE"; break;
	case (VM_KERN_MEMORY_OSFMK):            name = "VM_KERN_MEMORY_OSFMK"; break;
	case (VM_KERN_MEMORY_BSD):              name = "VM_KERN_MEMORY_BSD"; break;
	case (VM_KERN_MEMORY_IOKIT):            name = "VM_KERN_MEMORY_IOKIT"; break;
	case (VM_KERN_MEMORY_LIBKERN):          name = "VM_KERN_MEMORY_LIBKERN"; break;
	case (VM_KERN_MEMORY_OSKEXT):           name = "VM_KERN_MEMORY_OSKEXT"; break;
	case (VM_KERN_MEMORY_KEXT):             name = "VM_KERN_MEMORY_KEXT"; break;
	case (VM_KERN_MEMORY_IPC):              name = "VM_KERN_MEMORY_IPC"; break;
	case (VM_KERN_MEMORY_STACK):            name = "VM_KERN_MEMORY_STACK"; break;
	case (VM_KERN_MEMORY_CPU):              name = "VM_KERN_MEMORY_CPU"; break;
	case (VM_KERN_MEMORY_PMAP):             name = "VM_KERN_MEMORY_PMAP"; break;
	case (VM_KERN_MEMORY_PTE):              name = "VM_KERN_MEMORY_PTE"; break;
	case (VM_KERN_MEMORY_ZONE):             name = "VM_KERN_MEMORY_ZONE"; break;
	case (VM_KERN_MEMORY_KALLOC):           name = "VM_KERN_MEMORY_KALLOC"; break;
	case (VM_KERN_MEMORY_COMPRESSOR):       name = "VM_KERN_MEMORY_COMPRESSOR"; break;
	case (VM_KERN_MEMORY_COMPRESSED_DATA):  name = "VM_KERN_MEMORY_COMPRESSED_DATA"; break;
	case (VM_KERN_MEMORY_PHANTOM_CACHE):    name = "VM_KERN_MEMORY_PHANTOM_CACHE"; break;
	case (VM_KERN_MEMORY_WAITQ):            name = "VM_KERN_MEMORY_WAITQ"; break;
	case (VM_KERN_MEMORY_DIAG):             name = "VM_KERN_MEMORY_DIAG"; break;
	case (VM_KERN_MEMORY_LOG):              name = "VM_KERN_MEMORY_LOG"; break;
	case (VM_KERN_MEMORY_FILE):             name = "VM_KERN_MEMORY_FILE"; break;
	case (VM_KERN_MEMORY_MBUF):             name = "VM_KERN_MEMORY_MBUF"; break;
	case (VM_KERN_MEMORY_UBC):              name = "VM_KERN_MEMORY_UBC"; break;
	case (VM_KERN_MEMORY_SECURITY):         name = "VM_KERN_MEMORY_SECURITY"; break;
	case (VM_KERN_MEMORY_MLOCK):            name = "VM_KERN_MEMORY_MLOCK"; break;
	case (VM_KERN_MEMORY_REASON):           name = "VM_KERN_MEMORY_REASON"; break;
	case (VM_KERN_MEMORY_SKYWALK):          name = "VM_KERN_MEMORY_SKYWALK"; break;
	case (VM_KERN_MEMORY_LTABLE):           name = "VM_KERN_MEMORY_LTABLE"; break;
	case (VM_KERN_MEMORY_ANY):              name = "VM_KERN_MEMORY_ANY"; break;
	default:                                name = NULL; break;
	}
	if (name) {
		asprintf(&result, "%s", name);
	} else {
		asprintf(&result, "VM_KERN_MEMORY_%lld", tag);
	}
	return result;
}

static char *
kern_vm_counter_name(uint64_t tag)
{
	char * result;
	const char * name;
	switch (tag) {
	case (VM_KERN_COUNT_MANAGED):                   name = "VM_KERN_COUNT_MANAGED"; break;
	case (VM_KERN_COUNT_RESERVED):                  name = "VM_KERN_COUNT_RESERVED"; break;
	case (VM_KERN_COUNT_WIRED):                     name = "VM_KERN_COUNT_WIRED"; break;
	case (VM_KERN_COUNT_WIRED_BOOT):                name = "VM_KERN_COUNT_WIRED_BOOT"; break;
	case (VM_KERN_COUNT_WIRED_MANAGED):             name = "VM_KERN_COUNT_WIRED_MANAGED"; break;
	case (VM_KERN_COUNT_STOLEN):                    name = "VM_KERN_COUNT_STOLEN"; break;
	case (VM_KERN_COUNT_BOOT_STOLEN):               name = "VM_KERN_COUNT_BOOT_STOLEN"; break;
	case (VM_KERN_COUNT_LOPAGE):                    name = "VM_KERN_COUNT_LOPAGE"; break;
	case (VM_KERN_COUNT_MAP_KERNEL):                name = "VM_KERN_COUNT_MAP_KERNEL"; break;
	case (VM_KERN_COUNT_MAP_ZONE):                  name = "VM_KERN_COUNT_MAP_ZONE"; break;
	case (VM_KERN_COUNT_MAP_KALLOC):                name = "VM_KERN_COUNT_MAP_KALLOC"; break;
	case (VM_KERN_COUNT_WIRED_STATIC_KERNELCACHE):
		name = "VM_KERN_COUNT_WIRED_STATIC_KERNELCACHE";
		break;
	default:                                                                name = NULL; break;
	}
	if (name) {
		asprintf(&result, "%s", name);
	} else {
		asprintf(&result, "VM_KERN_COUNT_%lld", tag);
	}
	return result;
}

static void
MakeLoadTagKeys(const void * key, const void * value, void * context)
{
	CFMutableDictionaryRef newDict  = context;
	CFDictionaryRef        kextInfo = value;
	CFNumberRef            loadTag;
	uint32_t               loadTagValue;

	loadTag = (CFNumberRef)CFDictionaryGetValue(kextInfo, CFSTR(kOSBundleLoadTagKey));
	CFNumberGetValue(loadTag, kCFNumberSInt32Type, &loadTagValue);
	key = (const void *)(uintptr_t) loadTagValue;
	CFDictionarySetValue(newDict, key, value);
}

static CSSymbolicatorRef         gSym;
static CFMutableDictionaryRef    gTagDict;
static mach_memory_info_t *  gSites;

static char *
GetSiteName(int siteIdx, mach_zone_name_t * zoneNames, unsigned int zoneNamesCnt)
{
	const char      * name;
	uintptr_t         kmodid;
	char            * result;
	char            * append;
	mach_vm_address_t addr;
	CFDictionaryRef   kextInfo;
	CFStringRef       bundleID;
	uint32_t          type;

	const mach_memory_info_t * site;
	const char                   * fileName;
	CSSymbolRef                    symbol;
	const char                   * symbolName;
	CSSourceInfoRef                sourceInfo;

	name = NULL;
	result = NULL;
	site = &gSites[siteIdx];
	addr = site->site;
	type = (VM_KERN_SITE_TYPE & site->flags);
	kmodid = 0;

	if (VM_KERN_SITE_NAMED & site->flags) {
		asprintf(&result, "%s", &site->name[0]);
	} else {
		switch (type) {
		case VM_KERN_SITE_TAG:
			result = kern_vm_tag_name(addr);
			break;

		case VM_KERN_SITE_COUNTER:
			result = kern_vm_counter_name(addr);
			break;

		case VM_KERN_SITE_KMOD:

			kmodid = (uintptr_t) addr;
			kextInfo = CFDictionaryGetValue(gTagDict, (const void *)kmodid);
			if (kextInfo) {
				bundleID = (CFStringRef)CFDictionaryGetValue(kextInfo, kCFBundleIdentifierKey);
				name = CFStringGetCStringPtr(bundleID, kCFStringEncodingUTF8);
				//    wiredSize = (CFNumberRef)CFDictionaryGetValue(kextInfo, CFSTR(kOSBundleWiredSizeKey));
			}

			if (name) {
				asprintf(&result, "%s", name);
			} else {
				asprintf(&result, "(unloaded kmod)");
			}
			break;

		case VM_KERN_SITE_KERNEL:
			symbolName = NULL;
			if (addr) {
				symbol = CSSymbolicatorGetSymbolWithAddressAtTime(gSym, addr, kCSNow);
				symbolName = CSSymbolGetName(symbol);
			}
			if (symbolName) {
				asprintf(&result, "%s", symbolName);
				sourceInfo = CSSymbolicatorGetSourceInfoWithAddressAtTime(gSym, addr, kCSNow);
				fileName = CSSourceInfoGetPath(sourceInfo);
				if (fileName) {
					printf(" (%s:%d)", fileName, CSSourceInfoGetLineNumber(sourceInfo));
				}
			} else {
				asprintf(&result, "site 0x%qx", addr);
			}
			break;
		default:
			asprintf(&result, "");
			break;
		}
	}

	if (result
	    && (VM_KERN_SITE_ZONE & site->flags)
	    && zoneNames
	    && (site->zone < zoneNamesCnt)) {
		size_t namelen, zonelen;
		namelen = strlen(result);
		zonelen = strnlen(zoneNames[site->zone].mzn_name, sizeof(zoneNames[site->zone].mzn_name));
		if (((namelen + zonelen) > 61) && (zonelen < 61)) {
			namelen = (61 - zonelen);
		}
		asprintf(&append, "%.*s[%.*s]",
		    (int)namelen,
		    result,
		    (int)zonelen,
		    zoneNames[site->zone].mzn_name);
		free(result);
		result = append;
	}
	if (result && kmodid) {
		asprintf(&append, "%-64s%3ld", result, kmodid);
		free(result);
		result = append;
	}

	return result;
}

struct CompareThunk {
	mach_zone_name_t *zoneNames;
	unsigned int      zoneNamesCnt;
};

static int
SortName(void * thunk, const void * left, const void * right)
{
	const struct CompareThunk * t = (typeof(t))thunk;
	const int * idxL;
	const int * idxR;
	char * l;
	char * r;
	CFStringRef lcf;
	CFStringRef rcf;
	int result;

	idxL = (typeof(idxL))left;
	idxR = (typeof(idxR))right;
	l = GetSiteName(*idxL, t->zoneNames, t->zoneNamesCnt);
	r = GetSiteName(*idxR, t->zoneNames, t->zoneNamesCnt);

	lcf = CFStringCreateWithCString(kCFAllocatorDefault, l, kCFStringEncodingUTF8);
	rcf = CFStringCreateWithCString(kCFAllocatorDefault, r, kCFStringEncodingUTF8);

	result = (int) CFStringCompareWithOptionsAndLocale(lcf, rcf, CFRangeMake(0, CFStringGetLength(lcf)), kCFCompareNumerically, NULL);

	CFRelease(lcf);
	CFRelease(rcf);
	free(l);
	free(r);

	return result;
}

static int
SortSize(void * thunk, const void * left, const void * right)
{
	const mach_memory_info_t * siteL;
	const mach_memory_info_t * siteR;
	const int * idxL;
	const int * idxR;

	idxL = (typeof(idxL))left;
	idxR = (typeof(idxR))right;
	siteL = &gSites[*idxL];
	siteR = &gSites[*idxR];

	if (siteL->size > siteR->size) {
		return -1;
	} else if (siteL->size < siteR->size) {
		return 1;
	}
	return 0;
}


static void
PrintLarge(mach_memory_info_t *wiredInfo, unsigned int wiredInfoCnt,
    mach_zone_info_t *zoneInfo, mach_zone_name_t *zoneNames,
    unsigned int zoneCnt, uint64_t zoneElements,
    int (*func)(void *, const void *, const void *), boolean_t column)
{
	uint64_t zonetotal;
	uint64_t top_wired;
	uint64_t size;
	uint64_t elemsTagged;

	CFDictionaryRef allKexts;
	unsigned int idx, site, first;
	int sorted[wiredInfoCnt];
	char totalstr[40];
	char * name;
	bool   headerPrinted;

	zonetotal = totalsize;

	gSites = wiredInfo;

	gSym = CSSymbolicatorCreateWithMachKernel();

	allKexts = OSKextCopyLoadedKextInfo(NULL, NULL);
	gTagDict = CFDictionaryCreateMutable(
		kCFAllocatorDefault, (CFIndex) 0,
		(CFDictionaryKeyCallBacks *) 0,
		&kCFTypeDictionaryValueCallBacks);

	CFDictionaryApplyFunction(allKexts, &MakeLoadTagKeys, gTagDict);
	CFRelease(allKexts);

	top_wired = 0;

	for (idx = 0; idx < wiredInfoCnt; idx++) {
		sorted[idx] = idx;
	}
	first = 0; // VM_KERN_MEMORY_FIRST_DYNAMIC
	struct CompareThunk thunk;
	thunk.zoneNames    = zoneNames;
	thunk.zoneNamesCnt = zoneCnt;
	qsort_r(&sorted[first],
	    wiredInfoCnt - first,
	    sizeof(sorted[0]),
	    &thunk,
	    func);

	elemsTagged = 0;
	for (headerPrinted = false, idx = 0; idx < wiredInfoCnt; idx++) {
		site = sorted[idx];
		if ((VM_KERN_SITE_COUNTER & gSites[site].flags)
		    && (VM_KERN_COUNT_WIRED == gSites[site].site)) {
			top_wired = gSites[site].size;
		}
		if (VM_KERN_SITE_HIDE & gSites[site].flags) {
			continue;
		}
		if (!((VM_KERN_SITE_WIRED | VM_KERN_SITE_ZONE) & gSites[site].flags)) {
			continue;
		}

		if ((VM_KERN_SITE_ZONE & gSites[site].flags)
		    && gSites[site].zone < zoneCnt) {
			elemsTagged += gSites[site].size / zoneInfo[gSites[site].zone].mzi_elem_size;
		}

		if ((gSites[site].size < 1024) && (gSites[site].peak < 1024)) {
			continue;
		}

		name = GetSiteName(site, zoneNames, zoneCnt);
		if (!substr(zname, znamelen, name, strlen(name))) {
			continue;
		}
		if (!headerPrinted) {
			printf("-------------------------------------------------------------------------------------------------------------\n");
			printf("                                                               kmod          vm        peak               cur\n");
			printf("wired memory                                                     id         tag        size  waste       size\n");
			printf("-------------------------------------------------------------------------------------------------------------\n");
			headerPrinted = true;
		}
		printf("%-67s", name);
		free(name);
		printf("%12d", gSites[site].tag);

		if (gSites[site].peak) {
			PRINTK(" %10llu", gSites[site].peak);
		} else {
			printf(" %11s", "");
		}

		if (gSites[site].collectable_bytes) {
			PRINTK(" %5llu", gSites[site].collectable_bytes);
		} else {
			printf(" %6s", "");
		}

		PRINTK(" %9llu", gSites[site].size);

		if (!(VM_KERN_SITE_ZONE & gSites[site].flags)) {
			totalsize += gSites[site].size;
		}

		printf("\n");
	}

	if (!znamelen) {
		printf("%-67s", "zones");
		printf("%12s", "");
		printf(" %11s", "");
		printf(" %6s", "");
		PRINTK(" %9llu", zonetotal);
		printf("\n");
	}
	if (headerPrinted) {
		if (elemsTagged) {
			snprintf(totalstr, sizeof(totalstr), "%lld of %lld", elemsTagged, zoneElements);
			printf("zone tags%100s\n", totalstr);
		}
		snprintf(totalstr, sizeof(totalstr), "%6.2fM of %6.2fM", totalsize / 1024.0 / 1024.0, top_wired / 1024.0 / 1024.0);
		printf("total%104s\n", totalstr);
	}
	for (headerPrinted = false, idx = 0; idx < wiredInfoCnt; idx++) {
		site = sorted[idx];
		size = gSites[site].mapped;
		if (!size) {
			continue;
		}
		if (VM_KERN_SITE_HIDE & gSites[site].flags) {
			continue;
		}
		if ((size == gSites[site].size)
		    && ((VM_KERN_SITE_WIRED | VM_KERN_SITE_ZONE) & gSites[site].flags)) {
			continue;
		}

		name = GetSiteName(site, NULL, 0);
		if (!substr(zname, znamelen, name, strlen(name))) {
			continue;
		}
		if (!headerPrinted) {
			printf("-------------------------------------------------------------------------------------------------------------\n");
			printf("                                                                        largest        peak               cur\n");
			printf("maps                                                           free        free        size              size\n");
			printf("-------------------------------------------------------------------------------------------------------------\n");
			headerPrinted = true;
		}
		printf("%-55s", name);
		free(name);

		if (gSites[site].free) {
			PRINTK(" %10llu", gSites[site].free);
		} else {
			printf(" %11s", "");
		}
		if (gSites[site].largest) {
			PRINTK(" %10llu", gSites[site].largest);
		} else {
			printf(" %11s", "");
		}
		if (gSites[site].peak) {
			PRINTK(" %10llu", gSites[site].peak);
		} else {
			printf(" %11s", "");
		}
		PRINTK(" %16llu", size);

		printf("\n");
	}
	for (headerPrinted = false, idx = 0; idx < wiredInfoCnt; idx++) {
		site = sorted[idx];
		size = gSites[site].size;
		if (!size || !(VM_KERN_SITE_ZONE_VIEW & gSites[site].flags)) {
			continue;
		}

		name = GetSiteName(site, NULL, 0);
		if (!substr(zname, znamelen, name, strlen(name))) {
			continue;
		}
		if (!headerPrinted) {
			printf("-------------------------------------------------------------------------------------------------------------\n");
			printf("                                                                                                          cur\n");
			printf("zone views                                                                                              inuse\n");
			printf("-------------------------------------------------------------------------------------------------------------\n");
			headerPrinted = true;
		}
		printf("%-55s", name);
		free(name);

		printf(" %11s", "");
		printf(" %11s", "");
		printf(" %11s", "");
		PRINTK(" %16llu", size);

		printf("\n");
	}
	totalsize = zonetotal;
}
