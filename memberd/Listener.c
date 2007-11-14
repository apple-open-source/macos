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

#import "Listener.h"
#import "MembershipResolver.h"
#import "UserGroup.h"

#import <sys/types.h>
#import <sys/syscall.h>
#import <stdlib.h>
#import <string.h>
#import <unistd.h>
#import <mach/mach_error.h>
#import <mach/mach_time.h>
#import <servers/bootstrap.h>
#import <stdio.h>
#import <sys/syslog.h>

// XXX just for testing
#ifndef SYS_identitysvc
# define SYS_identitysvc 293
#endif
#define KAUTH_EXTLOOKUP_REGISTER	(0)
#define KAUTH_EXTLOOKUP_RESULT		(1<<0)
#define KAUTH_EXTLOOKUP_WORKER		(1<<1)

// XXX end just for testing

mach_port_t gServerPort = MACH_PORT_NULL;
mach_port_t gBootStrapPort = MACH_PORT_NULL;

pthread_mutex_t gListenMutex;
pthread_mutex_t gProcessMutex;

pthread_t gMainThread;

uint32_t gScaleNumerator = 0;
uint32_t gScaleDenominator = 0;

StatBlock* gStatBlock = NULL;

int gThreadsWaiting = 0;

char *gServiceName = "com.apple.memberd";

bool gDebug = false;

pthread_key_t gThreadKey;

boolean_t server_active(mach_port_t *restart_service_port)
{
        mach_port_t             bootstrap_port;
        kern_return_t           status;

        /* Getting bootstrap server port */
        status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
        if (status != KERN_SUCCESS) {
                fprintf(stderr, "task_get_bootstrap_port(): %s\n",
                        mach_error_string(status));
                exit (1);
        }

        /* Check "memberd" server status */
        status = bootstrap_check_in(bootstrap_port, gServiceName, restart_service_port);
        switch (status) {
                case BOOTSTRAP_SUCCESS :
                        /* if we are being restarted by mach_init */
                        gBootStrapPort = bootstrap_port;
                        break;
                case BOOTSTRAP_SERVICE_ACTIVE :
                case BOOTSTRAP_NOT_PRIVILEGED :
                        /* if another instance of the server is active (or starting) */
//                        fprintf(stderr, "'%s' server already active\n",
//                                gServiceName);
                        return TRUE;
                case BOOTSTRAP_UNKNOWN_SERVICE :
                        /* if the server is not currently registered/active */
                        *restart_service_port = MACH_PORT_NULL;
                        break;
                default :
                        fprintf(stderr, "bootstrap_check_in() failed: status=%d\n", status);
                        exit (1);
        }

        return FALSE;
}


void server_init(mach_port_t restart_service_port, int enableRestart)
{
	mach_port_t             bootstrap_port;
	mach_port_t             service_port    = restart_service_port;
	kern_return_t           status;

	/* Getting bootstrap server port */
	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		fprintf(stderr, "task_get_bootstrap_port(): %s", mach_error_string(status));
		exit (1);
	}

	if (service_port == MACH_PORT_NULL) {
		mach_port_t     service_send_port;

		/* Check "memberd" server status */
		status = bootstrap_check_in(bootstrap_port, gServiceName, &service_port);
		switch (status) {
			case BOOTSTRAP_SUCCESS :
				/* if we are being restarted by mach_init */
				gBootStrapPort = bootstrap_port;
				break;
			case BOOTSTRAP_NOT_PRIVILEGED :
				/* if another instance of the server is starting */
				fprintf(stderr, "'%s' server already starting", gServiceName);
				exit (1);
			case BOOTSTRAP_UNKNOWN_SERVICE :
				/* service not currently registered, "a good thing" (tm) */
				if (enableRestart) {
					status = bootstrap_create_server(bootstrap_port,
													 "/usr/sbin/memberd",
													 geteuid(),
													 FALSE,         /* not onDemand == restart now */
													 &gBootStrapPort);
					if (status != BOOTSTRAP_SUCCESS) {
						fprintf(stderr, "bootstrap_create_server() failed: status=%d", status);
						exit (1);
					}
				} else {
					gBootStrapPort = bootstrap_port;
				}

				status = bootstrap_create_service(gBootStrapPort, gServiceName, &service_send_port);
				if (status != BOOTSTRAP_SUCCESS) {
					fprintf(stderr, "bootstrap_create_service() failed: status=%d", status);
					exit (1);
				}

				status = bootstrap_check_in(gBootStrapPort, gServiceName, &service_port);
				if (status != BOOTSTRAP_SUCCESS) {
					fprintf(stderr, "bootstrap_check_in() failed: status=%d", status);
					exit (1);
				}
				break;
			case BOOTSTRAP_SERVICE_ACTIVE :
				/* if another instance of the server is active */
				fprintf(stderr, "'%s' server already active", gServiceName);
				exit (1);
			default :
				fprintf(stderr, "bootstrap_check_in() failed: status=%d", status);
				exit (1);
		}
	}

	/* we don't want to pass our priviledged bootstrap port along to any spawned helpers so... */
	status = bootstrap_unprivileged(gBootStrapPort, &bootstrap_port);
	if (status != BOOTSTRAP_SUCCESS) {
			fprintf(stderr, "bootstrap_unprivileged() failed: status=%d", status);
			exit (1);
	}

	status = task_set_bootstrap_port(mach_task_self(), bootstrap_port);
	if (status != BOOTSTRAP_SUCCESS) {
			fprintf(stderr, "task_set_bootstrap_port(): %s",
					mach_error_string(status));
			exit (1);
	}

	gServerPort = service_port;

	return;
}

