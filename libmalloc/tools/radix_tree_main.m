/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "../src/radix_tree_internal.h"

void
usage() {
	printf ("usage: radix-tree [-l 0xADDRESS | 0xSTART-0xEND] FILENAME\n");
	printf ("\n");
	printf ("This is a debugging tool for the radix-tree sidetable used to track VM allocations\n");
	printf ("under MallocStackLogging=lite.\n");
	printf ("\n");
	printf ("  radix-tree FILE                # print out radix tree as text\n");
	printf ("  radix-tree -l 0xf00 FILE       # lookup address in radix tree\n");
	printf ("  radix-tree -l 0xf00-0xba FILE  # lookup address range in radix tree\n");
	printf ("\n");
	exit(0);
}

uint64_t minsize = 4096;

int main(int argc, char **argv) {
	int ch;
	uint64_t start = 0, end = 0;
	while ((ch = getopt(argc, argv, "l:")) != -1) {
		switch (ch) {
			case 'l': {
				char *p = strchr(optarg, '-');
				if (p) {
					*p = 0;
					end = strtoull(p+1, NULL, 16);
					start = strtoull(optarg, NULL, 16);
				} else {
					start = strtoull(optarg, NULL, 16);
				}
				if (start%minsize || end%minsize) {
					usage();
				}
			}
			break;

			default:
			case '?':
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	NSData *data;
	if (0==strcmp(argv[0], "-")) {
		data = [[NSFileHandle fileHandleWithStandardInput] readDataToEndOfFile];
	} else {
		data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[0]]];
	}
	if (!data) {
		fprintf(stderr, "failed to read data\n");
		return 1;
	}

	struct radix_tree *tree = (void*) [data bytes];
	radix_tree_fsck(tree);

	if (start != 0 && end != 0) {
		uint64_t last_stackid = -1;
		uint64_t last_start = 0;
		for (uint64_t a = start; a <= end; a += minsize) {
			uint64_t stackid = a == end ? -1 : radix_tree_lookup(tree, a);
			if (last_stackid != -1) {
				if (stackid != last_stackid) {
					printf ("[%llx-%llx] -> %llx\n", last_start, a, last_stackid);
					last_stackid = stackid;
					last_start = a;
				}
			} else {
				if (stackid != -1) {
					last_stackid = stackid;
					last_start = a;
				}
			}
		}
	} else if (start != 0) {
		printf ("%llx -> %llx\n", start, radix_tree_lookup(tree, start));
	} else {
		radix_tree_print(tree);
	}
}
