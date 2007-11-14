/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

// ANSI / POSIX headers
#include <errno.h>		// for errno
#include <fcntl.h>		// for close()
#include <paths.h>		// for _PATH_VARRUN
#include <stdio.h>		// for fprintf(), stderr, et al
#include <unistd.h>		// for getopt(), getpid(), select() et al.
#include <sys/stat.h>
#include <c.h>
#include <servers/bootstrap.h>
#include <sys/sysctl.h>

#import "Listener.h"
#import "UserGroup.h"

#include "UDaemonHelpers.c"
#include "USimpleLog.c"

#include "memberd.h"

#ifndef DAEMON_NAME
#define DAEMON_NAME "memberd"
#endif

#define kMaxGUIDSInCacheClient 500
#define kMaxGUIDSInCacheServer 2500
#define kDefaultExpirationServer 1*60*60
#define kDefaultNegativeExpirationServer 30*60
#define kDefaultExpirationClient 4*60*60
#define kDefaultNegativeExpirationClient 2*60*60
#define kDefaultLoginExpiration 2*60
#define kDefaultLogSize 250
#define kMaxItemsInCacheStr "MaxItemsInCache"
#define kDefaultExpirationStr "DefaultExpirationInSecs"
#define kDefaultNegativeExpirationStr "DefaultFailureExpirationInSecs"
#define kDefaultLoginExpirationStr "DefaultLoginExpirationInSecs"
#define kDefaultLogSizeStr "NumLogEntries"

static const char * const	_PidPath = _PATH_VARRUN DAEMON_NAME ".pid";


static void _Usage (FILE *fp, const char *argv0)
{
	static const char * const	_szpUsage =
		"Version %2.1f\n"
		"Usage:\t%s [-dhvxsctrug]\n"
		"	-h		this message\n"
		"	-v		print build version and quit\n"
		"	-x		does not move to background\n"
		"	-d		same as -x but enables output to terminal\n"
		"	-s		prints usage statistics for running daemon\n"
		"	-c		clears statistics\n"
		"	-t		runs self test against running daemon\n"
		"	-r		resets the memberd cache\n"
		"	-u name maps user name to GUID\n"
		"	-g name maps group name to GUID\n"
		"	-l		dumps log to file\n"
		"	-L		dumps log and cache to file\n"
		;
    fprintf (fp, _szpUsage, BUILD_VERSION, argv0);
}

void PrintAverageTime(uint32_t numMicroSeconds)
{
	if (numMicroSeconds < 500)
		printf(" average time %d uSecs\n", numMicroSeconds);
	else if (numMicroSeconds < 10 * 1000)
	{
		int numMilliseconds = numMicroSeconds / 1000;
		int fraction = (numMicroSeconds % 1000) / 100;
		printf(" average time %d.%d mSecs\n", numMilliseconds, fraction);
	}
	else if (numMicroSeconds < 1000 * 1000)
	{
		int numMilliseconds = numMicroSeconds / 1000;
		printf(" average time %d mSecs\n", numMilliseconds);
	}
	else
	{
		int numSeconds = numMicroSeconds / 1000000;
		int fraction = (numMicroSeconds % 1000000) / 100000;
		printf(" average time %d.%d seconds\n", numSeconds, fraction);
	}
}

mach_port_t GetServerPort()
{
	kern_return_t result;
	static mach_port_t		bsPort = 0;
	static mach_port_t		fServerPort = 0;
	
	if (bsPort == 0)
	{
		result = task_get_bootstrap_port( mach_task_self(), &bsPort );
		result = bootstrap_look_up( bsPort, "com.apple.memberd", &fServerPort );

		if (result != MACH_MSG_SUCCESS)
		{
			printf("Got error %d on lookup (is memberd running?)\n", result);
			exit(0);
		}
	}

	return fServerPort;
}

void PrintStats()
{
	StatBlock stats;
	int temp, minutes, hours;
	_mbr_GetStats(GetServerPort(), &stats);

	temp = stats.fTotalUpTime / 60;
	minutes = temp % 60;
	temp /= 60;
	hours = temp % 24;
	temp /= 24;
	printf("memberd running for %d days, %d hours and %d minutes\n", temp, hours, minutes);
	printf("%d requests,", stats.fTotalCallsHandled);
	PrintAverageTime(stats.fAverageuSecPerCall);
	printf("%d cache hits, %d cache misses\n", stats.fCacheHits, stats.fCacheMisses);
	printf("%d ds record lookups (%d failed),", 
				stats.fTotalRecordLookups, stats.fNumFailedRecordLookups);
	PrintAverageTime(stats.fAverageuSecPerRecordLookup);
	printf("%d membership searches,", stats.fTotalMembershipSearches);
	PrintAverageTime(stats.fAverageuSecPerMembershipSearch);
	printf("%d searches for legacy groups,", stats.fTotalLegacySearches);
	PrintAverageTime(stats.fAverageuSecPerLegacySearch);
	printf("%d searches for groups containing user,", stats.fTotalGUIDMemberSearches);
	PrintAverageTime(stats.fAverageuSecPerGUIDMemberSearch);
	printf("%d nested group membership searches,", stats.fTotalNestedMemberSearches);
	PrintAverageTime(stats.fAverageuSecPerNestedMemberSearch);
}

