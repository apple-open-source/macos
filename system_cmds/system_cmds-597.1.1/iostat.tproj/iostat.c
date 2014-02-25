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
/*
 * Copyright (c) 1997, 1998, 2000, 2001  Kenneth D. Merry
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD: src/usr.sbin/iostat/iostat.c,v 1.22 2001/09/01 07:40:19 kris Exp $
 */
/*
 * Parts of this program are derived from the original FreeBSD iostat
 * program:
 */
/*-
 * Copyright (c) 1986, 1991, 1993
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
/*
 * Ideas for the new iostat statistics output modes taken from the NetBSD
 * version of iostat:
 */
/*
 * Copyright (c) 1996 John M. Vinopal
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by John M. Vinopal.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define IOKIT	1	/* to get io_name_t in device_types.h */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>
#include <mach/mach_host.h>	/* host_statistics */
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXDRIVES	16	/* most drives we will record */
#define MAXDRIVENAME	31	/* largest drive name we allow */

struct drivestats {
	io_registry_entry_t	driver;
	char			name[MAXDRIVENAME + 1];
	u_int64_t		blocksize;
	u_int64_t		total_bytes;
	u_int64_t		total_transfers;
	u_int64_t		total_time;
};

static struct drivestats drivestat[MAXDRIVES];

static struct timeval	cur_time, last_time;

struct statinfo {
	long		tk_nin;
	long		tk_nout;
	host_cpu_load_info_data_t load;
};

static struct statinfo cur, last;

static mach_port_t host_priv_port;
static mach_port_t masterPort;

static int num_devices;
static int maxshowdevs;
static int dflag = 0, Iflag = 0, Cflag = 0, Tflag = 0, oflag = 0, Uflag = 0, Kflag = 0;
static volatile sig_atomic_t phdr_flag = 0;
static IONotificationPortRef notifyPort;

/* local function declarations */
static void usage(void);
static void phdr(int signo);
static void do_phdr();
static void devstats(int perf_select, long double etime, int havelast);
static void cpustats(void);
static void loadstats(void);
static int readvar(const char *name, void *ptr, size_t len);

static int record_all_devices(void);
static void record_drivelist(void* context, io_iterator_t drivelist);
static void remove_drivelist(void* context, io_iterator_t drivelist);
static int record_one_device(char *name);
static int record_device(io_registry_entry_t drive);

static int compare_drivestats(const void* pa, const void* pb);

static long double compute_etime(struct timeval cur_time, 
				 struct timeval prev_time);

static void
usage(void)
{
	/*
	 * We also support the following 'traditional' syntax:
	 * iostat [drives] [wait [count]]
	 * This isn't mentioned in the man page, or the usage statement,
	 * but it is supported.
	 */
	fprintf(stderr, "usage: iostat [-CUdIKoT?] [-c count] [-n devs]\n"
		"\t      [-w wait] [drives]\n");
}

