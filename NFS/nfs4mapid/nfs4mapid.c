/*
 * Copyright (c) 2014 Apple Inc. All rights reserved.
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
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/kauth.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <uuid/uuid.h>

extern int nfsclnt(int, void *);

void
Usage()
{
	errx(1, "Usage: %s {-g gid | -u uid} | [-G] name", getprogname());
}

void
print_map(struct nfs_testmapid *map)
{
	uuid_string_t guidstr;
	
	if (map->ntm_name2id) {
		printf("%s maps to %s id %d\n", map->ntm_name, 
		       map->ntm_grpflag ? "group" : "user",
		       map->ntm_id);
	} else {
		printf("%s id %d maps to %s\n",
		       map->ntm_grpflag ? "group" : "user",
		       map->ntm_id, map->ntm_name);
	}
	uuid_unparse(map->ntm_guid.g_guid, guidstr);
	printf("\tmapping done through guid %s\n", guidstr);
}

int
main(int argc, char *argv[])
{
	int opt;
	char *eptr = NULL;
	struct nfs_testmapid map;
	int id2name = 0;
	int error;
	
	memset(&map, 0, sizeof (map));
	
	while ((opt = getopt(argc, argv, "u:g:G")) != -1) {
		switch (opt) {
		case 'g': map.ntm_grpflag = 1;
		case 'u': map.ntm_id = (uint32_t)strtoul(optarg, &eptr, 0);
			if (*eptr)
				errx(1, "%s is not a valid uid/gid", optarg);
			if (map.ntm_name2id)
				Usage();
			id2name = 1;
			break;
		case 'G': map.ntm_grpflag = 1;
			if (id2name)
				Usage();
			map.ntm_name2id = 1;
			break;
		default:
			Usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!id2name) {
		if (argc != 1)
			Usage();
		memcpy(map.ntm_name, *argv, strlen(*argv));
		map.ntm_name2id = 1;
	} else if (argc != 0)
		Usage();

	error = nfsclnt(NFSCLNT_TESTIDMAP, &map);
	print_map(&map);

	if (error)
		err(1, "nfsclnt failed");
	return (0);
}

		
