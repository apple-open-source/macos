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
#include <stdio.h>
#ifndef __linux__
#include <stdlib.h>
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "io.h"
#include "errors.h"

#import "pdisk.h"
#import "partition_map.h"
#import "dump.h"

#define CMD_FAIL	1
#define CMD_SUCCESS	0

#define MIN_PARTITION_SIZE 4

typedef int (*funcPtr_t)();

typedef struct {
    char *		name;
    funcPtr_t		func;
    int			nargs;
    char *		description;
} cmdline_option_t;


static int block_size();
static int create_partition();
static int delete_partition();
static int disk_is_partitioned();
static int initialize();
static int our_dump();
static int partition_display();
static int split_partition();

cmdline_option_t optionTable[] = {
    { "-blockSize", block_size, 0, "display the block size used by the map" },
    { "-createPartition", create_partition, 0, "create a new partition" },
    { "-deletePartition", delete_partition, 0, "delete a partition" },
    { "-dump", our_dump, 0, "dump the list of partitions" },
    { "-initialize", initialize, 0, "initialize the partition map" },
    { "-isDiskPartitioned", disk_is_partitioned, 0, "is the disk partitioned" },
    { "-partitionEntry", partition_display, 0, "get the partition name, type, base, and size" },
    { "-splitPartition", split_partition, 0, "split an existing partition in two pieces" },
    { 0, 0, 0 },
};

static int
initialize(char * name)
{
    partition_map_header * map;

    map = create_partition_map(name, NULL, O_RDWR);
    if (map == NULL)
	return (CMD_FAIL);
    add_partition_to_map("Apple", kMapType,
			 1, (map->media_size <= 128? 2: 63), map);
    write_partition_map(map);
    close_partition_map(map);
    return (CMD_SUCCESS);
}

static int
block_size(char * name)
{
    partition_map_header *map;
    int junk;

    map = open_partition_map(name, &junk, 0, O_RDONLY);
    if (map == NULL) {
	return (CMD_FAIL);
    }
    printf("%d\n", map->logical_block);
    close_partition_map(map);
    return (CMD_SUCCESS);
}

static int
our_dump(char * name)
{
    partition_map_header *map;
    int junk;

    map = open_partition_map(name, &junk, 0, O_RDONLY);
    if (map == NULL) {
	return (CMD_FAIL);
    }

    dump_partition_map(map, 1);

    close_partition_map(map);
    return (CMD_SUCCESS);
}

static int
disk_is_partitioned(char * name)
{
    partition_map_header *map;
    int junk;

    map = open_partition_map(name, &junk, 0, O_RDONLY);
    if (map) {
	close_partition_map(map);
	return (CMD_SUCCESS);
    }
    return (CMD_FAIL);
}

static int
partition_display(char * name, int argc, char * * argv)
{
    partition_map * 		entry;
    int				junk;
    int				j;
    partition_map_header * 	map;
    int				rv = CMD_FAIL;

    if (argc < 2) {
	fprintf(stderr, "pdisk: %s <partition number>\n", *argv);
	return (CMD_FAIL);
    }

    map = open_partition_map(name, &junk, 0, O_RDONLY);
    if (!map) {
	fprintf(stderr, "pdisk: disk is not partitioned\n");
	return (CMD_FAIL);
    }
    j = atoi(argv[1]);
    entry = find_entry_by_disk_address(j, map);
    if (!entry) {
	fprintf(stderr, "pdisk: partition %d does not exist\n", j);
    }
    else {
	DPME * p = entry->data;

	rv = CMD_SUCCESS;

	printf("%s %s %u %u\n", p->dpme_name, p->dpme_type, 
	    p->dpme_pblock_start, p->dpme_pblocks);
    }
    close_partition_map(map);
    return (rv);
}