int
main(int argc, char **argv)
{
	int c;
	int hflag = 0, cflag = 0, wflag = 0, nflag = 0;
	int count = 0, waittime = 0;
	int headercount;
	int num_devices_specified;
	int havelast = 0;

	CFRunLoopSourceRef rls;

	maxshowdevs = 3;

	while ((c = getopt(argc, argv, "c:CdIKM:n:oTUw:?")) != -1) {
		switch(c) {
			case 'c':
				cflag++;
				count = atoi(optarg);
				if (count < 1)
					errx(1, "count %d is < 1", count);
				break;
			case 'C':
				Cflag++;
				break;
			case 'd':
				dflag++;
				break;
			case 'I':
				Iflag++;
				break;
			case 'K':
				Kflag++;
				break;
			case 'n':
				nflag++;
				maxshowdevs = atoi(optarg);
				if (maxshowdevs < 0)
					errx(1, "number of devices %d is < 0",
					     maxshowdevs);
				break;
			case 'o':
				oflag++;
				break;
			case 'T':
				Tflag++;
				break;
			case 'U':
				Uflag++;
				break;
			case 'w':
				wflag++;
				waittime = atoi(optarg);
				if (waittime < 1)
					errx(1, "wait time is < 1");
				break;
			default:
				usage();
				exit(1);
				break;
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * Get the Mach private port.
	 */
	host_priv_port = mach_host_self();

	/*
	 * Get the I/O Kit communication handle.
	 */
	IOMasterPort(bootstrap_port, &masterPort);

	notifyPort = IONotificationPortCreate(masterPort);
	rls = IONotificationPortGetRunLoopSource(notifyPort);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	/*
	 * Make sure Tflag, Cflag and Uflag are set if dflag == 0.  If dflag is
	 * greater than 0, they may be 0 or non-zero.
	 */
	if (dflag == 0) {
		Cflag = 1;
		Tflag = 1;
		Uflag = 1;
	}

	/*
	 * TTY statistics are broken, disabling them.
	 */
	Tflag = 0;

	/*
	 * Figure out how many devices we should display if not given
	 * an explicit value.
	 */
	if (nflag == 0) {
		if (oflag > 0) {
			if ((dflag > 0) && (Cflag == 0) && (Tflag == 0))
				maxshowdevs = 5;
			else if ((dflag > 0) && (Tflag > 0) && (Cflag == 0))
				maxshowdevs = 5;
			else
				maxshowdevs = 4;
		} else {
			if ((dflag > 0) && (Cflag == 0))
				maxshowdevs = 4;		
			else
				maxshowdevs = 3;
		}
	}

	/*
	 * If the user specified any devices on the command line, record
	 * them for monitoring.
	 */
	for (num_devices_specified = 0; *argv; ++argv) {
		if (isdigit(**argv))
			break;
		if (record_one_device(*argv))
			errx(1, "can't record '%s' for monitoring", *argv);
		num_devices_specified++;
	}
	if (nflag == 0 && maxshowdevs < num_devices_specified)
		maxshowdevs = num_devices_specified;

	/* if no devices were specified, pick them ourselves */
	if ((num_devices_specified == 0) && record_all_devices())
		err(1, "can't find any devices to display");

	/*
	 * Look for the traditional wait time and count arguments.
	 */
	if (*argv) {
		waittime = atoi(*argv);

		/* Let the user know he goofed, but keep going anyway */
		if (wflag != 0) 
			warnx("discarding previous wait interval, using"
			      " %d instead", waittime);
		wflag++;

		if (*++argv) {
			count = atoi(*argv);
			if (cflag != 0)
				warnx("discarding previous count, using %d"
				      " instead", count);
			cflag++;
		} else
			count = -1;
	}

	/*
	 * If the user specified a count, but not an interval, we default
	 * to an interval of 1 second.
	 */
	if ((wflag == 0) && (cflag > 0))
		waittime = 1;

	/*
	 * If the user specified a wait time, but not a count, we want to
	 * go on ad infinitum.  This can be redundant if the user uses the
	 * traditional method of specifying the wait, since in that case we
	 * already set count = -1 above.  Oh well.
	 */
	if ((wflag > 0) && (cflag == 0))
		count = -1;

	cur.tk_nout = 0;
	cur.tk_nin = 0;

	/*
	 * Set the busy time to the system boot time, so the stats are
	 * calculated since system boot.
	 */
	if (readvar("kern.boottime", &cur_time,	sizeof(cur_time)) != 0)
		exit(1);

	/*
	 * If the user stops the program (control-Z) and then resumes it,
	 * print out the header again.
	 */
	(void)signal(SIGCONT, phdr);

	for (headercount = 1;;) {
		long tmp;
		long double etime;

		if (Tflag > 0) {
			if ((readvar("kern.tty_nin", &cur.tk_nin,
			     sizeof(cur.tk_nin)) != 0)
			 || (readvar("kern.tty_nout",
			     &cur.tk_nout, sizeof(cur.tk_nout))!= 0)) {
				Tflag = 0;
				warnx("disabling TTY statistics");
			}
		 }

		if (!--headercount || phdr_flag) {
			phdr_flag = 0;
			headercount = 20;
			do_phdr();
		}
		
		last_time = cur_time;
		gettimeofday(&cur_time, NULL);

		if (Tflag > 0) {
			tmp = cur.tk_nin;
			cur.tk_nin -= last.tk_nin;
			last.tk_nin = tmp;
			tmp = cur.tk_nout;
			cur.tk_nout -= last.tk_nout;
			last.tk_nout = tmp;
		}

		etime = compute_etime(cur_time, last_time);

		if (etime == 0.0)
			etime = 1.0;

		if (Tflag > 0)
			printf("%4.0Lf%5.0Lf", cur.tk_nin / etime, 
				cur.tk_nout / etime);

		devstats(hflag, etime, havelast);

		if (Cflag > 0)
			cpustats();

		if (Uflag > 0)
			loadstats();

		printf("\n");
		fflush(stdout);

		if (count >= 0 && --count <= 0)
			break;

		/*
		 * Instead of sleep(waittime), wait in
		 * the RunLoop for IONotifications.
		 */
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, (CFTimeInterval)waittime, 1);

		havelast = 1;
	}

	exit(0);
}

static void
phdr(int signo)
{

	phdr_flag = 1;	
}

static void
do_phdr() 
{
	register int i;

	if (Tflag > 0)
		(void)printf("      tty");

	for (i = 0; i < num_devices && i < maxshowdevs; i++){
		if (oflag > 0)
			(void)printf("%12.6s ", drivestat[i].name);
		else
			printf("%15.6s ", drivestat[i].name);
	}
		
	if (Cflag > 0)
		(void)printf("      cpu");

	if (Uflag > 0)
		(void)printf("     load average\n");
	else
		(void)printf("\n");

	if (Tflag > 0)
		(void)printf(" tin tout");

	for (i=0; i < num_devices && i < maxshowdevs; i++){
		if (oflag > 0) {
			if (Iflag == 0)
				(void)printf(" sps tps msps ");
			else
				(void)printf(" blk xfr msps ");
		} else {
			if (Iflag == 0)
				printf("    KB/t tps  MB/s ");
			else
				printf("    KB/t xfrs   MB ");
		}
	}

	if (Cflag > 0)
		(void)printf(" us sy id");

	if (Uflag > 0)
		(void)printf("   1m   5m   15m\n");
	else
		printf("\n");
}

static void
devstats(int perf_select, long double etime, int havelast)
{
	CFNumberRef number;
	CFDictionaryRef properties;
	CFDictionaryRef statistics;
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	u_int64_t value;
	u_int64_t total_bytes, total_transfers, total_blocks, total_time;
	u_int64_t interval_bytes, interval_transfers, interval_blocks;
	u_int64_t interval_time;
	long double interval_mb;
	long double blocks_per_second, ms_per_transaction;
	kern_return_t status;
	int i;

	for (i = 0; i < num_devices && i < maxshowdevs; i++) {

		/*
		 * If the drive goes away, we may not get any properties
		 * for it.  So take some defaults.
		 */
		total_bytes = 0;
		total_transfers = 0;
		total_time = 0;

		/* get drive properties */
		status = IORegistryEntryCreateCFProperties(drivestat[i].driver,
			(CFMutableDictionaryRef *)&properties,
			kCFAllocatorDefault,
			kNilOptions);
		if (status != KERN_SUCCESS)
			continue;

		/* get statistics from properties */
		statistics = (CFDictionaryRef)CFDictionaryGetValue(properties,
			CFSTR(kIOBlockStorageDriverStatisticsKey));
		if (statistics) {

			/*
			 * Get I/O volume.
			 */
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_bytes += value;
			}
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_bytes += value;
			}

			/*
			 * Get I/O counts.
			 */
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsReadsKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_transfers += value;
			}
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsWritesKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_transfers += value;
			}

			/*
			 * Get I/O time.
			 */
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsLatentReadTimeKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_time += value;
			}
			if ((number = (CFNumberRef)CFDictionaryGetValue(statistics,
				CFSTR(kIOBlockStorageDriverStatisticsLatentWriteTimeKey)))) {
				CFNumberGetValue(number, kCFNumberSInt64Type, &value);
				total_time += value;
			}

		}
		CFRelease(properties);

		/*
		 * Compute delta values and stats.
		 */
		interval_bytes = total_bytes - drivestat[i].total_bytes;
		interval_transfers = total_transfers 
			- drivestat[i].total_transfers;
		interval_time = total_time - drivestat[i].total_time;

		/* update running totals, only once for -I */
		if ((Iflag == 0) || (drivestat[i].total_bytes == 0)) {
			drivestat[i].total_bytes = total_bytes;
			drivestat[i].total_transfers = total_transfers;
			drivestat[i].total_time = total_time;
		}				

		interval_blocks = interval_bytes / drivestat[i].blocksize;
		total_blocks = total_bytes / drivestat[i].blocksize;

		blocks_per_second = interval_blocks / etime;
		transfers_per_second = interval_transfers / etime;
		mb_per_second = (interval_bytes / etime) / (1024 * 1024);

		kb_per_transfer = (interval_transfers > 0) ?
			((long double)interval_bytes / interval_transfers) 
			/ 1024 : 0;

		/* times are in nanoseconds, convert to milliseconds */
		ms_per_transaction = (interval_transfers > 0) ?
			((long double)interval_time / interval_transfers) 
			/ 1000 : 0;

		if (Kflag)
			total_blocks = total_blocks * drivestat[i].blocksize 
				/ 1024;

		if (oflag > 0) {
			int msdig = (ms_per_transaction < 100.0) ? 1 : 0;

			if (Iflag == 0)
				printf("%4.0Lf%4.0Lf%5.*Lf ",
				       blocks_per_second,
				       transfers_per_second,
				       msdig,
				       ms_per_transaction);
			else 
				printf("%4.1qu%4.1qu%5.*Lf ",
				       interval_blocks,
				       interval_transfers,
				       msdig,
				       ms_per_transaction);
		} else {
			if (Iflag == 0)
				printf(" %7.2Lf %3.0Lf %5.2Lf ", 
				       kb_per_transfer,
				       transfers_per_second,
				       mb_per_second);
			else {
				interval_mb = interval_bytes;
				interval_mb /= 1024 * 1024;

				printf(" %7.2Lf %3.1qu %5.2Lf ", 
				       kb_per_transfer,
				       interval_transfers,
				       interval_mb);
			}
		}
	}
}

