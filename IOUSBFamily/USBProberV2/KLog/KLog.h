/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef KLOG_H	
#define KLOG_H

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>

#include "KLogClient.h"

extern "C" {
    #include <sys/time.h>
}

//================================================================================================
//   Defines
//================================================================================================

#define kLogKextName 	"com_apple_iokit_KLog"

struct klog64_timeval{
	uint64_t	tv_sec;
	uint64_t	tv_usec;
};

//================================================================================================
//   Configuration constants
//================================================================================================

#define BUFSIZE 	512 	//bytes
#define MAXENTRIES	500
#define MAXUSERS 	5

//================================================================================================
//   Custom Types
//================================================================================================

typedef UInt32 KLogLevel;
typedef UInt32 KLogTag;

//================================================================================================
//   com_apple_iokit_KLog
//================================================================================================

class com_apple_iokit_KLog : public IOService
{

    OSDeclareDefaultStructors(com_apple_iokit_KLog)

    com_apple_iokit_KLogClient *mClientPtr[MAXUSERS+1];

    unsigned char *					mMsgBuffer;
    UInt8 							mClientCount;
    UInt16							mMsgSize;
    bool 							mErrFlag;
    struct klog64_timeval *			mTimeVal;
    IOLock *						mLogLock;
    
public:

    static com_apple_iokit_KLog	*	sLogger;
    
	// IOService overrides.
	
    virtual bool 		init(OSDictionary *dictionary = 0);
    virtual void 		free(void);
    
    virtual IOService *	probe(IOService *provider, SInt32 *score);
    virtual bool 		start(IOService *provider);
    virtual void 		stop(IOService *provider);
	
    void 				closeChild(com_apple_iokit_KLogClient *ptr);
    
    virtual IOReturn	newUserClient( task_t owningTask, void * securityID,
										UInt32 type, IOUserClient ** handler );

	// Class specific stuff.
	
	void 				setErr( bool set );

	// Write items into our buffer using these.

	virtual	SInt8		Log( KLogLevel level, KLogTag tag, const char *format, ... );
	virtual	SInt8		vLog( KLogLevel level, KLogTag tag, const char *format, va_list in_va_list );
};

#endif