static int
split_partition(char * name, int argc, char * * argv)
{
    partition_map * 		entry;
    int				junk;
    partition_map_header * 	map;
    u32 			new_size;
    int				part;
    int				rv = CMD_SUCCESS;
    char *			split_name;
    char *			split_type;

    if (argc < 5) {
	fprintf(stderr, "pdisk %s <partno> <1st part size> <2nd part name> <2nd part type>\n",
	       argv[0]);
	return (CMD_FAIL);
    }
    map = open_partition_map(name, &junk, 0, O_RDWR);
    if (!map) {
	fprintf(stderr, "pdisk: no valid partitions exist\n");
	return (CMD_FAIL);
    }
    part = atoi(argv[1]);
    new_size = strtoul(argv[2], NULL, 10);
    split_name = argv[3];
    split_type = argv[4];
    entry = find_entry_by_disk_address(part, map);
    rv = CMD_FAIL;
    if (!entry) {
	fprintf(stderr, "pdisk: partition %d does not exist\n", part);
    }
    else if (strcmp(entry->data->dpme_type, kFreeType) == 0
	     || strcmp(entry->data->dpme_type, kMapType) == 0) {
	fprintf(stderr, "pdisk: cannot split partition %d because its type is %s\n",
	       part, entry->data->dpme_type);
    }
    else if (!((new_size > 0)
	       && (new_size 
		   <= (entry->data->dpme_pblocks - MIN_PARTITION_SIZE))
	       && (new_size >= MIN_PARTITION_SIZE))) {
	fprintf(stderr, "pdisk: split size of"
	       " partition %d must be between %u and %u\n",
	       part, (u32)MIN_PARTITION_SIZE, 
	       (entry->data->dpme_pblocks - (u32)MIN_PARTITION_SIZE));
    }
    else {
	DPME save_dpme;

	save_dpme = *entry->data;
	delete_partition_from_map(entry);
	add_partition_to_map(save_dpme.dpme_name, save_dpme.dpme_type,
			     save_dpme.dpme_pblock_start, new_size, map);
	add_partition_to_map(split_name, split_type, 
			     save_dpme.dpme_pblock_start + new_size,
			     save_dpme.dpme_pblocks - new_size, map);
	write_partition_map(map);

        entry = find_entry_by_base(save_dpme.dpme_pblock_start + new_size, map);
        if (entry) {
            printf("%lu\n", entry->disk_address);
        }
	rv = CMD_SUCCESS;
    }

    close_partition_map(map);
    return (rv);
}

static int
create_partition(char * name, int argc, char * * argv)
{
    partition_map * 		entry;
    int				junk;
    partition_map_header * 	map;
    int				rv = CMD_SUCCESS;
    u32				part_base;
    char *			part_name;
    u32 			part_size;
    char *			part_type;

    if (argc < 5) {
	fprintf(stderr, "%s <name> <type> <base> <size>\n",
	       argv[0]);
	return (CMD_FAIL);
    }
    map = open_partition_map(name, &junk, 0, O_RDWR);
    if (!map) {
	fprintf(stderr, "pdisk: disk is not partitioned\n");
	return (CMD_FAIL);
    }
    part_name = argv[1];
    part_type = argv[2];
    part_base = strtoul(argv[3], NULL, 10);
    part_size = strtoul(argv[4], NULL, 10);
    rv = CMD_FAIL;
    if (add_partition_to_map(part_name, part_type, part_base, part_size, map) 
	== 1) {
	write_partition_map(map);
	entry = find_entry_by_base(part_base, map);
	if (entry) {
	    printf("%lu\n", entry->disk_address);
	    rv = CMD_SUCCESS;
	}
    }
    close_partition_map(map);
    return (rv);
}

static int
delete_partition(char * name, int argc, char * * argv)
{
    partition_map * 		cur;
    int				junk;
    partition_map_header * 	map;
    int				rv = CMD_SUCCESS;
    u32				part_num;

    if (argc < 2) {
        fprintf(stderr, "%s <part>\n",
               argv[0]);
        return (CMD_FAIL);
    }
    map = open_partition_map(name, &junk, 0, O_RDWR);
    if (!map) {
        fprintf(stderr, "pdisk: disk is not partitioned\n");
        return (CMD_FAIL);
    }
    part_num = atoi(argv[1]);
    rv = CMD_FAIL;

    cur = find_entry_by_disk_address(part_num, map);
    if (cur == NULL) {
        fprintf(stderr, "No such partition\n");
    } else {
        delete_partition_from_map(cur);
    }
    write_partition_map(map);
    rv = CMD_SUCCESS;   
    
    close_partition_map(map);
    return (rv);
}

void
do_command_help()
{
    cmdline_option_t * tbl_p;

    fprintf(stderr, "command:\n");
    for (tbl_p = optionTable; tbl_p->name; tbl_p++) {
	fprintf(stderr, "\t%-20s %s\n", tbl_p->name, tbl_p->description);
    }
}

int
do_command_line(int argc, char * argv[])
{
    char * 		devName = *argv;
    cmdline_option_t * 	options_p;

    argv++;
    argc--;
    if (!argc) {
        return (CMD_FAIL);
    }
    for (options_p = &optionTable[0]; options_p->func; options_p++) {
        if (strcmp(options_p->name, *argv) == 0) {
               exit ((options_p->func)(devName, argc, argv));
        }
    }
    return (CMD_FAIL);
}