static void
cpustats(void)
{
	mach_msg_type_number_t count;
	kern_return_t status;
	double time;

	/*
	 * Get CPU usage counters.
	 */
	count = HOST_CPU_LOAD_INFO_COUNT;
	status = host_statistics(host_priv_port, HOST_CPU_LOAD_INFO,
		(host_info_t)&cur.load, &count);
	if (status != KERN_SUCCESS)
		errx(1, "couldn't fetch CPU stats");

	/*
	 * Make 'cur' fields relative, update 'last' fields to current values,
	 * calculate total elapsed time.
	 */
	time = 0.0;
	cur.load.cpu_ticks[CPU_STATE_USER]
		-= last.load.cpu_ticks[CPU_STATE_USER];
	last.load.cpu_ticks[CPU_STATE_USER]
		+= cur.load.cpu_ticks[CPU_STATE_USER];
	time += cur.load.cpu_ticks[CPU_STATE_USER];
	cur.load.cpu_ticks[CPU_STATE_SYSTEM]
		-= last.load.cpu_ticks[CPU_STATE_SYSTEM];
	last.load.cpu_ticks[CPU_STATE_SYSTEM]
		+= cur.load.cpu_ticks[CPU_STATE_SYSTEM];
	time += cur.load.cpu_ticks[CPU_STATE_SYSTEM];
	cur.load.cpu_ticks[CPU_STATE_IDLE]
		-= last.load.cpu_ticks[CPU_STATE_IDLE];
	last.load.cpu_ticks[CPU_STATE_IDLE]
		+= cur.load.cpu_ticks[CPU_STATE_IDLE];
	time += cur.load.cpu_ticks[CPU_STATE_IDLE];
	
	/*
	 * Print times.
	 */
#define PTIME(kind) { \
	double cpu = rint(100. * cur.load.cpu_ticks[kind] / (time ? time : 1));\
	 printf("%*.0f", (100 == cpu) ? 4 : 3, cpu); \
}
	PTIME(CPU_STATE_USER);
	PTIME(CPU_STATE_SYSTEM);
	PTIME(CPU_STATE_IDLE);
}

