/*-
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mtree.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.sbin/mtree/mtree.c,v 1.29 2004/06/04 19:29:28 ru Exp $");

#include <CoreFoundation/CoreFoundation.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "metrics.h"
#include "mtree.h"
#include "extern.h"

#define SECONDS_IN_A_DAY (60 * 60 * 24)

int ftsoptions = FTS_PHYSICAL;
int cflag, dflag, eflag, iflag, nflag, qflag, rflag, sflag, uflag, Uflag, wflag, mflag, tflag, xflag;
int insert_mod, insert_birth, insert_access, insert_change, insert_parent;
struct timespec ts;
u_int keys;
char fullpath[MAXPATHLEN];
CFMutableDictionaryRef dict;
char *filepath;

static void usage(void);
static bool write_plist_to_file(void);

static void
do_cleanup(void) {

	if (mflag) {
		if (dict)
			CFRelease(dict);
		if (filepath)
			free(filepath);
	}
}

int
main(int argc, char *argv[])
{
	int error = 0;
	int ch;
	char *dir, *p;
	int status;
	FILE *spec1, *spec2;
	char *timestamp = NULL;
	char *timeformat = "%FT%T";
	FILE *file = NULL;

	dir = NULL;
	keys = KEYDEFAULT;
	init_excludes();
	spec1 = stdin;
	spec2 = NULL;
	set_metric_start_time(time(NULL));

	atexit(do_cleanup);
	atexit(print_metrics_to_file);

	while ((ch = getopt(argc, argv, "cdef:iK:k:LnPp:qrs:UuwxX:m:F:t:E:S")) != -1)
		switch((char)ch) {
		case 'c':
			cflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'f':
			if (spec1 == stdin) {
				spec1 = fopen(optarg, "r");
				if (spec1 == NULL) {
					error = errno;
					RECORD_FAILURE(88, error);
					errc(1, error, "%s", optarg);
				}
			} else if (spec2 == NULL) {
				spec2 = fopen(optarg, "r");
				if (spec2 == NULL) {
					error = errno;
					RECORD_FAILURE(89, error);
					errc(1, error, "%s", optarg);
				}
			} else {
				RECORD_FAILURE(90, WARN_USAGE);
				usage();
			}
			break;
		case 'i':
			iflag = 1;
			break;
		case 'K':
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'k':
			keys = F_TYPE;
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keys |= parsekey(p, NULL);
			break;
		case 'L':
			ftsoptions &= ~FTS_PHYSICAL;
			ftsoptions |= FTS_LOGICAL;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'P':
			ftsoptions &= ~FTS_LOGICAL;
			ftsoptions |= FTS_PHYSICAL;
			break;
		case 'p':
			dir = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 's':
			sflag = 1;
			crc_total = (uint32_t)~strtoul(optarg, &p, 0);
			if (*p) {
				RECORD_FAILURE(91, WARN_USAGE);
				errx(1, "illegal seed value -- %s", optarg);
			}
			break;
		case 'U':
			Uflag = 1;
			uflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case 'x':
			ftsoptions |= FTS_XDEV;
			break;
		case 'X':
			read_excludes_file(optarg);
			break;
		case 'm':
			mflag = 1;
			filepath = strdup(optarg);
			dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
							 &kCFTypeDictionaryKeyCallBacks,
							 &kCFTypeDictionaryValueCallBacks);
			insert_access = insert_mod = insert_birth = insert_change = 0;
			break;
		case 'F':
			timeformat = optarg;
			break;
		case 't':
			timestamp = optarg;
			tflag = 1;
			break;
		case 'E':
			if (!strcmp(optarg, "-")) {
				file = stdout;
			} else {
				file = fopen(optarg, "w");
			}
			if (file == NULL) {
				warnx("Could not open metrics log file %s", optarg);
			} else {
				set_metrics_file(file);
			}
			break;
		case 'S':
			xflag = 1;
			break;
		case '?':
		default:
			RECORD_FAILURE(92, WARN_USAGE);
			usage();
		}
	argc -= optind;
//	argv += optind;

	if (argc) {
		RECORD_FAILURE(93, WARN_USAGE);
		usage();
	}

	if (timestamp) {
		struct tm t = {};
		char *r = strptime(timestamp, timeformat, &t);
		if (r && r[0] == '\0') {
			ts.tv_sec = mktime(&t);
			if ((ts.tv_sec - time(NULL)) > 30 * SECONDS_IN_A_DAY) {
				RECORD_FAILURE(94, WARN_TIME);
				errx(1, "Time is more then 30 days in the future");
			} else if (ts.tv_sec < 0) {
				RECORD_FAILURE(95, WARN_TIME);
				errx(1, "Time is too far in the past");
			}
		} else {
			RECORD_FAILURE(96, WARN_TIME);
			errx(1,"Cannot parse timestamp '%s' using format \"%s\"\n", timestamp, timeformat);
		}
	}

	if (dir && chdir(dir)) {
		error = errno;
		RECORD_FAILURE(97, error);
		errc(1, error, "%s", dir);
	}

	if ((cflag || sflag) && !getwd(fullpath)) {
		RECORD_FAILURE(98, errno);
		errx(1, "%s", fullpath);
	}

	if (dir) {
		set_metric_path(dir);
	}

	if (cflag) {
		cwalk();
		exit(0);
	}
	if (spec2 != NULL) {
		status = mtree_specspec(spec1, spec2);
		if (Uflag & (status == MISMATCHEXIT)) {
			status = 0;
		} else {
			RECORD_FAILURE(99, status);
		}
	} else {
		status = mtree_verifyspec(spec1);
		if (Uflag & (status == MISMATCHEXIT)) {
			status = 0;
		} else if (status) {
			RECORD_FAILURE(100, status);
		}
		if (mflag && CFDictionaryGetCount(dict)) {
			if (!write_plist_to_file()) {
				RECORD_FAILURE(101, EIO);
				errx(1, "could not write manifest to the file\n");
			}
		}
	}
	exit(status);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: mtree [-LPUScdeinqruxw] [-f spec] [-K key] [-k key] [-p path] [-s seed] [-m xml dictionary] [-t timestamp]\n"
"\t[-X excludes]\n");
	exit(1);
}

static bool
write_plist_to_file(void)
{
	if (!dict || !filepath) {
		RECORD_FAILURE(102, EINVAL);
		return false;
	}

	CFIndex bytes_written = 0;
	bool status = true;

	CFStringRef file_path_str = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)filepath, kCFStringEncodingUTF8);

	// Create a URL specifying the file to hold the XML data.
	CFURLRef fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
							 file_path_str,      	  // file path name
							 kCFURLPOSIXPathStyle,   // interpret as POSIX path
							 false);                // is it a directory?

	if (!fileURL) {
		CFRelease(file_path_str);
		RECORD_FAILURE(103, EINVAL);
		return false;
	}

	CFWriteStreamRef output_stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, fileURL);

	if (!output_stream) {
		CFRelease(file_path_str);
		CFRelease(fileURL);
		RECORD_FAILURE(104, EIO);
		return false;
	}

	if ((status = CFWriteStreamOpen(output_stream))) {
		bytes_written = CFPropertyListWrite((CFPropertyListRef)dict, output_stream, kCFPropertyListXMLFormat_v1_0, 0, NULL);
		CFWriteStreamClose(output_stream);
	} else {
		status = false;
		RECORD_FAILURE(105, EIO);
		goto out;
	}

	if (!bytes_written) {
		status = false;
		RECORD_FAILURE(106, EIO);
	}

out:
	CFRelease(output_stream);
	CFRelease(fileURL);
	CFRelease(file_path_str);

	return status;
}
