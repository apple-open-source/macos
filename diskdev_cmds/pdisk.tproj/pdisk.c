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
//
// pdisk - an editor for Apple format partition tables
//
// Written by Eryk Vershen (eryk@apple.com)
//
// Still under development (as of 20 Dec 1996)
//

/*
 * Copyright 1996,1997 by Apple Computer, Inc.
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */

#include <stdio.h>
#ifdef __linux__
#include <getopt.h>
#else
#include <stdlib.h>
#include <unistd.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#endif

#include "pdisk.h"
#include "io.h"
#include "partition_map.h"
#include "errors.h"
#include "dump.h"
#include "version.h"

extern int do_command_line(int argc, char * argv[]);

//
// Defines
//
#define ARGV_CHUNK 5

//
// Types
//


//
// Global Constants
//
enum getopt_values {
    kLongOption = 0,
    kBadOption = '?',
    kOptionArg = 1000,
    kListOption = 1001,
    kLogicalOption = 1002
};


//
// Global Variables
//
int lflag;	/* list the device */
char *lfile;	/* list */
int vflag;	/* show version */
int hflag;	/* show help */
int dflag;	/* turn on debugging commands and printout */
int rflag;	/* open device read Only */
int interactive;


//
// Forward declarations
//
void do_add_intel_partition(partition_map_header *map);
void do_change_map_size(partition_map_header *map);
void do_create_partition(partition_map_header *map, int get_type);
void do_delete_partition(partition_map_header *map);
void do_display_block(partition_map_header *map);
int do_expert(partition_map_header *map);
void do_reorder(partition_map_header *map);
void do_write_partition_map(partition_map_header *map);
void edit(char *name, int ask_logical_size);
int get_base_argument(long *number, partition_map_header *map);
int get_command_line(int *argc, char ***argv);
int get_size_argument(long *number, partition_map_header *map);
int get_options(int argc, char **argv);
void interact();
void print_notes();


//
// Routines
//
#ifdef __linux__
int
main(int argc, char **argv)
{
    int name_index;

    if (sizeof(DPME) != PBLOCK_SIZE) {
	fatal(-1, "Size of partion map entry (%d) "
		"is not equal to block size (%d)\n",
		sizeof(DPME), PBLOCK_SIZE);
    }
    if (sizeof(Block0) != PBLOCK_SIZE) {
	fatal(-1, "Size of block zero structure (%d) "
		"is not equal to block size (%d)\n",
		sizeof(Block0), PBLOCK_SIZE);
    }

    name_index = get_options(argc, argv);

    if (vflag) {
	printf("version " VERSION " (" RELEASE_DATE ")\n");
    }
    if (hflag) {
 	do_help();
    } else if (interactive) {
	interact();
    } else if (lflag) {
	if (lfile != NULL) {
	    dump(lfile);
	} else if (name_index < argc) {
	    while (name_index < argc) {
		dump(argv[name_index++]);
	    }
	} else {
	    list_all_disks();
	}
    } else if (name_index < argc) {
	while (name_index < argc) {
	    edit(argv[name_index++], 0);
	}
    } else if (!vflag) {
	usage("no device argument");
 	do_help();
    }
}
#else
void
main(int argc, char * argv[])
{
    if (sizeof(DPME) != PBLOCK_SIZE) {
	fatal(-1, "Size of partion map entry (%d) "
		"is not equal to block size (%d)\n",
		sizeof(DPME), PBLOCK_SIZE);
    }
    if (sizeof(Block0) != PBLOCK_SIZE) {
	fatal(-1, "Size of block zero structure (%d) "
		"is not equal to block size (%d)\n",
		sizeof(Block0), PBLOCK_SIZE);
    }
    if (argc > 1) {
	init_program_name(argv);
	interactive = 0;
	exit(do_command_line(argc - 1, argv + 1));
    }
	
    interactive = 1;

    init_program_name(NULL);

    interact();

    printf("The end\n");
    exit(0);
}
#endif