static void
loadstats(void)
{
	double loadavg[3];

	if(getloadavg(loadavg,3)!=3)
		errx(1, "couldn't fetch load average");

	printf("  %4.2f %4.2f %4.2f",loadavg[0],loadavg[1],loadavg[2]);
}

static int
readvar(const char *name, void *ptr, size_t len)
{
	int	oid[4];
	int	oidlen;

	size_t nlen = len;

	if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
		if (errno != ENOENT) {
			warn("sysctl(%s) failed", name);
			return (1);
		}
		/*
		 * XXX fallback code to deal with systems where
		 * sysctlbyname can't find "old" OIDs, should be removed.
		 */
		if (!strcmp(name, "kern.boottime")) {
			oid[0] = CTL_KERN;
			oid[1] = KERN_BOOTTIME;
			oidlen = 2;
		} else {
			warn("sysctl(%s) failed", name);
			return (1);
		}

		nlen = len;
		if (sysctl(oid, oidlen, ptr, &nlen, NULL, 0) == -1) {
			warn("sysctl(%s) failed", name);
			return (1);
		}
	}
	if (nlen != len) {
		warnx("sysctl(%s): expected %lu, got %lu", name,
		      (unsigned long)len, (unsigned long)nlen);
		return (1);
	}
	return (0);
}