void DoSelfTest()
{
	struct kauth_identity_extlookup request;
	u_int32_t flags;
	char guidStr[100];
	uint64_t time;
	
	request.el_seqno = 1;  // used as byte order field
	request.el_flags = KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_VALID_GID | 
				KAUTH_EXTLOOKUP_WANT_MEMBERSHIP | KAUTH_EXTLOOKUP_WANT_UGUID | KAUTH_EXTLOOKUP_WANT_GGUID;
	request.el_uid = 501;
	request.el_gid = 80;
	
	time = GetElapsedMicroSeconds();
	_mbr_DoMembershipCall(GetServerPort(), &request);
	time = GetElapsedMicroSeconds() - time;
	
	PrintAverageTime((uint32_t)time);
	
	printf("Returned flags:\n");
	flags = request.el_flags;
	if (flags & KAUTH_EXTLOOKUP_VALID_UID) printf("kHasUserIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_UGUID) printf("kHasUserGUIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_USID) printf("kHasUserSIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_GID) printf("kHasGroupIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_GGUID) printf("kHasGroupGUIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_GSID) printf("kHasGroupSIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_UID) printf("kWantUserIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_UGUID) printf("kWantUserGUIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_USID) printf("kWantUserSIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_GID) printf("kWantGroupIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_GGUID) printf("kWantGroupGUIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_GSID) printf("kWantGroupSIDMask\n");
	if (flags & KAUTH_EXTLOOKUP_WANT_MEMBERSHIP) printf("kWantMembership\n");
	if (flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) printf("kHasMembershipInfo\n");
	if (flags & KAUTH_EXTLOOKUP_ISMEMBER) printf("kIsMember\n");
//	printf("Returned flags = %X\n", (unsigned int)msg.mem.el_flags);
	ConvertGUIDToString(&request.el_uguid, guidStr);
	printf("User GUID = %s\n", guidStr);
	ConvertGUIDToString(&request.el_gguid, guidStr);
	printf("Group GUID = %s\n", guidStr);
}

void DoMapName(char* name, int isUser)
{
	uint64_t time = GetElapsedMicroSeconds();
	guid_t guid;
	char guidStr[100];
	kern_return_t result = _mbr_MapName(GetServerPort(), isUser, name, &guid);
	if (result != KERN_SUCCESS)
		printf("Lookup failed\n");
	else
	{
		ConvertGUIDToString(&guid, guidStr);
		printf("Found GUID = %s\n", guidStr);
	}
	time = GetElapsedMicroSeconds() - time;
	PrintAverageTime((uint32_t)time);
}

int IsServer()
{
	struct stat sb;
	return (stat("/System/Library/CoreServices/ServerVersion.plist", &sb) == 0);
}

void ReadConfigFile(int* maxCache, int* defaultExpiration, int* defaultNegExpiration, int* logSize, int* loginExpiration)
{
	char* path = "/etc/memberd.conf";
	struct stat sb;
	char buffer[1024];
	int fd;
	size_t len;
	int rewriteConfig = 0;
	
	if (IsServer())
	{
		*maxCache = kMaxGUIDSInCacheServer;
		*defaultExpiration = kDefaultExpirationServer;
		*defaultNegExpiration = kDefaultNegativeExpirationServer;
	}
	else
	{
		*maxCache = kMaxGUIDSInCacheClient;
		*defaultExpiration = kDefaultExpirationClient;
		*defaultNegExpiration = kDefaultNegativeExpirationClient;
	}
	*logSize = kDefaultLogSize;
	*loginExpiration = kDefaultLoginExpiration;
	
	int result = stat(path, &sb);
	
	if ((result != 0) || (sb.st_size > 1023))
		rewriteConfig = 1;
	else
	{
		fd = open(path, O_RDONLY, 0);
		if (fd < 0) return;
		len = read(fd, buffer, sb.st_size);
		close(fd);
		if (strncmp(buffer, "#1.1", 4) != 0)
			rewriteConfig = 1;
	}
	
	if (rewriteConfig)
	{
		fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0755);
		if (fd < 0) return;
		sprintf(buffer, "#1.1\n%s %d\n%s %d\n%s %d\n%s %d\n%s %d\n", kMaxItemsInCacheStr, *maxCache, 
												kDefaultExpirationStr, *defaultExpiration, 
												kDefaultNegativeExpirationStr, *defaultNegExpiration,
												kDefaultLogSizeStr, *logSize,
												kDefaultLoginExpirationStr, *loginExpiration);
		len = write(fd, buffer, strlen(buffer));
		close(fd);
	}
	else
	{
		char* temp;
		int i;
	
		if (len != sb.st_size) return;
		buffer[len] = '\0';
		
		for (i = 0; i < len; i++)
			if (buffer[i] == '\n') buffer[i] = '\0';
		
		i = 0;
		while (i < len)
		{
			temp = buffer + i;
			if (strncmp(temp, kMaxItemsInCacheStr, strlen(kMaxItemsInCacheStr)) == 0)
			{
				temp += strlen(kMaxItemsInCacheStr);
				*maxCache = strtol(temp, &temp, 10);
				if (*maxCache < 250)
					*maxCache = 250;
				else if (*maxCache > 1000000)
					*maxCache = 1000000;
			}
			else if (strncmp(temp, kDefaultExpirationStr, strlen(kDefaultExpirationStr)) == 0)
			{
				temp += strlen(kDefaultExpirationStr);
				*defaultExpiration = strtol(temp, &temp, 10);
				if (*defaultExpiration < 30)
					*defaultExpiration = 30;
				else if (*defaultExpiration > 24 * 60 * 60)
					*defaultExpiration = 24 * 60 * 60;
			}
			else if (strncmp(temp, kDefaultLogSizeStr, strlen(kDefaultLogSizeStr)) == 0)
			{
				temp += strlen(kDefaultLogSizeStr);
				*logSize = strtol(temp, &temp, 10);
				if (*logSize < 50)
					*logSize = 50;
				else if (*logSize > 50000)
					*logSize = 50000;
			}
			else if (strncmp(temp, kDefaultNegativeExpirationStr, strlen(kDefaultNegativeExpirationStr)) == 0)
			{
				temp += strlen(kDefaultNegativeExpirationStr);
				*defaultNegExpiration = strtol(temp, &temp, 10);
				if (*defaultNegExpiration < 30)
					*defaultNegExpiration = 30;
				else if (*defaultNegExpiration > 24 * 60 * 60)
					*defaultNegExpiration = 24 * 60 * 60;
			}
			else if (strncmp(temp, kDefaultLoginExpirationStr, strlen(kDefaultLoginExpirationStr)) == 0)
			{
				temp += strlen(kDefaultLoginExpirationStr);
				*loginExpiration = strtol(temp, &temp, 10);
				if (*loginExpiration < 30)
					*loginExpiration = 30;
				else if (*loginExpiration > 1 * 60 * 60)
					*loginExpiration = 1 * 60 * 60;
			}
			
			i += strlen(temp) + 1;
		}
	}
}