void
interact()
{
    char *name;
    int command;
    int first = 1;
    int ask_logical_size;

    while (get_command("Top level command (? for help): ", first, &command)) {
	first = 0;
	ask_logical_size = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  v    print the version number and release date\n");
	    printf("  l    list device's map\n");
	    printf("  L    list all devices' maps\n");
	    printf("  e    edit device's map\n");
	    printf("  r    toggle readonly flag\n");
	    printf("  a    toggle abbreviate flag\n");
	    printf("  p    toggle physical flag\n");
	    if (dflag) {
		printf("  d    toggle debug flag\n");
		printf("  x    examine block n of device\n");
	    }
	    printf("  q    quit the program\n");
	    break;
	case 'Q':
	case 'q':
	    return;
	    break;
	case 'V':
	case 'v':
	    printf("version " VERSION " (" RELEASE_DATE ")\n");
	    break;
	case 'L':
	    list_all_disks();
	    break;
	case 'l':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    dump(name);
	    free(name);
	    break;
	case 'E':
	    ask_logical_size = 1;
	case 'e':
	    if (get_string_argument("Name of device: ", &name, 1) == 0) {
		bad_input("Bad name");
		break;
	    }
	    edit(name, ask_logical_size);
	    free(name);
	    break;
	case 'R':
	case 'r':
	    if (rflag) {
		rflag = 0;
	    } else {
		rflag = 1;
	    }
	    printf("Now in %s mode.\n", (rflag)?"readonly":"read/write");
	    break;
	case 'A':
	case 'a':
	    if (aflag) {
		aflag = 0;
	    } else {
		aflag = 1;
	    }
	    printf("Now in %s mode.\n", (aflag)?"abbreviate":"full type");
	    break;
	case 'P':
	case 'p':
	    if (pflag) {
		pflag = 0;
	    } else {
		pflag = 1;
	    }
	    printf("Now in %s mode.\n", (pflag)?"physical":"logical");
	    break;
	case 'D':
	case 'd':
	    if (dflag) {
		dflag = 0;
	    } else {
		dflag = 1;
	    }
	    printf("Now in %s mode.\n", (dflag)?"debug":"normal");
	    break;
	case 'X':
	case 'x':
	    if (dflag) {
		do_display_block(0);
		break;
	    }
	default:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
}


#ifdef __linux__
int
get_options(int argc, char **argv)
{
    int c;
    static struct option long_options[] =
    {
	// name		has_arg			&flag	val
	{"help",	no_argument,		0,	'h'},
	{"list",	optional_argument,	0,	kListOption},
	{"version",	no_argument,		0,	'v'},
	{"debug",	no_argument,		0,	'd'},
	{"readonly",	no_argument,		0,	'r'},
	{"abbr",	no_argument,		0,	'a'},
	{"logical",	no_argument,		0,	kLogicalOption},
	{"interactive",	no_argument,		0,	'i'},
	{0, 0, 0, 0}
    };
    int option_index = 0;
    extern int optind;
    extern char *optarg;
    int flag = 0;

    init_program_name(argv);

    lflag = 0;
    lfile = NULL;
    vflag = 0;
    hflag = 0;
    dflag = 0;
    rflag = 0;
    aflag = 0;
    pflag = 1;
    interactive = 0;

    optind = 0;	// reset option scanner logic
    while ((c = getopt_long(argc, argv, "hlvdri", long_options,
	    &option_index)) >= 0) {
	switch (c) {
	case kLongOption:
	    // option_index would be used here
	    break;
	case 'h':
	    hflag = 1;
	    break;
	case kListOption:
	    if (optarg != NULL) {
		lfile = optarg;
	    }
	    // fall through
	case 'l':
	    lflag = 1;
	    break;
	case 'v':
	    vflag = 1;
	    break;
	case 'd':
	    dflag = 1;
	    break;
	case 'r':
	    rflag = 1;
	    break;
	case 'i':
	    interactive = 1;
	    break;
	case 'a':
	    aflag = 1;
	    break;
	case kLogicalOption:
	    pflag = 0;
	    break;
	case kBadOption:
	default:
	    flag = 1;
	    break;
	}
    }
    if (flag) {
	usage("bad arguments");
    }
    return optind;
}
#endif

//
// Edit the file
//
void
edit(char *name, int ask_logical_size)
{
    partition_map_header *map;
    int command;
#ifdef __unix__
    int first = 1;
#else
    int first = 0;
#endif
    int order;
    int get_type;
    int valid_file;

    map = open_partition_map(name, &valid_file, ask_logical_size);
    if (!valid_file) {
    	return;
    }

    printf("%s\n", name);

    while (get_command("Command (? for help): ", first, &command)) {
	first = 0;
	order = 1;
	get_type = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    help\n");
	    printf("  p    print the partition table\n");
	    printf("  P    (print ordered by base address)\n");
	    printf("  i    initialize partition map\n");
	    printf("  s    change size of partition map\n");
	    printf("  c    create new partition (standard MkLinux type)\n");
	    printf("  C    (create with type also specified)\n");
	    printf("  d    delete a partition\n");
	    printf("  r    reorder partition entry in map\n");
	    if (!rflag) {
		printf("  w    write the partition table\n");
	    }
	    printf("  q    quit editing (don't save changes)\n");
	    if (dflag) {
		printf("  x    extra extensions for experts\n");
	    }
	    break;
	case 'P':
	    order = 0;
	    // fall through
	case 'p':
	    dump_partition_map(map, order);
	    break;
	case 'Q':
	case 'q':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'I':
	case 'i':
	    map = init_partition_map(name, map);
	    break;
	case 'C':
	    get_type = 1;
	    // fall through
	case 'c':
	    do_create_partition(map, get_type);
	    break;
	case 'D':
	case 'd':
	    do_delete_partition(map);
	    break;
	case 'R':
	case 'r':
	    do_reorder(map);
	    break;
	case 'S':
	case 's':
	    do_change_map_size(map);
	    break;
	case 'X':
	case 'x':
	    if (!dflag) {
		goto do_error;
	    } else if (do_expert(map)) {
		flush_to_newline(0);
		goto finis;
	    }
	    break;
	case 'W':
	case 'w':
	    if (!rflag) {
		do_write_partition_map(map);
		break;
	    }
	default:
	do_error:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:

    close_partition_map(map);
}


void
do_create_partition(partition_map_header *map, int get_type)
{
    long base;
    long length;
    char *name;
    char *type_name;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
// XXX add help feature (i.e. '?' in any argument routine prints help string)
    if (get_base_argument(&base, map) == 0) {
	return;
    }
    if (get_size_argument(&length, map) == 0) {
	return;
    }

    if (get_string_argument("Name of partition: ", &name, 1) == 0) {
	bad_input("Bad name");
	return;
    }
    if (get_type == 0) {
	add_partition_to_map(name, kUnixType, base, length, map);
	goto xit1;
    } else if (get_string_argument("Type of partition: ", &type_name, 1) == 0) {
	bad_input("Bad type");
	goto xit1;
    } else {
	if (strncmp(type_name, kFreeType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Free type");
	    goto xit2;
	}
	if (strncmp(type_name, kMapType, DPISTRLEN) == 0) {
	    bad_input("Can't create a partition with the Map type");
	    goto xit2;
	}
	add_partition_to_map(name, type_name, base, length, map);
    }
xit2:
    free(type_name);
xit1:
    free(name);
    return;
}


int
get_base_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
    int c;
    int result = 0;

    if (get_number_argument("First block: ", number, kDefault) == 0) {
	bad_input("Bad block number");
    } else {
	result = 1;
	c = getch();

	if (c == 'p' || c == 'P') {
	    entry = find_entry_by_disk_address(*number, map);
	    if (entry == NULL) {
		bad_input("Bad partition number");
		result = 0;
	    } else {
		*number = entry->data->dpme_pblock_start;
	    }
	} else if (c > 0) {
	    ungetch(c);
	}
    }
    return result;
}


int
get_size_argument(long *number, partition_map_header *map)
{
    partition_map * entry;
    int c;
    int result = 0;
    long multiple;

    if (get_number_argument("Length in blocks: ", number, kDefault) == 0) {
	bad_input("Bad length");
    } else {
	result = 1;
	multiple = get_multiplier(map->logical_block);
	if (multiple != 1) {
	    *number *= multiple;
	} else {
	    c = getch();

	    if (c == 'p' || c == 'P') {
		entry = find_entry_by_disk_address(*number, map);
		if (entry == NULL) {
		    bad_input("Bad partition number");
		    result = 0;
		} else {
		    *number = entry->data->dpme_pblocks;
		}
	    } else if (c > 0) {
		ungetch(c);
	    }
	}
    }
    return result;
}


void
do_delete_partition(partition_map_header *map)
{
    partition_map * cur;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("Partition number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

	// find partition and delete it
    cur = find_entry_by_disk_address(index, map);
    if (cur == NULL) {
	printf("No such partition\n");
    } else {
	delete_partition_from_map(cur);
    }
}


void
do_reorder(partition_map_header *map)
{
    long old_index;
    long index;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("Partition number: ", &old_index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }
    if (get_number_argument("New number: ", &index, kDefault) == 0) {
	bad_input("Bad partition number");
	return;
    }

    move_entry_in_map(old_index, index, map);
}


void
do_write_partition_map(partition_map_header *map)
{
    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (map->changed == 0) {
	bad_input("The map has not been changed.");
	return;
    }
    if (map->writeable == 0) {
	bad_input("The map is not writeable.");
	return;
    }
    printf("Writing the map destroys what was there before. ");
    if (get_okay("Is that okay? [n/y]: ", 0) != 1) {
	return;
    }

    write_partition_map(map);

    // exit(0);
}

int
do_expert(partition_map_header *map)
{
    int command;
    int first = 0;
    int quit = 0;

    while (get_command("Expert command (? for help): ", first, &command)) {
	first = 0;

	switch (command) {
	case '?':
	    print_notes();
	case 'H':
	case 'h':
	    printf("Commands are:\n");
	    printf("  h    print help\n");
	    printf("  x    return to main menu\n");
	    printf("  d    dump block n\n");
	    printf("  p    print the partition table\n");
	    if (dflag) {
		printf("  P    (show data structures  - debugging)\n");
	    }
	    printf("  s    change size of partition map\n");
	    if (!rflag) {
		printf("  w    write the partition table\n");
	    }
	    printf("  q    quit without saving changes\n");
	    break;
	case 'X':
	case 'x':
	    flush_to_newline(1);
	    goto finis;
	    break;
	case 'Q':
	case 'q':
	    quit = 1;
	    goto finis;
	    break;
	case 'S':
	case 's':
	    do_change_map_size(map);
	    break;
	case 'P':
	    if (dflag) {
		show_data_structures(map);
		break;
	    }
	    // fall through
	case 'p':
	    dump_partition_map(map, 1);
	    break;
	case 'W':
	case 'w':
	    if (!rflag) {
		do_write_partition_map(map);
		break;
	    }
	case 'D':
	case 'd':
	    do_display_block(map);
	    break;
	default:
	    bad_input("No such command (%c)", command);
	    break;
	}
    }
finis:
    return quit;
}

void
do_change_map_size(partition_map_header *map)
{
    long size;

    if (map == NULL) {
	bad_input("No partition map exists");
	return;
    }
    if (!rflag && map->writeable == 0) {
	printf("The map is not writeable.\n");
    }
    if (get_number_argument("New size: ", &size, kDefault) == 0) {
	bad_input("Bad size");
	return;
    }
    resize_map(size, map);
}


void
print_notes()
{
    printf("Notes:\n");
    printf("  Base and length fields are blocks, which vary in size between media.\n");
    printf("  The name of a partition is descriptive text.\n");
    printf("\n");
}


void
do_display_block(partition_map_header *map)
{
    media *fd;
    long number;
    char *name;
    static unsigned char *display_block;
    int i;
    
    if (map == NULL) {
	if (get_string_argument("Name of device: ", &name, 1) == 0) {
	    bad_input("Bad name");
	    return;
	}
	fd = open_media(name, O_RDONLY);
	if (fd == 0) {
	    error(errno, "can't open file '%s'", name);
	    free(name);
	    return;
	}
    } else {
    	name = 0;
	fd = map->fd;
    }
    if (get_number_argument("Block number: ", &number, kDefault) == 0) {
	bad_input("Bad block number");
	goto xit;
    }
    if (display_block == NULL) {
	display_block = (unsigned char *) malloc(PBLOCK_SIZE);
	if (display_block == NULL) {
	    error(errno, "can't allocate memory for display block buffer");
	    goto xit;
	}
    }
#define LINE_LEN 32
#define UNIT_LEN  4
    if (read_media(map->fd, number, (char *)display_block, 0) != 0) {
	for (i = 0; i < PBLOCK_SIZE; i++) {
	    if (i % LINE_LEN == 0) {
		printf("\n%03x: ", i);
	    }
	    if (i % UNIT_LEN == 0) {
		printf(" ");
	    }
	    printf("%02x", display_block[i]);
	}
	printf("\n");
    }
xit:
    if (name) {
	close_media(fd);
	free(name);
    }
    return;
}