void InitializeListener()
{
	kern_return_t result = 0;
	mach_port_t server_port = MACH_PORT_NULL;
	
	if (server_active(&server_port))
	{
		fprintf(stderr, "memberd service already active\n");
		exit(1);
	}
	
	server_init(server_port, 1);

	// must be superuser when we make this call, could drop privs immediately afterwards
	if ((result = syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_REGISTER, 0)) != 0)
	{
		syslog(LOG_CRIT, "Got error %d trying to register with kernel\n", result);
		// XXX this is not really good, as whatever is starting us will just try again
		exit(1);
	}
	
	pthread_mutex_init (&gListenMutex, NULL) ;
	pthread_mutex_init (&gProcessMutex, NULL) ;
	pthread_key_create(&gThreadKey, NULL);
//	printf("senum = %lu, sdenom = %lu\n", mti.numer, mti.denom);
}

uint32_t GetElapsedSeconds()
{
	uint64_t elapsed = mach_absolute_time();
	if (gScaleNumerator == 0)
	{
		struct mach_timebase_info mti = {0};
		mach_timebase_info(&mti);
		gScaleNumerator = mti.numer;
		gScaleDenominator = mti.denom;
	}
	long double temp = (long double)(((long double)elapsed *
								(long double)gScaleNumerator)/(long double)gScaleDenominator);
	uint32_t elapsedSeconds = (uint32_t) (temp/(long double)NSEC_PER_SEC);
	return elapsedSeconds;
}

uint64_t GetElapsedMicroSeconds()
{
	uint64_t elapsed = mach_absolute_time();
	if (gScaleNumerator == 0)
	{
		struct mach_timebase_info mti = {0};
		mach_timebase_info(&mti);
		gScaleNumerator = mti.numer;
		gScaleDenominator = mti.denom;
	}
	long double temp = (long double)(((long double)elapsed *
								(long double)gScaleNumerator)/(long double)gScaleDenominator);
	uint64_t elapsedMicroSeconds = (uint64_t) (temp/(long double)NSEC_PER_USEC);
//	printf("returning %llu uSecs (raw = %llu)\n", elapsedMicroSeconds, elapsed);
	return elapsedMicroSeconds;
}

void AddToAverage(uint32_t* average, uint32_t* numDataPoints, uint32_t newDataPoint)
{
//	printf("average before %lu for %lu dps, new dp = %lu\n", *average, *numDataPoints, newDataPoint);
	*average = (((*average) * (*numDataPoints)) + newDataPoint) / (*numDataPoints + 1);
	*numDataPoints = *numDataPoints + 1;
//	printf("average after %lu for %lu dps\n", *average, *numDataPoints);
}

void* StartListening(void* dummy)
{
	int flags = 0;
	pthread_setspecific(gThreadKey, &flags);

	mach_msg_server(memberd_server, sizeof(kauth_identity_extlookup) + 1024,  gServerPort, 0);

	return NULL;
}