int main (int argc, char * const argv[]) {
	const char * const	argv0 = argv[0];
	static const char * const	argString = "dhvxsctru:g:lL";
	char		c;
	bool				daemonize = !gDebug;
	int maxCache, defExp, defNegExp, logSize, loginExp;
	int mib[6];
	int oldstate;
	size_t oldsize;
	int newstate;

    // Process command line arguments, if any
    while ((opterr == 1) && (c = getopt(argc, argv, argString)) != EOF) {
        switch (c) {
			case 'h':
				_Usage(stdout, argv0);
				exit (0);
			case 'v':
				printf("Version %2.1f\n", BUILD_VERSION);
				exit (0);
				
			case 'd':
				gDebug = true;
				/* FALLTHRU */
			case 'x':
				daemonize = false;
				break;
				
			case 's':
				PrintStats();
				exit(0);

			case 'c':
				_mbr_ClearStats(GetServerPort());
				exit(0);

			case 'r':
				_mbr_ClearCache(GetServerPort());
				exit(0);

			case 't':
				DoSelfTest();
				exit(0);
				
			case 'u':
				DoMapName(optarg, 1);
				exit(0);
				
			case 'g':
				DoMapName(optarg, 0);
				exit(0);
				
			case 'l':
				_mbr_DumpState(GetServerPort(), 1);
				exit(0);
				
			case 'L':
				_mbr_DumpState(GetServerPort(), 0);
				exit(0);
				
			default:
				_Usage(stderr, argv0);
				exit(1);
        }
    }

	if (!gDebug && getuid()) {
		fprintf(stderr, "%s must be executed as root.\n"
				 "See the man pages for %s, su, and sudo.\n", argv0, argv0);
		exit(1);
	}

	// Close / redirect all file descriptors.
	_CloseFileDescriptors (daemonize);

	openlog(DAEMON_NAME, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	syslog(LOG_NOTICE, "memberd starting up");

	// Everything looks good. Daemonize now.
	if (daemonize)
		_Detach();

	_WritePidFile(_PidPath);

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROCDELAYTERM;

	oldstate = 0;
	oldsize = 4;
	newstate  = 1;

	if (sysctl(mib, 2, &oldstate, &oldsize,  &newstate, 4) < 0) {
		syslog(LOG_INFO, "cannot mark pid for delayed termination");
	}

	ReadConfigFile(&maxCache, &defExp, &defNegExp, &logSize, &loginExp);
	InitializeUserGroup(maxCache, defExp, defNegExp, logSize, loginExp);
	InitializeListener();
	
	StartListeningKernel();
	StartListening(NULL);

	dprintf(LOG_INFO, "Shutting down.");
	_CloseLogFile();
    return 0;
}
