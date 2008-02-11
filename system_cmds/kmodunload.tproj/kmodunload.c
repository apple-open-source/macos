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
 *	"kldunload.c,v 1.7 1998/11/07 00:42:52 des Exp"
 */

#ifndef lint
static const char rcsid[] =
	"$Id: kmodunload.c,v 1.5 2002/04/24 20:03:48 lindak Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>

static int verbose = 0;
#define v_printf	if (verbose) printf

static void
machwarn(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "kmodunload: %s: %s\n", message, mach_error_string(error));
}

static void
macherr(int error, const char *message)
{
	if (error == KERN_SUCCESS) return;
	fprintf(stderr, "kmodunload: %s: %s\n", message, mach_error_string(error));
	exit(1);
}

static mach_port_t kernel_priv_port;

static void
stop_module(kmod_t id)
{
	int r;
	void * args = 0;
	int argsCount= 0;

	r = kmod_control(kernel_priv_port, id, KMOD_CNTL_STOP, &args, &argsCount);
	macherr(r, "kmod_control(stop) failed");

	v_printf("kmodunload: kmod id %d successfully stopped.\n", id);
}

static void
unload_module(kmod_t id)
{
	int r;

	r = kmod_destroy(kernel_priv_port, id);
	macherr(r, "kmod_destroy() failed");

	v_printf("kmodunload: kmod id %d successfully unloaded.\n", id);
}

static void
usage(void)
{
	fprintf(stderr, "usage: kmodunload [-v] -i id\n");
	fprintf(stderr, "       kmodunload [-v] -n name\n");
	exit(1);
}

int
main(int argc, char** argv)
{
	int c;
	int id = 0;
	char* name = 0;
	kmod_info_t *info;
	int r;
	int count;
	mach_port_t kernel_port;

    fprintf(stderr, "%s is deprecated; use kextunload(8) instead\n", argv[0]);
    sleep(5);

	while ((c = getopt(argc, argv, "i:n:v")) != -1)
		switch (c) {
		case 'i':
			id = atoi(optarg);
			break;
		case 'n':
			name = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!id && !name && (argc == 1)) {
		name = *argv;
		argc--;
	}
    
	if ((argc != 0) || (id && name))
		usage();

	if ((id == 0) && (name == 0))
		usage();

	r = task_for_pid(mach_task_self(), 0, &kernel_port);
	machwarn(r, "unable to get kernel task port");
	if (r) {
		fprintf(stderr, "kmodunload: Are you running as root?\n");
		exit(1);
	}

	r = kmod_get_info(kernel_port, (void *)&info, &count);
	macherr(r, "kmod_get_info() failed");

	if (count < 1) {
		fprintf(stderr, "kmodunload: there is nothing to unload?\n");
		exit(1);
	}

	if (name) {
		kmod_info_t *k = info;
		while (k) {
			if (!strcmp(k->name, name)) {
				id = k->id;
				break;
			}
			k = (k->next) ? (k + 1) : 0;
		}
		if (!k) {
			fprintf(stderr, "kmodunload: can't kmod named: %s.\n", name);
			exit(1);
		}
	} else {
		kmod_info_t *k = info;
		while (k) {
			if (id == k->id) {
				name = k->name;
				break;
			}
			k = (k->next) ? (k + 1) : 0;
		}
		if (!name) {
			fprintf(stderr, "kmodunload: can't find kmod id %d.\n", id);
			exit(1);
		}
	}
	
	v_printf("kmodunload: found kmod %s, id %d.\n", name, id);
	kernel_priv_port = mach_host_self(); /* if we are privileged */

	stop_module(id);
	unload_module(id);

	return 0;
}