kern_return_t Server_mbr_DoMembershipCall( mach_port_t server, kauth_identity_extlookup *request)
{
	// mig calls all use seqno as a byte order field, so check if we need to swap
	int needsSwap = (request->el_seqno != 1) && (request->el_seqno == ntohl(1));
	
	if (needsSwap)
		SwapRequest(request);

	pthread_mutex_lock(&gProcessMutex);
	ProcessLookup(request);
	pthread_mutex_unlock(&gProcessMutex);

	if (needsSwap)
		SwapRequest(request);

	return KERN_SUCCESS;
}

kern_return_t Server_mbr_GetStats(mach_port_t server, StatBlock *stats)
{
	pthread_mutex_lock(&gProcessMutex);
	memcpy(stats, gStatBlock, sizeof(StatBlock));
	pthread_mutex_unlock(&gProcessMutex);
	stats->fTotalUpTime = GetElapsedSeconds() - stats->fTotalUpTime;
	
	return KERN_SUCCESS;
}

kern_return_t Server_mbr_ClearStats(mach_port_t server)
{
	pthread_mutex_lock(&gProcessMutex);
	memset(gStatBlock, 0, sizeof(StatBlock));
	pthread_mutex_unlock(&gProcessMutex);

	return KERN_SUCCESS;
}

kern_return_t Server_mbr_MapName(mach_port_t server, uint8_t isUser, string name, guid_t *guid)
{
	int result;
	pthread_mutex_lock(&gProcessMutex);
	result = ProcessMapName(isUser, name, guid);
	pthread_mutex_unlock(&gProcessMutex);

	return (kern_return_t)result;
}

kern_return_t Server_mbr_GetGroups(mach_port_t server, uint32_t uid, uint32_t* numGroups, GIDArray gids)
{
	int result;

	pthread_mutex_lock(&gProcessMutex);
	SetThreadFlags(kUseLoginTimeOutMask);
	result = ProcessGetGroups(uid, numGroups, gids);
	SetThreadFlags(0);
	pthread_mutex_unlock(&gProcessMutex);

	return (kern_return_t)result;
}

kern_return_t Server_mbr_ClearCache(mach_port_t server)
{
	pthread_mutex_lock(&gProcessMutex);
	ProcessResetCache();
	pthread_mutex_unlock(&gProcessMutex);

	return KERN_SUCCESS;
}

kern_return_t Server_mbr_DumpState(mach_port_t server, uint8_t logOnly)
{
	pthread_mutex_lock(&gProcessMutex);
	DumpState(logOnly);
	pthread_mutex_unlock(&gProcessMutex);

	return KERN_SUCCESS;
}

void *ListenKernel(void *dummy)
{
	kauth_identity_extlookup request;
	int result, workresult;
	int loop = 1;
	pthread_t newThread;
	int flags = 0;
	pthread_setspecific(gThreadKey, &flags);


	workresult = 0;
	while (loop) {
		// XXX need to check here for graceful shutdown, call in with result but without KAUTH_EXTLOOKUP_WORKER
		result = syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_WORKER | (workresult ? KAUTH_EXTLOOKUP_RESULT : 0), &request);
		if (result != 0)
		{
			syslog(LOG_CRIT, "Fatal error %d submitting to kernel (%d: %s)\n", result, errno, strerror(errno));
			exit(1);
		}
//		printf("Received request from kernel\n");
		pthread_mutex_lock(&gProcessMutex);
		gThreadsWaiting--;
		if (gThreadsWaiting == 0)
		{
			// This thread was the only thread waiting, so fire off another one
			gThreadsWaiting++;
			pthread_create(&newThread, NULL, ListenKernel, NULL);
		}
		ProcessLookup(&request);
		if (gThreadsWaiting >= 5)
			loop = 0;
		else
			gThreadsWaiting++;
		pthread_mutex_unlock(&gProcessMutex);
//		printf("replying to kernel\n");
		workresult = 1;
	}
	
	// send the last reply
	result = syscall(SYS_identitysvc, KAUTH_EXTLOOKUP_RESULT, &request);
	return NULL;
}

void StartListeningKernel(void)
{
	pthread_t newThread;
	
	gThreadsWaiting = 2;
	pthread_create(&newThread, NULL, ListenKernel, NULL);
	pthread_create(&newThread, NULL, ListenKernel, NULL);
}

int GetThreadFlags()
{
	int* flags = (int*)pthread_getspecific(gThreadKey);
	return *flags;
}

void SetThreadFlags(int flags)
{
	int* flagPtr = (int*)pthread_getspecific(gThreadKey);
	*flagPtr = flags;
}
