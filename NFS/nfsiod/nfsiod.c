/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define	NFS_DEFASYNCTHREAD 	16	/* Def. # nfsiod threads */
#define	NFS_MAXASYNCTHREAD 	128	/* max # nfsiod threads */

void
usage(void)
{
	fprintf(stderr, "usage: nfsiod [-n num_servers]\n");
	exit(1);
}

/*
 * nfsiod does asynchronous buffered I/O on behalf of the NFS client.
 * It does not have to be running for correct operation, but will
 * improve throughput.
 */
int
main(int argc, char *argv[])
{
	int ch, rv, num_servers, old_servers;
	size_t num_size, old_size;

	num_servers = old_servers = -1;
	num_size = sizeof(num_servers);
	old_size = sizeof(old_servers);

	while ((ch = getopt(argc, argv, "n:")) != EOF)
		switch (ch) {
		case 'n':
			num_servers = atoi(optarg);
			if (num_servers < 0)
				errx(1, "invalid count: %d", num_servers);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Backward compatibility, trailing number is the count of threads.
	 */
	if (argc > 1)
		usage();
	if (argc == 1) {
		num_servers = atoi(argv[0]);
		if (num_servers < 0)
			errx(1, "invalid count: %d", num_servers);
	}

	if (num_servers > NFS_MAXASYNCTHREAD) {
		warnx("nfsiod count %d; clamped to %d", num_servers, NFS_MAXASYNCTHREAD);
		num_servers = NFS_MAXASYNCTHREAD;
	}

	if (num_servers == -1)
		rv = sysctlbyname("vfs.generic.nfs.client.nfsiod_thread_max", &old_servers, &old_size, NULL, 0);
	else
		rv = sysctlbyname("vfs.generic.nfs.client.nfsiod_thread_max", &old_servers, &old_size, &num_servers, num_size);
	if (rv < 0)
		err(1, "sysctl(vfs.generic.nfs.client.nfsiod_thread_max)");

	printf("nfs.client.nfsiod_thread_max: ");
	printf("%d", old_servers);
	if (num_servers != -1)
		printf(" -> %d", num_servers);
	printf("\n");

	exit (0);
}
