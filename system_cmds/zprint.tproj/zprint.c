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

static void printzone();
static void colprintzone();
static void colprintzoneheader();
static void printk();
static int strnlen();
static boolean_t substr();

static char *program;

static boolean_t ShowWasted = FALSE;
static boolean_t SortZones = FALSE;
static boolean_t ColFormat = TRUE;
static boolean_t PrintHeader = TRUE;

static unsigned int totalsize = 0;
static unsigned int totalused = 0;

static void
usage()
{
	fprintf(stderr, "usage: %s [-w] [-s] [-c] [-h] [name]\n", program);
	exit(1);
}

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	zone_name_t name_buf[1024];
	zone_name_t *name = name_buf;
	unsigned int nameCnt = sizeof name_buf/sizeof name_buf[0];
	zone_info_t info_buf[1024];
	zone_info_t *info = info_buf;
	unsigned int infoCnt = sizeof info_buf/sizeof info_buf[0];

	char		*zname = NULL;
	int		znamelen = 0;

	kern_return_t	kr;
	int		i, j;

	if (0 != reexec_to_match_kernel()) {
		fprintf(stderr, "Could not re-execute: %d\n", errno);
		exit(1);
	}

	program = strrchr(argv[0], '/');
	if (program == NULL)
		program = argv[0];
	else
		program++;

	for (i = 1; i < argc; i++) {
		if (streql(argv[i], "-w"))
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
		else if (streql(argv[i], "--")) {
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

	kr = host_zone_info(mach_host_self(),
			    &name, &nameCnt, &info, &infoCnt);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "%s: host_zone_info: %s\n",
		     program, mach_error_string(kr));
		exit(1);
	}
	else if (nameCnt != infoCnt) {
		fprintf(stderr, "%s: host_zone_info: counts not equal?\n",
			program);
		exit(1);
	}

	if (SortZones) {
		for (i = 0; i < nameCnt-1; i++)
			for (j = i+1; j < nameCnt; j++) {
				int wastei, wastej;

				wastei = (info[i].zi_cur_size -
					  (info[i].zi_elem_size *
					   info[i].zi_count));
				wastej = (info[j].zi_cur_size -
					  (info[j].zi_elem_size *
					   info[j].zi_count));

				if (wastej > wastei) {
					zone_info_t tinfo;
					zone_name_t tname;

					tinfo = info[i];
					info[i] = info[j];
					info[j] = tinfo;

					tname = name[i];
					name[i] = name[j];
					name[j] = tname;
				}
			}
	}

	if (ColFormat) {
		colprintzoneheader();
	}
	for (i = 0; i < nameCnt; i++)
		if (substr(zname, znamelen, name[i].zn_name,
			   strnlen(name[i].zn_name, sizeof name[i].zn_name)))
		    	if (ColFormat)
				colprintzone(&name[i], &info[i]);
			else
				printzone(&name[i], &info[i]);

	if ((name != name_buf) && (nameCnt != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) name,
				   (vm_size_t) (nameCnt * sizeof *name));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: vm_deallocate: %s\n",
			     program, mach_error_string(kr));
			exit(1);
		}
	}

	if ((info != info_buf) && (infoCnt != 0)) {
		kr = vm_deallocate(mach_task_self(), (vm_address_t) info,
				   (vm_size_t) (infoCnt * sizeof *info));
		if (kr != KERN_SUCCESS) {
			fprintf(stderr, "%s: vm_deallocate: %s\n",
			     program, mach_error_string(kr));
			exit(1);
		}
	}

	if (ShowWasted && PrintHeader) {
		printf("TOTAL SIZE   = %u\n", totalsize);
		printf("TOTAL USED   = %u\n", totalused);
		printf("TOTAL WASTED = %d\n", totalsize - totalused);
	}

	exit(0);
}

static int
strnlen(s, n)
	char *s;
	int n;
{
	int len = 0;

	while ((len < n) && (*s++ != '\0'))
		len++;

	return len;
}

static boolean_t
substr(a, alen, b, blen)
	char *a;
	int alen;
	char *b;
	int blen;
{
	int i;

	for (i = 0; i <= blen - alen; i++)
		if (strneql(a, b+i, alen))
			return TRUE;

	return FALSE;
}

static void
printzone(name, info)
	zone_name_t *name;
	zone_info_t *info;
{
	unsigned int used, size;

	printf("%.*s zone:\n", (int)sizeof name->zn_name, name->zn_name);
	printf("\tcur_size:    %dK bytes (%d elements)\n",
	       info->zi_cur_size/1024,
	       info->zi_cur_size/info->zi_elem_size);
	printf("\tmax_size:    %dK bytes (%d elements)\n",
	       info->zi_max_size/1024,
	       info->zi_max_size/info->zi_elem_size);
	printf("\telem_size:   %d bytes\n",
	       info->zi_elem_size);
	printf("\t# of elems:  %d\n",
	       info->zi_count);
	printf("\talloc_size:  %dK bytes (%d elements)\n",
	       info->zi_alloc_size/1024,
	       info->zi_alloc_size/info->zi_elem_size);
	if (info->zi_pageable)
		printf("\tPAGEABLE\n");
	if (info->zi_collectable)
		printf("\tCOLLECTABLE\n");

	if (ShowWasted) {
		totalused += used = info->zi_elem_size * info->zi_count;
		totalsize += size = info->zi_cur_size;
		printf("\t\t\t\t\tWASTED: %d\n", size - used);
	}
}

static void
printk(fmt, i)
	char *fmt;
	int i;
{
	printf(fmt, i / 1024);
	putchar('K');
}

static void
colprintzone(zone_name, info)
	zone_name_t *zone_name;
	zone_info_t *info;
{
	char *name = zone_name->zn_name;
	int j, namewidth;
	unsigned int used, size;

	namewidth = 25;
	if (ShowWasted) {
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
	printf("%5d", info->zi_elem_size);
	printk("%6d", info->zi_cur_size);
	if (info->zi_max_size >= 99999 * 1024) {
		printf("   ----");
	} else {
		printk("%6d", info->zi_max_size);
	}
	printf("%7d", info->zi_cur_size / info->zi_elem_size);
	if (info->zi_max_size >= 99999 * 1024) {
		printf("   ----");
	} else {
		printf("%7d", info->zi_max_size / info->zi_elem_size);
	}
	printf("%6d", info->zi_count);
	printk("%5d", info->zi_alloc_size);
	printf("%6d", info->zi_alloc_size / info->zi_elem_size);

	totalused += used = info->zi_elem_size * info->zi_count;
	totalsize += size = info->zi_cur_size;
	if (ShowWasted) {
		printf("%7d", size - used);
	}

	printf("%c%c\n",
	       (info->zi_pageable ? 'P' : ' '),
	       (info->zi_collectable ? 'C' : ' '));
}

static void
colprintzoneheader()
{
	if (! PrintHeader) {
		return;
	}
	if (ShowWasted) {
		printf("                   elem    cur    max    cur    max%s",
		       "   cur alloc alloc\n");
		printf("zone name          size   size   size  #elts  #elts%s",
		       " inuse  size count wasted\n");
	} else {
		printf("                          elem    cur    max    cur%s",
		       "    max   cur alloc alloc\n");
		printf("zone name                 size   size   size  #elts%s",
		       "  #elts inuse  size count\n");
	}
	printf("-----------------------------------------------%s",
	       "--------------------------------\n");
}