static long double
compute_etime(struct timeval cur_time, struct timeval prev_time)
{
	struct timeval busy_time;
	u_int64_t busy_usec;
	long double etime;

	timersub(&cur_time, &prev_time, &busy_time);

	busy_usec = busy_time.tv_sec;  
	busy_usec *= 1000000;          
	busy_usec += busy_time.tv_usec;
	etime = busy_usec;
	etime /= 1000000;

	return(etime);
}

/*
 * Record all "whole" IOMedia objects as being interesting.
 */
static int
record_all_devices(void)
{
	io_iterator_t drivelist;
	CFMutableDictionaryRef match;
	kern_return_t status;

	/*
	 * Get an iterator for IOMedia objects.
	 */
	match = IOServiceMatching("IOMedia");
	CFDictionaryAddValue(match, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);

	CFRetain(match);
	status = IOServiceAddMatchingNotification(notifyPort, kIOFirstMatchNotification, match, &record_drivelist, NULL, &drivelist);

	if (status != KERN_SUCCESS)
		errx(1, "couldn't match whole IOMedia devices");

	/*
	 * Scan all of the IOMedia objects, and for each
	 * object that has a parent IOBlockStorageDriver, save
	 * the object's name and the parent (from which we can
	 * fetch statistics).
	 *
	 * XXX What about RAID devices?
	 */

	record_drivelist(NULL, drivelist);


	status = IOServiceAddMatchingNotification(notifyPort, kIOTerminatedNotification, match, &remove_drivelist, NULL, &drivelist);

	if (status != KERN_SUCCESS)
		errx(1, "couldn't match whole IOMedia device removal");

	remove_drivelist(NULL, drivelist);

	return(0);
}

static void record_drivelist(void* context, io_iterator_t drivelist)
{
	io_registry_entry_t drive;
	while ((drive = IOIteratorNext(drivelist))) {
		if (num_devices < MAXDRIVES) {
			record_device(drive);
			phdr_flag = 1;
		}
		IOObjectRelease(drive);
	}
	qsort(drivestat, num_devices, sizeof(struct drivestats), &compare_drivestats);
}

