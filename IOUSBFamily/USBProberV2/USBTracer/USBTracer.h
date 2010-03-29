/*
 * Copyright © 2009 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
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

//—————————————————————————————————————————————————————————————————————————————
//	Includes
//—————————————————————————————————————————————————————————————————————————————

#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <spawn.h>

#include <mach/clock_types.h>
#include <mach/mach_time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/file.h>

#include <libutil.h>		// for reexec_to_match_kernel()
#include <mach/mach_host.h> // for host_info()

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBTracepoints.h>
	
#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif // KERNEL_PRIVATE

#include "IOUSBFamilyInfoPlist.pch"

#define DEBUG 			0

#ifndef USBTRACE_VERSION
	#define	USBTRACE_VERSION "100.4.0"
#endif

#define log(a,b,c,d,x,...)			if ( PrintHeader(a,b,c,d) ) { if (x){fprintf(stdout,x, ##__VA_ARGS__);} fprintf(stdout, "\n"); }
#define logs(a,b,c,x...)			log(a,b,c,0,x...)
#define	vlog(x...)					if ( gVerbose ) { fprintf(stdout,x); }
#define	elog(x...)					fprintf(stderr, x)

//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kTraceBufferSampleSize			65500
#define kMicrosecondsPerSecond			1000000
#define kMicrosecondsPerMillisecond		1000
#define kPrintMaskAllTracepoints		0x80000000
#define kTimeStringSize					17
#define kTimeStampKernel				0x1
#define kTimeStampLocalTime				0x2
#define kPrintStartToken				"->"
#define kPrintEndToken					"<-"
#define kPrintMedialToken				"  "
#define	kFilePathMaxSize				256
#define	kMicrosecondsPerCollectionDelay	20
#define gTimeStampDivisorString			"TimeStampDivisor="
#define kInvalid						0xdeadbeef
#define kDivisorEntry					0xfeedface
#define kKernelTraceCodes				"/usr/local/share/misc/trace.codes"

//—————————————————————————————————————————————————————————————————————————————
//	Types
//—————————————————————————————————————————————————————————————————————————————

typedef struct {
	uint64_t	timestamp;
	uintptr_t	thread;       /* will hold current thread */
	uint32_t	debugid;
	uint32_t	cpuid;
} trace_info;

typedef struct {
	uint64_t timestamp;
	uintptr_t arg1;
	uintptr_t arg2;
	uintptr_t arg3;
	uintptr_t arg4;
	uintptr_t arg5;
	uint32_t debugid;
	uint64_t delta;	// time delta between START and END
	uint32_t cpuid;
} raw_data_t;

//—————————————————————————————————————————————————————————————————————————————
//	Codes
//—————————————————————————————————————————————————————————————————————————————

#define kTPAllUSB					USB_TRACE ( 0, 0, 0 )

#pragma mark Prototypes
//———————————————————————————————————————————————————————————————————————————
//	Prototypes
//———————————————————————————————————————————————————————————————————————————

static void EnableUSBTracing ( void );
static void EnableTraceBuffer ( int val );
static void SignalHandler ( int signal );
static void GetDivisor ( void );
static void RegisterSignalHandlers ( void );
static void AllocateTraceBuffer ( void );
static void RemoveTraceBuffer ( void );
static void SetTraceBufferSize ( int nbufs );
static void GetTraceBufferInfo ( kbufinfo_t * val );
static void Quit ( const char * s );
static void ResetDebugFlags ( void );
static void InitializeTraceBuffer ( void );
static void Reinitialize ( void );
static void SetInterest ( unsigned int type );
static void ParseArguments ( int argc, const char * argv[] );
static void PrintUsage ( void );

static void CollectTrace ( void );
static void CollectWithAlloc( void );
static void ProcessTracepoint( kd_buf tracepoint );

static void CollectTraceController( kd_buf tracepoint );		
static void CollectTraceControllerUserClient( kd_buf tracepoint );
static void CollectTraceDevice ( kd_buf tracepoint ); //2,
static void CollectTraceDeviceUserClient ( kd_buf tracepoint ); //3,
static void CollectTraceHub ( kd_buf tracepoint ); //4,
static void CollectTraceHubPort ( kd_buf tracepoint ); //5,
static void CollectTraceHSHubUserClient ( kd_buf tracepoint ); //6,
static void CollectTraceHID	( kd_buf tracepoint ); //7,
static void CollectTracePipe ( kd_buf tracepoint ); //8,
static void CollectTraceInterfaceUserClient	( kd_buf tracepoint ); //9,

static void CollectTraceEnumeration( kd_buf tracepoint ); //10
// UIM groupings
static void CollectTraceUHCI ( kd_buf tracepoint ); //11,
static void CollectTraceUHCIUIM	( kd_buf tracepoint ); 
static void CollectTraceUHCIInterrupts ( kd_buf tracepoint ); //13,
static void CollectTraceOHCI ( kd_buf tracepoint ); //14,
static void CollectTraceOHCIInterrupts ( kd_buf tracepoint ); //15,
static void CollectTraceEHCI ( kd_buf tracepoint ); //20,
static void CollectTraceEHCIUIM	( kd_buf tracepoint ); //21,
static void CollectTraceEHCIHubInfo	( kd_buf tracepoint ); //22,
static void CollectTraceEHCIInterrupts	( kd_buf tracepoint ); //23,
// 21-25 reserved
static void CollectTraceHubPolicyMaker	( kd_buf tracepoint ); //35,
static void CollectTraceCompositeDriver ( kd_buf tracepoint ); //36,
// Actions
static void CollectTraceOutstandingIO ( kd_buf tracepoint ); //42,

// Other drivers
static void CollectTraceAudioDriver ( kd_buf tracepoint ); //50,

static void CollectTraceUnknown ( kd_buf tracepoint );

static void CollectCodeFile ( void );
static void ReadRawFile( const char * filepath );
static void CollectToRawFile ( FILE * file );
static void PrependDivisorEntry ( FILE * file );
static void CollectTraceUnknown ( kd_buf tracepoint );
static char * DecodeID ( uint32_t id, char * string, const int max );
static void CollectTraceBasic ( kd_buf tracepoint );

static char * ConvertTimestamp ( uint64_t timestamp, char * timestring );
static bool PrintHeader ( trace_info info, const char * group, const char * method, uintptr_t theThis );
static void TabIndent ( int numOfTabs );
static void Indent ( int numOfTabs );
static void IndentIn ( int numOfTabs );
static void IndentOut ( int numOfTabs );

const char * DecodeUSBTransferType( uint32_t type );
