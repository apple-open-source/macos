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
/*-
 * Copyright (c) 1997 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Original code from:
 *	"kldstat.c,v 1.5 1998/11/07 00:29:09 des Exp";
 */

#ifndef lint
static const char rcsid[] =
	"$Id: kmodstat.c,v 1.4 2002/04/18 18:48:42 lindak Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>

static void
machwarn(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "kmodstat: %s: %s\n", message, mach_error_string(error));
}

static void
macherr(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "kmodstat: %s: %s\n", message, mach_error_string(error));
	exit(1);
}

static int
kmod_compare(const void *a, const void *b)
{
	return (((kmod_info_t *)a)->id - ((kmod_info_t *)b)->id);
}

static void
usage(void)
{
	fprintf(stderr, "usage: kmodstat [-i id] [-n name]\n");
	exit(1);
}

int
main(int argc, char** argv)
{
	int c, idset = 0, id = 0;
	char* name = 0;
	kmod_info_t *info, *k;
	kmod_reference_t *r;
	int i, j, rc, foundit, count, rcount;
	mach_port_t kernel_port;

    fprintf(stderr, "%s is deprecated; use kextstat(8) instead\n", argv[0]);
    sleep(5);

	while ((c = getopt(argc, argv, "i:n:")) != -1)
		switch (c) {
		case 'i':
			idset++;
			id = atoi(optarg);
			break;
		case 'n':
			name = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!idset && !name && (argc == 1)) {
		name = *argv;
		argc--;
	}
    
	if (argc != 0) usage();

	rc = task_for_pid(mach_task_self(), 0, &kernel_port);
	machwarn(rc, "unable to get kernel task port");
	if (rc) {
		fprintf(stderr, "kmodstat: Are you running as root?\n");
		exit(1);
	}

	rc= kmod_get_info(kernel_port, (void *)&info, &count);
	macherr(rc, "kmod_get_info() failed");

	k = info; count = 0;
	while (k) {
		count++;
		k  = (k->next) ? (k + 1) : 0;
	}

	k = info; r = (kmod_reference_t *)(info + count);
	while (k) {
		if ((rcount = (int)k->reference_list)) {
			k->reference_list = r;
			for (i=0; i < rcount; i++) {
				foundit = 0;
				for (j=0; j < count; j++) {
					if (r->info == info[j].next) {
						r->info = (kmod_info_t *)info[j].id;
						foundit++;
						break;
					}
				}
				// force the id in here, the sorting below messes up the pointers
				if (!foundit) r->info = (kmod_info_t *)info[count - 1].id;
				r->next = r + 1;
				r++;
			}
			k->reference_list[rcount - 1].next = 0;
		}
		k  = (k->next) ? (k + 1) : 0;
	}

	printf("Id Refs Address    Size       Wired      Name (Version) <Linked Against>\n");

	if (!count) return 0;

	qsort(info, count, sizeof(kmod_info_t), kmod_compare);

	if (idset || name) {
		kmod_info_t *k = info;
		int match_count = 0;
		for (i=0; i < count; i++, k++) {
			if ((idset && id == k->id) || (name && !strcmp(k->name, name))) {
				info[match_count++] = *k;
			}
		}
		count = match_count;
	} 
	for (i=0; i < count; i++, info++) {
		printf("%2d %4d %-10p %-10p %-10p %s (%s)",
		       info->id, info->reference_count, (void *)info->address, 
		       (void *)info->size, (void *)(info->size - info->hdr_size),
		       info->name, info->version);

		if ((r = info->reference_list)) {
			printf(" <%d", (int)r->info);
			r = r->next;
			while (r) {
				printf(" %d", (int)r->info);
				r = r->next;
			}
			printf(">");
		}
		printf("\n");
	}

	return 0;
}