static void remove_drivelist(void* context, io_iterator_t drivelist)
{
	io_registry_entry_t drive;
	while ((drive = IOIteratorNext(drivelist))) {
		kern_return_t status;
		char bsdname[MAXDRIVENAME];
		CFDictionaryRef properties;
		CFStringRef name;

		/* get drive properties */
		status = IORegistryEntryCreateCFProperties(drive,
			(CFMutableDictionaryRef *)&properties,
			kCFAllocatorDefault,
			kNilOptions);
		if (status != KERN_SUCCESS) continue;

		/* get name from properties */
		name = (CFStringRef)CFDictionaryGetValue(properties,
			CFSTR(kIOBSDNameKey));

		if (name && CFStringGetCString(name, bsdname, MAXDRIVENAME, kCFStringEncodingUTF8)) {
			int i;
			for (i = 0; i < num_devices; ++i) {
				if (strcmp(bsdname,drivestat[i].name) == 0) {
					if (i < MAXDRIVES-1) {
						memmove(&drivestat[i], &drivestat[i+1], sizeof(struct drivestats)*(MAXDRIVES-i));
					}
					--num_devices;
					phdr_flag = 1;
					qsort(drivestat, num_devices, sizeof(struct drivestats), &compare_drivestats);
					break;
				}
			}
		}
		CFRelease(properties);
		IOObjectRelease(drive);
	}
}

/*
 * Try to record the named device as interesting.  It
 * must be an IOMedia device.
 */
static int
record_one_device(char *name)
{
	io_iterator_t drivelist;
	io_registry_entry_t drive;
	kern_return_t status;

	/*
	 * Find the device.
	 */
	status = IOServiceGetMatchingServices(masterPort,
		IOBSDNameMatching(masterPort, kNilOptions, name),
		&drivelist);
	if (status != KERN_SUCCESS)
		errx(1, "couldn't match '%s'", name);

	/*
	 * Get the first match (should only be one)
	 */
	if (!(drive = IOIteratorNext(drivelist)))
		errx(1, "'%s' not found", name);
	if (!IOObjectConformsTo(drive, "IOMedia"))
		errx(1, "'%s' is not a storage device", name);

	/*
	 * Record the device.
	 */
	if (record_device(drive))
		errx(1, "could not record '%s' for monitoring", name);

	IOObjectRelease(drive);
	IOObjectRelease(drivelist);

	return(0);
}

/*
 * Determine whether an IORegistryEntry refers to a valid
 * I/O device, and if so, record it.
 */
static int
record_device(io_registry_entry_t drive)
{
	io_registry_entry_t parent;
	CFDictionaryRef properties;
	CFStringRef name;
	CFNumberRef number;
	kern_return_t status;
	
	/* get drive's parent */
	status = IORegistryEntryGetParentEntry(drive,
		kIOServicePlane, &parent);
	if (status != KERN_SUCCESS)
		errx(1, "device has no parent");
	if (IOObjectConformsTo(parent, "IOBlockStorageDriver")) {
		drivestat[num_devices].driver = parent;

		/* get drive properties */
		status = IORegistryEntryCreateCFProperties(drive,
			(CFMutableDictionaryRef *)&properties,
			kCFAllocatorDefault,
			kNilOptions);
		if (status != KERN_SUCCESS)
			errx(1, "device has no properties");

		/* get name from properties */
		name = (CFStringRef)CFDictionaryGetValue(properties,
			CFSTR(kIOBSDNameKey));
		if (name)
			CFStringGetCString(name, drivestat[num_devices].name, 
					   MAXDRIVENAME, kCFStringEncodingUTF8);
		else {
			errx(1, "device does not have a BSD name");
		}

		/* get blocksize from properties */
		number = (CFNumberRef)CFDictionaryGetValue(properties,
			CFSTR(kIOMediaPreferredBlockSizeKey));
		if (number)
			CFNumberGetValue(number, kCFNumberSInt64Type,
					 &drivestat[num_devices].blocksize);
		else
			errx(1, "device does not have a preferred block size");

		/* clean up, return success */
		CFRelease(properties);
		num_devices++;
		return(0);
	}

	/* failed, don't keep parent */
	IOObjectRelease(parent);
	return(1);
}

static int
compare_drivestats(const void* pa, const void* pb)
{
	struct drivestats* a = (struct drivestats*)pa;
	struct drivestats* b = (struct drivestats*)pb;
	return strcmp(a->name, b->name);
}
